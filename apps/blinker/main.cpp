// ssb11_blinker - alternately blink LED1 and LED2
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

int main () {
    halInit();
    chSysInit();

    // this works on an LPC11C24 LPCXpresso board (LED on PIO0_7, ENDSTOP0)
    // and on a WaveShare Open11C14 dev board (2 LEDs, same pins as on SSB)

    palClearPad(GPIO0, GPIO0_LED);
    palClearPad(GPIO3, GPIO3_LED1);
    palSetPad(GPIO3, GPIO3_LED2);

    do {
        chThdSleepMilliseconds(500);
        palTogglePad(GPIO0, GPIO0_LED);
        palTogglePad(GPIO3, GPIO3_LED1);
        palTogglePad(GPIO3, GPIO3_LED2);
    } while (true);

    return 0;
}
