#ifndef WEBSERVER_H
#define WEBSERVER_H
#include "global.h" // Include global definitions and configurations

// Function to initialize and start the HTTP web server
void start_webserver(void);
esp_err_t index_get_handler(httpd_req_t *req);
esp_err_t config_device_handler(httpd_req_t *req);
#endif // End of WEBSERVER_H header guard
