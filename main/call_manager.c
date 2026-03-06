#include "call_manager.h"
#include "mqtt_client.h"

static call_info_t g_current_call = {
    .type = CALL_TYPE_NONE,
    .state = CALL_STATE_IDLE,
    .start_time_ms = 0,
    .is_attended = false,
    .device_type = DEVICE_TYPE_BUTTON
};

static SemaphoreHandle_t call_mutex = NULL;
static esp_mqtt_client_handle_t mqtt_client = NULL;
static TimerHandle_t escalation_timer = NULL;

#define MQTT_TOPIC_CALL "local/call"
#define CALL_TIMEOUT_MS (2 * 60 * 1000)  // 2 minutes

// Forward declarations
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
static void escalation_timer_callback(TimerHandle_t xTimer);
static esp_err_t init_mqtt_client(void);
static void send_call_to_mqtt(call_type_t call_type, device_type_t device_type, const char *uid, bool is_attended);

/**
 * Initialize MQTT client for local server communication
 */
static esp_err_t init_mqtt_client(void)
{
    if (mqtt_client != NULL) {
        return ESP_OK;
    }

    // Wait for WiFi to connect before trying MQTT
    int retry_count = 0;
    while (!wifi_is_connected() && retry_count < 30) {
        ESP_LOGI(TAG, "Waiting for WiFi connection... (%d/30)", retry_count + 1);
        vTaskDelay(pdMS_TO_TICKS(500));
        retry_count++;
    }
    
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "WiFi not connected, MQTT will retry later");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WiFi connected, initializing MQTT...");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.1.7:1883",  // Your MQTT server
        // .keepalive = 60,
        // .buffer_size = 1024,
        // .task_stack = 6144,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t ret = esp_mqtt_client_start(mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT client started and connecting...");
    return ESP_OK;
}

/**
 * MQTT event handler
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✓ MQTT CONNECTED to broker");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "✗ MQTT DISCONNECTED from broker");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT unsubscribed, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "✓ MQTT message published, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT data received");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "✗ MQTT ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "MQTT TCP Error: %d", event->error_handle->esp_transport_sock_errno);
            }
            break;
        default:
            ESP_LOGD(TAG, "MQTT other event id: %d", event->event_id);
            break;
    }
}

/**
 * Escalation timer callback - escalates call to EMERGENCY after 2 minutes
 */
static void escalation_timer_callback(TimerHandle_t xTimer)
{
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // escalate only if it's a normal CALL; do not override BLUECODE
        if (g_current_call.state == CALL_STATE_ACTIVE && !g_current_call.is_attended
            && g_current_call.type == CALL_TYPE_CALL) {
            ESP_LOGI(TAG, "Call escalated to EMERGENCY after 2 minutes");
            g_current_call.type = CALL_TYPE_EMERGENCY;
            g_current_call.state = CALL_STATE_ESCALATED;
            send_call_to_mqtt(CALL_TYPE_EMERGENCY, g_current_call.device_type, "", false);
            led_set_color_call_manager(LED_RED);  // Static LED - red until canceled or attended
        }
        xSemaphoreGive(call_mutex);
    }
}

/**
 * Send call information to local MQTT server
 */
