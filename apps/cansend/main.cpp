// ssb11 can send - send periodic ADC readings out to the CAN bus
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

static uint16_t readAdc (uint8_t num) {
  LPC_ADC->CR = 0x0B00 | (1 << num);// select AD1..3, set up clock (sec. 25.5.1)
  LPC_ADC->CR |= 1 << 24; // set "Start Conversion Now" bit (sec. 25.5.1)
  while (LPC_ADC->DR[num] < 0x7FFFFFFF)
    ; // wait for "done" bit to be set (sec. 25.5.4)
  return (LPC_ADC->DR[num] >> 6) & 0x3FF;
}

int main () {
  halInit();
  chSysInit();
  initCan();

  // brief red flash on startup
  palTogglePad(GPIO0, GPIO0_LED2);
  chThdSleepMilliseconds(100);
  palTogglePad(GPIO0, GPIO0_LED2);

  for (;;) {
    chThdSleepMilliseconds(500);

    uint16_t adc1 = readAdc(1);
    uint16_t adc2 = readAdc(2);
    uint16_t adc3 = readAdc(3);
    
    msg_obj.msgobj  = 0;
    msg_obj.mode_id = 0x456;
    msg_obj.mask    = 0x0;
    msg_obj.dlc     = 6;
    msg_obj.data[0] = adc1;
    msg_obj.data[1] = adc1 >> 8;
    msg_obj.data[2] = adc2;
    msg_obj.data[3] = adc2 >> 8;
    msg_obj.data[4] = adc3;
    msg_obj.data[5] = adc3 >> 8;

    LPC_CCAN_API->can_transmit(&msg_obj);

    // palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO3, GPIO3_MOTOR_MS1);
  }

  return 0;
}
