// boot send - send firmware via CAN @ 100 KHz using CANopen
// jcw, 2014-04-14

#include "ch.h"
#include "hal.h"
#include "data.h"
#include <string.h>

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T msg_obj, msg_SDO_Seg_TX;
volatile bool ready;

static void CAN_rxCallback (uint8_t msg_obj_num) {
  msg_obj.msgobj = msg_obj_num;
  LPC_CCAN_API->can_receive(&msg_obj);
  palTogglePad(GPIO3, GPIO3_MOTOR_EN); // PIO3_0 = LED1 on Open11C14 board
  ready = true;
}

static void CAN_txCallback (uint8_t /* msg_obj_num */) {}

static void CAN_errorCallback (uint32_t error_info) {
  if (error_info & 0x0002)
    palTogglePad(GPIO3, GPIO3_MOTOR_SLEEP); // PIO3_2 = LED3 on Open11C14 board
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
    // 0x000076C5UL  // CAN_BTR, 500 Kbps CAN bus rate
    0x000076DDUL  // CAN_BTR, 100 Kbps CAN bus rate
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
    palTogglePad(GPIO3, GPIO3_MOTOR_MS1); // LED4 on Open11C14 board
    // palTogglePad(GPIO2, GPIO2_HALL_MODE);
  }
  return 0;
}

static void sendAndWait(uint8_t node_id, uint16_t index, uint8_t subindex, const uint8_t* data, int length) {
	msg_SDO_Seg_TX.mode_id = 0x600 + node_id;
	msg_SDO_Seg_TX.mask = 0x00;
	msg_SDO_Seg_TX.dlc = 8;
	msg_SDO_Seg_TX.msgobj = 8;
  // msg_SDO_Seg_TX.data[0] = 1<<5 | (4-length)<<2 | 0x02 | 0x01;
  msg_SDO_Seg_TX.data[0] = 2<<5; // SDO read
	msg_SDO_Seg_TX.data[1] = index;
	msg_SDO_Seg_TX.data[2] = index >> 8;
	msg_SDO_Seg_TX.data[3] = subindex;
	msg_SDO_Seg_TX.data[4] = 0x00;
	msg_SDO_Seg_TX.data[5] = 0x00;
	msg_SDO_Seg_TX.data[6] = 0x00;
	msg_SDO_Seg_TX.data[7] = 0x00;

  while (--length >= 0)
		msg_SDO_Seg_TX.data[length+4] = *data++;

	LPC_CCAN_API->can_transmit(&msg_SDO_Seg_TX);

  palTogglePad(GPIO3, GPIO3_DIGIPOT_CS); // LED2 on Open11C14 board
  while (!ready)
    chThdYield();
  palTogglePad(GPIO3, GPIO3_DIGIPOT_CS);
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
  
  /* Configure message object 1 to receive all 11-bit messages 0x5FD */
  msg_obj.msgobj = 1;
  msg_obj.mode_id = 0x5FD;
  msg_obj.mask = 0x7FF;
  LPC_CCAN_API->config_rxmsgobj(&msg_obj);
  sendAndWait(0x7D, 0x1000, 0, 0, 0);

  // int i = 0;
  // // ....
  // msg_obj.msgobj  = 1;
  // msg_obj.mode_id = 0x67D;
  // msg_obj.mask    = 0x0;
  // msg_obj.dlc     = 8;
  // memcpy(msg_obj.data, bootData + 8 * i++, 8);
  // LPC_CCAN_API->can_transmit(&msg_obj);

  for (;;) {
    chThdSleepMilliseconds(500);
  }

  return 0;
}
