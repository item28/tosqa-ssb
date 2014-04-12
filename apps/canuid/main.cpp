// ssb11 can uid - report the LPC11C24's unique ID to the CAN bus
// jcw, 2014-04-10

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

// see http://www.lpcware.com/content/forum/read-serial-number
static void readUid (uint32_t* uid) {
  unsigned param_table[5];
  unsigned result_table[5];
  param_table[0] = 58; // IAP command
  iap_entry(param_table, result_table);
  if (result_table[0] == 0) {
    uid[0] = result_table[1];
    uid[1] = result_table[2];
    uid[2] = result_table[3];
    uid[3] = result_table[4];
  } else {
    uid[0] = uid[1] = uid[2] = uid[3] = 0;
  }
}

int main () {
  halInit();
  chSysInit();
  initCan();

  // brief red flash on startup
  palTogglePad(GPIO0, GPIO0_LED2);
  chThdSleepMilliseconds(100);
  palTogglePad(GPIO0, GPIO0_LED2);

  uint32_t uid[4];
  readUid(uid);
  
  for (;;) {
    msg_obj.msgobj  = 1;
    msg_obj.mode_id = 0x414;
    msg_obj.mask    = 0x0;
    msg_obj.dlc     = 8;
    msg_obj.data[0] = uid[0];
    msg_obj.data[1] = uid[0] >> 8;
    msg_obj.data[2] = uid[0] >> 16;
    msg_obj.data[3] = uid[0] >> 24;
    msg_obj.data[4] = uid[1];
    msg_obj.data[5] = uid[1] >> 8;
    msg_obj.data[6] = uid[1] >> 16;
    msg_obj.data[7] = uid[1] >> 24;

    LPC_CCAN_API->can_transmit(&msg_obj);
    chThdSleepMilliseconds(500);

    msg_obj.msgobj  = 2;
    msg_obj.mode_id = 0x415;
    msg_obj.mask    = 0x0;
    msg_obj.dlc     = 8;
    msg_obj.data[0] = uid[2];
    msg_obj.data[1] = uid[2] >> 8;
    msg_obj.data[2] = uid[2] >> 16;
    msg_obj.data[3] = uid[2] >> 24;
    msg_obj.data[4] = uid[3];
    msg_obj.data[5] = uid[3] >> 8;
    msg_obj.data[6] = uid[3] >> 16;
    msg_obj.data[7] = uid[3] >> 24;

    LPC_CCAN_API->can_transmit(&msg_obj);
    chThdSleepMilliseconds(500);

    palTogglePad(GPIO1, GPIO1_LED1);
    // palTogglePad(GPIO3, GPIO3_MOTOR_MS1);
  }

  return 0;
}
