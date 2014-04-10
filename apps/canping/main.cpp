// ssb11 can ping - send a periodic massage out to the CAN bus
// jcw, 2014-04-10

#include "ch.h"
#include "hal.h"

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "../../common/nxp/romapi_11xx.h"
#include "../../common/nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T msg_obj;

static void CAN_rxCallback (uint8_t /* msg_obj_num */) {}

static void CAN_txCallback (uint8_t /* msg_obj_num */) {}

static void CAN_errorCallback (uint32_t error_info) {
  if (error_info & 0x0002)
    palTogglePad(GPIO3, 1); // turn on LED2 on Open11C14 board
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

  static uint32_t clkInitTable[2] = {
    0x00000000UL, // CANCLKDIV, running at 48 MHz
    0x000076C5UL  // CAN_BTR, 500 Kbps CAN bus rate
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
    // palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO3, GPIO3_MOTOR_MS1);
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

  for (;;) {
    chThdSleepMilliseconds(500);

    msg_obj.msgobj  = 0;
    msg_obj.mode_id = 0x456;
    msg_obj.mask    = 0x0;
    msg_obj.dlc     = 2;
    msg_obj.data[0] = 0xAA;
    msg_obj.data[1] = 0x55;

    LPC_CCAN_API->can_transmit(&msg_obj);
  }

  return 0;
}
