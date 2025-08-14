#include "global.h"
#include "ota.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_err.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

//static const char *TAG = "OTA";

// Replace with your GitHub raw URL to the version JSON file
#define VERSION_JSON_URL "https://raw.githubusercontent.com/circuit12345/firmware/refs/heads/firmware/version.json"

#define OTA_BUFFER_SIZE 1024

typedef struct {
    char version[16];
    char firmware_url[256];
} ota_version_info_t;

// Download HTTP content fully into a buffer (caller must free)
static esp_err_t http_download_to_buffer(const char *url, uint8_t **out_buf, int *out_len)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(OTA_TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to open HTTP connection: %d", err);
        esp_http_client_cleanup(client);
        return err;
    }
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(OTA_TAG, "Invalid content length");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    uint8_t *buffer = malloc(content_length);
    if (!buffer) {
        ESP_LOGE(OTA_TAG, "Failed to allocate memory for download");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int read_len = 0;
    while (read_len < content_length) {
        int to_read = OTA_BUFFER_SIZE;
        if ((content_length - read_len) < OTA_BUFFER_SIZE) {
            to_read = content_length - read_len;
        }
        int ret = esp_http_client_read(client, (char *)buffer + read_len, to_read);
        if (ret <= 0) {
            ESP_LOGE(OTA_TAG, "Error in reading HTTP data");
            free(buffer);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        read_len += ret;
    }
    esp_http_client_cleanup(client);

    if (read_len != content_length) {
        ESP_LOGE(TAG, "Downloaded length mismatch");
        free(buffer);
        return ESP_FAIL;
    }

    *out_buf = buffer;
    *out_len = content_length;
    return ESP_OK;
}

// Parse JSON version info
static esp_err_t parse_version_json(const uint8_t *json_data, int json_len, ota_version_info_t *out_info)
{
    cJSON *root = cJSON_ParseWithLength((const char *)json_data, json_len);
    if (!root) {
        ESP_LOGE(OTA_TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON *ver = cJSON_GetObjectItem(root, "version");
    cJSON *fw = cJSON_GetObjectItem(root, "firmware_url");

    if (!ver || !fw) {
        ESP_LOGE(OTA_TAG, "Invalid JSON format");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    strncpy(out_info->version, ver->valuestring, sizeof(out_info->version) - 1);
    strncpy(out_info->firmware_url, fw->valuestring, sizeof(out_info->firmware_url) - 1);
    cJSON_Delete(root);
    return ESP_OK;
}

// Simple string compare to check if new_ver > current_ver
static bool is_newer_version(const char *new_ver, const char *cur_ver)
{
    return strcmp(new_ver, cur_ver) > 0;
}

// Check if free heap is enough for download buffer
static bool check_free_heap(size_t needed)
{
    size_t free = esp_get_free_heap_size();
    ESP_LOGI(OTA_TAG, "Free heap: %u bytes, Needed: %u bytes", free, (unsigned int)needed);
    return free > needed;
}

// Perform OTA update
static esp_err_t perform_ota_update(const char *firmware_url)
{
    ESP_LOGI(OTA_TAG, "Starting OTA update from %s", firmware_url);

    esp_http_client_config_t config = {
        .url = firmware_url,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(OTA_TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to open HTTP connection: %d", err);
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(OTA_TAG, "Invalid content length");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Check heap before downloading firmware
    if (!check_free_heap(OTA_BUFFER_SIZE * 2)) {
        ESP_LOGE(OTA_TAG, "Not enough free heap to download firmware");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(OTA_TAG, "No OTA partition found");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(OTA_TAG, "Writing to partition subtype %d at offset 0x%x",
        (unsigned int)update_partition->subtype,
        (unsigned int)update_partition->address);


    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "esp_ota_begin failed: %d", err);
        esp_http_client_cleanup(client);
        return err;
    }

    int data_read = 0;
    uint8_t ota_buffer[OTA_BUFFER_SIZE];
    bool success = true;

    while (data_read < content_length) {
        int to_read = OTA_BUFFER_SIZE;
        if ((content_length - data_read) < OTA_BUFFER_SIZE) {
            to_read = content_length - data_read;
        }

        int read_len = esp_http_client_read(client, (char *)ota_buffer, to_read);
        if (read_len <= 0) {
            ESP_LOGE(OTA_TAG, "HTTP read error");
            success = false;
            break;
        }

        err = esp_ota_write(ota_handle, (const void *)ota_buffer, read_len);
        if (err != ESP_OK) {
            ESP_LOGE(OTA_TAG, "esp_ota_write failed: %d", err);
            success = false;
            break;
        }

        data_read += read_len;
    }

    if (success) {
        err = esp_ota_end(ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(OTA_TAG, "esp_ota_end failed: %d", err);
            success = false;
        }
    } else {
        esp_ota_abort(ota_handle);
    }

    esp_http_client_cleanup(client);

    if (!success) {
        ESP_LOGE(OTA_TAG, "OTA update failed, rollback initiated");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "esp_ota_set_boot_partition failed: %d", err);
        return err;
    }

    ESP_LOGI(OTA_TAG, "OTA update successful, rebooting...");
    esp_restart();

    // Never reached
    return ESP_OK;
}

// Main function to check version and update
esp_err_t ota_check_and_update(void)
{
    ESP_LOGI(OTA_TAG, "Checking for firmware update...");

    uint8_t *json_buf = NULL;
    int json_len = 0;

    esp_err_t err = http_download_to_buffer(VERSION_JSON_URL, &json_buf, &json_len);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to download version JSON");
        return err;
    }

    ota_version_info_t remote_info = {0};
    err = parse_version_json(json_buf, json_len, &remote_info);
    free(json_buf);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "Failed to parse version JSON");
        return err;
    }

    ESP_LOGW(OTA_TAG, "Current version: %s, Available version: %s", APP_VERSION, remote_info.version);

    if (!is_newer_version(remote_info.version, APP_VERSION)) {
        ESP_LOGE(TAG, "No new firmware available");
        return ESP_OK;
    }

    ESP_LOGW(OTA_TAG, "New firmware available: %s", remote_info.version);

    return perform_ota_update(remote_info.firmware_url);
}
