#include "rtc.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include "global.h"
//static const char *TAG = "RTC";
static char timestamp_str[25];  // YYYY-MM-DD HH:MM:SS

// Convert normal decimal numbers to binary coded decimal
static uint8_t decToBcd(uint8_t val) {
    return ((val / 10 * 16) + (val % 10));
}

// Convert binary coded decimal to normal decimal numbers
static uint8_t bcdToDec(uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

static esp_err_t ds3231_set_time(struct tm *timeinfo) {
    uint8_t buffer[7];
    buffer[0] = decToBcd(timeinfo->tm_sec);
    buffer[1] = decToBcd(timeinfo->tm_min);
    buffer[2] = decToBcd(timeinfo->tm_hour);
    buffer[3] = decToBcd(timeinfo->tm_wday ? timeinfo->tm_wday : 7);
    buffer[4] = decToBcd(timeinfo->tm_mday);
    buffer[5] = decToBcd(timeinfo->tm_mon + 1);
    buffer[6] = decToBcd(timeinfo->tm_year - 100);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true); // start at register 0
    i2c_master_write(cmd, buffer, sizeof(buffer), true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

static esp_err_t ds3231_get_time(struct tm *timeinfo) {
    uint8_t buffer[7];

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    for (int i = 0; i < 6; i++) {
        i2c_master_read_byte(cmd, &buffer[i], I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &buffer[6], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) return ret;

    timeinfo->tm_sec  = bcdToDec(buffer[0]);
    timeinfo->tm_min  = bcdToDec(buffer[1]);
    timeinfo->tm_hour = bcdToDec(buffer[2]);
    timeinfo->tm_wday = bcdToDec(buffer[3]);
    timeinfo->tm_mday = bcdToDec(buffer[4]);
    timeinfo->tm_mon  = bcdToDec(buffer[5]) - 1;
    timeinfo->tm_year = bcdToDec(buffer[6]) + 100;

    return ESP_OK;
}

esp_err_t rtc_hw084_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret != ESP_OK) return ret;

    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

esp_err_t rtc_set_time_from_ntp(void) {

    setenv("TZ", "IST-5:30", 1);
    tzset();

    int retry = 0;
    const int retry_count = 8;
    time_t now = 0;
    struct tm timeinfo = { 0 };

    while (timeinfo.tm_year < (2020 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGE(TAG, "Failed to get NTP time");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "NTP time acquired: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    return ds3231_set_time(&timeinfo);
}

esp_err_t rtc_get_time(struct tm *timeinfo) {
    return ds3231_get_time(timeinfo);
}

// char *rtc_get_timestamp(void) {
//     struct tm timeinfo;
//     if (rtc_get_time(&timeinfo) == ESP_OK) {
//         snprintf(timestamp_str, sizeof(timestamp_str),
//                  "%04d-%02d-%02d %02d:%02d:%02d",
//                  timeinfo.tm_year + 1900,
//                  timeinfo.tm_mon + 1,
//                  timeinfo.tm_mday,
//                  timeinfo.tm_hour,
//                  timeinfo.tm_min,
//                  timeinfo.tm_sec);
//     } else {
//         strcpy(timestamp_str, "0000-00-00 00:00:00");
//     }
//     return timestamp_str;
// }
const char* rtc_get_timestamp(void)
{
    struct tm timeinfo;
    if (rtc_get_time(&timeinfo) == ESP_OK) {
        strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        strcpy(timestamp_str, "0000-00-00 00:00:00");
    }
    return timestamp_str;
}
esp_err_t rtc_sync_if_needed(uint32_t interval_hours) {
    static time_t last_sync = 0;
    time_t now;
    time(&now);
    if (difftime(now, last_sync) >= (interval_hours * 3600)) {
        if (rtc_set_time_from_ntp() == ESP_OK) {
            last_sync = now;
            return ESP_OK;
        } else {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

void time_sync_task(void *arg) {
    while (1) {
        esp_err_t res = rtc_sync_if_needed(1); // sync every 1 hour
        if (res == ESP_OK) {
            ESP_LOGI(TAG, "Time sync successful");
        } else {
            ESP_LOGW(TAG, "Time sync failed");
        }
        vTaskDelay(pdMS_TO_TICKS(60000)); // wait 60 seconds before next check
    }
}

void sntp_init_once()
{
    ESP_LOGI(TAG, "Starting SNTP...");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}