#include "spiffs_logger.h"
#include "call_manager.h"

static const char *LOG_PATH = "/spiffs/uid_log.txt";

esp_err_t spiffs_logger_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
    } else {
        size_t total = 0, used = 0;
        esp_spiffs_info(NULL, &total, &used);
        ESP_LOGI(TAG, "SPIFFS mounted. total: %d, used: %d", (int)total, (int)used);
    }
    return ret;
}

esp_err_t spiffs_logger_save(const char *json_line)
{
    FILE *f = fopen(LOG_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed open log for append");
        return ESP_FAIL;
    }
    fprintf(f, "%s\n", json_line);
    fclose(f);
    ESP_LOGI(TAG, "Saved log line");
    return ESP_OK;
}

esp_err_t spiffs_logger_send_all(void)
{
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) {
        ESP_LOGI(TAG, "No stored logs to send");
        return ESP_OK;
    }

    char line[1024];
    bool any = false;
    while (fgets(line, sizeof(line), f)) {
        any = true;
        /* trim newline */
        line[strcspn(line, "\r\n")] = 0;
        ESP_LOGI(TAG, "Sending stored: %s", line);
        esp_err_t res = http_client_send_json(line);
        if (res != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send stored line, will keep rest for next attempt");
            fclose(f);
            return res;
        }
        /* Give server time before next request */
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    fclose(f);

    if (any) {
        if (remove(LOG_PATH) == 0) {
            ESP_LOGI(TAG, "Stored log file deleted after successful sync");
        } else {
            ESP_LOGW(TAG, "Failed to delete stored log file");
        }
    }
    return ESP_OK;
}

// void spiffs_sync_task(void *pvParameters)
// {
//     for (;;)
//     {
//         /* Wait until WiFi connected (blocks) */
//         xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
//         ESP_LOGI(TAG, "WiFi connected; attempting to sync stored logs");
//         spiffs_logger_send_all();
//         //rtc_sync_if_needed(6);
//         /* Wait a bit before next sync in case new logs arrive rapidly */
//         vTaskDelay(pdMS_TO_TICKS(5000));
//     }
// }

// void spiffs_sync_task(void *pvParameters)
// {
//     TickType_t last_ota_check = 0;

//     for (;;)
//     {
//         // Wait until WiFi connected (blocks)
//         xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

//         ESP_LOGI(TAG, "WiFi connected; attempting to sync stored logs");
//         led_set_color_indefinite(LED_MAGENTA);
//         spiffs_logger_send_all();

//         TickType_t now = xTaskGetTickCount();
//         if (now - last_ota_check > pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS)) {
//             ESP_LOGI(TAG, "Time to check for OTA update");
//             esp_err_t err = ota_check_and_update();
//             if (err == ESP_OK) {
//                 ESP_LOGI(TAG, "OTA check/update finished");
//                 // If OTA succeeds, device will reboot immediately.
//             } else {
//                 ESP_LOGW(TAG, "OTA check/update failed or no update");
//             }
//             last_ota_check = now;
//         }

//         // Delay before next log sync iteration
//         vTaskDelay(pdMS_TO_TICKS(5000));
//     }
// }

void maintenance_task(void *pvParameters)
{
    TickType_t last_ota_check = 0;
    TickType_t last_time_sync = 0;

    while (1)
    {
        // Wait until WiFi connected (blocks until connected)
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                            pdFALSE, pdFALSE, portMAX_DELAY);

        // --- SPIFFS Log Sync ---
        ESP_LOGI(TAG, "WiFi connected; attempting to sync stored logs");
        // Only show sync status if no active call is blinking
        if (!is_call_blinking())
        {
            led_set_color_indefinite(LED_WHITE);
        }
        spiffs_logger_send_all();
        // if (!is_call_blinking())
        // {
        //     led_set_color_indefinite(LED_OFF);
        // }
        TickType_t now = xTaskGetTickCount();

        // --- Time Sync (every 1 hour) ---
        if ((now - last_time_sync) > pdMS_TO_TICKS(3600000)) // 1 hour
        {
            esp_err_t res = rtc_sync_if_needed(0);
            if (res == ESP_OK)
                ESP_LOGI(TAG, "Time sync successful");
            else
                ESP_LOGW(TAG, "Time sync failed");

            last_time_sync = now;
        }

        // --- OTA Check (every OTA_CHECK_INTERVAL_MS) ---
        if ((now - last_ota_check) > pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS))
        {
            ESP_LOGI(TAG, "Time to check for OTA update");
            esp_err_t err = ota_check_and_update();
            if (err == ESP_OK)
            {
                ESP_LOGI(TAG, "OTA check/update finished");
                // OTA success will reboot device automatically
            }
            else
            {
                ESP_LOGW(TAG, "OTA check/update failed or no update");
            }
            last_ota_check = now;
        }

        // --- Delay before next sync iteration ---
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5 sec loop
    }
}
