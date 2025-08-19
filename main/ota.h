#ifndef OTA_H
#define OTA_H

#include "esp_err.h"

//#define APP_VERSION "1.0.4"  // Current firmware version string
#define APP_VERSION "1.0.6"
// Call this function to check and perform OTA update if new firmware is available
esp_err_t ota_check_and_update(void);

#endif // OTA_H
