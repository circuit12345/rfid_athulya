#include "rfid.h"


static rc522_handle_t scanner = NULL;

/* rc522 event handler (similar to what you posted) */
void rc522_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    led_set_color(LED_GREEN);

    rc522_event_data_t *data = (rc522_event_data_t *)event_data;

    if (event_id == RC522_EVENT_TAG_SCANNED) {
        rc522_tag_t *tag = (rc522_tag_t *)data->ptr;

        if (tag->serial_number == 0) {
            ESP_LOGE(TAG, "Invalid serial number");
            return;
        }

        uint8_t uid_bytes[7];
        uid_bytes[0] = (tag->serial_number >> 32) & 0xFF;
        uid_bytes[1] = (tag->serial_number >> 24) & 0xFF;
        uid_bytes[2] = (tag->serial_number >> 16) & 0xFF;
        uid_bytes[3] = (tag->serial_number >> 8)  & 0xFF;
        uid_bytes[4] = (tag->serial_number)       & 0xFF;
        uid_bytes[5] = (tag->serial_number >> 48) & 0xFF;
        uid_bytes[6] = (tag->serial_number >> 56) & 0xFF;

        char uid_str[32];
        snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X%02X",
                 uid_bytes[4], uid_bytes[3], uid_bytes[2], uid_bytes[1], uid_bytes[0]);

        ESP_LOGI(TAG, "Tag scanned: UID = %s", uid_str);

        rfid_message_t msg;
        snprintf(msg.uid, UID_MAX_LEN, "%s", uid_str);
        snprintf(msg.timestamp, sizeof(msg.timestamp), "%lld", esp_timer_get_time() / 1000);
        /* Push to queue (blocking short time to avoid dropping if possible) */
        if (rfid_queue && xQueueSend(rfid_queue, &msg, pdMS_TO_TICKS(10)) == pdTRUE) {
            ESP_LOGI(TAG, "Queued UID");
        } else {
            ESP_LOGW(TAG, "Queue full or unavailable - saving to SPIFFS");
            /* Build JSON for storage */
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "uid", msg.uid);
            cJSON_AddStringToObject(root, "timestamp", msg.timestamp);
            cJSON_AddStringToObject(root, "placeType", g_placeType);
            cJSON_AddStringToObject(root, "room", g_roomNumber);
            cJSON_AddStringToObject(root, "location", g_location);
            cJSON_AddStringToObject(root, "tower", g_tower);
            cJSON_AddStringToObject(root, "floorNumber", g_floorNumber);
            char *json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            if (json) {
                spiffs_logger_save(json);
                free(json);
            }
            led_set_color(LED_OFF);

        }
    }
}

void rfid_init_module(void)
{
    rc522_config_t config = {
        .spi.host = VSPI_HOST,
        .spi.miso_gpio = 19,
        .spi.mosi_gpio = 23,
        .spi.sck_gpio = 18,
        .spi.sda_gpio = 5
    };

    ESP_ERROR_CHECK(rc522_create(&config, &scanner));
    ESP_ERROR_CHECK(rc522_register_events(scanner, RC522_EVENT_ANY, rc522_handler, NULL));
    ESP_ERROR_CHECK(rc522_start(scanner));
    ESP_LOGI(TAG, "RC522 scanner started");
}