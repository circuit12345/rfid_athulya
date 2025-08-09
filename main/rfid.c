#include "rfid.h"


static void rc522_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    rc522_event_data_t *data = (rc522_event_data_t *)event_data;

    switch (event_id) {
    case RC522_EVENT_TAG_SCANNED: {
        rc522_tag_t *tag = (rc522_tag_t *)data->ptr;

        // Check if the serial number is valid
        if (tag->serial_number == 0) {
            ESP_LOGE(TAG, "Invalid or empty serial number detected, skipping tag.");
            return;  // Skip invalid tag
        }

        // Debugging - Check the raw serial_number (UID)
        ESP_LOGI(TAG, "Raw Serial Number (UID): %016" PRIx64, tag->serial_number);

        // Extract the last 7 bytes from the 8-byte serial_number
        uint8_t uid[7];
        uid[0] = (tag->serial_number >> 32) & 0xFF;  // Extract byte 1
        uid[1] = (tag->serial_number >> 24) & 0xFF;  // Extract byte 2
        uid[2] = (tag->serial_number >> 16) & 0xFF;  // Extract byte 3
        uid[3] = (tag->serial_number >> 8) & 0xFF;   // Extract byte 4
        uid[4] = tag->serial_number & 0xFF;          // Extract byte 5
        uid[5] = (tag->serial_number >> 48) & 0xFF;  // Extract byte 6
        uid[6] = (tag->serial_number >> 56) & 0xFF;  // Extract byte 7

        // Format and print the 7-byte UID as a single hexadecimal string
        char uid_str[15];  // Enough space for 7 bytes and null terminator
        snprintf(uid_str, sizeof(uid_str), "%02X%02X%02X%02X%02X", 
                   uid[4], uid[3], uid[2], uid[1], uid[0]);

        // Save the UID string to a variable for future use
        static char saved_uid[15];  // Static variable to store UID string
        snprintf(saved_uid, sizeof(saved_uid), "%s", uid_str);

        // Print the formatted UID
        ESP_LOGI(TAG, "Tag scanned: UID = %s", saved_uid);

        // Optionally, you can now use `saved_uid` for any further processing
        // Example: Save to memory, send it over network, etc.

        break;
    }
    case RC522_EVENT_ANY:
        ESP_LOGI(TAG, "Tag removed");
        break;
    default:
        break;
    }
}

void rfid_int()
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
}