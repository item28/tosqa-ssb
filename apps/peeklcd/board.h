/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

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
 * Setup for the Olimex STM32-LCD proto board.
 */

/*
 * Board identifier.
 */
#define BOARD_MINI_STM32V
#define BOARD_NAME              "HY-MiniSTM32V"

/*
 * Board frequencies.
 */
#define STM32_LSECLK            32768
#define STM32_HSECLK            8000000

/*
 * MCU type, supported types are defined in ./os/hal/platforms/hal_lld.h.
 */
#define STM32F10X_HD

/*
 * IO pins assignments.
 */
#define GPIOB_USB_DISC          7

#define GPIOB_TFT_LIGHT			5

#define GPIOC_BUTTONA           13
#define GPIOB_BUTTONB           2
#define GPIOB_LED1              0
#define GPIOB_LED2              1

#define GPIOD_LCD_CS            7
#define GPIOD_LCD_RS            11
#define GPIOD_LCD_WR            5
#define GPIOD_LCD_RD            4

/*
 * I/O ports initial setup, this configuration is established soon after reset
 * in the initialization code.
 *
 * The digits have the following meaning:
 *   0 - Analog input.
 *   1 - Push Pull output 10MHz.
 *   2 - Push Pull output 2MHz.
 *   3 - Push Pull output 50MHz.
 *   4 - Digital input.
 *   5 - Open Drain output 10MHz.
 *   6 - Open Drain output 2MHz.
 *   7 - Open Drain output 50MHz.
 *   8 - Digital input with PullUp or PullDown resistor depending on ODR.
 *   9 - Alternate Push Pull output 10MHz.
 *   A - Alternate Push Pull output 2MHz.
 *   B - Alternate Push Pull output 50MHz.
 *   C - Reserved.
 *   D - Alternate Open Drain output 10MHz.
 *   E - Alternate Open Drain output 2MHz.
 *   F - Alternate Open Drain output 50MHz.
 * Please refer to the STM32 Reference Manual for details.
 */

/*
 * Port A setup.
 * Everything input with pull-up except:
 * PA0  - Normal input      (USB P).
 * PA2  - Alternate output  (USART2 TX).
 * PA3  - Normal input      (USART2 RX).
 * PA9  - Alternate output  (USART1 TX).
 * PA10 - Normal input      (USART1 RX).
 * PA11 - Normal input      (USB DM).
 * PA12 - Normal input      (USB DP).
 */
#define VAL_GPIOACRL            0x88884B84      /*  PA7...PA0 */
#define VAL_GPIOACRH            0x888444B8      /* PA15...PA8 */
#define VAL_GPIOAODR            0xFFFFFFFF

/*
 * Port B setup.
 * Everything input with pull-up except:
 * PB0  - Digital Output	(LED 1)
 * PB1  - Digital Output	(LED 2)
 * PB2  - Normal input      (BUTTON B).
 * PB5  - Digital Output	(TFT LIGHT)
 */
#define VAL_GPIOBCRL            0x88388833      /*  PB7...PB0 */
#define VAL_GPIOBCRH            0x88888888      /* PB15...PB8 */
#define VAL_GPIOBODR            0xFFFFFFFF

/*
 * Port C setup.
 * Everything input with pull-up except:
 * PC8  - Alternate PP 50M	(SD_D0).
 * PC9  - Alternate PP 50M	(SD_D1).
 * PC10 - Alternate PP 50M	(SD_D2).
 * PC11 - Alternate PP 50M	(SD_D3).
 * PC12 - Alternate PP 50M	(SD_CLK).
 * PC13 - Normal input		(BUTTON A).
 * PC14 - Normal input		(XTAL).
 * PC15 - Normal input		(XTAL).
 */
#define VAL_GPIOCCRL            0x88888888      /*  PC7...PC0 */
#define VAL_GPIOCCRH            0x448BBBBB	    /* PC15...PC8 */
#define VAL_GPIOCODR            0xFFFFFFFF