static void send_call_to_mqtt(call_type_t call_type, device_type_t device_type, const char *uid, bool is_attended)
{
    cJSON *root = cJSON_CreateObject();
    
    // Add required fields
    cJSON_AddStringToObject(root, "room", g_roomNumber);
    cJSON_AddStringToObject(root, "floor", g_floorNumber);
    
    // Add call type
    const char *call_type_str = "NONE";
    if (call_type == CALL_TYPE_CALL) call_type_str = "CALL";
    else if (call_type == CALL_TYPE_EMERGENCY) call_type_str = "EMERGENCY";
    else if (call_type == CALL_TYPE_BLUECODE) call_type_str = "BLUECODE";
    else if (call_type == CALL_TYPE_CANCELLED) call_type_str = "CANCELLED";
    
    cJSON_AddStringToObject(root, "callType", call_type_str);
    
    // Add device type
    const char *device_type_str = (device_type == DEVICE_TYPE_REMOTE) ? "bathroom module" : "bed module";
    cJSON_AddStringToObject(root, "device_type", device_type_str);
    
    // If RFID attended, add UID and other details
    if (is_attended && uid && strlen(uid) > 0) {
        cJSON_AddStringToObject(root, "uid", uid);
        cJSON_AddStringToObject(root, "location", g_location);
        cJSON_AddStringToObject(root, "tower", g_tower);
        cJSON_AddStringToObject(root, "placeType", g_placeType);
        cJSON_AddBoolToObject(root, "attended", true);
    } else {
        cJSON_AddBoolToObject(root, "attended", false);
    }
    
    // Add timestamp string from RTC
    cJSON_AddStringToObject(root, "timestamp", rtc_get_timestamp());
    
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json) {
        ESP_LOGI(TAG, "[MQTT] Call event ready: %s", json);
        
        if (mqtt_client != NULL) {
            int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_CALL, json, strlen(json), 1, 0);
            if (msg_id >= 0) {
                ESP_LOGI(TAG, "[MQTT] Published successfully (msg_id=%d)", msg_id);
            } else {
                ESP_LOGE(TAG, "[MQTT] Failed to publish (msg_id=%d)", msg_id);
            }
        } else {
            ESP_LOGW(TAG, "[MQTT] Client not connected - message queued for later: %s", json);
        }
        free(json);
    }
}

/**
 * CALL button pressed - raises a call if no call is currently active
 */
void call_button_pressed(void)
{
    ESP_LOGI(TAG, "[BUTTON] CALL button action triggered");
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_current_call.state == CALL_STATE_IDLE) {
            ESP_LOGI(TAG, "[BUTTON] CALL button pressed - raising call");
            g_current_call.type = CALL_TYPE_CALL;
            g_current_call.state = CALL_STATE_ACTIVE;
            g_current_call.start_time_ms = esp_timer_get_time() / 1000;
            g_current_call.is_attended = false;
            g_current_call.device_type = DEVICE_TYPE_BUTTON;  // Physical bed module button
            
            // Start escalation timer
            if (escalation_timer == NULL) {
                escalation_timer = xTimerCreate("escalation_timer", pdMS_TO_TICKS(CALL_TIMEOUT_MS), 
                                               pdFALSE, NULL, escalation_timer_callback);
            }
            if (escalation_timer != NULL) {
                xTimerStart(escalation_timer, pdMS_TO_TICKS(100));
            }
            
            send_call_to_mqtt(CALL_TYPE_CALL, DEVICE_TYPE_BUTTON, "", false);
            led_set_color_call_manager(LED_YELLOW);  // Static LED - overrides other tasks until call ends
        } else {
            ESP_LOGW(TAG, "[BUTTON] Call already active - ignoring new call");
        }
        xSemaphoreGive(call_mutex);
    } else {
        ESP_LOGE(TAG, "[BUTTON] Failed to acquire mutex for CALL button");
    }
}

/**
 * CANCEL button pressed - cancels active call
 */
void cancel_button_pressed(void)
{
    ESP_LOGI(TAG, "[BUTTON] CANCEL button action triggered");
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_current_call.state != CALL_STATE_IDLE) {
            ESP_LOGI(TAG, "[BUTTON] CANCEL button pressed - canceling call");
            
            // Send MQTT notification that call was cancelled
            send_call_to_mqtt(CALL_TYPE_CANCELLED, g_current_call.device_type, "", false);
            
            g_current_call.type = CALL_TYPE_NONE;
            g_current_call.state = CALL_STATE_IDLE;
            g_current_call.is_attended = false;
            
            // Stop escalation timer
            if (escalation_timer != NULL) {
                xTimerStop(escalation_timer, pdMS_TO_TICKS(100));
            }
            
            // Release LED control when call is canceled
            led_release_call_manager();
        } else {
            ESP_LOGW(TAG, "[BUTTON] No active call to cancel");
        }
        xSemaphoreGive(call_mutex);
    } else {
        ESP_LOGE(TAG, "[BUTTON] Failed to acquire mutex for CANCEL button");
    }
}

/**
 * BLUECODE button pressed - raises a BLUECODE alert
 */
