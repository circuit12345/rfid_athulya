#ifndef DS3231_H
#define DS3231_H

#include "driver/i2c.h"
#include "time.h"

// I2C address of the DS3231
#define DS3231_ADDR          0x68

// DS3231 register addresses
#define DS3231_REG_TIME    0x00  // The register to read time
#define DS3231_REG_TEMP    0x11  // The register to read temperature

// Convert BCD to decimal
static uint8_t bcd_to_decimal(uint8_t bcd);

// Function to initialize I2C and read time from DS3231
esp_err_t ds3231_get_time(struct tm *timeinfo);

#endif // DS3231_H
