/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio
    LPC11xx EXT driver - Copyright (C) 2013 Marcin Jokel
                       - Copyright (C) 2013 mike brown

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

/*
 * LPC1114 drivers configuration.
 * The following settings override the default settings present in
 * the various device driver implementation headers.
 * Note that the settings for each driver only have effect if the driver
 * is enabled in halconf.h.
 *
 * IRQ priorities:
 * 3...0        Lowest...highest.
 */

/*
 * HAL driver system settings.
 */
#define LPC11xx_PLLCLK_SOURCE               SYSPLLCLKSEL_SYSOSC
#define LPC11xx_SYSPLL_MUL                  4
#define LPC11xx_SYSPLL_DIV                  4
#define LPC11xx_MAINCLK_SOURCE              SYSMAINCLKSEL_PLLOUT
#define LPC11xx_SYSABHCLK_DIV               1

/*
 * GPT driver system settings.
 */
#define LPC11xx_GPT_USE_CT16B0              FALSE
#define LPC11xx_GPT_USE_CT16B1              FALSE
#define LPC11xx_GPT_USE_CT32B0              FALSE
#define LPC11xx_GPT_USE_CT32B1              FALSE
#define LPC11xx_GPT_CT16B0_IRQ_PRIORITY     1
#define LPC11xx_GPT_CT16B1_IRQ_PRIORITY     3
#define LPC11xx_GPT_CT32B0_IRQ_PRIORITY     2
#define LPC11xx_GPT_CT32B1_IRQ_PRIORITY     2

/*
 * SERIAL driver system settings.
 */
#define LPC11xx_SERIAL_USE_UART0            FALSE
#define LPC11xx_SERIAL_FIFO_PRELOAD         16
#define LPC11xx_SERIAL_UART0CLKDIV          1
#define LPC11xx_SERIAL_UART0_IRQ_PRIORITY   3

/*
 * SPI driver system settings.
 */
#define LPC11xx_SPI_USE_SSP0                FALSE
#define LPC11xx_SPI_USE_SSP1                FALSE
#define LPC11xx_SPI_SSP0CLKDIV              1
#define LPC11xx_SPI_SSP1CLKDIV              1
#define LPC11xx_SPI_SSP0_IRQ_PRIORITY       1
#define LPC11xx_SPI_SSP1_IRQ_PRIORITY       1
#define LPC11xx_SPI_SSP_ERROR_HOOK(spip)    chSysHalt()
#define LPC11xx_SPI_SCK0_SELECTOR           SCK0_IS_PIO2_11

/*
 * EXT driver system settings.
 */
#define LPC11xx_EXT_USE_EXT0                FALSE
#define LPC11xx_EXT_USE_EXT1                FALSE
#define LPC11xx_EXT_USE_EXT2                FALSE
#define LPC11xx_EXT_USE_EXT3                FALSE
#define LPC11xx_EXT_EXTI0_IRQ_PRIORITY      3
#define LPC11xx_EXT_EXTI1_IRQ_PRIORITY      3
#define LPC11xx_EXT_EXTI2_IRQ_PRIORITY      3
#define LPC11xx_EXT_EXTI3_IRQ_PRIORITY      3

/*
 * I2C driver system settings.
 */
#define LPC11xx_I2C_IRQ_PRIORITY            3

/*
 * PWM driver system settings.
 */
#define LPC11xx_PWM_USE_CT16B0              FALSE
#define LPC11xx_PWM_USE_CT16B1              FALSE
#define LPC11xx_PWM_USE_CT32B0              FALSE
#define LPC11xx_PWM_USE_CT32B1              FALSE
#define LPC11xx_PWM_USE_CT16B0_CH0          FALSE
#define LPC11xx_PWM_USE_CT16B0_CH1          FALSE
#define LPC11xx_PWM_USE_CT16B1_CH0          FALSE
#define LPC11xx_PWM_USE_CT16B1_CH1          FALSE
#define LPC11xx_PWM_USE_CT32B0_CH0          FALSE
#define LPC11xx_PWM_USE_CT32B0_CH1          FALSE
#define LPC11xx_PWM_USE_CT32B1_CH0          FALSE
#define LPC11xx_PWM_USE_CT32B1_CH1          FALSE
#define LPC11xx_PWM_CT16B0_IRQ_PRIORITY     3
#define LPC11xx_PWM_CT16B1_IRQ_PRIORITY     3
#define LPC11xx_PWM_CT32B0_IRQ_PRIORITY     3
#define LPC11xx_PWM_CT32B1_IRQ_PRIORITY     3