void bluecode_button_pressed(void)
{
    ESP_LOGI(TAG, "[BUTTON] BLUECODE button action triggered");
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_current_call.state == CALL_STATE_IDLE) {
            ESP_LOGI(TAG, "[BUTTON] BLUECODE button pressed - raising BLUECODE alert");
            g_current_call.type = CALL_TYPE_BLUECODE;
            g_current_call.state = CALL_STATE_ACTIVE;
            g_current_call.start_time_ms = esp_timer_get_time() / 1000;
            g_current_call.is_attended = false;
            g_current_call.device_type = DEVICE_TYPE_BUTTON;  // Physical bed module button
            
            // Start escalation timer for BLUECODE as well
            if (escalation_timer == NULL) {
                escalation_timer = xTimerCreate("escalation_timer", pdMS_TO_TICKS(CALL_TIMEOUT_MS), 
                                               pdFALSE, NULL, escalation_timer_callback);
            }
            if (escalation_timer != NULL) {
                xTimerStart(escalation_timer, pdMS_TO_TICKS(100));
            }
            
            send_call_to_mqtt(CALL_TYPE_BLUECODE, DEVICE_TYPE_BUTTON, "", false);
            led_set_color_call_manager(LED_BLUE);  // Static LED - blue until canceled or attended
        } else {
            ESP_LOGW(TAG, "[BUTTON] Call already active - ignoring BLUECODE");
        }
        xSemaphoreGive(call_mutex);
    } else {
        ESP_LOGE(TAG, "[BUTTON] Failed to acquire mutex for BLUECODE button");
    }
}

/**
 * RFID response to active call - attends/fulfills the call
 */
void rfid_response_to_call(const char *uid, const char *timestamp)
{
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_current_call.state != CALL_STATE_IDLE) {
            ESP_LOGI(TAG, "RFID attended to call: UID=%s", uid);
            g_current_call.is_attended = true;
            
            // Send attended response to MQTT
            send_call_to_mqtt(g_current_call.type, g_current_call.device_type, uid, true);
            
            // Stop escalation timer
            if (escalation_timer != NULL) {
                xTimerStop(escalation_timer, pdMS_TO_TICKS(100));
            }
            
            // Reset call state
            g_current_call.type = CALL_TYPE_NONE;
            g_current_call.state = CALL_STATE_IDLE;
            
            // Release LED control and set brief green flash for attended call
            led_set_color_call_manager(LED_GREEN);  // Indicate attended with green
        }
        xSemaphoreGive(call_mutex);
    }
    
    // After releasing the mutex, wait briefly then turn off LED
    if (g_current_call.is_attended) {
        vTaskDelay(pdMS_TO_TICKS(1500));
        led_release_call_manager();
    }
}

/**
 * Get current call information (thread-safe)
 */
call_info_t get_current_call_info(void)
{
    call_info_t call_copy = { 0 };
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        call_copy = g_current_call;
        xSemaphoreGive(call_mutex);
    }
    return call_copy;
}

/**
 * Check if a call is currently active
 */
bool is_call_active(void)
{
    return get_current_call_info().state != CALL_STATE_IDLE;
}

bool is_call_blinking(void)
{
    call_info_t info = get_current_call_info();
    return (info.state != CALL_STATE_IDLE && 
            (info.type == CALL_TYPE_BLUECODE || info.type == CALL_TYPE_CALL));
}


/**
 * Initialize call manager (call buttons, MQTT, etc.)
 */
