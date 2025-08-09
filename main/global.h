#ifndef GLOBAL_H
#define GLOBAL_H
#include "global.h"
#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "rc522.h"
#include "rfid.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "time.h"
#include "ds3231.h"
#include "rtc.h"

static const char *TAG = "rc522-demo";
static rc522_handle_t scanner;

#define I2C_MASTER_SCL_IO    22    /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO    21    /*!< GPIO number used for I2C master data */
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   1000000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define DS3231_ADDR          0x68   /*!< I2C address of the DS3231 RTC module */

//static const char *TAG = "DS3231";

#endif