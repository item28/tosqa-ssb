// ssb11 stepper - test stepper motor pulsing
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

int main () {
    halInit();
    chSysInit();

    palSetPad(GPIO3, GPIO3_MOTOR_SLEEP);
    palClearPad(GPIO1, GPIO1_MOTOR_MS3);
    palClearPad(GPIO1, GPIO1_MOTOR_MS2);
    palClearPad(GPIO3, GPIO3_MOTOR_MS1);
    palSetPad(GPIO1, GPIO1_MOTOR_RESET);
    palClearPad(GPIO3, GPIO3_MOTOR_EN);
    palClearPad(GPIO1, GPIO1_MOTOR_DIR);
    palClearPad(GPIO0, GPIO0_MOTOR_STEP);

    for (;;) {
        palTogglePad(GPIO1, GPIO1_MOTOR_DIR);

        for (int i = 0; i < 1000; ++i) {
            palTogglePad(GPIO0, GPIO0_MOTOR_STEP);
            
            chThdSleepMilliseconds(2);
        }
    }

    return 0;
}
