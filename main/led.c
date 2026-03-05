#include "led.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "call_manager.h"

static SemaphoreHandle_t led_mutex;

static TimerHandle_t glow_timer;
static TimerHandle_t blink_timer;

static led_mode_t current_mode = LED_MODE_INDEFINITE;
static led_color_t indefinite_color = LED_OFF;
static led_color_t override_color = LED_OFF;

static bool blink_state = false;

// Call manager LED ownership variables
static bool call_manager_owns_led = false;
static led_color_t call_manager_color = LED_OFF;

// Buzzer PWM control
static uint8_t buzzer_volume = 50;  // 0-100% volume (default 50%)
#define BUZZER_LEDC_CHANNEL LEDC_CHANNEL_0
#define BUZZER_LEDC_TIMER LEDC_TIMER_0
#define BUZZER_PWM_FREQ 4000  // 4kHz PWM frequency for buzzer
#define BUZZER_PWM_RESOLUTION LEDC_TIMER_8_BIT  // 8-bit resolution (0-255)

static void buzzer_set_pwm(uint8_t duty)
{
    // Set buzzer PWM duty cycle (0-255, where 255 is max volume)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_LEDC_CHANNEL);
}

static void set_gpio_levels(led_color_t color)
{
    // Turn on/off GPIO pins based on color enum
    switch (color)
    {
    case LED_OFF:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 0);
        buzzer_set_pwm(0);  // PWM off for buzzer
        break;
    case LED_RED:
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 0);
        buzzer_set_pwm(0);
        break;
    case LED_GREEN:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 0);
        buzzer_set_pwm((buzzer_volume * 255) / 100);  // Set buzzer to current volume
        break;
    case LED_BLUE:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 1);
        buzzer_set_pwm(0);
        break;
    case LED_YELLOW: // RED + GREEN
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 0);
        buzzer_set_pwm(0);
        break;
    case LED_CYAN: // GREEN + BLUE
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 1);
        buzzer_set_pwm(0);
        break;
    case LED_MAGENTA: // RED + BLUE
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 1);
        buzzer_set_pwm(0);
        break;
    case LED_WHITE: // RED + GREEN + BLUE
        gpio_set_level(RED_LED, 1);
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(BLUE_LED, 1);
        buzzer_set_pwm(0);
        break;
    case RING:
        buzzer_set_pwm(0);
        break;
    default:
        gpio_set_level(RED_LED, 0);
        gpio_set_level(GREEN_LED, 0);
        gpio_set_level(BLUE_LED, 0);
        buzzer_set_pwm(0);
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
        return;
    }

    // Configure LED GPIO pins as outputs (but NOT buzzer - it will use LEDC)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RED_LED) | (1ULL << GREEN_LED) | (1ULL << BLUE_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Configure LEDC PWM for buzzer control
    ledc_timer_config_t pwm_timer = {
        .duty_resolution = BUZZER_PWM_RESOLUTION,
        .freq_hz = BUZZER_PWM_FREQ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = BUZZER_LEDC_TIMER,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&pwm_timer);

    ledc_channel_config_t pwm_channel = {
        .channel = BUZZER_LEDC_CHANNEL,
        .duty = 0,
        .gpio_num = BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = BUZZER_LEDC_TIMER,
    };
    ledc_channel_config(&pwm_channel);

    // Create timers for glow (3s timeout) and blinking (periodic)
    glow_timer = xTimerCreate("GlowTimer", pdMS_TO_TICKS(1000), pdFALSE, NULL, glow_timer_callback);
    if (glow_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create glow_timer");
        return;
    }

    blink_timer = xTimerCreate("BlinkTimer", pdMS_TO_TICKS(500), pdTRUE, NULL, blink_timer_callback);
    if (blink_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create blink_timer");
        return;
    }

    // Initialize internal state variables
    indefinite_color = LED_OFF;
    override_color = LED_OFF;
    current_mode = LED_MODE_INDEFINITE;
    blink_state = false;

    // Turn off all LEDs initially
    set_gpio_levels(LED_OFF);

    ESP_LOGI(TAG, "LED module initialized successfully - Buzzer volume: %d%%", buzzer_volume);
}

