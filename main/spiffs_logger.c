#include "spiffs_logger.h"

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

void spiffs_sync_task(void *pvParameters)
{
    for (;;)
    {
        /* Wait until WiFi connected (blocks) */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        ESP_LOGI(TAG, "WiFi connected; attempting to sync stored logs");
        spiffs_logger_send_all();
        /* Wait a bit before next sync in case new logs arrive rapidly */
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
