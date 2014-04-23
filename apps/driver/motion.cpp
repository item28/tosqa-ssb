// motion code, this is the low-level timer/pwm stepping stuff
// TODO: velocity is ignored, only linear stepping for now

#define MHZ 48  // timer clock rate, i.e. use system clock w/o prescaler

static struct {
    MotionParams      params;     // motor limits
    int               targetStep; // where the stepper needs to go, in steps
    int               direction;  // +1 if forward, -1 if backward
    volatile uint32_t stepsToGo;  // steps to go, drops to zero when done
    int               residue;    // timer rate error, in clock cycles
} motion;

extern "C" void Vector88 (); // CT32B0
CH_FAST_IRQ_HANDLER(Vector88)  {
    LPC_TMR32B0->IR = LPC_TMR32B0->IR;  // clear all interrupts for this timer
    if (--motion.stepsToGo == 0)
      LPC_TMR32B0->MR3 = ~0; // set timer count to max, but don't stop it
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

// return the current position of the stepper motor in steps
static int currentStep () {
    // if there are still steps to go, then target position hasn't been reached
    return motion.targetStep - motion.direction * motion.stepsToGo;
}

// set the motion configuration parameters
void motionParams (const MotionParams& p) {
    motion.params = p;

    if (motion.params.posFactor == 0)
        motion.params.posFactor = 256; // 1.0 as 24.8
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
    LPC_TMR32B0->MR3 = ~0; // set timer count to max, so it won't change now
    
    // determine step position to end on, rounding from a 24.8 to a 24.0 int
    int endStep = (s.position * motion.params.posFactor + 128) / 256;
    if (s.relative)
        endStep += currentStep();
    
    if (endStep < motion.params.minPos)
        endStep = motion.params.minPos;
    else if (endStep > motion.params.maxPos)
        endStep = motion.params.maxPos;
    
    int stepDiff = endStep - currentStep();
    motion.direction = stepDiff > 0 ? 1 : -1;
    motion.stepsToGo = stepDiff * motion.direction;
    motion.targetStep = endStep;

    if (motion.stepsToGo > 0) {
        uint32_t clocks = MHZ * 1000L * s.time + motion.residue;
        uint32_t rate = (clocks + motion.stepsToGo / 2) / motion.stepsToGo;
        motion.residue = clocks - rate * motion.stepsToGo;
    
        if (rate < MHZ * 10)                // enforce a soft timer rate limit
            rate = MHZ * 10;                // at least 10 µs, i.e. max 100 KHz
        LPC_TMR32B0->MR3 = rate;

        palWritePad(GPIO1, GPIO1_MOTOR_DIR, stepDiff > 0 ? 1 : 0);
        palClearPad(GPIO3, GPIO3_MOTOR_EN); // active low, on
        LPC_TMR32B0->TCR = 1;               // start timer, if not running

        while (motion.stepsToGo > 0)        // move!
            chThdYield();                   // uses idle polling
    } else if (s.time > 0) {
        LPC_TMR32B0->TCR = 0;               // stop timer
        chThdSleepMilliseconds(s.time);     // dwell
    }

    if (s.velocity == 0)
        palSetPad(GPIO3, GPIO3_MOTOR_EN);   // active low, off
}

// stop all motion, regardless of what is currently happening
void motionStop () {
    Setpoint next;
    next.time = 1;          // stop as soon as possible
    next.position = 0;      // request to stop on current pos
    next.relative = 1;      // ... by specifying a relative motion of zero
    next.velocity = 0;      // ends with stepper not moving
    motionTarget(next);
}
