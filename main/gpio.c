#include "gpio.h"

void init_gpio()
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << APMODE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    is_ap_mode = (gpio_get_level(APMODE) == 0);
    switch_wifi_mode(is_ap_mode);

    if (wifi_switch_semaphore == NULL)
        wifi_switch_semaphore = xSemaphoreCreateBinary();

    esp_err_t ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) ESP_LOGE(TAG, "ISR service install failed: %d", ret);

    ret = gpio_isr_handler_add(APMODE, gpio_isr_handler, NULL);
    if (ret != ESP_OK) ESP_LOGE(TAG, "ISR handler add failed: %d", ret);


    
}
void wifi_mode_switch_task(void *arg)
{
    ESP_LOGI(TAG, "WiFi mode switch task started");
    while (1) {
        if (xSemaphoreTake(wifi_switch_semaphore, portMAX_DELAY)) {
            is_ap_mode = !is_ap_mode;
            ESP_LOGI(TAG, "Switching Wi-Fi mode to: %s", is_ap_mode ? "AP" : "STA");
            switch_wifi_mode(is_ap_mode);
        }
    }
}