#ifndef RF_H
#define RF_H

#include "global.h"

// RF Remote button types
typedef enum {
    RF_BUTTON_NONE = 0,
    RF_BUTTON_CALL = 1,      // Call button from remote
    RF_BUTTON_CANCEL = 2,    // Cancel button from remote
    RF_BUTTON_BLUECODE = 3   // Bluecode button from remote
} rf_button_t;

// GPIO Pin for RF 433MHz receiver
#define RF_RECEIVER_GPIO    GPIO_NUM_5    // RF 433MHz receiver input pin

// RF receiver configuration
#define RF_RECEIVER_TIMEOUT_MS 100        // Max time to wait between bits
#define RF_SIGNAL_LEVEL_HIGH 1
#define RF_SIGNAL_LEVEL_LOW 0

// Function prototypes
void custom_rf_init(void);
void rf_task(void *arg);
void rf_isr_handler(void *arg);

#endif // RF_H