void led_set_color_indefinite(led_color_t color)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        // Don't change LED if call manager owns it
        if (call_manager_owns_led) {
            xSemaphoreGive(led_mutex);
            ESP_LOGD(TAG, "LED indefinite request blocked - call manager owns LED");
            return;
        }

        // Stop any overrides UNLESS a call is actively blinking
        if (current_mode != LED_MODE_INDEFINITE)
        {
            if (xTimerIsTimerActive(glow_timer))
            {
                xTimerStop(glow_timer, 0);
            }
            // Only stop blink if no active call is using it
            if (xTimerIsTimerActive(blink_timer) && !is_call_blinking())
            {
                xTimerStop(blink_timer, 0);
            }
        }
        current_mode = LED_MODE_INDEFINITE;
        indefinite_color = color;
        override_color = LED_OFF;
        // Only change LED if not actively blinking a call
        if (!is_call_blinking())
        {
            set_gpio_levels(color);
        }
        xSemaphoreGive(led_mutex);

        //ESP_LOGI(TAG, "Set indefinite LED color %d", color);
    }
}

void led_override_glow_3s(led_color_t color)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        // Don't change LED if call manager owns it
        if (call_manager_owns_led) {
            xSemaphoreGive(led_mutex);
            ESP_LOGD(TAG, "LED glow request blocked - call manager owns LED");
            return;
        }

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
        // Don't change LED if call manager owns it
        if (call_manager_owns_led) {
            xSemaphoreGive(led_mutex);
            ESP_LOGD(TAG, "LED blink request blocked - call manager owns LED");
            return;
        }

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

/**
 * @brief Set LED color with call manager ownership - static/persistent until released
 * This prevents other tasks from changing the LED until led_release_call_manager() is called
 */
void led_set_color_call_manager(led_color_t color)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        // Stop any active timers since call manager now owns the LED
        if (xTimerIsTimerActive(glow_timer))
        {
            xTimerStop(glow_timer, 0);
        }
        if (xTimerIsTimerActive(blink_timer))
        {
            xTimerStop(blink_timer, 0);
        }

        call_manager_owns_led = true;
        call_manager_color = color;
        current_mode = LED_MODE_INDEFINITE;  // Static mode - no timers
        override_color = LED_OFF;
        blink_state = false;

        set_gpio_levels(color);

        xSemaphoreGive(led_mutex);
        ESP_LOGI(TAG, "Call manager took LED ownership with color %d", color);
    }
}

/**
 * @brief Release LED control back to normal mode (LED_OFF)
 */
void led_release_call_manager(void)
{
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        call_manager_owns_led = false;
        call_manager_color = LED_OFF;
        indefinite_color = LED_OFF;
        override_color = LED_OFF;
        current_mode = LED_MODE_INDEFINITE;
        blink_state = false;

        // Turn off all LEDs
        set_gpio_levels(LED_OFF);

        xSemaphoreGive(led_mutex);
        ESP_LOGI(TAG, "Call manager released LED control");
    }
}

/**
 * @brief Check if call manager owns the LED
 */
bool led_is_owned_by_call_manager(void)
{
    bool owns = false;
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        owns = call_manager_owns_led;
        xSemaphoreGive(led_mutex);
    }
    return owns;
}

/**
 * @brief Set buzzer volume level
 * @param volume 0-100 (percentage)
 */
void led_set_buzzer_volume(uint8_t volume)
{
    if (volume > 100) volume = 100;
    
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        buzzer_volume = volume;
        // If buzzer is currently on (green LED), update PWM duty
        if (indefinite_color == LED_GREEN || override_color == LED_GREEN) {
            buzzer_set_pwm((buzzer_volume * 255) / 100);
        }
        xSemaphoreGive(led_mutex);
        ESP_LOGI(TAG, "Buzzer volume set to %d%%", volume);
    }
}

/**
 * @brief Get current buzzer volume level
 * @return volume 0-100 (percentage)
 */
uint8_t led_get_buzzer_volume(void)
{
    uint8_t vol = 0;
    if (xSemaphoreTake(led_mutex, pdMS_TO_TICKS(100)))
    {
        vol = buzzer_volume;
        xSemaphoreGive(led_mutex);
    }
    return vol;
}
