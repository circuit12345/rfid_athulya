#ifndef OTAHANDLE_H
#define OTAHANDLE_H

#include "global.h"         // Include global definitions and configurations

// Function to perform OTA firmware update using a binary file stored in SPIFFS
void ota_fw_update_from_spiffs(const char *file_path);

// HTTP handler to initiate OTA firmware update through web request
esp_err_t ota_fw_update_handler(httpd_req_t *req);

#endif  // End of OTA_HANDLE_H header guard
