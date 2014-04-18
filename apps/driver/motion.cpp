// motion code, this is the low-level timer/pwm stepping stuff
// TODO: velocity is ignored, only linear stepping for now

#define MHZ 48  // timer rate, same as system clock w/o prescaler

static struct {
    MotionParams params;     // motor limits
    int          targetPos;  // where the stepper needs to go
    int          incOrDec;   // +1 if forward, -1 if backward
    uint32_t     stepsToGo;  // drops to 0 when motion is done
} motion;

extern "C" void Vector88 (); // CT32B0
CH_FAST_IRQ_HANDLER(Vector88)  {
    LPC_TMR32B0->IR = LPC_TMR32B0->IR; // clear all interrupts for this timer
    --motion.stepsToGo;
}

void motionInit () {
    // set the static stepper I/O pins to a default state
    palSetPad(GPIO3, GPIO3_MOTOR_SLEEP);    // active low, disabled
    palSetPad(GPIO1, GPIO1_MOTOR_RESET);    // active low, disabled
    palSetPad(GPIO3, GPIO3_MOTOR_EN);       // active low, off
    
    motionParams(motion.params);            // initialise to sane values

    LPC_IOCON->R_PIO0_11 = 0xD3;            // MOTOR_STEP, CT32B0_MAT3
    LPC_SYSCON->SYSAHBCLKCTRL |= 1<<9;      // enable clock for CT32B0
    LPC_TMR32B0->MCR |= (1<<9) | (1<<10);   // MR3I + MR3R, p.366
    LPC_TMR32B0->PWMC = 1<<3;               // pwm enable, p.372

    nvicEnableVector(TIMER_32_0_IRQn, LPC11xx_PWM_CT32B0_IRQ_PRIORITY);
}

// figure out the current position of the stepper motor
static int currentPosition () {
    // if there are still steps to go, then the targetPos hasn't been reached
    // FIXME: there's a roundoff error when going from microsteps to position
    int pendingTravel = (motion.stepsToGo << 8) / motion.params.posFactor;
    return motion.targetPos - motion.incOrDec * pendingTravel;
}

// set the motion configuration parameters
void motionParams (const MotionParams& p) {
    motion.params = p;

    if (motion.params.posFactor == 0)
        motion.params.posFactor = 1 << 8;
    if (motion.params.maxPos <= motion.params.minPos) {
        motion.params.minPos = -32768;
        motion.params.maxPos = 32767;
    }

    // copy microstep configuration to the corresponding pins
    palWritePad(GPIO3, GPIO3_MOTOR_MS1, (p.microStep >> 2) & 1);
    palWritePad(GPIO1, GPIO1_MOTOR_MS2, (p.microStep >> 1) & 1);
    palWritePad(GPIO1, GPIO1_MOTOR_MS3, (p.microStep >> 0) & 1);
}

// move to the given setpoint
void motionTarget (const Setpoint& s) {
    LPC_TMR32B0->TCR = 0; // stop interrupts so stepsToGo won't change
    
    int pos = s.position;
    if (s.relative)
        pos += currentPosition();
    
    if (pos < motion.params.minPos)
        pos = motion.params.minPos;
    else if (pos > motion.params.maxPos)
        pos = motion.params.maxPos;
    
    int diff = pos - currentPosition();
    motion.incOrDec = diff > 0 ? 1 : -1;
    motion.stepsToGo = (diff * motion.incOrDec * motion.params.posFactor) >> 8;
    motion.targetPos = pos;

    if (motion.stepsToGo > 0) {
        uint32_t rate = (MHZ * 1000 * s.time) / motion.stepsToGo; // linear
        if (rate < MHZ * 10)
            rate = MHZ * 10;        // 10 µs, max 100 KHz
        LPC_TMR32B0->MR3 = rate;
    
        palWritePad(GPIO1, GPIO1_MOTOR_DIR, diff > 0 ? 1 : 0);
        palClearPad(GPIO3, GPIO3_MOTOR_EN); // active low, on
        LPC_TMR32B0->TCR = 1;               // start timer

        while (motion.stepsToGo > 0)        // move!
            chThdYield();                   // uses idle polling
    }

    if (s.velocity == 0)
        palSetPad(GPIO3, GPIO3_MOTOR_EN);   // active low, off
}

// stop all motion, regardless of what is currently happening
void motionStop () {
    Setpoint next;
    next.time = 1;                          // stop as soon as possible
    next.position = currentPosition();      // request to stop on current pos
    next.velocity = 0;                      // ends with stepper not moving
    motionTarget(next);
}
