// ssb11 rotaries - interface two rotary encoders to the CAN bus
// jcw, 2014-04-27

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

static void CAN_errorCallback (uint32_t /* error_info */) {}

static CCAN_CALLBACKS_T callbacks = {
  CAN_rxCallback, CAN_txCallback, CAN_errorCallback, 0, 0, 0, 0, 0
};

extern "C" void Vector74 ();
CH_IRQ_HANDLER(Vector74) {
  CH_IRQ_PROLOGUE();
  LPC_CCAN_API->isr();
  CH_IRQ_EPILOGUE();
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

// see https://www.circuitsathome.com/mcu/
//  rotary-encoder-interrupt-service-routine-for-avr-micros
const int8_t qeMap[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};

struct Rotary {
  volatile int16_t count;
  int16_t          next, last;
  volatile uint8_t state;
  uint8_t          lastBtn;
} rot1, rot2;

static void rotChange1 () {
  int state = 0;
  if (LPC_GPIO3->DATA & (1<<0)) state |= 0b01;
  if (LPC_GPIO1->DATA & (1<<1)) state |= 0b10;
  rot1.count += qeMap[(rot1.state<<2) | state];
  rot1.state = state;
}

static void rotChange2 () {
  int state = 0;
  if (LPC_GPIO3->DATA & (1<<2)) state |= 0b01;
  if (LPC_GPIO1->DATA & (1<<4)) state |= 0b10;
  rot2.count += qeMap[(rot2.state<<2) | state];
  rot2.state = state;
}

static void rotSend (int addr, struct Rotary& r, int button) {
  r.next = r.count >> 2;
  button = ~button & 1;
  int diff = r.next - r.last;
  if (diff != 0 || button != r.lastBtn) {
    msg_obj.msgobj  = 0;
    msg_obj.mode_id = addr;
    msg_obj.mask    = 0x0;
    msg_obj.dlc     = 3;
    msg_obj.data[0] = r.next;
    msg_obj.data[1] = r.next >> 8;
    msg_obj.data[2] = (diff > 0 ? diff : -diff) & 0x3F;
    msg_obj.data[2] |= (button << 7) | (r.lastBtn << 6);
    LPC_CCAN_API->can_transmit(&msg_obj);
  }
  r.last = r.next;
  r.lastBtn = button;
}

extern "C" void VectorB0 ();
CH_FAST_IRQ_HANDLER(VectorB0) {
  int mis3 = LPC_GPIO3->MIS;
  LPC_GPIO3->IC = mis3;       // clear interrupts for PIO3
  if (mis3 & (1<<0))
    rotChange1();
  if (mis3 & (1<<2))
    rotChange2();
}

extern "C" void VectorB8 ();
CH_FAST_IRQ_HANDLER(VectorB8) {
  int mis1 = LPC_GPIO1->MIS;
  LPC_GPIO1->IC = mis1;       // clear interrupts for PIO1
  if (mis1 & (1<<1))
    rotChange1();
  if (mis1 & (1<<4))
    rotChange2();
}

int main () {
  halInit();
  chSysInit();
  initCan();

  LPC_GPIO2->DIR &= ~(1<<3);  // push1 = PIO2_3, input
  LPC_IOCON->PIO2_3 = 0xF0;   // GPIO, pull-up, hysteresis, digital
  
  LPC_GPIO3->DIR &= ~(1<<0);  // rot1A = PIO3_0, input
  LPC_IOCON->PIO3_0 = 0xF0;   // GPIO, pull-up, hysteresis, digital
  LPC_GPIO3->IBE |= 1<<0;     // trigger on both edges
  LPC_GPIO3->IE |= 1<<0;      // interrupt not masked

  LPC_GPIO1->DIR &= ~(1<<1);  // rot1B = PIO1_1, input
  LPC_IOCON->R_PIO1_1 = 0xF1; // GPIO, pull-up, hysteresis, digital
  LPC_GPIO1->IBE |= 1<<1;     // trigger on both edges
  LPC_GPIO1->IE |= 1<<1;      // interrupt not masked

  LPC_GPIO1->DIR &= ~(1<<6);  // push2 = PIO1_6, input
  LPC_IOCON->PIO1_6 = 0xF0;   // GPIO, pull-up, hysteresis, digital
  
  LPC_GPIO3->DIR &= ~(1<<2);  // rot2A = PIO3_2, input
  LPC_IOCON->PIO3_2 = 0xF0;   // GPIO, pull-up, hysteresis, digital
  LPC_GPIO3->IBE |= 1<<2;     // trigger on both edges
  LPC_GPIO3->IE |= 1<<2;      // interrupt not masked

  LPC_GPIO1->DIR &= ~(1<<4);  // rot2B = PIO1_4, input
  LPC_IOCON->PIO1_4 = 0xF0;   // GPIO, pull-up, hysteresis, digital
  LPC_GPIO1->IBE |= 1<<4;     // trigger on both edges
  LPC_GPIO1->IE |= 1<<4;      // interrupt not masked

  NVIC_EnableIRQ(EINT3_IRQn);
  NVIC_EnableIRQ(EINT1_IRQn);

  rot1.last = rot2.last = 1<<15; // force initial mismatch
  for (;;) {
    chThdSleepMilliseconds(100);
    
    rotSend(0x456, rot1, LPC_GPIO2->DATA >> 3);
    rotSend(0x457, rot2, LPC_GPIO1->DATA >> 6);
  }

  return 0;
}
