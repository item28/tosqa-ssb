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

CCAN_MSG_OBJ_T rxMsg, txMsg;
volatile bool ready;

static void CAN_rxCallback (uint8_t msg_obj_num) {
  rxMsg.msgobj = msg_obj_num;
  LPC_CCAN_API->can_receive(&rxMsg);
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

static void sendAndWait(uint8_t header, uint16_t index, uint8_t subindex, const uint8_t* data, int length) {
  txMsg.mode_id = 0x67D;
	txMsg.mask = 0x00;
	txMsg.dlc = 8;
	txMsg.msgobj = 8;
  txMsg.data[0] = header;
  txMsg.data[0] = header;
	txMsg.data[1] = index;
	txMsg.data[2] = index >> 8;
	txMsg.data[3] = subindex;
  for (int i = 0; i < 4; ++i)
		txMsg.data[4+i] = i < length ? *data++ : 0;

  ready = false;
	LPC_CCAN_API->can_transmit(&txMsg);
  palTogglePad(GPIO3, GPIO3_DIGIPOT_CS); // LED2 on Open11C14 board
  while (!ready)
    chThdYield();
  palTogglePad(GPIO3, GPIO3_DIGIPOT_CS);
}

static uint8_t zero[4];

// not supported by LPC CAN boot loader?
// uint32_t sdoReadExpedited(uint16_t index, uint8_t subindex, uint8_t length) {
//   sendAndWait((2<<5) | ((4-length)<<2) | 0b11, index, subindex, zero, length);
//   return (rxMsg.data[7] << 24) | (rxMsg.data[6] << 16) |
//           (rxMsg.data[5] << 8) | rxMsg.data[4];
// }

void sdoWriteExpedited(uint16_t index, uint8_t subindex, const uint32_t data, uint8_t length) {
  sendAndWait((1<<5) | ((4-length)<<2) | 0b11, index, subindex, (uint8_t*) &data, length);
}

uint32_t sdoReadSegmented(uint16_t index, uint8_t subindex) {
  sendAndWait(0x40, index, subindex, zero, 4);
  return (rxMsg.data[7] << 24) | (rxMsg.data[6] << 16) |
          (rxMsg.data[5] << 8) | rxMsg.data[4];
}

// void sdoWriteSegmented(uint16_t index, uint8_t subindex, int length) {
//   sendAndWait(0x21, index, subindex, (uint8_t*) &length, 4);
// }

uint32_t devType, uid[4];

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
  static CCAN_MSG_OBJ_T msg;
  msg.msgobj = 1;
  msg.mode_id = 0x5FD;
  msg.mask = 0x7FF;
  LPC_CCAN_API->config_rxmsgobj(&msg);

  // read device type ("LPC1")
  devType = sdoReadSegmented(0x1000, 0);
  // unlock for upload
  uint16_t unlock = 23130;
  sdoWriteExpedited(0x5000, 0, unlock, sizeof unlock);
  // read 16-byte serial number
  uid[0] = sdoReadSegmented(0x5100, 1);
  uid[1] = sdoReadSegmented(0x5100, 2);
  uid[2] = sdoReadSegmented(0x5100, 3);
  uid[3] = sdoReadSegmented(0x5100, 4);
  // prepare sectors
  sdoWriteExpedited(0x5020, 0, 0x0000, 2);
  // erase sectors
  sdoWriteExpedited(0x5030, 0, 0x0000, 2);
  // G 0000
  sdoWriteExpedited(0x5070, 0, 0, 4);
  sdoWriteExpedited(0x1F51, 1, 1, 1);

  for (;;) {
    chThdSleepMilliseconds(500);
  }

  return 0;
}
