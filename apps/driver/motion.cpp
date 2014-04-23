// motion code, this is the low-level timer/pwm stepping stuff
// TODO: velocity is ignored, only linear stepping for now

#define MHZ 48  // timer rate, same as system clock w/o prescaler

static struct {
    MotionParams params;    // motor limits
    int          ssTarget;  // where the stepper needs to go, in sub-steps
    int          direction; // +1 if forward, -1 if backward
    volatile int ssToGo;    // sub-steps to go, drops to <= 0 when done
} motion;

extern "C" void Vector88 (); // CT32B0
CH_FAST_IRQ_HANDLER(Vector88)  {
    LPC_TMR32B0->IR = LPC_TMR32B0->IR;  // clear all interrupts for this timer
    motion.ssToGo -= 256;               // move in real, not fractional, steps
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

// return the current position of the stepper motor in sub-steps (steps * 256)
static int currentSubStep () {
    // if there are still steps to go, then target position hasn't been reached
    return motion.ssTarget - motion.direction * motion.ssToGo;
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
    LPC_TMR32B0->TCR = 0; // stop interrupts so ssToGo won't change
    
    int ssEndPos = s.position * motion.params.posFactor;
    if (s.relative)
        ssEndPos += currentSubStep();
    
    if (ssEndPos < motion.params.minPos * motion.params.posFactor)
        ssEndPos = motion.params.minPos * motion.params.posFactor;
    else if (ssEndPos > motion.params.maxPos * motion.params.posFactor)
        ssEndPos = motion.params.maxPos * motion.params.posFactor;
    
    int ssDiff = ssEndPos - currentSubStep();
    motion.direction = ssDiff > 0 ? 1 : -1;
    motion.ssToGo = ssDiff * motion.direction;
    motion.ssTarget = ssEndPos;

    if (ssDiff != 0) {
        // use 64-bit by 32-bit division to get max accuracy without overflow
        // this will add 2 KB to the code size...
        uint32_t rate = (MHZ * 1000LL * s.time * 256 + 128) / motion.ssToGo;
        if (rate < MHZ * 10)
            rate = MHZ * 10;                // at least 10 µs, i.e. max 100 KHz
        LPC_TMR32B0->MR3 = rate;
    
        palWritePad(GPIO1, GPIO1_MOTOR_DIR, ssDiff > 0 ? 1 : 0);
        palClearPad(GPIO3, GPIO3_MOTOR_EN); // active low, on
        LPC_TMR32B0->TCR = 1;               // start timer

        while (motion.ssToGo > 0)           // move!
            chThdYield();                   // uses idle polling
    } else if (s.time > 0)
        chThdSleepMilliseconds(s.time);     // dwell

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
