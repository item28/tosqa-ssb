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

#ifndef _BOARD_H_
#define _BOARD_H_

/*
 * Setup for Tosqa Single Stepper Board.
 */

/*
 * Board identifiers.
 */
#define BOARD_TOSQA_SSB
#define BOARD_NAME "Tosqa Single Stepper Board"

/*
 * Board frequencies.
 */
#define SYSOSCCLK               12000000

/*
 * SCK0 connection on this board.
 */
#define LPC11xx_SPI_SCK0_SELECTOR SCK0_IS_PIO2_11

/*
 * GPIO 0 initial setup.
 */
#define VAL_GPIO0DIR \
                                PAL_PORT_BIT(GPIO0_LED)                   | \
                                PAL_PORT_BIT(GPIO0_LEDR)                  | \
                                PAL_PORT_BIT(GPIO0_LEDB)                  | \
                      0
#define VAL_GPIO0DATA \
                                PAL_PORT_BIT(GPIO0_LED)                   | \
                                PAL_PORT_BIT(GPIO0_LEDR)                  | \
                                PAL_PORT_BIT(GPIO0_LEDB)                  | \
                      0
/*
 * GPIO 1 initial setup.
 */
#define VAL_GPIO1DIR \
                      0
#define VAL_GPIO1DATA \
                      0
/*
 * GPIO 2 initial setup.
 */
#define VAL_GPIO2DIR \
                      0
#define VAL_GPIO2DATA \
                      0
/*
 * GPIO 3 initial setup.
 */
#define VAL_GPIO3DIR \
                      0
#define VAL_GPIO3DATA \
                      0
/*
 * Pin definitions.
 */

#define GPIO0_LED               7

#define GPIO0_LEDR              8
// #define GPIO0_LEDG              10 // SWCLK!
#define GPIO0_LEDB              9

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* _BOARD_H_ */
