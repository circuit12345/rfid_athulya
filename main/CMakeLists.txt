
idf_component_register(
    SRCS "main.c" "global.c" "gpio.c" "nvs_handle.c" "rfid.c" "webserver.c" "wifi.c" "http_client.c" "spiffs_logger.c" "rtc.c" "ota.c" "led.c" "call_manager.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_system
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        driver
        esp_timer
        esp_http_client
        esp_https_ota
        esp_hw_support
        esp_pm
        esp_ringbuf
        esp_timer
        esp_adc
        esp_eth
        esp_http_server
        freertos
        vfs
        fatfs
        spiffs
        json
        mqtt
        lwip
        rc522
        app_update
)
