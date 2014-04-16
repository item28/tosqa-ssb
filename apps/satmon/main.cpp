// aoaa sat demo - show incoming CAN bus data on the display
// jcw, 2014-04-11

#include "ch.h"
#include "hal.h"
#include "aoa/oled.h"
#include "chprintf.h"

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T msg_obj, rxMsg;
volatile bool received;

static void CAN_rxCallback (uint8_t msg_obj_num) {
  msg_obj.msgobj = msg_obj_num;
  LPC_CCAN_API->can_receive(&msg_obj);
  if (!received) {
    rxMsg = msg_obj;
    received = true;
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

extern "C" void Vector74 ();
CH_IRQ_HANDLER(Vector74)  {
  CH_IRQ_PROLOGUE();
  LPC_CCAN_API->isr();
  CH_IRQ_EPILOGUE();
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

// keep a LED flashing at 1 Hz in the background, but very briefly and dimly
static WORKING_AREA(waBlinkTh, 64);
static msg_t BlinkTh (void*) {
  for (;;) {
    palTogglePad(GPIO0, GPIO0_LED);
    chThdSleepMilliseconds(999);
    palTogglePad(GPIO0, GPIO0_LED);
    chThdSleepMilliseconds(1);
  }
  return 0;
}

int main () {
  halInit();
  chSysInit();
  initCan();
  
  static const I2CConfig i2cconfig = { I2C_FAST_MODE_PLUS, 2000000 };
  i2cStart(&I2CD1, &i2cconfig); // 2 MHz seems to work fine for this display

  init_OLED();
  
  // brief red flash on startup
  palTogglePad(GPIO0, GPIO0_LED);
  chThdSleepMilliseconds(100);
  palTogglePad(GPIO0, GPIO0_LED);

  // launch the blinker background thread
  chThdCreateStatic(waBlinkTh, sizeof(waBlinkTh), NORMALPRIO, BlinkTh, NULL);

  /* Configure message object 1 to receive all 11-bit messages */
  msg_obj.msgobj = 1;
  msg_obj.mode_id = 0x0;
  msg_obj.mask = 0x0;
  LPC_CCAN_API->config_rxmsgobj(&msg_obj);

  chThdSleepMilliseconds(1000);
  clearDisplay();
  
  static char buf[17];
  for (int y = 0; ; ++y) {
    while (!received)
      chThdYield(); // wait until data available from receiver interrupt
    
    switch (rxMsg.mode_id) {
      case 0x415: // change the receive address & mask
        rxMsg.msgobj = 1;
        rxMsg.mode_id = rxMsg.data[0] | (rxMsg.data[1] << 8) |
                        (rxMsg.data[2] << 16) | (rxMsg.data[3] << 24);
        rxMsg.mask = rxMsg.data[0] | (rxMsg.data[1] << 8) |
                     (rxMsg.data[2] << 16) | (rxMsg.data[3] << 24);
        LPC_CCAN_API->config_rxmsgobj(&rxMsg);
        // display settings on line 7 of the display
        chsnprintf(buf, sizeof buf, "%8x%8x", rxMsg.mode_id, rxMsg.mask);
        sendStrXY(buf, 7, 0);
        break;
        
      case 0x416: // set display message, part 1
      case 0x417: // set display message, part 2
        for (int i = 0; i < 8; ++i)
          buf[i] = rxMsg.data[i];
        buf[8] = 0;
        sendStrXY(buf, 7, rxMsg.mode_id == 0x415 ? 0 : 8);
        break;
        
      default: { // show message in top six lines of display, cycling
        int line = (y * 2) % 6;

        palTogglePad(GPIO0, GPIO0_LEDB);    // blue LED on, very briefly
        sendStrXY(" ", (line + 4) % 6, 0);  // clear the previous cursor
        palTogglePad(GPIO0, GPIO0_LEDB);    // blue LED off again
    
        // display first line
        chsnprintf(buf, sizeof buf, ">%03x            ", rxMsg.mode_id);
        sendStrXY(buf, line, 0);
    
        // display second line
        chsnprintf(buf, sizeof buf, "%02X%02X%02x%02x%02X%02X%02x%02x",
          rxMsg.data[0], rxMsg.data[1], rxMsg.data[2], rxMsg.data[3],
          rxMsg.data[4], rxMsg.data[5], rxMsg.data[6], rxMsg.data[7]);
        for (int i = msg_obj.dlc; i < 8; ++i)
          buf[2*i] = buf[2*i+1] = ' '; // clear display past the received bytes
        sendStrXY(buf, line + 1, 0);
        break;
      }
    }    
    
    // release message buffer for re-use - since display updating is slow,
    // some received messages may have been dropped during the update cycle
    received = false;
  }

  return 0;
}
