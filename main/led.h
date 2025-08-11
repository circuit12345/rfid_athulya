#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "global.h"
typedef enum {
    LED_OFF,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    RING,
    LED_YELLOW,   // RED + GREEN
    LED_CYAN,     // GREEN + BLUE
    LED_MAGENTA,  // RED + BLUE
    LED_WHITE     // RED + GREEN + BLUE
} led_color_t;

typedef enum {
    LED_MODE_INDEFINITE,
    LED_MODE_GLOW_3S,
    LED_MODE_BLINK,
} led_mode_t;

void led_init(void);

/**
 * @brief Set LED color indefinitely (normal mode)
 */
void led_set_color_indefinite(led_color_t color);

/**
 * @brief Override LED color to glow for 3 seconds, then revert to indefinite
 */
void led_override_glow_3s(led_color_t color);

/**
 * @brief Override LED color to blink until overridden or stopped
 */
void led_override_blink(led_color_t color);

/**
 * @brief Get current LED mode
 */
led_mode_t led_get_mode(void);

#endif // LED_H
