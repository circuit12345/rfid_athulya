#include "nvs_handle.h"
// TO STORE STRINGS SUCH AS MODE

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
