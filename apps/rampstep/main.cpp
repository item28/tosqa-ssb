// ssb11 pwm ramp - test stepper motor pulsing with PWM and linear ramps
// jcw, 2014-04-16

#include "ch.h"
#include "hal.h"

// keep a LED blinking at 2 Hz in the background
static WORKING_AREA(waBlinkTh, 128);
static msg_t BlinkTh(void*) {
  for (;;) {
    chThdSleepMilliseconds(250);
    palTogglePad(GPIO1, GPIO1_LED1);
  }
  return 0;
}

static void motorInit() {
  // set the static stepper I/O pins to a default state
  palSetPad(GPIO3, GPIO3_MOTOR_SLEEP);    // active low, disabled
  palSetPad(GPIO1, GPIO1_MOTOR_MS3);      // 16-th microstepping
  palSetPad(GPIO1, GPIO1_MOTOR_MS2);      // 16-th microstepping
  palSetPad(GPIO3, GPIO3_MOTOR_MS1);      // 16-th microstepping
  palSetPad(GPIO1, GPIO1_MOTOR_RESET);    // active low, disabled

  LPC_IOCON->R_PIO0_11 = 0xD3;  // MOTOR_STEP, CT32B0_MAT3
  LPC_SYSCON->SYSAHBCLKCTRL |= 1<<9; // enable clock for CT32B0
  LPC_TMR32B0->PR = 12; // prescaler -> 4 MHz
  LPC_TMR32B0->MCR |= (1<<9) | (1<<10); // MR3I + MR3R, p.366
  LPC_TMR32B0->PWMC = 1<<3; // pwm enable, p.372

  nvicEnableVector(TIMER_32_0_IRQn, LPC11xx_PWM_CT32B0_IRQ_PRIORITY);
}

static void motorTimer(bool on) {
  LPC_TMR32B0->TCR = on ? 1 : 0;
}

static void motorRate(uint32_t r) {
  LPC_TMR32B0->MR3 = r >> 8; // treat as 24.8 fixed point
}

static void motorDirection(bool forward) {
  if (forward)
    palClearPad(GPIO1, GPIO1_MOTOR_DIR);
  else
    palSetPad(GPIO1, GPIO1_MOTOR_DIR);
}

static void motorCurrent(bool on) {
  if (on)
    palClearPad(GPIO3, GPIO3_MOTOR_EN);   // active low
  else
    palSetPad(GPIO3, GPIO3_MOTOR_EN);     // active low
}

#define CSLOW  (25000 << 8)
#define CFAST  (500 << 8)

static enum {
  rampIdle, rampUp, rampMax, rampDown, rampLast
} state;

static volatile bool running;
static int pos, inc, rate, stepNo, stepDown, move, midPt, denom;

extern "C" void Vector88 (); // CT32B0
CH_IRQ_HANDLER(Vector88)  {
  CH_IRQ_PROLOGUE();

  uint16_t sr  = LPC_TMR32B0->IR;
  LPC_TMR32B0->IR = sr;

  switch (state) {
    case rampUp:
      if (stepNo == midPt) {
        state = rampDown;
        denom = ((stepNo - move) << 2) + 1;
        if ((move & 1) == 0) {
          denom += 4;
          break;
        }
      }
      // fall through
    case rampDown:
      if (stepNo == move - 1) {
        state = rampLast;
        break;
      }
      denom += 4;
      rate -= (rate << 1) / denom;
      if (rate <= CFAST) {
        state = rampMax;
        stepDown = move - stepNo;
        rate = CFAST;
      }
      break;
    case rampMax:
      if (stepNo == stepDown) {
        state = rampDown;
        denom = ((stepNo - move) << 2) + 5;
      }
      break;
    case rampLast:
      state = rampIdle;
      motorCurrent(false);
      motorTimer(false);
      running = false;
      break;
    case rampIdle:
      return;
  }

  pos += inc;
  ++stepNo;
  motorRate(rate);

  CH_IRQ_EPILOGUE();
}

static void moveTo(int posNew) {
  if (posNew < pos) {
    move = pos - posNew;
    inc = -1;
  } else if (posNew > pos) {
    move = posNew - pos;
    inc = 1;
  } else
    return;
  midPt = (move - 1) >> 1;
  rate = CSLOW;
  stepNo = 0;
  denom = 1;
  state = rampUp;
  running = true;
  
  motorDirection(inc > 0);
  motorRate(rate);
  motorCurrent(true);
  motorTimer(true);
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

  motorInit();

  for (;;) {
    moveTo(10000);
    while (running)
      chThdYield();

    moveTo(0);
    while (running)
      chThdYield();

    // switch to alternate LED colour
    palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO0, GPIO0_LED2);
  }

  return 0;
}
