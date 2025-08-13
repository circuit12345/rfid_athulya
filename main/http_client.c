#include "http_client.h"


esp_err_t http_client_send_json(const char *json)
{
    if (!wifi_is_connected()) {
        ESP_LOGW(HTTP_TAG, "WiFi not connected, refusing to send");
        return ESP_ERR_INVALID_STATE;
    }

    esp_http_client_config_t config = {
        .url = WEB_SERVER_URL,
        .cert_pem = NULL, // No certificate
        .skip_cert_common_name_check = true, // Skip CN check
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .use_global_ca_store = false,
        .crt_bundle_attach = NULL,
        .auth_type = HTTP_AUTH_TYPE_NONE
    };
    
    
    // Disable verification (NOT recommended for production)
    config.skip_cert_common_name_check = true;
    
    

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(HTTP_TAG, "Failed to init http client");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_err_t err = esp_http_client_set_post_field(client, json, strlen(json));
    if (err != ESP_OK) {
        ESP_LOGE(HTTP_TAG, "set_post_field failed (%s)", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(HTTP_TAG, "HTTP POST Status = %d", status);
        if (status >= 200 && status < 300) {
            esp_http_client_cleanup(client);
            return ESP_OK;
        } else {
            ESP_LOGW(HTTP_TAG, "Server returned %d", status);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(HTTP_TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
}

void http_send_task(void *pvParameters)
{
    rfid_message_t msg;
    for (;;)
    {
        if (xQueueReceive(rfid_queue, &msg, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(HTTP_TAG, "Sending UID: %s", msg.uid);

            /* Build JSON */
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "uid", msg.uid);
            cJSON_AddStringToObject(root, "timestamp", rtc_get_timestamp());

            cJSON_AddStringToObject(root, "placeType", g_placeType);
            cJSON_AddStringToObject(root, "room", g_roomNumber);
            cJSON_AddStringToObject(root, "location", g_location);
            cJSON_AddStringToObject(root, "tower", g_tower);
            cJSON_AddStringToObject(root, "floorNumber", g_floorNumber);

            char *json_str = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            if (!json_str)
            {
                ESP_LOGE(HTTP_TAG, "Failed to create JSON");
                continue;
            }

            /* If connected, try send, otherwise save to SPIFFS */
            if (wifi_is_connected())
            {
                esp_err_t res = http_client_send_json(json_str);
                if (res != ESP_OK)
                {
                    ESP_LOGW(HTTP_TAG, "Send failed, saving to SPIFFS");
                    spiffs_logger_save(json_str);
                }
                else
                {
                    ESP_LOGI(HTTP_TAG, "Sent successfully");
                }
            }
            else
            {
                ESP_LOGI(HTTP_TAG, "WiFi not connected, saving to SPIFFS");
                spiffs_logger_save(json_str);
            }

            free(json_str);
        }
    }
}