void call_manager_init(void)
{
    // Create mutex for thread safety
    call_mutex = xSemaphoreCreateMutex();
    if (call_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create call mutex");
        return;
    }

    // Initialize MQTT client (non-critical - buttons work without it)
    if (init_mqtt_client() != ESP_OK) {
        ESP_LOGW(TAG, "MQTT initialization failed - will retry on next connection");
        // Don't return - buttons still work fine
    } else {
        ESP_LOGI(TAG, "MQTT client initialized successfully");
    }

    // Configure button GPIOs as simple inputs with pull-up (no interrupts)
    gpio_config_t button_conf = {
        .intr_type = GPIO_INTR_DISABLE,    // Disable interrupts to avoid conflicts
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CALL_BUTTON_GPIO) | (1ULL << CANCEL_BUTTON_GPIO) | (1ULL << BLUECODE_BUTTON_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    
    esp_err_t ret = gpio_config(&button_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure button GPIOs: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Button GPIOs configured successfully (polling mode)");
    ESP_LOGI(TAG, "CALL_BUTTON: GPIO %d, CANCEL_BUTTON: GPIO %d, BLUECODE_BUTTON: GPIO %d", 
             CALL_BUTTON_GPIO, CANCEL_BUTTON_GPIO, BLUECODE_BUTTON_GPIO);

    ESP_LOGI(TAG, "Call manager initialized successfully");
}

/**
 * Call manager task - polls button states and detects transitions
 */
void call_manager_task(void *arg)
{
    ESP_LOGI(TAG, "=== Starting button polling task ===");
    
    vTaskDelay(pdMS_TO_TICKS(500)); // Wait for system to stabilize
    
    // Read ACTUAL initial states (don't assume they are 1)
    int prev_call_level = gpio_get_level(CALL_BUTTON_GPIO);
    int prev_cancel_level = gpio_get_level(CANCEL_BUTTON_GPIO);
    int prev_bluecode_level = gpio_get_level(BLUECODE_BUTTON_GPIO);
    
    ESP_LOGI(TAG, "Initial GPIO states - CALL: %d, CANCEL: %d, BLUECODE: %d",
             prev_call_level, prev_cancel_level, prev_bluecode_level);
    
    while (1) {
        // Read current GPIO levels
        int call_level = gpio_get_level(CALL_BUTTON_GPIO);
        int cancel_level = gpio_get_level(CANCEL_BUTTON_GPIO);
        int bluecode_level = gpio_get_level(BLUECODE_BUTTON_GPIO);
        
        // Detect FALLING edge (1 -> 0 = button pressed)
        if (prev_call_level == 1 && call_level == 0) {
            ESP_LOGI(TAG, "[POLL] CALL button PRESSED (falling edge detected)");
            call_button_pressed();
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        }
        
        if (prev_cancel_level == 1 && cancel_level == 0) {
            ESP_LOGI(TAG, "[POLL] CANCEL button PRESSED (falling edge detected)");
            cancel_button_pressed();
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        }
        
        if (prev_bluecode_level == 1 && bluecode_level == 0) {
            ESP_LOGI(TAG, "[POLL] BLUECODE button PRESSED (falling edge detected)");
            bluecode_button_pressed();
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        }
        
        // Also detect RISING edge (0 -> 1 = button released)
        if (prev_call_level == 0 && call_level == 1) {
            ESP_LOGI(TAG, "[POLL] CALL button RELEASED");
        }
        if (prev_cancel_level == 0 && cancel_level == 1) {
            ESP_LOGI(TAG, "[POLL] CANCEL button RELEASED");
        }
        if (prev_bluecode_level == 0 && bluecode_level == 1) {
            ESP_LOGI(TAG, "[POLL] BLUECODE button RELEASED");
        }
        
        // Store current states for next iteration
        prev_call_level = call_level;
        prev_cancel_level = cancel_level;
        prev_bluecode_level = bluecode_level;
        
        // Check active call status
        call_info_t call = get_current_call_info();
        if (call.state != CALL_STATE_IDLE) {
            uint64_t elapsed_ms = (esp_timer_get_time() / 1000) - call.start_time_ms;
            // ESP_LOGI(TAG, "[CALL STATE] Type: %d, State: %d, Attended: %d, Elapsed: %lld ms", 
            //          call.type, call.state, call.is_attended, elapsed_ms);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100ms (debounce friendly)
    }
}

/**
 * RF CALL button pressed (433MHz remote) - raises a call if no call is currently active
 */
void rf_call_button_pressed(void)
{
    ESP_LOGI(TAG, "[RF REMOTE] CALL button action triggered");
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_current_call.state == CALL_STATE_IDLE) {
            ESP_LOGI(TAG, "[RF REMOTE] CALL button pressed - raising call");
            g_current_call.type = CALL_TYPE_CALL;
            g_current_call.state = CALL_STATE_ACTIVE;
            g_current_call.start_time_ms = esp_timer_get_time() / 1000;
            g_current_call.is_attended = false;
            g_current_call.device_type = DEVICE_TYPE_REMOTE;  // RF 433MHz bathroom module remote
            
            // Start escalation timer
            if (escalation_timer == NULL) {
                escalation_timer = xTimerCreate("escalation_timer", pdMS_TO_TICKS(CALL_TIMEOUT_MS), 
                                               pdFALSE, NULL, escalation_timer_callback);
            }
            if (escalation_timer != NULL) {
                xTimerStart(escalation_timer, pdMS_TO_TICKS(100));
            }
            
            send_call_to_mqtt(CALL_TYPE_CALL, DEVICE_TYPE_REMOTE, "", false);
            led_set_color_call_manager(LED_YELLOW);  // Static LED - overrides other tasks until call ends
        } else {
            ESP_LOGW(TAG, "[RF REMOTE] Call already active - ignoring new call");
        }
        xSemaphoreGive(call_mutex);
    } else {
        ESP_LOGE(TAG, "[RF REMOTE] Failed to acquire mutex for CALL button");
    }
}

/**
 * RF CANCEL button pressed (433MHz remote) - cancels active call
 */
void rf_cancel_button_pressed(void)
{
    ESP_LOGI(TAG, "[RF REMOTE] CANCEL button action triggered");
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_current_call.state != CALL_STATE_IDLE) {
            ESP_LOGI(TAG, "[RF REMOTE] CANCEL button pressed - canceling call");
            
            // Send MQTT notification that call was cancelled
            send_call_to_mqtt(CALL_TYPE_CANCELLED, g_current_call.device_type, "", false);
            
            g_current_call.type = CALL_TYPE_NONE;
            g_current_call.state = CALL_STATE_IDLE;
            g_current_call.is_attended = false;
            
            // Stop escalation timer
            if (escalation_timer != NULL) {
                xTimerStop(escalation_timer, pdMS_TO_TICKS(100));
            }
            
            // Release LED control when call is canceled
            led_release_call_manager();
        } else {
            ESP_LOGW(TAG, "[RF REMOTE] No active call to cancel");
        }
        xSemaphoreGive(call_mutex);
    } else {
        ESP_LOGE(TAG, "[RF REMOTE] Failed to acquire mutex for CANCEL button");
    }
}

