#include "led.h"

void led_init(void)
{
    gpio_reset_pin(RED_LED);
    gpio_set_direction(RED_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(RED_LED, 0);

    gpio_reset_pin(GREEN_LED);
    gpio_set_direction(GREEN_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(GREEN_LED, 0);

    gpio_reset_pin(BLUE_LED);
    gpio_set_direction(BLUE_LED, GPIO_MODE_OUTPUT);
    gpio_set_level(BLUE_LED, 0);
}

void led_set_color(led_color_t color)
{
    // Turn off all LEDs first
    gpio_set_level(RED_LED, 0);
    gpio_set_level(GREEN_LED, 0);
    gpio_set_level(BLUE_LED, 0);

    switch (color) {
        case LED_RED:
            gpio_set_level(RED_LED, 1);
            break;
        case LED_GREEN:
            gpio_set_level(GREEN_LED, 1);
            break;
        case LED_BLUE:
            gpio_set_level(BLUE_LED, 1);
            break;
        case LED_YELLOW:  // RED + GREEN
            gpio_set_level(RED_LED, 1);
            gpio_set_level(GREEN_LED, 1);
            break;
        case LED_CYAN:    // GREEN + BLUE
            gpio_set_level(GREEN_LED, 1);
            gpio_set_level(BLUE_LED, 1);
            break;
        case LED_MAGENTA: // RED + BLUE
            gpio_set_level(RED_LED, 1);
            gpio_set_level(BLUE_LED, 1);
            break;
        case LED_WHITE:   // RED + GREEN + BLUE
            gpio_set_level(RED_LED, 1);
            gpio_set_level(GREEN_LED, 1);
            gpio_set_level(BLUE_LED, 1);
            break;
        case LED_OFF:
        default:
            // All LEDs off already
            break;
    }
}
