// ssb11 driver - command processor for motor stepping via the CAN bus
// jcw, 2014-04-18


#include "ch.h"
#include "hal.h"

// Olimex LPC-P11C24 board has green/yellow LEDs, but on different pins
#if 1
#define GPIO1_OLI_LED1    GPIO1_DIGIPOT_UD  // PIO1_11, green
#define GPIO1_OLI_LED2    GPIO1_POWER_TERM  // PIO1_10, yellow
#endif

// defined elsewhere
static void blinkerInit ();     // blinker.cpp
static void canBusInit ();      // canbus.cpp

#include "blinker.cpp"
#include "canbus.cpp"

int main () {
    halInit();
    chSysInit();
    canBusInit();
    blinkerInit();

    for (;;) {
        msg_t rxMsg;
        if (chMBFetch(&canBus.rxPending, &rxMsg, TIME_INFINITE) == RDY_OK) {
            palTogglePad(GPIO1, GPIO1_OLI_LED1);
        }
    }

    return 0;
}
