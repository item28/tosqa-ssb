// keep a LED blinking at 2 Hz in the background

static WORKING_AREA(waBlinkTh, 64);

static msg_t blinkerTh (void*) {
    for (;;) {
        chThdSleepMilliseconds(250);
        #ifdef GPIO1_OLI_LED2
            palTogglePad(GPIO1, GPIO1_OLI_LED2);
        #else
            palTogglePad(GPIO1, GPIO1_LED1);
        #endif
        palTogglePad(GPIO2, GPIO2_HALL_MODE); // PIO2_10 = LED on LPC11C24-DK-A
    }
    return 0;
}

void blinkerInit () {
    // launch the blinker background thread
    chThdCreateStatic(waBlinkTh, sizeof(waBlinkTh), NORMALPRIO, blinkerTh, 0);
}
