// ssb11 stepper - test stepper motor pulsing
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

// keep a LED blinking at 2 Hz in the background
static WORKING_AREA(waBlinkTh, 64);
static msg_t BlinkTh(void*) {
    for (;;) {
        chThdSleepMilliseconds(250);
        palTogglePad(GPIO1, GPIO1_LED1);
    }
    return 0;
}

int main () {
    halInit();
    chSysInit();
    
    // brief red flash on startup
    palTogglePad(GPIO0, GPIO0_LED2);
    chThdSleepMilliseconds(100);
    palTogglePad(GPIO0, GPIO0_LED2);

    // launch the blinker background thread
    chThdCreateStatic(waBlinkTh, sizeof(waBlinkTh), NORMALPRIO, BlinkTh, NULL);

    // set all the stepper I/O pins to a basic enabled state
    palSetPad(GPIO3, GPIO3_MOTOR_SLEEP);    // active low, disabled
    palClearPad(GPIO1, GPIO1_MOTOR_MS3);
    palClearPad(GPIO1, GPIO1_MOTOR_MS2);
    palClearPad(GPIO3, GPIO3_MOTOR_MS1);
    palSetPad(GPIO1, GPIO1_MOTOR_RESET);    // active low, disabled
    palClearPad(GPIO3, GPIO3_MOTOR_EN);     // active low, enabled
    palClearPad(GPIO1, GPIO1_MOTOR_DIR);
    palClearPad(GPIO0, GPIO0_MOTOR_STEP);

    // take 500 steps @ 250 Hz in one direction, then in the other, forever
    for (;;) {
        for (int i = 0; i < 1000; ++i) {
            palTogglePad(GPIO0, GPIO0_MOTOR_STEP);
            chThdSleepMilliseconds(2);
        }

        // switch direction and LEDs
        palTogglePad(GPIO1, GPIO1_MOTOR_DIR);
        palTogglePad(GPIO1, GPIO1_LED1);
        palTogglePad(GPIO0, GPIO0_LED2);
    }

    return 0;
}
