#include "rf.h"
#include "call_manager.h"

#define RF_TAG "RF_433MHz"

static SemaphoreHandle_t rf_signal_semaphore = NULL;
static volatile uint32_t rf_signal_code = 0;
static volatile bool rf_signal_received = false;

/**
 * RF ISR handler - triggered on GPIO transitions
 */
void rf_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Signal the RF task that a signal was received
    if (rf_signal_semaphore != NULL) {
        xSemaphoreGiveFromISR(rf_signal_semaphore, &xHigherPriorityTaskWoken);
    }
    
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * Decode RF signal and determine which button was pressed
 * In a real implementation, this would decode the actual 433MHz protocol
 * For now, we'll use a simplified approach with GPIO level detection
 */
static rf_button_t decode_rf_signal(void)
{
    // In a full implementation, you would:
    // 1. Measure pulse widths and durations
    // 2. Decode Manchester/PT2262/other RF protocols
    // 3. Validate checksums
    // 4. Compare against registered remote codes
    
    // For now, sample multiple times to determine signal pattern
    uint32_t signal_pattern = 0;
    for (int i = 0; i < 8; i++) {
        vTaskDelay(pdMS_TO_TICKS(5));
        signal_pattern = (signal_pattern << 1) | gpio_get_level(RF_RECEIVER_GPIO);
    }
    
    ESP_LOGD(RF_TAG, "RF Signal pattern: 0x%08" PRIx32, signal_pattern);
    
    // Map signal patterns to buttons
    // These values are placeholders - adjust based on your actual remote codes
    if (signal_pattern == 0xAAAAAAAA) {
        return RF_BUTTON_CALL;
    } else if (signal_pattern == 0x55555555) {
        return RF_BUTTON_CANCEL;
    } else if (signal_pattern == 0xFFFFFFFF) {
        return RF_BUTTON_BLUECODE;
    }
    
    return RF_BUTTON_NONE;
}

/**
 * RF receiver task - handles incoming 433MHz RF signals
 */
void rf_task(void *arg)
{
    ESP_LOGI(RF_TAG, "RF 433MHz receiver task started");
    
    uint32_t debounce_ticks = 0;
    const uint32_t DEBOUNCE_TIME_MS = 500;  // Debounce time to prevent multiple detections
    
    while (1) {
        // Wait for RF signal or timeout (periodically check)
        if (xSemaphoreTake(rf_signal_semaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            // Check if we should debounce (prevent rapid clicks)
            uint32_t current_ticks = xTaskGetTickCount();
            if ((current_ticks - debounce_ticks) < pdMS_TO_TICKS(DEBOUNCE_TIME_MS)) {
                ESP_LOGD(RF_TAG, "RF signal ignored - debounce active");
                continue;
            }
            
            // Decode the RF signal
            rf_button_t button = decode_rf_signal();
            debounce_ticks = xTaskGetTickCount();
            
            if (button == RF_BUTTON_NONE) {
                ESP_LOGW(RF_TAG, "Unknown RF signal received");
                continue;
            }
            
            ESP_LOGI(RF_TAG, "RF button detected: %d (bathroom module)", button);
            
            // Handle RF button press with device_type="bathroom module"
            switch (button) {
                case RF_BUTTON_CALL:
                    ESP_LOGI(RF_TAG, "RF CALL button pressed");
                    rf_call_button_pressed();
                    break;
                    
                case RF_BUTTON_CANCEL:
                    ESP_LOGI(RF_TAG, "RF CANCEL button pressed");
                    rf_cancel_button_pressed();
                    break;
                    
                case RF_BUTTON_BLUECODE:
                    ESP_LOGI(RF_TAG, "RF BLUECODE button pressed");
                    rf_bluecode_button_pressed();
                    break;
                    
                default:
                    break;
            }
        }
    }
}

/**
 * Initialize RF 433MHz receiver
 */
void custom_rf_init(void)
{
    // ESP_LOGI(RF_TAG, "Initializing RF 433MHz receiver on GPIO %d", RF_RECEIVER_GPIO);
    //     gpio_config_t button_conf = {
    //     .intr_type = GPIO_INTR_DISABLE,    // Disable interrupts to avoid conflicts
    //     .mode = GPIO_MODE_INPUT,
    //     .pin_bit_mask = (1ULL << CALL_BUTTON_GPIO) | (1ULL << CANCEL_BUTTON_GPIO) | (1ULL << BLUECODE_BUTTON_GPIO),
    //     .pull_up_en = GPIO_PULLUP_ENABLE,
    // };
    // Configure GPIO for RF receiver
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,     // Trigger on both rising and falling edges
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << RF_RECEIVER_GPIO,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Create semaphore for RF signal detection
    rf_signal_semaphore = xSemaphoreCreateBinary();
    if (rf_signal_semaphore == NULL) {
        ESP_LOGE(RF_TAG, "Failed to create RF signal semaphore");
        return;
    }
    
    // Register ISR handler
    esp_err_t ret = gpio_isr_handler_add(RF_RECEIVER_GPIO, rf_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(RF_TAG, "Failed to add RF ISR handler: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(RF_TAG, "RF receiver initialized successfully");
}
