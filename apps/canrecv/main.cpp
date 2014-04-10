// ssb11 can receiver - reflect incoming can messages on the LEDs
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8

#define LPC_ROM_API               (*((LPC_ROM_API_T        * *) LPC_ROM_API_BASE_LOC))

#include "../../common/nxp/romapi_11xx.h"
#include "../../common/nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T msg_obj;

static uint32_t clkInitTable[2] = {
  0x00000000UL, // CANCLKDIV, running at 48 MHz
  0x000076C5UL  // CAN_BTR, produces 500 Kbps
};

void CAN_rxCallback (uint8_t msg_obj_num) {
  msg_obj.msgobj = msg_obj_num;
  LPC_CCAN_API->can_receive(&msg_obj);
  // quick sanity check, ignore packets with more than 1 byte of data
  if (msg_obj.dlc == 1) {
    /* payload.byte = msg_obj.data[0]; */
    /* copyToPins(msg_obj.mode_id); */
  }
}

void CAN_txCallback (uint8_t msg_obj_num) {
}

void CAN_errorCallback (uint32_t error_info) {
  if (error_info & 0x0002)
    palClearPad(GPIO3, 1); // turn on LED2 on Open11C14 board
}

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

  LPC_CCAN_API->init_can(clkInitTable, true);
  LPC_CCAN_API->config_calb(&callbacks);
  NVIC_EnableIRQ(CAN_IRQn);
}

// keep a LED blinking at 2 Hz in the background
static WORKING_AREA(waBlinkTh, 64);
static msg_t BlinkTh (void*) {
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

  chThdSleepMilliseconds(50);

  msg_obj.msgobj  = 0;
  msg_obj.mode_id = 0x456;
  msg_obj.mask    = 0x0;
  msg_obj.dlc     = 2;
  msg_obj.data[0] = 0xAA;
  msg_obj.data[0] = 0x55;
  LPC_CCAN_API->can_transmit(&msg_obj);

  for (;;) {
    chThdSleepMilliseconds(500);
  }

  return 0;
}
