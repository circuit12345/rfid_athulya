#ifndef RFID_H
#define RFID_H
#include "global.h"
void rc522_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data);
void rfid_init_module(void);

#endif