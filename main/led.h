#ifndef LED_H
#define LED_H

#include "global.h"
typedef enum {
    LED_OFF,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_YELLOW,   // RED + GREEN
    LED_CYAN,     // GREEN + BLUE
    LED_MAGENTA,  // RED + BLUE
    LED_WHITE     // RED + GREEN + BLUE
} led_color_t;
void led_init(void);
void led_set_color(led_color_t color);

#endif // LED_H
