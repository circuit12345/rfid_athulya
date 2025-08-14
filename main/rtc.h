#ifndef RTC_H
#define RTC_H
#include "global.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include <time.h>

#define I2C_MASTER_SCL_IO           22      // Change to your SCL pin
#define I2C_MASTER_SDA_IO           21      // Change to your SDA pin
#define I2C_MASTER_NUM              I2C_NUM_0
//#define I2C_MASTER_FREQ_HZ          100000
#define DS3231_ADDR                 0x68

esp_err_t rtc_hw084_init(void);
esp_err_t rtc_set_time_from_ntp(void);
esp_err_t rtc_get_time(struct tm *timeinfo);
// char *rtc_get_timestamp(void);
const char* rtc_get_timestamp(void);
esp_err_t rtc_sync_if_needed(uint32_t interval_hours);
void time_sync_task(void *arg);
void sntp_init_once();
#endif
