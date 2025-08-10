#include "nvs_handle.h"
// TO STORE STRINGS SUCH AS MODE
void store_string_in_nvs(const char *key, const char *string_to_store)
{
    esp_err_t err;
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
    err = nvs_set_str(my_handle, key, string_to_store);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Failed to write string to NVS!");
    }
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Failed to commit changes!");
    }
    else
    {
        ESP_LOGI(NVS_TAG, "String stored successfully in NVS!");
    }
    nvs_close(my_handle);
}
// TO STORE INTEGER SUCH AS BAUD RATE
void store_uint32_in_nvs(const char *key, uint32_t value)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(NVS_TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
    err = nvs_set_u32(my_handle, key, value);
    if (err == ESP_OK)
    {
        nvs_commit(my_handle);
        ESP_LOGI(NVS_TAG, "uint32_t value stored successfully: %lu", value);
    }
    else
    {
        ESP_LOGE(NVS_TAG, "Error storing uint32_t (%s)!", esp_err_to_name(err));
    }

    nvs_close(my_handle);
}
// FUNCTION TO READ STORED STRING
char *read_string_from_nvs(const char *key)
{
    esp_err_t err;
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return NULL;
    }

    size_t required_size = 0;
    err = nvs_get_str(my_handle, key, NULL, &required_size);
    if (err == ESP_OK)
    {
        char *stored_string = malloc(required_size);
        if (stored_string == NULL)
        {
            printf("Memory allocation failed!\n");
            nvs_close(my_handle);
            return NULL;
        }

        err = nvs_get_str(my_handle, key, stored_string, &required_size);
        if (err == ESP_OK)
        {
            nvs_close(my_handle);
            return stored_string;
        }
        else
        {
            printf("Error (%s) reading string from NVS!\n", esp_err_to_name(err));
            free(stored_string);
            nvs_close(my_handle);
            return NULL;
        }
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        printf("String not found in NVS.\n");
        nvs_close(my_handle);
        return NULL;
    }
    else
    {
        printf("Error (%s) getting string size from NVS!\n", esp_err_to_name(err));
        nvs_close(my_handle);
        return NULL;
    }
}
// FUNCTION TO READ STORED INTEGER
uint32_t read_uint32_from_nvs(const char *key)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    uint32_t value = 0;

    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        return 0;
    }

    err = nvs_get_u32(my_handle, key, &value);
    if (err == ESP_OK)
    {
        // printf("uint32_t value read successfully: %lu\n", value);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        printf("Value for key '%s' not found in NVS.\n", key);
    }
    else
    {
        printf("Error reading uint32_t (%s)!\n", esp_err_to_name(err));
    }

    nvs_close(my_handle);
    return value;
}

#include "nvs.h"
#include "nvs_flash.h"
#include "global.h"

void load_config_from_nvs(void)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;

    // Initialize NVS — call once at boot in your main app if not done already
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or new version found, erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK)
    {
        // Helper function to load string from NVS or use default
        #define LOAD_STR_OR_DEFAULT(key, dest) do { \
            size_t required_size = 0; \
            err = nvs_get_str(nvs_handle, key, NULL, &required_size); \
            if (err == ESP_OK && required_size < sizeof(dest)) { \
                err = nvs_get_str(nvs_handle, key, dest, &required_size); \
                if (err != ESP_OK) { \
                    ESP_LOGE("NVS", "Failed to read %s", key); \
                    strncpy(dest, #key "_default", sizeof(dest)); /* fallback default string */ \
                } \
            } else { \
                /* Use hardcoded default values if not found in NVS */ \
                ESP_LOGI("NVS", "%s not found in NVS, using default", key); \
            } \
        } while(0)

        LOAD_STR_OR_DEFAULT("ssid", g_ssid);
        LOAD_STR_OR_DEFAULT("password", g_password);
        LOAD_STR_OR_DEFAULT("placeType", g_placeType);
        LOAD_STR_OR_DEFAULT("roomNumber", g_roomNumber);
        LOAD_STR_OR_DEFAULT("location", g_location);
        LOAD_STR_OR_DEFAULT("tower", g_tower);
        LOAD_STR_OR_DEFAULT("floorNumber", g_floorNumber);

        nvs_close(nvs_handle);
    }
    else
    {
        ESP_LOGW("NVS", "Failed to open wifi_config namespace, using defaults");
        // globals already initialized with default literals, so nothing to do
    }

    #undef LOAD_STR_OR_DEFAULT

    ESP_LOGI("NVS", "Loaded config from NVS:");
    ESP_LOGI("NVS", "SSID: %s", g_ssid);
    ESP_LOGI("NVS", "Password: %s", g_password);
    ESP_LOGI("NVS", "PlaceType: %s", g_placeType);
    ESP_LOGI("NVS", "RoomNumber: %s", g_roomNumber);
    ESP_LOGI("NVS", "Location: %s", g_location);
    ESP_LOGI("NVS", "Tower: %s", g_tower);
    ESP_LOGI("NVS", "FloorNumber: %s", g_floorNumber);
}

// void save_ap_credentials_to_nvs(const char *ssid, const char *pass) {
//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &handle);
//     if (err == ESP_OK) {
//         nvs_set_str(handle, "ap_ssid", ssid);
//         nvs_set_str(handle, "ap_pass", pass);
//         nvs_commit(handle);
//         nvs_close(handle);
//     }
// }

// #include <string.h>
// #include "nvs_flash.h"
// #include "nvs.h"
// #include "global.h"

// void load_ap_credentials_from_nvs(char *ssid, size_t ssid_size, char *pass, size_t pass_size) {
//     nvs_handle_t handle;
//     esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &handle);
//     if (err == ESP_OK) {
//         if (nvs_get_str(handle, "ap_ssid", ssid, &ssid_size) != ESP_OK) {
//             strncpy(ssid, g_ap_ssid, ssid_size);
//         }
//         if (nvs_get_str(handle, "ap_pass", pass, &pass_size) != ESP_OK) {
//             strncpy(pass, g_ap_pass, pass_size);
//         }
//         nvs_close(handle);
//     } else {
//         strncpy(ssid, g_ap_ssid, ssid_size);
//         strncpy(pass, g_ap_pass, pass_size);
//     }
// }

