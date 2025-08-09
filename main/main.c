#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "rc522.h"

static const char *TAG = "rc522-demo";
static rc522_handle_t scanner;

static void rc522_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    rc522_event_data_t *data = (rc522_event_data_t *)event_data;

    switch (event_id) {
    case RC522_EVENT_TAG_SCANNED: {
        rc522_tag_t *tag = (rc522_tag_t *)data->ptr;

        // v2.x exposes only the serial number (UID as 64-bit number)
        ESP_LOGI(TAG, "Tag scanned: SN (dec) = %" PRIu64, tag->serial_number);
        ESP_LOGI(TAG, "Tag scanned: SN (hex) = 0x%016" PRIx64, tag->serial_number);
        break;
    }
    case RC522_EVENT_ANY:
        ESP_LOGI(TAG, "Tag removed");
        break;
    default:
        break;
    }
}

void app_main(void)
{
    rc522_config_t config = {
        .spi.host      = VSPI_HOST,
        .spi.miso_gpio = 19,  // MISO
        .spi.mosi_gpio = 23,  // MOSI
        .spi.sck_gpio  = 18,  // SCK
        .spi.sda_gpio  = 5,   // SS (SDA on module)
       // .reset_gpio    = 26,  // RST
    };

    ESP_ERROR_CHECK(rc522_create(&config, &scanner));
    ESP_ERROR_CHECK(rc522_register_events(scanner, RC522_EVENT_ANY, rc522_handler, NULL));
    ESP_ERROR_CHECK(rc522_start(scanner));

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
