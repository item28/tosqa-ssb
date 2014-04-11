// ssb11 can receiver - reflect incoming can messages on the LEDs
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T msg_obj;

static void copyToPins (uint8_t bits) {
  palWritePad(GPIO3, GPIO3_MOTOR_SLEEP, ~(bits >> 7) & 1); // active low
  palWritePad(GPIO1, GPIO1_MOTOR_MS3,    (bits >> 6) & 1);
  palWritePad(GPIO1, GPIO1_MOTOR_MS2,    (bits >> 5) & 1);
  palWritePad(GPIO3, GPIO3_MOTOR_MS1,    (bits >> 4) & 1);
  palWritePad(GPIO1, GPIO1_MOTOR_RESET, ~(bits >> 3) & 1); // active low
  palWritePad(GPIO3, GPIO3_MOTOR_EN,    ~(bits >> 2) & 1); // active low
  palWritePad(GPIO1, GPIO1_MOTOR_DIR,    (bits >> 1) & 1);
  palWritePad(GPIO0, GPIO0_MOTOR_STEP,   (bits >> 0) & 1);
}

static void CAN_rxCallback (uint8_t msg_obj_num) {
  msg_obj.msgobj = msg_obj_num;
  LPC_CCAN_API->can_receive(&msg_obj);
  // quick sanity check, ignore packets with anything but 1 byte of data
  if (msg_obj.dlc == 1)
    copyToPins(msg_obj.data[0]);
}

static void CAN_txCallback (uint8_t /* msg_obj_num */) {}

static void CAN_errorCallback (uint32_t /* error_info */) {}

static CCAN_CALLBACKS_T callbacks = {
  CAN_rxCallback,
  CAN_txCallback,
  CAN_errorCallback,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

// void CAN_IRQHandler (void) {
extern "C" void Vector74 ();
void Vector74 () {
  LPC_CCAN_API->isr();
}

static void initCan () {
  LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 17; // SYSCTL_CLOCK_CAN

  static uint32_t clkInitTable[2] = {
    0x00000000UL, // CANCLKDIV, running at 48 MHz
    0x000076C5UL  // CAN_BTR, produces 500 Kbps
  };

  LPC_CCAN_API->init_can(clkInitTable, true);
  LPC_CCAN_API->config_calb(&callbacks);
  NVIC_EnableIRQ(CAN_IRQn);
}

// keep a LED blinking at 2 Hz in the background
static WORKING_AREA(waBlinkTh, 64);
static msg_t BlinkTh (void*) {
  for (;;) {
    chThdSleepMilliseconds(250);
    palTogglePad(GPIO1, GPIO1_LED1);
  }
  return 0;
}

int main () {
  halInit();
  chSysInit();
  initCan();

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

  /* Configure message object 1 to receive all 11-bit messages 0x410-0x413 */
  msg_obj.msgobj = 1;
  msg_obj.mode_id = 0x410;
  msg_obj.mask = 0x7FC;
  LPC_CCAN_API->config_rxmsgobj(&msg_obj);

  for (;;) {
    chThdSleepMilliseconds(500);
  }

  return 0;
}
