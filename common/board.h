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
                                PAL_PORT_BIT(GPIO0_ENDSTOP1)              | \
                                PAL_PORT_BIT(GPIO0_ENDSTOP_SCL)           | \
                                PAL_PORT_BIT(GPIO0_ENDSTOP_SDA)           | \
                                PAL_PORT_BIT(GPIO0_HALL_DEC)              | \
                                PAL_PORT_BIT(GPIO0_HALL_INC)              | \
                                PAL_PORT_BIT(GPIO0_LED2)                  | \
                                PAL_PORT_BIT(GPIO0_MOTOR_STEP)            | \
                      0
#define VAL_GPIO0DATA \
                                PAL_PORT_BIT(GPIO0_ENDSTOP1)              | \
                                PAL_PORT_BIT(GPIO0_ENDSTOP_SCL)           | \
                                PAL_PORT_BIT(GPIO0_ENDSTOP_SDA)           | \
                                PAL_PORT_BIT(GPIO0_HALL_DEC)              | \
                                PAL_PORT_BIT(GPIO0_HALL_INC)              | \
                                PAL_PORT_BIT(GPIO0_LED2)                  | \
                                PAL_PORT_BIT(GPIO0_MOTOR_STEP)            | \
                      0
/*
 * GPIO 1 initial setup.
 */
#define VAL_GPIO1DIR \
                                PAL_PORT_BIT(GPIO1_DIGIPOT_UD)            | \
                                PAL_PORT_BIT(GPIO1_LED1)                  | \
                                PAL_PORT_BIT(GPIO1_MOTOR_DIR)             | \
                                PAL_PORT_BIT(GPIO1_MOTOR_MS2)             | \
                                PAL_PORT_BIT(GPIO1_MOTOR_MS3)             | \
                                PAL_PORT_BIT(GPIO1_MOTOR_RESET)           | \
                                PAL_PORT_BIT(GPIO1_POWER_TERM)            | \
                      0
#define VAL_GPIO1DATA \
                                PAL_PORT_BIT(GPIO1_DIGIPOT_UD)            | \
                                PAL_PORT_BIT(GPIO1_LED1)                  | \
                                PAL_PORT_BIT(GPIO1_MOTOR_DIR)             | \
                                PAL_PORT_BIT(GPIO1_MOTOR_MS2)             | \
                                PAL_PORT_BIT(GPIO1_MOTOR_MS3)             | \
                                PAL_PORT_BIT(GPIO1_MOTOR_RESET)           | \
                                PAL_PORT_BIT(GPIO1_POWER_TERM)            | \
                      0
/*
 * GPIO 2 initial setup.
 */
#define VAL_GPIO2DIR \
                                PAL_PORT_BIT(GPIO2_BOOT_AUTORESET)        | \
                                PAL_PORT_BIT(GPIO2_ENDSTOP0)              | \
                                PAL_PORT_BIT(GPIO2_ENDSTOP_I2C)           | \
                                PAL_PORT_BIT(GPIO2_HALL_MODE)             | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_CS)           | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_MISO)         | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_MOSI)         | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_SCK)          | \
                                PAL_PORT_BIT(GPIO2_POWER_RJ12)            | \
                      0
#define VAL_GPIO2DATA \
                                PAL_PORT_BIT(GPIO2_ENDSTOP0)              | \
                                PAL_PORT_BIT(GPIO2_ENDSTOP_I2C)           | \
                                PAL_PORT_BIT(GPIO2_HALL_MODE)             | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_CS)           | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_MISO)         | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_MOSI)         | \
                                PAL_PORT_BIT(GPIO2_HALL_SPI_SCK)          | \
                                PAL_PORT_BIT(GPIO2_POWER_RJ12)            | \
                      0
/*
 * GPIO 3 initial setup.
 */
#define VAL_GPIO3DIR \
                                PAL_PORT_BIT(GPIO3_DIGIPOT_CS)            | \
                                PAL_PORT_BIT(GPIO3_MOTOR_EN)              | \
                                PAL_PORT_BIT(GPIO3_MOTOR_MS1)             | \
                                PAL_PORT_BIT(GPIO3_MOTOR_SLEEP)           | \
                      0
#define VAL_GPIO3DATA \
                                PAL_PORT_BIT(GPIO3_DIGIPOT_CS)            | \
                                PAL_PORT_BIT(GPIO3_MOTOR_EN)              | \
                                PAL_PORT_BIT(GPIO3_MOTOR_MS1)             | \
                                PAL_PORT_BIT(GPIO3_MOTOR_SLEEP)           | \
                      0
/*
 * Pin definitions.
 */

#define GPIO3_MOTOR_SLEEP       2
#define GPIO1_MOTOR_MS3         6
#define GPIO1_MOTOR_MS2         7
#define GPIO3_MOTOR_MS1         3
#define GPIO1_MOTOR_RESET       5
#define GPIO3_MOTOR_EN          0
#define GPIO1_MOTOR_DIR         4
#define GPIO0_MOTOR_STEP        11

#define GPIO0_HALL_DEC          6
#define GPIO0_HALL_INC          7
#define GPIO2_HALL_SPI_CS       0
#define GPIO2_HALL_SPI_SCK      1
#define GPIO2_HALL_SPI_MISO     2
#define GPIO2_HALL_SPI_MOSI     3
#define GPIO2_HALL_MODE         10

#define GPIO1_LED1              8
#define GPIO0_LED2              9

#define GPIO0_BOOT              1
#define GPIO0_BOOT_SEL          3
#define GPIO2_BOOT_AUTORESET    6

#define GPIO2_ENDSTOP0          7
#define GPIO0_ENDSTOP1          2
#define GPIO2_ENDSTOP_I2C       8
#define GPIO0_ENDSTOP_SCL       4
#define GPIO0_ENDSTOP_SDA       5

#define GPIO1_TEMP_NTC          2
#define GPIO1_LEVEL_5V          0
#define GPIO1_LEVEL_VMOT        1

#define GPIO3_DIGIPOT_CS        1
#define GPIO1_DIGIPOT_UD        11
                                                                                                        
#define GPIO1_POWER_TERM        10
#define GPIO2_POWER_RJ12        11

// common LED position on dev boards, harmless re-use, used for testing
#define GPIO0_LED               GPIO0_HALL_INC

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
