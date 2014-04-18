// ssb11 driver - command processor for motor stepping via the CAN bus
// jcw, 2014-04-18


#include "ch.h"
#include "hal.h"

// Olimex LPC-P11C24 board has green/yellow LEDs, but on different pins
#define GPIO1_OLI_LED1    GPIO1_DIGIPOT_UD  // PIO1_11, green
#define GPIO1_OLI_LED2    GPIO1_POWER_TERM  // PIO1_10, yellow

int main () {
    halInit();
    chSysInit();

    for (;;) {
        palTogglePad(GPIO1, GPIO1_OLI_LED1);
        palTogglePad(GPIO1, GPIO1_OLI_LED2);
        chThdSleepMilliseconds(250);
    }

    return 0;
}
