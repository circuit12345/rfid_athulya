#include "global.h" 
void gpio_input_test_task(void *arg)
{
    while(1) {
        int level = gpio_get_level(APMODE);
        ESP_LOGI(TAG, "GPIO %d level = %d", APMODE, level);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
// void app_main(void)
// {
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
//     {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);
//     load_config_from_nvs();

//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());

//     wifi_event_group = xEventGroupCreate();

//     // Initialize Wi-Fi driver once
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     // Create default Wi-Fi station interface AFTER esp_wifi_init()
//     //esp_netif_create_default_wifi_sta();
//    // wifi_init_sta();

//     init_gpio(); // your GPIO init (independent of Wi-Fi)

//     ESP_LOGI(TAG, "APMODE GPIO Level: %d", gpio_get_level(GPIO_NUM_14));
//     vTaskDelay(pdMS_TO_TICKS(100));

//     /* Initialize queue BEFORE modules that may use it */
//     rfid_queue = xQueueCreate(RFID_QUEUE_LEN, sizeof(rfid_message_t));
//     if (!rfid_queue)
//     {
//         ESP_LOGE(TAG, "Failed to create RFID queue");
//         return;
//     }

//     /* Initialize SPIFFS */
//     if (spiffs_logger_init() != ESP_OK)
//     {
//         ESP_LOGW(TAG, "SPIFFS init failed - offline logging disabled");
//     }

//     /* Initialize RFID module after queue exists */
//     rfid_init_module();

//     /* Start tasks after Wi-Fi manager and RFID init */
//     xTaskCreate(http_send_task, "http_send_task", 8192, NULL, 5, NULL);
//     xTaskCreate(spiffs_sync_task, "spiffs_sync_task", 6144, NULL, 5, NULL);
//    // xTaskCreate(gpio_input_test_task, "gpio_input_test_task", 2048, NULL, 5, NULL);

//     ESP_LOGI(TAG, "Returned from app_main()");
// }
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    load_ap_credentials_from_nvs();
    ESP_LOGI("MAIN", "Starting AP with SSID=%s", g_ap_ssid);
    load_config_from_nvs();



    wifi_event_group = xEventGroupCreate();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

 // Register Wi-Fi event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Create both default interfaces ONCE here
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    init_gpio();

    //bool ap_mode = (gpio_get_level(GPIO_NUM_14) == 0); // your logic to pick mode
    switch_wifi_mode(false);
    
    rfid_queue = xQueueCreate(RFID_QUEUE_LEN, sizeof(rfid_message_t));
    if (!rfid_queue)
    {
        ESP_LOGE(TAG, "Failed to create RFID queue");
        return;
    }

    /* Initialize SPIFFS */
    if (spiffs_logger_init() != ESP_OK)
    {
        ESP_LOGW(TAG, "SPIFFS init failed - offline logging disabled");
    }

    /* Initialize RFID module after queue exists */
    rfid_init_module();

    /* Start tasks after Wi-Fi manager and RFID init */
    xTaskCreate(http_send_task, "http_send_task", 8192, NULL, 5, NULL);
    xTaskCreate(spiffs_sync_task, "spiffs_sync_task", 6144, NULL, 5, NULL);
    xTaskCreate(wifi_mode_switch_task, "wifi_switch_task", 4096, NULL, 5, NULL);
   // xTaskCreate(gpio_input_test_task, "gpio_input_test_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Returned from app_main()");
    // rest of your initialization...
}
