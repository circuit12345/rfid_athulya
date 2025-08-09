// ds3231.c
#include "ds3231.h"
#include "esp_log.h"

// Convert BCD to decimal
static uint8_t bcd_to_decimal(uint8_t bcd) {
    return (bcd / 16 * 10) + (bcd % 16);
}

// Function to get time from DS3231
esp_err_t ds3231_get_time(struct tm *timeinfo) {
    uint8_t data[7]; // Array to store time data
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start the I2C command
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);  // Send write command to DS3231
    i2c_master_write_byte(cmd, DS3231_REG_TIME, true);  // Write the register address (time register)
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);  // Send read command to DS3231
    i2c_master_read(cmd, data, 7, I2C_MASTER_ACK);  // Read 7 bytes of data (time)
    i2c_master_stop(cmd);

    // Increased timeout to 2000 ms (2 seconds)
    ESP_ERROR_CHECK(i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(2000)));  // Timeout increased to 2000 ms

    // Clean up the command handle
    i2c_cmd_link_delete(cmd);

    // Convert BCD values to decimal
    timeinfo->tm_sec = bcd_to_decimal(data[0] & 0x7F);   // Seconds
    timeinfo->tm_min = bcd_to_decimal(data[1] & 0x7F);   // Minutes
    timeinfo->tm_hour = bcd_to_decimal(data[2] & 0x3F);  // Hour
    timeinfo->tm_mday = bcd_to_decimal(data[4] & 0x3F);  // Day
    timeinfo->tm_mon = bcd_to_decimal(data[5] & 0x1F) - 1; // Month (0-based)
    timeinfo->tm_year = bcd_to_decimal(data[6]) + 100; // Year offset from 1900

    return ESP_OK;
}
