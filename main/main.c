#include "global.h"


void app_main(void)
{
    rfid_int();
    //int_rtc();

    while (true) {
        //vTaskDelay(pdMS_TO_TICKS(1000));  // Keep the task running
    }
}
