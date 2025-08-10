#include "ota_handle.h"
void ota_fw_update_from_spiffs(const char *file_path)
{
    const esp_partition_t *ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!ota_partition)
    {
        ESP_LOGE(TAG, "No OTA partition found.");
        return;
    }

    ESP_LOGI(TAG, "Starting OTA from file: %s", file_path);

    FILE *file = fopen(file_path, "rb");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", file_path);
        return;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        fclose(file);
        return;
    }

    uint8_t buffer[1024];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        err = esp_ota_write(ota_handle, buffer, bytes_read);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
            fclose(file);
            esp_ota_end(ota_handle);
            return;
        }
    }

    fclose(file);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        return;
    }

    err = esp_ota_set_boot_partition(ota_partition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "OTA update successful! Restarting...");
    esp_restart();
}


// Handle OTA update trigger
esp_err_t ota_fw_update_handler(httpd_req_t *req)
{
    const char *dir_path = "/spiffs/"; // Directory to search
    char file_path[256];               // Increased buffer size to accommodate larger paths
    bool firmware_found = false;

    DIR *dir = opendir(dir_path);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", dir_path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open directory.");
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        if (strstr(entry->d_name, "SIMULATOR") != NULL)
        { // Look for "SIMULATOR" keyword in file name
            // Ensure snprintf does not exceed buffer size
            if (snprintf(file_path, sizeof(file_path), "%s%s", dir_path, entry->d_name) >= sizeof(file_path))
            {
                ESP_LOGE(TAG, "File path truncated: %s%s", dir_path, entry->d_name);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "File path too long.");
                closedir(dir);
                return ESP_FAIL;
            }
            firmware_found = true;
            break; // Stop after finding the first match
        }
    }
    closedir(dir);

    if (firmware_found)
    {
        ota_fw_update_from_spiffs(file_path);
        httpd_resp_send(req, "Firmware update started! Check logs.", HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        ESP_LOGW(TAG, "No firmware file found.");
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "No firmware file found.");
    }

    return ESP_OK;
}