/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio
    LPC11C24 EA Board support - Copyright (C) 2013 mike brown

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "ch.h"
#include "hal.h"

/**
 * @brief   PAL setup.
 * @details Digital I/O ports static configuration as defined in @p board.h.
 *          This variable is used by the HAL when initializing the PAL driver.
 */
#if HAL_USE_PAL || defined(__DOXYGEN__)
const PALConfig pal_default_config = {
 {VAL_GPIO0DATA, VAL_GPIO0DIR},
 {VAL_GPIO1DATA, VAL_GPIO1DIR},
 {VAL_GPIO2DATA, VAL_GPIO2DIR},
 {VAL_GPIO3DATA, VAL_GPIO3DIR},
};
#endif

/*
 * Early initialization code.
 * This initialization must be performed just after stack setup and before
 * any other initialization.
 */
void __early_init(void) {

  lpc111x_clock_init();
}

/*
 * Board-specific initialization code.
 */
void boardInit(void) {

  /*
   * Extra, board-specific, initializations.
   */
  LPC_IOCON->R_PIO0_11 = 0xD1;  // MOTOR_STEP, gpio

  // ADC setup, see http://eewiki.net/display/microcontroller/
  //  Getting+Started+with+NXP%27s+LPC11XX+Cortex-M0+ARM+Microcontrollers
  LPC_SYSCON->PDRUNCFG        &= ~(0x1<<4); //power the ADC (sec. 3.5.35)
  LPC_SYSCON->SYSAHBCLKCTRL   |= (1<<13);   //enable clock for ADC (sec. 3.5.14)

  LPC_IOCON->R_PIO1_0 &= ~(0x97); // set LEVEL_5V to analog in (sec. 7.4.29)
  LPC_IOCON->R_PIO1_0 |= (1<<1);  // set to ADC mode for pin 33 (sec. 7.4.29)

  LPC_IOCON->R_PIO1_1 &= ~(0x97); // set LEVEL_VMOT to analog in (sec. 7.4.30)
  LPC_IOCON->R_PIO1_1 |= (1<<1);  // set to ADC mode for pin 34 (sec. 7.4.30)

  LPC_IOCON->R_PIO1_2 &= ~(0x97); // set TEMP_NTC to analog in (sec. 7.4.31)
  LPC_IOCON->R_PIO1_2 |= (1<<1);  // set to ADC mode for pin 35 (sec. 7.4.31)
}
