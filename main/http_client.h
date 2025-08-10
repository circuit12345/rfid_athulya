#ifndef CLIENT_H
#define CLIENT_H
#include "global.h"
esp_err_t http_client_send_json(const char *json);
void http_send_task(void *pvParameters);
#endif