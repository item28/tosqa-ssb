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

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "../../common/usbconf.h"

BaseSequentialStream *serial = (BaseSequentialStream*)&SDU1;

// This is a periodic thread that does absolutely nothing except flashing a LED.
static WORKING_AREA(waBlinker, 128);
static msg_t Blinker(void *arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
    palSetPad(GPIOD, GPIOD_LED3);       /* Orange.  */
    chThdSleepMilliseconds(500);
    palClearPad(GPIOD, GPIOD_LED3);     /* Orange.  */
    chThdSleepMilliseconds(500);
  }
  return 0;
}

static char* appendHex (char *p, uint8_t v) {
  const char* hex = "0123456789ABCDEF";
  *p++ = hex[v>>4];
  *p++ = hex[v&0x0F];
  return p;
}

// CAN receive thread class.
static WORKING_AREA(waCanRecv, 256);
static msg_t CanRecv(void *arg) {
  (void) arg;
  chRegSetThreadName("canrecv");
  CANRxFrame rxMsg;

  while(true) {
    while (canReceive(&CAND1, CAN_ANY_MAILBOX, &rxMsg, 100) == RDY_OK) {
      char buf [30];
      uint32_t id = rxMsg.EID;
      if (!rxMsg.IDE)
        id &= 0x7FF;
      chsnprintf(buf, sizeof buf, "S%x#", id);
      char* p = buf + strlen(buf);
      int i;
      for (i = 0; i < rxMsg.DLC; ++i)
        p = appendHex(p, rxMsg.data8[i]);
      strcpy(p, "\r\n");
      chprintf(serial, buf);
    }
  }

  return 0;
}

// CAN send thread class.
static uint8_t hexDigit (char c) {
    if (c > '9')
        c -= 7;
    return c & 0xF;
}

static WORKING_AREA(waCanSend, 256);
static msg_t CanSend(void *arg) {
  (void) arg;
  chRegSetThreadName("cansend");
  CANTxFrame txmsg_can1;
  txmsg_can1.RTR = CAN_RTR_DATA;

  uint8_t nibble = 0;
  enum { sNONE, sADDR, sDATA } state = sNONE;

  while (true) {
    char ch = chSequentialStreamGet(serial);

    switch (state) {
    case sNONE:
        if (ch == 'S') {
            state = sADDR;
            txmsg_can1.EID = 0;
        }
        break;
    case sADDR:
        if (ch == '#') {
            state = sDATA;
            nibble = 0;
        } else {
            txmsg_can1.EID = (txmsg_can1.EID << 4) | hexDigit(ch);
        }
        break;
    case sDATA:
        if (ch >= '0') {
            uint8_t* p = txmsg_can1.data8 + nibble/2;
            *p = (*p << 4) | hexDigit(ch);
            ++nibble;
        } else {
            txmsg_can1.IDE = txmsg_can1.EID > 0x7FF ? CAN_IDE_EXT : CAN_IDE_STD;
            txmsg_can1.DLC = nibble / 2;
            canTransmit(&CAND1, 1, &txmsg_can1, 100);
            state = sNONE;
        }
    }
  }

  return 0;
}

// Application entry point.
int main(void) {
  halInit();
  chSysInit();

  // Initializes a serial-over-USB CDC driver.
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);
  chprintf(serial, "\r\n[disco407]\r\n");

  // Activates the USB driver and then the USB bus pull-up on D+.
  // Note, a delay is inserted in order to not have to disconnect the cable
  // after a reset.
  usbDisconnectBus(serusbcfg.usbp);
  // chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  GPIOD->AFRL = (GPIOD->AFRL & ~0xFF) | 0x99; // CAN RX/TX on PD0/PD1
  // palSetGroupMode(GPIOD, PAL_PORT_BIT(0), 0, PAL_STM32_MODE_ALTERNATE);
  // palSetGroupMode(GPIOD, PAL_PORT_BIT(1), 0, PAL_STM32_MODE_ALTERNATE);
  GPIOD->MODER = (GPIOD->MODER & ~0xF) | 0b1010;

  // Activates CAN driver.
  static const CANConfig cancfg = {
    CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
    // CAN_BTR_LBKM |
    CAN_BTR_SJW(1) | CAN_BTR_TS2(2) |
    CAN_BTR_TS1(7) | CAN_BTR_BRP(6)
  };
  canStart(&CAND1, &cancfg);
  
  chThdCreateStatic(waBlinker, sizeof(waBlinker), NORMALPRIO, Blinker, NULL);
  chThdCreateStatic(waCanRecv, sizeof(waCanRecv), NORMALPRIO, CanRecv, NULL);
  chThdCreateStatic(waCanSend, sizeof(waCanSend), NORMALPRIO, CanSend, NULL);

  while (TRUE) {
    chThdSleepMilliseconds(500);
  }
}
