#include "spiffs_logger.h"
#include "call_manager.h"
#include "nvs.h"  // for reboot scheduling
#include <time.h> // for daily reboot timing
#include "esp_mac.h"  // for ESP_MAC_WIFI_STA constant
#include "esp_system.h" // for esp_read_mac prototype

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

    // Create temporary file to store only failed entries
    const char *TEMP_PATH = "/spiffs/uid_log_temp.txt";
    FILE *temp = fopen(TEMP_PATH, "w");
    if (!temp) {
        ESP_LOGE(TAG, "Failed to create temp log file");
        fclose(f);
        return ESP_FAIL;
    }

    char line[1024];
    int sent_count = 0;
    int failed_count = 0;
    
    while (fgets(line, sizeof(line), f)) {
        /* trim newline */
        line[strcspn(line, "\r\n")] = 0;
        ESP_LOGI(TAG, "Sending stored: %s", line);
        esp_err_t res = http_client_send_json(line);
        if (res != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send line - will retry later");
            // Write failed line back to temp file for retry
            fprintf(temp, "%s\n", line);
            failed_count++;
        } else {
            ESP_LOGI(TAG, "Successfully sent line");
            sent_count++;
        }
        /* Give server time before next request */
        vTaskDelay(pdMS_TO_TICKS(800));
    }
    fclose(f);
    fclose(temp);

    // Replace original file with temp file (only if there were failures)
    if (failed_count > 0) {
        remove(LOG_PATH);
        rename(TEMP_PATH, LOG_PATH);
        ESP_LOGI(TAG, "Sync complete: sent=%d, failed=%d (keeping failed lines for retry)", sent_count, failed_count);
        return ESP_ERR_TIMEOUT;  // Signal that sync was partial
    } else {
        // All succeeded, delete both files
        remove(LOG_PATH);
        remove(TEMP_PATH);
        ESP_LOGI(TAG, "All %d stored logs sent successfully, cleaned up", sent_count);
        return ESP_OK;
    }
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

    /* daily reboot scheduling variables */
    bool reboot_configured = false;
    int daily_offset_minutes = 0;
    int last_reboot_day = -1;
    nvs_handle_t nvs_handle = 0;

    while (1)
    {
        // configure reboot info once after NVS is available
        if (!reboot_configured) {
            esp_err_t err = nvs_open("reboot", NVS_READWRITE, &nvs_handle);
            if (err == ESP_OK) {
                int32_t stored = 0;
                if (nvs_get_i32(nvs_handle, "offset", &stored) == ESP_OK) {
                    daily_offset_minutes = stored;
                } else {
                    uint8_t mac[6];
                    esp_read_mac(mac, ESP_MAC_WIFI_STA);
                    daily_offset_minutes = ((mac[5] << 8) | mac[4]) % 1440;
                    nvs_set_i32(nvs_handle, "offset", daily_offset_minutes);
                }
                if (nvs_get_i32(nvs_handle, "last_day", &stored) == ESP_OK) {
                    last_reboot_day = stored;
                } else {
                    last_reboot_day = -1;
                }
                nvs_commit(nvs_handle);
            } else {
                ESP_LOGW(TAG, "Unable to open NVS namespace for reboot: %s", esp_err_to_name(err));
            }
            reboot_configured = true;
            ESP_LOGI(TAG, "Reboot offset set to minute %d of each day", daily_offset_minutes);
        }


        // Wait until WiFi connected (blocks until connected)
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                            pdFALSE, pdFALSE, portMAX_DELAY);

        // --- SPIFFS Log Sync ---
        // ESP_LOGI(TAG, "WiFi connected; attempting to sync stored logs");
        if (!is_call_blinking())
        {
            led_set_color_indefinite(LED_WHITE);
        }
        spiffs_logger_send_all();
        TickType_t now = xTaskGetTickCount();

        // --- Daily reboot check ---
        {
            time_t t = time(NULL);
            struct tm tm;
            localtime_r(&t, &tm);
            int today = tm.tm_yday;
            int minute_of_day = tm.tm_hour * 60 + tm.tm_min;
            if (today != last_reboot_day && minute_of_day >= daily_offset_minutes) {
                ESP_LOGI(TAG, "Performing scheduled daily reboot (day %d minute %d)", today, minute_of_day);
                last_reboot_day = today;
                if (nvs_handle) {
                    nvs_set_i32(nvs_handle, "last_day", last_reboot_day);
                    nvs_commit(nvs_handle);
                }
                esp_restart();
            }
        }

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
