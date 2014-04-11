// aoaa sat demo - show some demo info on the attached 128x64 oled display
// jcw, 2014-04-11

#include "ch.h"
#include "hal.h"
#include "aoa/oled.h"

// keep a LED blinking at 2 Hz in the background
static WORKING_AREA(waBlinkTh, 64);
static msg_t BlinkTh (void*) {
  for (;;) {
    chThdSleepMilliseconds(250);
    palTogglePad(GPIO0, GPIO0_LED);
  }
  return 0;
}

int main () {
  halInit();
  chSysInit();
  
  static const I2CConfig i2cconfig = { I2C_FAST_MODE_PLUS, 2000000 };
  i2cStart(&I2CD1, &i2cconfig);

  init_OLED();
  
  // launch the blinker background thread
  chThdCreateStatic(waBlinkTh, sizeof(waBlinkTh), NORMALPRIO, BlinkTh, NULL);
  
  chThdSleepMilliseconds(1000);
  clearDisplay();
  printBigDigit(1, 0, 0);
  chThdSleepMilliseconds(250);
  printBigDigit(2, 0, 3);
  chThdSleepMilliseconds(250);
  printBigDigit(3, 0, 6);
  chThdSleepMilliseconds(250);
  printBigDigit(4, 0, 9);
  chThdSleepMilliseconds(250);
  printBigDigit(5, 0, 12);
  chThdSleepMilliseconds(250);
  printBigDigit(6, 4, 0);
  chThdSleepMilliseconds(250);
  printBigDigit(7, 4, 3);
  chThdSleepMilliseconds(250);
  printBigDigit(8, 4, 6);
  chThdSleepMilliseconds(250);
  printBigDigit(9, 4, 9);
  chThdSleepMilliseconds(250);
  printBigDigit(0, 4, 12);
  chThdSleepMilliseconds(1000);

  // display some changing text on the display
  static char buf[17];
  for (int y = 0; ; ++y) {
    chThdSleepMilliseconds(500);
    for (int x = 0; x < 16; ++x)
      buf[x] = ' ' + x + y % 80;
    sendStrXY(buf, y % 8, 0);
  }

  return 0;
}
