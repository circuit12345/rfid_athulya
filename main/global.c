#include "global.h"
QueueHandle_t rfid_queue = NULL;
char g_ssid[MAX_PARAM_LEN] = "Sriramsrk";
char g_password[MAX_PARAM_LEN] = "srk12345";
char g_placeType[MAX_PARAM_LEN] = "office";
char g_roomNumber[MAX_PARAM_LEN] = "102";
char g_location[MAX_PARAM_LEN] = "Bangalore";
char g_tower[MAX_PARAM_LEN] = "A";
char g_floorNumber[MAX_PARAM_LEN] = "2";
volatile bool is_ap_mode = false;
EventGroupHandle_t wifi_event_group = NULL; // Initialize to NULL, will create later
const int WIFI_CONNECTED_BIT = BIT0;
SemaphoreHandle_t wifi_switch_semaphore = NULL;