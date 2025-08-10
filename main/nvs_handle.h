#ifndef NVS_HANDLE_H         // Header guard to prevent multiple inclusion of this header file
#define NVS_HANDLE_H

#include "global.h"          // Include global definitions and configurations

// Function to store a string value in NVS with the specified key
void store_string_in_nvs(const char *key, const char *string_to_store);

// Function to store a 32-bit unsigned integer value in NVS with the specified key
void store_uint32_in_nvs(const char *key, uint32_t value);

// Function to read a stored string from NVS using the specified key
char *read_string_from_nvs(const char *key);

// Function to read a 32-bit unsigned integer from NVS using the specified key
uint32_t read_uint32_from_nvs(const char *key);
void load_config_from_nvs(void);
#endif  // End of NVS_HANDLE_H header guard
