#include "wifi.h"
static esp_netif_t *netif_ap = NULL;
//static esp_netif_t *netif_sta = NULL;
void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Toggle mode flag atomically (optional here, can do in task)
    is_ap_mode = !is_ap_mode;

    // Notify the task
    xSemaphoreGiveFromISR(wifi_switch_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// void switch_wifi_mode(bool apmode)
// {
//     ESP_LOGI(TAG, "Switching Wi-Fi mode to: %s", apmode ? "AP" : "STA");

//     // Stop WiFi if running
//     // ESP_ERROR_CHECK(esp_wifi_stop());
//     esp_err_t err = esp_wifi_stop();
//     if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT)
//     {
//         ESP_LOGE(TAG, "Failed to stop WiFi (0x%x)", err);
//         return; // or handle error accordingly
//     }
//     if (apmode)
//     {
//         wifi_init_ap();
//         start_webserver();
//     }
//     else
//     {
//         wifi_init_sta();
//     }
// }
void switch_wifi_mode(bool apmode)
{
    ESP_LOGI(TAG, "Switching Wi-Fi mode to: %s", apmode ? "AP" : "STA");

    ESP_ERROR_CHECK(esp_wifi_stop());

    if (apmode)
    {
        wifi_config_t ap_config = {
            .ap = {
                .ssid = {0},
                .ssid_len = 0,
                .max_connection = 4,
                .password = {0},
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            },
        };
        strncpy((char *)ap_config.ap.ssid, g_ap_ssid, sizeof(ap_config.ap.ssid) - 1);
        strncpy((char *)ap_config.ap.password, g_ap_pass, sizeof(ap_config.ap.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        start_webserver();
    }
    else
    {
        wifi_config_t sta_config = {
            .sta = {
                .ssid = {0},
                .password = {0},
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg = {
                    .capable = true,
                    .required = false,
                },
            },
        };

        strncpy((char *)sta_config.sta.ssid, g_ssid, sizeof(sta_config.sta.ssid) - 1);
        strncpy((char *)sta_config.sta.password, g_password, sizeof(sta_config.sta.password) - 1);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}

// void wifi_init_ap(void)
// {
//     // ESP_ERROR_CHECK(nvs_flash_init());
//     // ESP_ERROR_CHECK(esp_netif_init());
//     if (netif_ap)
//     {
//         esp_netif_destroy(netif_ap);
//         netif_ap = NULL;
//     }
//    wifi_event_group = xEventGroupCreate();
//     esp_netif_create_default_wifi_ap();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     wifi_config_t wifi_config = {
//         .ap = {
//             .ssid = "MyESP32_AP",
//             .ssid_len = 0,
//             .max_connection = 4,
//             .password = "12345678",
//             .authmode = WIFI_AUTH_WPA_WPA2_PSK},
//     };

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(TAG, "Wi-Fi AP started.");
// }

// void wifi_init_sta(void)
// {
// //         if (netif_sta)
// //     {
// //         esp_netif_destroy(netif_sta);
// //         netif_ap = NULL;
// //     }
// //    wifi_event_group = xEventGroupCreate();
//     esp_netif_create_default_wifi_sta();
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
//                                                         ESP_EVENT_ANY_ID,
//                                                         &wifi_event_handler,
//                                                         NULL,
//                                                         NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
//                                                         IP_EVENT_STA_GOT_IP,
//                                                         &wifi_event_handler,
//                                                         NULL,
//                                                         NULL));
//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = {0},
//             .password = {0},
//             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
//             .pmf_cfg = {
//                 .capable = true,
//                 .required = false},
//         },
//     };

//     // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     // ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     // ESP_ERROR_CHECK(esp_wifi_start());
//     strncpy((char *)wifi_config.sta.ssid, g_ssid, sizeof(wifi_config.sta.ssid) - 1);
//     strncpy((char *)wifi_config.sta.password, g_password, sizeof(wifi_config.sta.password) - 1);

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());

//     ESP_LOGI(TAG, "Wi-Fi STA started with SSID: %s", g_ssid);
// }

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
            esp_wifi_connect();
            ESP_LOGI(TAG, "Disconnected. Reconnecting...");
            break;
        case WIFI_EVENT_AP_START:
            // You can set event bits for AP start if needed
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGI(TAG, "Got IP. Connected.");
        }
    }
}
bool wifi_is_connected(void)
{
    return (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}


void save_ap_credentials_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_set_str(handle, "ap_ssid", g_ap_ssid);
        nvs_set_str(handle, "ap_pass", g_ap_pass);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGI("NVS", "AP credentials saved: SSID=%s, PASS=%s", g_ap_ssid, g_ap_pass);
    } else {
        ESP_LOGE("NVS", "Failed to open NVS for writing: %s", esp_err_to_name(err));
    }
}

void load_ap_credentials_from_nvs(void) {
    nvs_handle_t handle;
    size_t ssid_size = sizeof(g_ap_ssid);
    size_t pass_size = sizeof(g_ap_pass);

    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &handle);
    if (err == ESP_OK) {
        if (nvs_get_str(handle, "ap_ssid", g_ap_ssid, &ssid_size) != ESP_OK) {
            // Keep default from global.c
        }
        ssid_size = sizeof(g_ap_ssid); // reset before reuse

        if (nvs_get_str(handle, "ap_pass", g_ap_pass, &pass_size) != ESP_OK) {
            // Keep default from global.c
        }
        nvs_close(handle);

        ESP_LOGI("NVS", "AP credentials loaded: SSID=%s, PASS=%s", g_ap_ssid, g_ap_pass);
    } else {
        // Defaults from global.c will be used
        ESP_LOGE("NVS", "Failed to open NVS: %s, using defaults", esp_err_to_name(err));
    }
}

