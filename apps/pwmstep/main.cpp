// ssb11 pwm step - test stepper motor pulsing with PWM
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
  palSetPad(GPIO1, GPIO1_MOTOR_MS3);      // 16-th microstepping
  palSetPad(GPIO1, GPIO1_MOTOR_MS2);      // 16-th microstepping
  palSetPad(GPIO3, GPIO3_MOTOR_MS1);      // 16-th microstepping
  palSetPad(GPIO1, GPIO1_MOTOR_RESET);    // active low, disabled
  palClearPad(GPIO3, GPIO3_MOTOR_EN);     // active low, enabled
  palClearPad(GPIO1, GPIO1_MOTOR_DIR);
  // palClearPad(GPIO0, GPIO0_MOTOR_STEP);

  LPC_IOCON->R_PIO0_11 = 0xD3;  // MOTOR_STEP, CT32B0_MAT3
  LPC_SYSCON->SYSAHBCLKCTRL |= 1<<9; // enable clock for CT32B0
  LPC_TMR32B0->PR = 47; // prescaler -> 1 MHz
  LPC_TMR32B0->MCR |= 1<<10; // MR3R p.366
  LPC_TMR32B0->MR3 = 2000/16; // reset at 2000 -> 500 Hz (200-stepper: 150 rpm)
  LPC_TMR32B0->PWMC = 1<<3; // pwm enable p.372
  LPC_TMR32B0->TCR = 1; // start

  // step for 2s in one direction, then in the other, forever
  for (;;) {
    chThdSleepMilliseconds(2000);
    // switch direction and LEDs
    palTogglePad(GPIO1, GPIO1_MOTOR_DIR);
    palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO0, GPIO0_LED2);
  }

  return 0;
}
