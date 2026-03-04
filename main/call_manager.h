#ifndef CALL_MANAGER_H
#define CALL_MANAGER_H

#include "global.h"

// Call types
typedef enum {
    CALL_TYPE_NONE = 0,
    CALL_TYPE_CALL = 1,
    CALL_TYPE_EMERGENCY = 2,
    CALL_TYPE_BLUECODE = 3
} call_type_t;

// Call state
typedef enum {
    CALL_STATE_IDLE = 0,
    CALL_STATE_ACTIVE = 1,
    CALL_STATE_ESCALATED = 2
} call_state_t;

// Call information structure
typedef struct {
    call_type_t type;
    call_state_t state;
    uint64_t start_time_ms;
    bool is_attended;
} call_info_t;

// GPIO Pins for buttons
#define CALL_BUTTON_GPIO    GPIO_NUM_2     // CALL button
#define CANCEL_BUTTON_GPIO  GPIO_NUM_12    // CANCEL button
#define BLUECODE_BUTTON_GPIO GPIO_NUM_15   // BLUECODE button

// Function prototypes
void call_manager_init(void);
void call_button_pressed(void);
void cancel_button_pressed(void);
void bluecode_button_pressed(void);
void rfid_response_to_call(const char *uid, const char *timestamp);
void call_manager_task(void *arg);

// Get current call state
call_info_t get_current_call_info(void);
bool is_call_active(void);

#endif // CALL_MANAGER_H
