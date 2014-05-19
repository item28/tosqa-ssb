// minimal blinker, using stripped ChibiOS makefiles, without ChibiOS itself
// jcw, 2014-04-20

#include "LPC11xx.h"

int main (void) {
    // no clock setup, running on IRC @ 12 MHz

    // LPC_GPIO0->DIR |= (1 << 7);    // LPCxpresso 11C24
    LPC_GPIO2->DIR |= (1 << 10);      // LPC11C24-DK-A

    volatile int i;
    while (1) {
        // LPC_GPIO0->DATA ^= 1 << 7; // LPCxpresso 11C24
        LPC_GPIO2->DATA ^= 1 << 10;   // LPC11C24-DK-A
        for (i = 0; i < 100000; ++i)
        	;
    }
    return 0 ;
}
