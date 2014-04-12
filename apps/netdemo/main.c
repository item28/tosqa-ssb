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

#include "ch.h"
#include "hal.h"
#include "test.h"
#include "shell.h"
#include "chprintf.h"
#include "lwipthread.h"
#include "web/web.h"

#define SHELL_WA_SIZE   THD_WA_SIZE(1024)
#define TEST_WA_SIZE    THD_WA_SIZE(256)

BaseSequentialStream* chp1 = (BaseSequentialStream*) &SD1;

/*
 * Green LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1 (void *arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
    palClearPad(GPIO2, GPIO2_RGB1_RED);
    chThdSleepMilliseconds(500);
    palSetPad(GPIO2, GPIO2_RGB1_RED);
    chThdSleepMilliseconds(500);
  }
  return 0;
}

// static void cmd_pwrdown(BaseSequentialStream *chp, int argc, char*argv[]){
//   (void)argv;
//   if (argc > 0) {
//     chprintf(chp, "Usage: pwrdown\r\n");
//     return;
//   }
//   chprintf(chp, "Going to power down.\r\n");
//   chThdSleepMilliseconds(200);
//   chSysLock();
//   LPC_SC->PCON |= 0x03; // set PM0 and PM1
//   __WFI();
// }

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreStatus());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
  static const char *states[] = {THD_STATE_NAMES};
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "    addr    stack prio refs     state time\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%.8lx %.8lx %4lu %4lu %9s %lu\r\n",
            (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
            (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
            states[tp->p_state], (uint32_t)tp->p_time);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: test\r\n");
    return;
  }
  tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriority(),
                           TestThread, chp);
  if (tp == NULL) {
    chprintf(chp, "out of memory\r\n");
    return;
  }
  chThdWait(tp);
}

/*
 * Transmitter thread.
 */
static WORKING_AREA(can_tx_wa, 256);
static msg_t can_tx(void * p) {
  CANTxFrame txmsg_can1;
  (void)p;
  chRegSetThreadName("transmitter");
  txmsg_can1.IDE = CAN_IDE_STD;
  txmsg_can1.EID = 0x412;
  txmsg_can1.RTR = CAN_RTR_DATA;
  txmsg_can1.DLC = 1;

  while (!chThdShouldTerminate()) {
    txmsg_can1.data8[0] = 0x05;
    canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg_can1, MS2ST(1000));
    chThdSleepMilliseconds(10);
    txmsg_can1.data8[0] = 0x04;
    canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg_can1, MS2ST(1000));
    chThdSleepMilliseconds(500);
  }
  return 0;
}

/*
 * Application entry point.
 */
int main(void) {

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Activates the serial driver 6 using the driver default configuration.
   */
  sdStart(&SD1, NULL);

  /*
   * Activates CAN driver 1.
   */
  static const CANConfig cancfg = {
    0,
    CANBTR_SJW(0) | CANBTR_TESG2(1) |
    CANBTR_TESG1(8) | CANBTR_BRP(19)
  };
  canStart(&CAND1, &cancfg);

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, can_tx, NULL);

  /*
   * Creates the CAN transmit thread.
   */
  chThdCreateStatic(can_tx_wa, sizeof(can_tx_wa), NORMALPRIO, Thread1, NULL);

  /*
   * Creates the LWIP threads (it changes priority internally).
   */
  // chThdCreateStatic(wa_lwip_thread, LWIP_THREAD_STACK_SIZE, NORMALPRIO + 1,
  //                   lwip_thread, NULL);

  /*
   * Creates the HTTP thread (it changes priority internally).
   */
  // chThdCreateStatic(wa_http_server, sizeof(wa_http_server), NORMALPRIO + 1,
  //                   http_server, NULL);

  chprintf(chp1, "\r\n[satmon]\r\n");

  static const ShellCommand commands[] = {
    // {"pwrdown", cmd_pwrdown},
    {"mem", cmd_mem},
    {"threads", cmd_threads},
    {"test", cmd_test},
    {NULL, NULL}
  };

  static const ShellConfig shell_cfg1 = {
    (BaseSequentialStream  *)&SD1,
    commands
  };
  /* Shell initialization.*/
  shellInit();
  static WORKING_AREA(waShell, SHELL_WA_SIZE);
  shellCreateStatic(&shell_cfg1, waShell, sizeof(waShell), NORMALPRIO);

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (TRUE) {
    chThdSleepMilliseconds(500);
  }
}
