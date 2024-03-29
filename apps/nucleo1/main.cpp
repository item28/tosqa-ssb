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

#include "ch.hpp"
#include "hal.h"
#include "test.h"
#include "chprintf.h"

using namespace chibios_rt;

// #define TESTER 1

#define GPIOA_LED     5
#define GPIOC_BUTTON  13
#define GPIOB_CAN_RX  8   // alt mode 0b10
#define GPIOB_CAN_TX  9   // alt mode 0b10

BaseSequentialStream *serial = (BaseSequentialStream*)&SD2;
  
/*
 * LED blink sequences.
 * NOTE: Sequences must always be terminated by a GOTO instruction.
 */
#define SLEEP           0
#define GOTO            1
#define STOP            2
#define BITCLEAR        3
#define BITSET          4

typedef struct {
  uint8_t       action;
  uint32_t      value;
} seqop_t;

// Flashing sequence for LED1.
static const seqop_t LED1_sequence[] =
{
  {BITCLEAR, PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    200},
  {BITSET,   PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    800},
  {BITCLEAR, PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    400},
  {BITSET,   PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    600},
  {BITCLEAR, PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    600},
  {BITSET,   PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    400},
  {BITCLEAR, PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    800},
  {BITSET,   PAL_PORT_BIT(GPIOA_LED)},
  {SLEEP,    200},
  {GOTO,     0}
};

// Sequencer thread class. It can drive LEDs or other output pins.
class SequencerThread : public BaseStaticThread<128> {
private:
  const seqop_t *base, *curr;                   // Thread local variables.

protected:
  virtual msg_t main(void) {
    while (true) {
      switch(curr->action) {
        case SLEEP:    sleep(curr->value); break;
        case GOTO:     curr = &base[curr->value]; continue;
        case STOP:     return 0;
        case BITCLEAR: palClearPort(GPIOA, curr->value); break;
        case BITSET:   palSetPort(GPIOA, curr->value); break;
      }
      curr++;
    }
    return 0;
  }

public:
  SequencerThread(const seqop_t *sequence) : BaseStaticThread<128>() {
    base = curr = sequence;
  }
};

#if TESTER
// Tester thread class. This thread executes the test suite.
class TesterThread : public BaseStaticThread<256> {

protected:
  virtual msg_t main(void) {
    setName("tester");
    return TestThread(&SD2);
  }

public:
  TesterThread(void) : BaseStaticThread<256>() {}
};
#endif

static char* appendHex (char *p, uint8_t v) {
  const char* hex = "0123456789ABCDEF";
  *p++ = hex[v>>4];
  *p++ = hex[v&0x0F];
  return p;
}

// CAN receive thread class.
class CanRecvThread : public BaseStaticThread<256> {

protected:
  virtual msg_t main(void) {
    setName("canrecv");
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

public:
  CanRecvThread(void) : BaseStaticThread<256>() {}
};

// CAN send thread class.

static uint8_t hexDigit (char c) {
    if (c > '9')
        c -= 7;
    return c & 0xF;
}

class CanSendThread : public BaseStaticThread<256> {

protected:
  virtual msg_t main(void) {
    setName("cansend");

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

public:
  CanSendThread(void) : BaseStaticThread<256>() {}
};

// Static threads instances
#if TESTER
static TesterThread tester;
#endif
static CanRecvThread canReceiver;
static CanSendThread canSender;
static SequencerThread blinker1(LED1_sequence);

// Application entry point.
int main () {
  halInit();
  System::init();

  palSetGroupMode(GPIOA, PAL_PORT_BIT(GPIOA_LED), 0, PAL_MODE_OUTPUT_PUSHPULL);
  palSetGroupMode(GPIOC, PAL_PORT_BIT(GPIOC_BUTTON), 0, PAL_MODE_INPUT);
  
  AFIO->MAPR = (AFIO->MAPR & ~(0b11<<13)) | (0b10<<13); // CAN RX/TX on PB8/PB9
  palSetGroupMode(GPIOB, PAL_PORT_BIT(GPIOB_CAN_RX), 0, PAL_MODE_INPUT);
  palSetGroupMode(GPIOB, PAL_PORT_BIT(GPIOB_CAN_TX), 0, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  
  sdStart(&SD2, NULL);
  chprintf(serial, "\r\n[nucleo1]\r\n");

  // Activates CAN driver.
  static const CANConfig cancfg = {
    CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
    CAN_BTR_SJW(1) | CAN_BTR_TS2(2) |
    CAN_BTR_TS1(7) | CAN_BTR_BRP(2)
  };
  canStart(&CAND1, &cancfg);
  
  canReceiver.start(NORMALPRIO);
  canSender.start(NORMALPRIO);
  blinker1.start(NORMALPRIO + 10);

  while (true) {
#if TESTER
    if (!palReadPad(GPIOC, GPIOC_BUTTON)) {
      tester.start(NORMALPRIO);
      tester.wait();
    };
#endif
    BaseThread::sleep(100);
  }

  return 0;
}
