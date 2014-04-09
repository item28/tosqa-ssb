#include "ch.h"
#include "hal.h"

static WORKING_AREA(waThread1, 64);

static msg_t Thread1 (void*) {
  while (true) {
    /* LPC_GPIO_PORT->NOT0 = 1 << LED_RED; */
    chThdSleepMilliseconds(500);
  }
  return 0;
}

int main () {
  halInit();
  chSysInit();

  /* LPC_GPIO_PORT->DIR0 |= (1 << LED_RED) | (1 << LED_GREEN) | (1 << LED_BLUE); */

  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  do {
    chThdSleepMilliseconds(500);
  } while (true);
  
  return 0;
}
