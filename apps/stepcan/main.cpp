// ssb11 step can - send CAN bus commands to drive a board running ../canrecv
// jcw, 2014-04-11

#include "ch.h"
#include "hal.h"

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

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

int main () {
  halInit();
  chSysInit();
  initCan();

  // brief red flash on startup
  palTogglePad(GPIO0, GPIO0_LED2);
  chThdSleepMilliseconds(100);
  palTogglePad(GPIO0, GPIO0_LED2);

  // take 500 steps @ 250 Hz in one direction, then in the other, forever
  uint8_t cmd = 0b00000100; // set ENABLE

  for (;;) {
    for (int i = 0; i < 1000; ++i) {
      cmd ^= 0b001; // toggle STEP

      msg_obj.msgobj  = 0;
      msg_obj.mode_id = 0x413;
      msg_obj.mask    = 0x0;
      msg_obj.dlc     = 1;
      msg_obj.data[0] = cmd;

      LPC_CCAN_API->can_transmit(&msg_obj);

      chThdSleepMilliseconds(2);
    }

    cmd ^= 0b010; // toggle DIR

    // palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO3, GPIO3_MOTOR_MS1);
  }

  return 0;
}
