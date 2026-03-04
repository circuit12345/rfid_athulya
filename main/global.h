#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>  // STANDARD I/O FUNCTIONS LIKE printf(), scanf()
#include <string.h> // STRING HANDLING FUNCTIONS LIKE strcpy(), strlen()
#include <time.h>
#include <stdlib.h>     // STANDARD LIBRARY FUNCTIONS LIKE malloc(), free(), atoi()
#include "esp_log.h"    // LOGGING FUNCTIONS PROVIDED BY ESP-IDF (e.g., ESP_LOGI, ESP_LOGE)
#include "esp_err.h"    // DEFINES ERROR CODES AND HANDLING FUNCTIONS
#include "nvs_flash.h"  // NON-VOLATILE STORAGE (NVS) FUNCTIONS FOR FLASH MEMORY
#include "esp_wifi.h"   // WIFI CONFIGURATION AND MANAGEMENT FUNCTIONS
#include "esp_event.h"  // EVENT LOOP LIBRARY FOR HANDLING SYSTEM EVENTS
#include "esp_spiffs.h" // SPIFFS FILE SYSTEM SUPPORT FOR ESP-IDF
#include "esp_timer.h"  // HIGH-RESOLUTION TIMER FUNCTIONS
#include <esp_system.h> // SYSTEM UTILITIES AND FUNCTIONS (e.g., RESET, UNIQUE CHIP ID)
#include <driver/gpio.h> // GPIO DRIVER FUNCTIONS FOR INPUT/OUTPUT OPERATIONS
#include "inttypes.h"          // FORMATTED PRINTING OF INTEGER TYPES (PRIx32, PRIu64, ETC.)
#include "esp_http_server.h"   // HTTP SERVER LIBRARY FOR WEB SERVER IMPLEMENTATION
#include "esp_event.h"         // (DUPLICATE INCLUDE) EVENT LOOP LIBRARY (CAN BE REMOVED)
#include "freertos/FreeRTOS.h" // FREE RTOS OPERATING SYSTEM CORE HEADER
#include "freertos/task.h"     // TASK MANAGEMENT FUNCTIONS FOR FREE RTOS
#include "freertos/semphr.h"   // SEMAPHORE FUNCTIONS FOR SYNCHRONIZATION IN FREE RTOS
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "dirent.h"      // DIRECTORY HANDLING FUNCTIONS (e.g., opendir, readdir, closedir)
#include "nvs.h"         // INCLUDES THE NON-VOLATILE STORAGE (NVS) API
#include "esp_ota_ops.h" // INCLUDES APIS FOR OVER - THE - AIR (OTA) FIRMWARE UPDATE OPERATIONS
#include "ctype.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "stdbool.h"
#include "nvs_handle.h" // Non-Volatile Storage (NVS) read/write utility functions
#include "wifi.h"       // Wi-Fi initialization, connection, and network configuration
#include "gpio.h"
#include "webserver.h"
#include "rfid.h"
#include "rc522.h"
#include "http_client.h"
#include "spiffs_logger.h"
#include "esp_netif.h"
#include "rtc.h"
#include "ota.h"
#include "led.h"
#include "mqtt_client.h"
#include "call_manager.h"

#define RESPONSE_BUFFER_SIZE 14000 
#define WEB_SERVER_URL "https://tamsen.in/rfid/log_entry.php"
#define OTA_CHECK_INTERVAL_MS (30 * 1000)
#define UID_MAX_LEN 32
#define MAX_PARAM_LEN 64
#define RFID_QUEUE_LEN 10
#define I2C_MASTER_SCL_IO 22 /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO 21 /*!< GPIO number used for I2C master data */
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 1000000
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define DS3231_ADDR 0x68 /*!< I2C address of the DS3231 RTC module */

extern char g_ap_ssid[MAX_PARAM_LEN];
extern char g_ap_pass[MAX_PARAM_LEN];
extern char g_ssid[MAX_PARAM_LEN];
extern char g_password[MAX_PARAM_LEN];
extern char g_placeType[MAX_PARAM_LEN];
extern char g_roomNumber[MAX_PARAM_LEN];
extern char g_location[MAX_PARAM_LEN];
extern char g_tower[MAX_PARAM_LEN];
extern char g_floorNumber[MAX_PARAM_LEN];

extern volatile bool is_ap_mode;

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;
extern SemaphoreHandle_t wifi_switch_semaphore;
// TAGS FOR LOGGING
#define TAG "NODE"
#define WIFI_TAG "WIFI_LOG"     // General log tag for UVS system messages
#define NVS_TAG "NVS_LOG" // Log tag for NVS (Non-Volatile Storage) operations
#define GPIO_TAG "GPIO_LOG"
#define HTTP_TAG "HTTP_LOG"
#define LED_TAG "LED_LOG"
#define OTA_TAG "OTA_LOG"
#define RFID_TAG "RFID_LOG"
#define RTC_TAG "RTC_LOG"

#define RED_LED GPIO_NUM_25
#define GREEN_LED GPIO_NUM_13
#define BLUE_LED GPIO_NUM_27
#define BUZZER GPIO_NUM_4
#define APMODE GPIO_NUM_14
#define CALL_BUTTON GPIO_NUM_2
#define CANCEL_BUTTON GPIO_NUM_12
#define BLUECODE_BUTTON GPIO_NUM_15

typedef struct
{
    char uid[15];
    char timestamp[32];
} rfid_message_t;

extern QueueHandle_t rfid_queue;
#endif
