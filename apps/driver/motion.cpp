// motion code, this is the low-level timer/pwm steppign stuff

static struct {
    MotionParams params;
    int          targetPos;
    int          incOrDec;
    int          stepsToGo;
} motion;

extern "C" void Vector88 (); // CT32B0
CH_FAST_IRQ_HANDLER(Vector88)  {
    LPC_TMR32B0->IR = LPC_TMR32B0->IR; // clear all interrupts for this timer
    --motion.stepsToGo;
}

void motionInit () {
    // set the static stepper I/O pins to a default state
    palSetPad(GPIO3, GPIO3_MOTOR_SLEEP);    // active low, disabled
    palSetPad(GPIO1, GPIO1_MOTOR_MS3);      // 16-th microstepping
    palSetPad(GPIO1, GPIO1_MOTOR_MS2);      // 16-th microstepping
    palSetPad(GPIO3, GPIO3_MOTOR_MS1);      // 16-th microstepping
    palSetPad(GPIO1, GPIO1_MOTOR_RESET);    // active low, disabled
    palSetPad(GPIO3, GPIO3_MOTOR_EN);       // active low, off

    LPC_IOCON->R_PIO0_11 = 0xD3;            // MOTOR_STEP, CT32B0_MAT3
    LPC_SYSCON->SYSAHBCLKCTRL |= 1<<9;      // enable clock for CT32B0
    LPC_TMR32B0->PR = 48;                   // prescaler -> 1 MHz
    LPC_TMR32B0->MCR |= (1<<9) | (1<<10);   // MR3I + MR3R, p.366
    LPC_TMR32B0->PWMC = 1<<3;               // pwm enable, p.372

    nvicEnableVector(TIMER_32_0_IRQn, LPC11xx_PWM_CT32B0_IRQ_PRIORITY);
}

static int currentPosition () {
    return motion.targetPos - motion.incOrDec * motion.stepsToGo;
}

void motionParams (const MotionParams* p) {
    motion.params = *p;
}

void motionTarget (Setpoint& s) {
    LPC_TMR32B0->TCR = 0; // stop interrupts so stepsToGo won't change
    
    int distance = s.position - currentPosition();
    motion.incOrDec = distance > 0 ? 1 : -1;
    motion.stepsToGo = distance * motion.incOrDec;
    motion.targetPos = s.position;

    int duration = s.time - chTimeNow();    // number of milliseconds for move
    LPC_TMR32B0->MR3 = 1000 * motion.stepsToGo / duration; // linear motion
    
    palWritePad(GPIO1, GPIO1_MOTOR_DIR, motion.incOrDec);
    palClearPad(GPIO3, GPIO3_MOTOR_EN);     // active low, on
    LPC_TMR32B0->TCR = 1;                   // start timer

    while (motion.stepsToGo > 0)            // move!
        chThdYield();

    if (s.velocity == 0)
        palSetPad(GPIO3, GPIO3_MOTOR_EN);   // active low, off
}

// stop all motion, regardless of what is currently happening
void motionStop () {
    Setpoint next;
    next.time = chTimeNow() + 1;            // stop as soon as possible
    next.position = motion.targetPos - motion.incOrDec * motion.stepsToGo;
    next.velocity = 0;                      // ends with stepper not moving
    motionTarget(next);
}
