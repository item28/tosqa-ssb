// Drive the stepper motor. This is a separate task which takes incoming
// setpoint commands and makes the stepper perform the requested motion.
// Setpoints include a target position as well as an "exit" velocity.
// If there is no work, the stepper will stop as quickly as possible.

#define SETPOINT_QUEUE_SIZE 250

// internal state
static struct {
    Mailbox     mailbox;
    msg_t       buffer [SETPOINT_QUEUE_SIZE-1];
    Setpoint    setpoints [SETPOINT_QUEUE_SIZE];
    uint8_t     inUse [(SETPOINT_QUEUE_SIZE + 7)/8]; // 1 bit per entry
} setpoint;

static bool isInUse (uint32_t n) {
    return (setpoint.inUse[n/8] >> (n%8)) & 1;
}

static void setInUse (uint32_t n) {
    setpoint.inUse[n/8] |= 1 << (n%8);
}

static void clearInUse (uint32_t n) {
    setpoint.inUse[n/8] &= ~ (1 << (n%8));
}

static WORKING_AREA(waStepperTh, 128);

// the stepper thread processes incoming setpoint commands in order of arrival
// there's no lookahead, just take each command and do it as well as possible
static msg_t stepperTh (void*) {
    systime_t timeout = TIME_IMMEDIATE;
    for (;;) {
        msg_t spIndex;
        if (chMBFetch(&setpoint.mailbox, &spIndex, timeout) == RDY_OK) {
            #ifdef GPIO1_OLI_LED1
                palClearPad(GPIO1, GPIO1_OLI_LED1); // active low, on
            #endif
            motionTarget(setpoint.setpoints[spIndex]);
            clearInUse(spIndex);
            timeout = TIME_IMMEDIATE;
        } else {
            // there is no work, stop the stepper and wait for next cmd
            motionStop();
            #ifdef GPIO1_OLI_LED1
                palSetPad(GPIO1, GPIO1_OLI_LED1); // active low, off
            #endif
            timeout = TIME_INFINITE;
        }
    }
    return 0;
}

void setpointInit () {
    // make sure there is always at least one unused entry in setpoints[]
    chMBInit(&setpoint.mailbox, setpoint.buffer, SETPOINT_QUEUE_SIZE-1);
    // launch the stepper background thread
    chThdCreateStatic(waStepperTh, sizeof(waStepperTh), NORMALPRIO, stepperTh, 0);
}

// add a next setpoint for the stepper to go to, will wait if queue is full
void setpointAdd (const Setpoint& s) {
    // TODO: silly approach, should use first/last indices into circular buffer
    for (int i = 0; i < SETPOINT_QUEUE_SIZE; ++i)
        if (!isInUse(i)) {
            setInUse(i);
            setpoint.setpoints[i] = s;
            chMBPost(&setpoint.mailbox, i, TIME_INFINITE);
            return;
        }
    // never reached
}

void setpointReset () {
    chMBReset(&setpoint.mailbox);
    memset(setpoint.inUse, 0, sizeof setpoint.inUse);
}