// /* PD.00(D2), PD.01(D3), PD.04(RD), PD.5(WR), PD.7(CS), PD.8(D13), PD.9(D14),
//    PD.10(D15), PD.11(RS) PD.14(D0) PD.15(D1) */
// GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_7 | 
//                                GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11 | 
//                                GPIO_Pin_14 | GPIO_Pin_15;
// GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
// GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
// GPIO_Init(GPIOD, &GPIO_InitStructure);

/*
 * Port D setup.
 * Everything input with pull-up except:
 * PD0  - Alternate PP 50M  (FSMC_D2)
 * PD1  - Alternate PP 50M  (FSMC_D3)
 * PD2  - Alternate PP 50M	(SD_CMD)
 * PD4  - Alternate PP 50M	(TFT_RD)
 * PD5  - Alternate PP 50M	(TFT_WR)
 * PD7  - Alternate PP 50M  (TFT_CS)
 * PD8  - Alternate PP 50M  (FSMC_D13)
 * PD9  - Alternate PP 50M  (FSMC_D14)
 * PD10 - Alternate PP 50M  (FSMC_D15)
 * PD11 - Alternate PP 50M  (TFT_RS)
 * PD14 - Alternate PP 50M  (FSMC_D0)
 * PD15 - Alternate PP 50M  (FSMC_D1)
 */
#define VAL_GPIODCRL            0x34334433      /*  PD7...PD0 */
#define VAL_GPIODCRH            0x33883333      /* PD15...PD8 */
#define VAL_GPIODODR            0xFFFFFFFF

// /* PE.07(D4), PE.08(D5), PE.09(D6), PE.10(D7), PE.11(D8), PE.12(D9),
//    PE.13(D10), PE.14(D11), PE.15(D12) */
// GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
//                                GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | 
//                                GPIO_Pin_15;
// GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
// GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
// GPIO_Init(GPIOE, &GPIO_InitStructure);

/*
 * Port E setup.
 * Everything input with pull-up except:
 * PE7  - Alternate PP 50M	(FSMC_D4)
 * PE8  - Alternate PP 50M  (FSMC_D5)
 * PE9  - Alternate PP 50M  (FSMC_D6)
 * PE10 - Alternate PP 50M  (FSMC_D7)
 * PE11 - Alternate PP 50M  (FSMC_D8)
 * PE12 - Alternate PP 50M  (FSMC_D9)
 * PE13 - Alternate PP 50M  (FSMC_D10)
 * PE14 - Alternate PP 50M  (FSMC_D11)
 * PE15 - Alternate PP 50M  (FSMC_D12)
 */
#define VAL_GPIOECRL            0x34444444      /*  PE7...PE0 */
#define VAL_GPIOECRH            0x33333333      /* PE15...PE8 */
#define VAL_GPIOEODR            0xFFFFFFFF

/*
 * Port F setup.
 * Everything input with pull-up expect:
 */
#define VAL_GPIOFCRL            0x88888888      /*  PF7...PF0 */
#define VAL_GPIOFCRH            0x88888888      /* PF15...PF8 */
#define VAL_GPIOFODR            0xFFFFFFFF

/*
 * Port G setup.
 * Everything input with pull-up expect:
 */
#define VAL_GPIOGCRL            0x88888888      /*  PG7...PG0 */
#define VAL_GPIOGCRH            0x88888888      /* PG15...PG8 */
#define VAL_GPIOGODR            0xFFFFFFFF

/*
 * USB bus activation macro, required by the USB driver.
 */
#define usb_lld_connect_bus(usbp) palClearPad(GPIOB, GPIOB_USB_DISC)

/*
 * USB bus de-activation macro, required by the USB driver.
 */
#define usb_lld_disconnect_bus(usbp) palSetPad(GPIOB, GPIOB_USB_DISC)

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
