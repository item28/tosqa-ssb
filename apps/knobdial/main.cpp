// ssb11 knob dial - use rotary switch knobs to control steppers over CAN
// jcw, 2014-04-23

#include "ch.h"
#include "hal.h"
#include <string.h>

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

static void CAN_rxCallback (uint8_t /* msg_obj_num */) {}

static CCAN_CALLBACKS_T callbacks = {
  CAN_rxCallback, 0, 0, 0, 0, 0, 0, 0,
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
    0x000076C5UL  // CAN_BTR, 500 Kbps CAN bus rate
  };

  LPC_CCAN_API->init_can(clkInitTable, true);
  LPC_CCAN_API->config_calb(&callbacks);
  NVIC_EnableIRQ(CAN_IRQn);
}

// a setpoint defines a time in the future with given position and velocity
// this is a "wish", the stepper code tries to get there as well as it can
typedef struct {
    uint16_t time;          // milliseconds, wraps around roughly once a minute
    int16_t  position;      // absolute target position, signed
    int16_t  velocity;      // maximum velocity, signed
    uint16_t relative  :1;  // position is relative to previous one
    uint16_t unused    :15; // could be used for triggers, LEDs, etc
} Setpoint;

static void sendSetPoint(int id, int ms, int pos) {
    static Setpoint sp;
    sp.time = ms * 8; // time steps are in multiples of 125 µs
    sp.position = pos;
    sp.relative = 1;
    sp.velocity = 1;

    CCAN_MSG_OBJ_T txMsg;
    txMsg.msgobj  = 1;
    txMsg.mode_id = 0x100 + id;
    txMsg.mask    = 0x0;
    txMsg.dlc     = 8;
    memcpy(txMsg.data, &sp, 8);
    LPC_CCAN_API->can_transmit(&txMsg);

    chThdSleepMilliseconds(3); // no back-pressure yet, throttle instead
}

int main () {
    halInit();
    chSysInit();
    initCan();

    for (;;) {
        // TODO: no rotary switches yet, run a simulation instead
        sendSetPoint(1, 1000,  100);
        sendSetPoint(1,  500, -100);
        sendSetPoint(1,  500,  100);
        sendSetPoint(1, 1000, -100);

        chThdSleepMilliseconds(2000);
    }

    return 0;
}
