// ssb11 can receiver - reflect incoming can messages on the LEDs
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

// keep a LED blinking at 2 Hz in the background
static WORKING_AREA(waBlinkTh, 64);
static msg_t BlinkTh(void*) {
    for (;;) {
        chThdSleepMilliseconds(250);
        // palTogglePad(GPIO1, GPIO1_LED1);
        palTogglePad(GPIO3, GPIO3_MOTOR_SLEEP);
        palTogglePad(GPIO3, GPIO3_MOTOR_MS1);
        palTogglePad(GPIO3, GPIO3_MOTOR_EN);
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

    for (;;) {
        chThdSleepMilliseconds(500);
    }

    return 0;
}
