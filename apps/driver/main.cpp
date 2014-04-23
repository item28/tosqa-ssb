// ssb11 driver - command processor for motor stepping via the CAN bus
// jcw, 2014-04-18

#include "ch.h"
#include "hal.h"
#include <string.h>

// Olimex LPC-P11C24 board has green/yellow LEDs, but on different pins
#if 1
    #define GPIO1_OLI_LED1    GPIO1_DIGIPOT_UD  // PIO1_11, green
    #define GPIO1_OLI_LED2    GPIO1_POWER_TERM  // PIO1_10, yellow
#endif

// these stepper parameters need to be set up before use
typedef struct {
    int16_t minPos;         // do not move below this value
    int16_t maxPos;         // do not move above this value
    uint16_t posFactor;     // 8.8 multiplier from pos to real step counts
    uint16_t microStep :3;  // microstepping, other I/O settings, etc
    uint16_t unused    :13; // microstepping, other I/O settings, etc
} MotionParams;

// a setpoint defines a time in the future with given position and velocity
// this is a "wish", the stepper code tries to get there as well as it can
typedef struct {
    uint16_t time;          // milliseconds, wraps around roughly once a minute
    int16_t  position;      // absolute target position, signed
    int16_t  velocity;      // maximum velocity, signed
    uint16_t relative  :1;  // position is relative to previous one
    uint16_t unused    :15; // could be used for triggers, LEDs, etc
} Setpoint;

// defined in included files below
static void canBusInit ();
static void motionInit ();
static void motionParams (const MotionParams& p);
static void motionTarget (const Setpoint& s);
static void motionStop ();
static void setpointInit ();
static void setpointAdd (const Setpoint& s);
static void setpointReset ();
static void blinkerInit ();

#include "blinker.cpp"
#include "canbus.cpp"
#include "motion.cpp"
#include "setpoint.cpp"

static void processIncomingRequest () {
    const uint8_t* p = canBus.rxMessage.data;
    switch ((canBus.rxMessage.mode_id & 0x300) | canBus.rxMessage.dlc) {
        case 0x108:
            setpointAdd(*(const Setpoint*) p);
            break;
        case 0x208:
            motionParams(*(const MotionParams*) p);
            break;
        case 0x301:
            switch (*p) {
                case 0: // abort
                    motionStop();
                    setpointReset();
                    break;
            }
            break;
    }
}

static void reportQueueAvail (int slots) {
    if (canBus.shortId == 0)
        return; // no assigned ID, can't send report packets out
    CCAN_MSG_OBJ_T txMsg;
    txMsg.msgobj = 10;
    txMsg.mode_id = 0x100 | canBus.shortId;
    txMsg.dlc = 2;
    txMsg.data[0] = slots;
    txMsg.data[1] = 0; // TODO: status and mode info
    LPC_CCAN_API->can_transmit(&txMsg);
}

int main () {
    halInit();
    chSysInit();
    canBusInit();
    motionInit();
    setpointInit();
    blinkerInit();

    #if 1
        static Setpoint sp;
        sp.time = 2000;
        sp.position = 200;  // 10 ms/step
        sp.velocity = 1;
        setpointAdd(sp);
        sp.time = 500;
        sp.position = 100;  // 5 ms/step
        setpointAdd(sp);
        sp.time = 1000;
        sp.position = -500; // 2 ms/step
        sp.relative = 1;
        setpointAdd(sp);
    #endif
    
    bool sendSlotCount = true;
    for (;;) {
        msg_t rxMsg;
        switch (chMBFetch(&canBus.rxPending, &rxMsg, 100)) {
            case RDY_OK:
                // process each CAN bus message as it comes in via the mailbox
                processIncomingRequest();
                sendSlotCount = true; // report at least once
                break;
            case RDY_TIMEOUT:
                // report queue free after incoming msgs, until queue is empty
                if (sendSlotCount) {
                    reportQueueAvail(chMBGetFreeCountI(&setpoint.mailbox));
                    sendSlotCount = chMBGetUsedCountI(&setpoint.mailbox) > 0;
                }
                break;
        }
    }

    return 0;
}
