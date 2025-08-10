#ifndef WIFI_H                    // Header guard to prevent multiple inclusion of this header file
#define WIFI_H

#include "global.h"               // Include global definitions and configurations
// void wifi_init_sta(void);
// void wifi_init_ap(void);
bool wifi_is_connected(void);
void switch_wifi_mode(bool apmode);
void gpio_isr_handler(void* arg);
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
// void switch_wifi_mode(bool apmode);
// void wifi_init_common(void);
#endif  // End of WIFI_H header guard