/**
 * RF BLUECODE button pressed (433MHz remote) - raises a BLUECODE alert
 */
void rf_bluecode_button_pressed(void)
{
    ESP_LOGI(TAG, "[RF REMOTE] BLUECODE button action triggered");
    if (xSemaphoreTake(call_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (g_current_call.state == CALL_STATE_IDLE) {
            ESP_LOGI(TAG, "[RF REMOTE] BLUECODE button pressed - raising BLUECODE alert");
            g_current_call.type = CALL_TYPE_BLUECODE;
            g_current_call.state = CALL_STATE_ACTIVE;
            g_current_call.start_time_ms = esp_timer_get_time() / 1000;
            g_current_call.is_attended = false;
            g_current_call.device_type = DEVICE_TYPE_REMOTE;  // RF 433MHz bathroom module remote
            
            // Start escalation timer for BLUECODE as well
            if (escalation_timer == NULL) {
                escalation_timer = xTimerCreate("escalation_timer", pdMS_TO_TICKS(CALL_TIMEOUT_MS), 
                                               pdFALSE, NULL, escalation_timer_callback);
            }
            if (escalation_timer != NULL) {
                xTimerStart(escalation_timer, pdMS_TO_TICKS(100));
            }
            
            send_call_to_mqtt(CALL_TYPE_BLUECODE, DEVICE_TYPE_REMOTE, "", false);
            led_set_color_call_manager(LED_BLUE);  // Static LED - blue until canceled or attended
        } else {
            ESP_LOGW(TAG, "[RF REMOTE] Call already active - ignoring BLUECODE");
        }
        xSemaphoreGive(call_mutex);
    } else {
        ESP_LOGE(TAG, "[RF REMOTE] Failed to acquire mutex for BLUECODE button");
    }
}
