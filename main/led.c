#include "led.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"

static SemaphoreHandle_t led_mutex;

static TimerHandle_t glow_timer;
static TimerHandle_t blink_timer;

static led_mode_t current_mode = LED_MODE_INDEFINITE;
static led_color_t indefinite_color = LED_OFF;
static led_color_t override_color = LED_OFF;

static bool blink_state = false;

static void set_gpio_levels(led_color_t color)
{
    // Turn on/off GPIO pins based on color enum
    switch (color)
    {
    case LED_OFF:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 0);
        gpio_set_level(BUZZER, 0);
        break;
    case LED_RED:
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 0);
        gpio_set_level(BUZZER, 0);
        break;
    case LED_GREEN:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 0);
        gpio_set_level(BUZZER, 1);

        break;
    case LED_BLUE:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 1);
        gpio_set_level(BUZZER, 0);
        break;
    case LED_YELLOW: // RED + GREEN
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 0);
        gpio_set_level(BUZZER, 0);
        break;
    case LED_CYAN: // GREEN + BLUE
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 1);
        gpio_set_level(BUZZER, 0);
        break;
    case LED_MAGENTA: // RED + BLUE
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 1);
        gpio_set_level(BUZZER, 0);
        break;
    case LED_WHITE: // RED + GREEN + BLUE
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 1);
        gpio_set_level(BUZZER, 0);
        break;
    case RING: // RED + GREEN + BLUE
        gpio_set_level(BUZZER, 0);
        break;
    default:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 0);
        gpio_set_level(BUZZER, 0);
        break;
    }
}

static void glow_timer_callback(TimerHandle_t xTimer)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        current_mode = LED_MODE_INDEFINITE;
        override_color = LED_OFF;
        set_gpio_levels(indefinite_color);
        xSemaphoreGive(led_mutex);
        //ESP_LOGI(TAG, "Glow 3s override ended, reverted to indefinite color");
    }
}

static void blink_timer_callback(TimerHandle_t xTimer)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        blink_state = !blink_state;
        if (blink_state)
        {
            set_gpio_levels(override_color);
        }
        else
        {
            set_gpio_levels(LED_OFF);
        }
        xSemaphoreGive(led_mutex);
    }
}

void led_init(void)
{
    // Create mutex for LED control synchronization
    led_mutex = xSemaphoreCreateMutex();
    if (led_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create led_mutex");
        // Handle error: mutex creation failed — you may want to abort or retry
        return;
    }

    // Configure LED GPIO pins as outputs
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RED_LED) | (1ULL << GREEN_LED) | (1ULL << BUZZER) | (1ULL << BLUE_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Create timers for glow (3s timeout) and blinking (periodic)
    glow_timer = xTimerCreate("GlowTimer", pdMS_TO_TICKS(1000), pdFALSE, NULL, glow_timer_callback);
    if (glow_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create glow_timer");
        // Handle error
        return;
    }

    blink_timer = xTimerCreate("BlinkTimer", pdMS_TO_TICKS(500), pdTRUE, NULL, blink_timer_callback);
    if (blink_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create blink_timer");
        // Handle error
        return;
    }

    // Initialize internal state variables
    indefinite_color = LED_OFF;
    override_color = LED_OFF;
    current_mode = LED_MODE_INDEFINITE;
    blink_state = false;

    // Turn off all LEDs initially
    set_gpio_levels(LED_OFF);

    //ESP_LOGI(TAG, "LED module initialized successfully");
}

void led_set_color_indefinite(led_color_t color)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        // Stop any overrides
        if (current_mode != LED_MODE_INDEFINITE)
        {
            if (xTimerIsTimerActive(glow_timer))
            {
                xTimerStop(glow_timer, 0);
            }
            if (xTimerIsTimerActive(blink_timer))
            {
                xTimerStop(blink_timer, 0);
            }
        }
        current_mode = LED_MODE_INDEFINITE;
        indefinite_color = color;
        override_color = LED_OFF;
        set_gpio_levels(color);
        xSemaphoreGive(led_mutex);

        //ESP_LOGI(TAG, "Set indefinite LED color %d", color);
    }
}

void led_override_glow_3s(led_color_t color)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        // Stop blink timer if running
        if (current_mode == LED_MODE_BLINK && xTimerIsTimerActive(blink_timer))
        {
            xTimerStop(blink_timer, 0);
        }

        current_mode = LED_MODE_GLOW_3S;
        override_color = color;
        set_gpio_levels(color);

        xTimerStop(glow_timer, 0);
        xTimerStart(glow_timer, 0);

        xSemaphoreGive(led_mutex);

        //ESP_LOGI(TAG, "Override glow 3s with color %d", color);
    }
}

void led_override_blink(led_color_t color)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        // Stop glow timer if running
        if (current_mode == LED_MODE_GLOW_3S && xTimerIsTimerActive(glow_timer))
        {
            xTimerStop(glow_timer, 0);
        }

        current_mode = LED_MODE_BLINK;
        override_color = color;
        blink_state = false;

        // Start blinking timer
        xTimerStop(blink_timer, 0);
        xTimerStart(blink_timer, 0);

        xSemaphoreGive(led_mutex);

        //ESP_LOGI(TAG, "Override blinking with color %d", color);
    }
}

led_mode_t led_get_mode(void)
{
    led_mode_t mode = LED_MODE_INDEFINITE;
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        mode = current_mode;
        xSemaphoreGive(led_mutex);
    }
    return mode;
}
