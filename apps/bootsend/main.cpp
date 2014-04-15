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

static void sendAndWait(uint8_t header, int length) {
  txMsg.mode_id = 0x67D;
	txMsg.mask = 0x00;
	txMsg.dlc = length;
	txMsg.msgobj = 8;
  txMsg.data[0] = header;

  ready = false;
	LPC_CCAN_API->can_transmit(&txMsg);
  palTogglePad(GPIO3, GPIO3_DIGIPOT_CS); // LED2 on Open11C14 board
  while (!ready)
    chThdYield();
  palTogglePad(GPIO3, GPIO3_DIGIPOT_CS);
}

static void sendReq(uint8_t header, uint16_t index, uint8_t subindex, const uint8_t* data, int length) {
	txMsg.data[1] = index;
	txMsg.data[2] = index >> 8;
	txMsg.data[3] = subindex;
  for (int i = 0; i < 4; ++i)
		txMsg.data[4+i] = i < length ? *data++ : 0;
  sendAndWait(header, 8);
  
  if (txMsg.data[7] & 0x08)
    for (;;)
      ; // error reported, hang
}

// not supported by LPC CAN boot loader?
// uint32_t sdoReadExpedited(uint16_t index, uint8_t subindex, uint8_t length) {
//   sendReq((2<<5) | ((4-length)<<2) | 0b11, index, subindex, zero, length);
//   return (rxMsg.data[7] << 24) | (rxMsg.data[6] << 16) |
//           (rxMsg.data[5] << 8) | rxMsg.data[4];
// }

void sdoWriteExpedited(uint16_t index, uint8_t subindex, const uint32_t data, uint8_t length) {
  sendReq((1<<5) | ((4-length)<<2) | 0b11, index, subindex, (uint8_t*) &data, length);
}

uint32_t sdoReadSegmented(uint16_t index, uint8_t subindex) {
  uint32_t zero = 0;
  sendReq(0x40, index, subindex, (uint8_t*) &zero, 4);
  return (rxMsg.data[7] << 24) | (rxMsg.data[6] << 16) |
          (rxMsg.data[5] << 8) | rxMsg.data[4];
}

void sdoWriteSegmented(uint16_t index, uint8_t subindex, int length) {
  sendReq(0x21, index, subindex, (uint8_t*) &length, 4);
}

void sdoWriteDataSegment(uint8_t header, const uint8_t* data, int length) {
  for (int i = 0; i < 7; ++i)
		txMsg.data[1+i] = i < length ? *data++ : 0;
  sendAndWait(header, 8);
}

uint32_t devType, uid[4];

int main () {
  halInit();
  chSysInit();
  initCan();

  // brief red flash on startup
  palTogglePad(GPIO0, GPIO0_LED2);
  chThdSleepMilliseconds(100);
  palTogglePad(GPIO0, GPIO0_LED2);
  
  /* Configure message object 1 to receive all 11-bit messages 0x5FD */
  static CCAN_MSG_OBJ_T msg;
  msg.msgobj = 1;
  msg.mode_id = 0x5FD;
  msg.mask = 0x7FF;
  LPC_CCAN_API->config_rxmsgobj(&msg);

  // read device type ("LPC1")
  devType = sdoReadSegmented(0x1000, 0);
  // read 16-byte serial number
  uid[0] = sdoReadSegmented(0x5100, 1);
  uid[1] = sdoReadSegmented(0x5100, 2);
  uid[2] = sdoReadSegmented(0x5100, 3);
  uid[3] = sdoReadSegmented(0x5100, 4);
  
  // unlock for upload
  uint16_t unlock = 23130;
  sdoWriteExpedited(0x5000, 0, unlock, sizeof unlock);  
  // prepare sectors
  sdoWriteExpedited(0x5020, 0, 0x0000, 2);
  // erase sectors
  sdoWriteExpedited(0x5030, 0, 0x0000, 2);

#if 1
  // start write to RAM
  sdoWriteExpedited(0x5015, 0, 0x10001000, 4); // ram address
  // upload code to RAM
  sdoWriteSegmented(0x1F50, 1, 0x1000);
  uint8_t toggle = 0;
  for (size_t i = 0; i < sizeof bootData; i += 7) {
    sdoWriteDataSegment(toggle, bootData + i, 7);
    toggle ^= 0x10;
  }

  // prepare sectors
  sdoWriteExpedited(0x5020, 0, 0x0000, 2);
  // copy ram to flash
  sdoWriteExpedited(0x5050, 1, 0x00000000, 4); // flash address
  sdoWriteExpedited(0x5050, 2, 0x10001000, 4); // ram address
  sdoWriteExpedited(0x5050, 3, 0x1000, 2); // 4096 bytes
#endif
  
  // G 0000
  sdoWriteExpedited(0x5070, 0, 0, 4);
  sdoWriteExpedited(0x1F51, 1, 1, 1);

  // disable CAN so that it no longer interferes with other CAN traffic
  LPC_SYSCON->SYSAHBCLKCTRL &= ~(1<<17); // SYSCTL_CLOCK_CAN

  // blink LED at 2 Hz
  for (;;) {
    chThdSleepMilliseconds(500);
    // palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO3, GPIO3_MOTOR_MS1); // LED4 on Open11C14 board
    // palTogglePad(GPIO2, GPIO2_HALL_MODE);
  }

  return 0;
}
