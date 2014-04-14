// ssb11 digipot - move digital vref pot through its entire range
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

  // send 64 pulses to move the digipot, then reverse direction
  for (;;) {
    palTogglePad(GPIO1, GPIO1_DIGIPOT_UD);
    chThdSleepMilliseconds(1);
    palClearPad(GPIO3, GPIO3_DIGIPOT_CS);
    chThdSleepMilliseconds(1);
    for (int i = 0; i < 64; ++i) {
      palTogglePad(GPIO1, GPIO1_DIGIPOT_UD);
      chThdSleepMilliseconds(1);
      palTogglePad(GPIO1, GPIO1_DIGIPOT_UD);
      chThdSleepMilliseconds(98);
    }
    palSetPad(GPIO3, GPIO3_DIGIPOT_CS);
    chThdSleepMilliseconds(1);

    // switch direction and LEDs
    palTogglePad(GPIO1, GPIO1_MOTOR_DIR);
    palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO0, GPIO0_LED2);
  }

  return 0;
}
