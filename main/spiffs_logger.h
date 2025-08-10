#ifndef SPIFFS_H
#define SPIFFS_H
#include "global.h"
/* Save a JSON string to SPIFFS (appends with newline). Caller may pass full JSON. */
esp_err_t spiffs_logger_save(const char *json_line);

/* Send and clear all stored JSON lines (calls http_client to send). */
esp_err_t spiffs_logger_send_all(void);

/* Initialize SPIFFS */
esp_err_t spiffs_logger_init(void);
void spiffs_sync_task(void *pvParameters);
#endif