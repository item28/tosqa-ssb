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
 * Setup for Embedded Artists LPCXpresso AOAA w/ LPC1769.
 */

/*
 * Board identifiers.
 */
#define BOARD_EA_AOAA
#define BOARD_NAME "Embedded Artists Android Open Accessory Application Kit"

/*
 * Ethernet PHY type.
 */
#define MII_LAN8720_ID          0x0007C0F0
#define BOARD_PHY_ID            MII_LAN8720_ID
#define BOARD_PHY_RMII

//#define BOARD_PHY_ADDRESS

#define LPC17xx_HAS_CAN1        TRUE
#define LPC17xx_HAS_CAN2        FALSE

/*
 * Board frequencies.
 */
#define MAINOSCCLK               12000000UL
#define RTCOSCCLK                32768UL

/*
 * GPIO 0 initial setup.
 */
#define VAL_GPIO0DIR            PAL_PORT_BIT(GPIO0_LED2_RED) | \
                                PAL_PORT_BIT(GPIO0_MMC_PWR) | \
                                PAL_PORT_BIT(GPIO0_MMC_SSEL)
#define VAL_GPIO0DATA           PAL_PORT_BIT(GPIO0_MMC_PWR) | \
                                PAL_PORT_BIT(GPIO0_MMC_SSEL)

/*
 * GPIO 1 initial setup.
 */
#define VAL_GPIO1DIR            0x00000000
#define VAL_GPIO1DATA           0x00000000

/*
 * GPIO 2 initial setup.
 */
#define VAL_GPIO2DIR            PAL_PORT_BIT(GPIO2_RGB1_RED) | \
                                PAL_PORT_BIT(GPIO2_RGB1_BLUE) | \
                                PAL_PORT_BIT(GPIO2_RGB1_GREEN) | \
                                PAL_PORT_BIT(GPIO2_RGB2_RED) | \
                                PAL_PORT_BIT(GPIO2_RGB2_BLUE) | \
                                PAL_PORT_BIT(GPIO2_RGB2_GREEN)
#define VAL_GPIO2DATA           PAL_PORT_BIT(GPIO2_RGB1_RED) | \
                                PAL_PORT_BIT(GPIO2_RGB1_BLUE) | \
                                PAL_PORT_BIT(GPIO2_RGB1_GREEN) | \
                                PAL_PORT_BIT(GPIO2_RGB2_RED) | \
                                PAL_PORT_BIT(GPIO2_RGB2_BLUE) | \
                                PAL_PORT_BIT(GPIO2_RGB2_GREEN)

/*
 * GPIO 3 initial setup.
 */
#define VAL_GPIO3DIR            0x00000000
#define VAL_GPIO3DATA           0x00000000

/*
 * GPIO 4 initial setup.
 */
#define VAL_GPIO4DIR            0x00000000
#define VAL_GPIO4DATA           0x00000000

/*
 * Pin definitions.
 */
#define GPIO2_RGB1_RED          0
#define GPIO2_RGB1_BLUE         1
#define GPIO2_RGB1_GREEN        2
#define GPIO2_KEY1              11

#define GPIO2_RGB2_RED          3
#define GPIO2_RGB2_BLUE         4
#define GPIO2_RGB2_GREEN        5
#define GPIO2_KEY2              12

#define GPIO0_LED2_RED          22
// #define GPIO0_LED3_EXT          28  /* Connect LED to board. */

#define GPIO2_PIN12_TO_GND      12
                                  
#define GPIO0_MMC_PWR           18
#define GPIO0_MMC_CD            19
#define GPIO0_MMC_SSEL          6

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
