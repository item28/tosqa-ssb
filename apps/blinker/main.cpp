// ssb11 blinker - alternately blink LED1 and LED2
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

int main () {
    halInit();
    chSysInit();

    for (;;) {
        palTogglePad(GPIO0, GPIO0_HALL_INC);  // PIO0_7 = LED on LPCX11C24, etc
        palTogglePad(GPIO2, GPIO2_HALL_MODE); // PIO2_10 = LED on LPC11C24-DK-A

        palTogglePad(GPIO1, GPIO1_LED1);
        chThdSleepMilliseconds(250);
        palTogglePad(GPIO0, GPIO0_LED2);
    }

    return 0;
}
