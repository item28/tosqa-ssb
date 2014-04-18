// ssb11 driver - command processor for motor stepping via the CAN bus
// jcw, 2014-04-18


#include "ch.h"
#include "hal.h"

// Olimex LPC-P11C24 board has green/yellow LEDs, but on different pins
#if 1
#define GPIO1_OLI_LED1    GPIO1_DIGIPOT_UD  // PIO1_11, green
#define GPIO1_OLI_LED2    GPIO1_POWER_TERM  // PIO1_10, yellow
#endif

// these stepper parameters need to be set up before use
typedef struct {
    uint16_t minPos;        // do not move below this value
    uint16_t maxPos;        // do not move above this value
    uint16_t posMultiplier; // 8.8 multiplier from pos to real step counts
    uint16_t configuration; // microstepping, other I/O settings, etc
} MotionParams;

// a setpoint defines a time in the future with given position and velocity
// this is a "wish", the stepper code tries to get there as well as it can
typedef struct {
    uint16_t time;          // milliseconds, wraps around roughly once a minute
    int16_t  position;      // absolute target position, signed
    int16_t  velocity;      // maximum velocity, signed
    uint16_t other;         // could be used for triggers, LEDs, etc
} Setpoint;

// defined in included files below
static void canBusInit ();
static void motionInit ();
static void motionParams (const MotionParams* p);
static void motionTarget (Setpoint& s);
static void motionStop ();
static void setpointInit ();
static void setpointAdd (const Setpoint* s);
static void blinkerInit ();

#include "blinker.cpp"
#include "canbus.cpp"
#include "motion.cpp"
#include "setpoint.cpp"

int main () {
    halInit();
    chSysInit();
    canBusInit();
    motionInit();
    setpointInit();
    blinkerInit();

    Setpoint sp = { 0, 0, 1, 0 };
    sp.time = chTimeNow() + 1000;
    sp.position = 3000;
    setpointAdd(&sp);
    sp.time = chTimeNow() + 2000;
    sp.position = 0;
    setpointAdd(&sp);

    for (;;) {
        // process each incoming CAN bus message as it comes in via the mailbox
        msg_t rxMsg;
        if (chMBFetch(&canBus.rxPending, &rxMsg, TIME_INFINITE) == RDY_OK) {
            if (canBus.rxMessage.dlc == 8) {
                const void* p = canBus.rxMessage.data;
                switch (canBus.rxMessage.mode_id) {
                    case 0x410:
                        motionParams((const MotionParams*) p);
                        break;
                    case 0x411:
                        setpointAdd((const Setpoint*) p);
                        break;
                }
            }
        }
    }

    return 0;
}
