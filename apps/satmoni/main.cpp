// aoaa sat demo - show incoming CAN bus data on the display
// jcw, 2014-04-11

#include "ch.h"
#include "hal.h"
#include "aoa/oled.h"

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T msg_obj;

static void CAN_rxCallback (uint8_t msg_obj_num) {
  msg_obj.msgobj = msg_obj_num;
  LPC_CCAN_API->can_receive(&msg_obj);
  // quick sanity check, ignore packets with anything but 1 byte of data
  if (msg_obj.dlc == 1) {
    // copyToPins(msg_obj.data[0]);
  }
}

static void CAN_txCallback (uint8_t /* msg_obj_num */) {}

static void CAN_errorCallback (uint32_t error_info) {
  if (error_info & 0x0002)
    palTogglePad(GPIO0, GPIO0_LEDR); // turn on red LED of RGB
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
    palTogglePad(GPIO0, GPIO0_LED);
  }
  return 0;
}

int main () {
  halInit();
  chSysInit();
  initCan();
  
  static const I2CConfig i2cconfig = { I2C_FAST_MODE_PLUS, 2000000 };
  i2cStart(&I2CD1, &i2cconfig);

  init_OLED();
  
  // brief red flash on startup
  palTogglePad(GPIO0, GPIO0_LED);
  chThdSleepMilliseconds(100);
  palTogglePad(GPIO0, GPIO0_LED);

  // launch the blinker background thread
  chThdCreateStatic(waBlinkTh, sizeof(waBlinkTh), NORMALPRIO, BlinkTh, NULL);

  /* Configure message object 1 to receive all 11-bit messages 0x410-0x413 */
  msg_obj.msgobj = 1;
  msg_obj.mode_id = 0x410;
  msg_obj.mask = 0x7FC;
  LPC_CCAN_API->config_rxmsgobj(&msg_obj);

  chThdSleepMilliseconds(1000);
  
  static char buf[17];
  for (int y = 0; ; ++y) {
    chThdSleepMilliseconds(500);
    for (int x = 0; x < 16; ++x)
      buf[x] = ' ' + x + y % 80;
    sendStrXY(buf, y % 8, 0);
  }

  return 0;
}
