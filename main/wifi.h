#ifndef WIFI_H                    // Header guard to prevent multiple inclusion of this header file
#define WIFI_H

#include "global.h"               // Include global definitions and configurations
bool wifi_is_connected(void);
void switch_wifi_mode(bool apmode);
void gpio_isr_handler(void* arg);
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void ap_name_creation();
void save_ap_credentials_to_nvs(void);
void load_ap_credentials_from_nvs(void) ;

#endif  // End of WIFI_H header guard
