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
#include "ff.h"
#include "evtimer.h"
#include <string.h>

/*===========================================================================*/
/* Card insertion monitor.                                                   */
/*===========================================================================*/

#define POLLING_INTERVAL                10
#define POLLING_DELAY                   10

/**
 * @brief   Card monitor timer.
 */
static VirtualTimer tmr;

/**
 * @brief   Debounce counter.
 */
static unsigned cnt;

/**
 * @brief   Card event sources.
 */
static EventSource inserted_event, removed_event;

/**
 * @brief   Insertion monitor timer callback function.
 *
 * @param[in] p         pointer to the @p BaseBlockDevice object
 *
 * @notapi
 */
static void tmrfunc(void *p) {
  BaseBlockDevice *bbdp = p;

  /* The presence check is performed only while the driver is not in a
     transfer state because it is often performed by changing the mode of
     the pin connected to the CS/D3 contact of the card, this could disturb
     the transfer.*/
  blkstate_t state = blkGetDriverState(bbdp);
  chSysLockFromIsr();
  if ((state != BLK_READING) && (state != BLK_WRITING)) {
    /* Safe to perform the check.*/
    if (cnt > 0) {
      if (blkIsInserted(bbdp)) {
        if (--cnt == 0) {
          chEvtBroadcastI(&inserted_event);
        }
      }
      else
        cnt = POLLING_INTERVAL;
    }
    else {
      if (!blkIsInserted(bbdp)) {
        cnt = POLLING_INTERVAL;
        chEvtBroadcastI(&removed_event);
      }
    }
  }
  chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, bbdp);
  chSysUnlockFromIsr();
}

/**
 * @brief   Polling monitor start.
 *
 * @param[in] p         pointer to an object implementing @p BaseBlockDevice
 *
 * @notapi
 */
static void tmr_init(void *p) {

  chEvtInit(&inserted_event);
  chEvtInit(&removed_event);
  chSysLock();
  cnt = POLLING_INTERVAL;
  chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, p);
  chSysUnlock();
}

/* Board-related functions related to the MMC_SPI driver.*/
bool_t mmc_lld_is_card_inserted(MMCDriver *mmcp) {

  (void)mmcp;
  return !palReadPad(GPIO0, GPIO0_MMC_CD);
}

bool_t mmc_lld_is_write_protected(MMCDriver *mmcp) {

  (void)mmcp;
  return 0; // palReadPad(IOPORT2, PB_WP1);
}

/*===========================================================================*/
/* FatFs related.                                                            */
/*===========================================================================*/

/**
 * @brief FS object.
 */
FATFS MMC_FS;

/**
 * MMC driver instance.
 */
MMCDriver MMCD1;

/* FS mounted and ready.*/
static bool_t fs_ready = FALSE;

/* Maximum speed SPI configuration (18MHz, CPHA=0, CPOL=0).*/
static SPIConfig hs_spicfg = {
  NULL,
  GPIO0,
  GPIO0_MMC_SSEL,
  CR0_DSS8BIT | CR0_FRFSPI | CR0_CLOCKRATE(2),
  2
};

/* Low speed SPI configuration (281.250kHz, CPHA=0, CPOL=0).*/
static SPIConfig ls_spicfg = {
  NULL,
  GPIO0,
  GPIO0_MMC_SSEL,
  CR0_DSS8BIT | CR0_FRFSPI | CR0_CLOCKRATE(2),
  254
};

/* MMC/SD over SPI driver configuration.*/
static MMCConfig mmccfg = {&SPID2, &ls_spicfg, &hs_spicfg};

/* Generic large buffer.*/
uint8_t fbuff[1024];

static FRESULT scan_files(BaseSequentialStream *chp, char *path) {
  FRESULT res;
  FILINFO fno;
  DIR dir;
  int i;
  char *fn;

#if _USE_LFN
  fno.lfname = 0;
  fno.lfsize = 0;
#endif
  res = f_opendir(&dir, path);
  if (res == FR_OK) {
    i = strlen(path);
    for (;;) {
      res = f_readdir(&dir, &fno);
      if (res != FR_OK || fno.fname[0] == 0)
        break;
      if (fno.fname[0] == '.')
        continue;
      fn = fno.fname;
      if (fno.fattrib & AM_DIR) {
        path[i++] = '/';
        strcpy(&path[i], fn);
        res = scan_files(chp, path);
        if (res != FR_OK)
          break;
        path[--i] = 0;
      }
      else {
        chprintf(chp, "%s/%s\r\n", path, fn);
      }
    }
  }
  return res;
}

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WA_SIZE(1024)
#define TEST_WA_SIZE    THD_WA_SIZE(256)

BaseSequentialStream* chp1 = (BaseSequentialStream*) &SD1;

/*
 * Red LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1 (void *arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
    palTogglePad(GPIO2, GPIO2_RGB1_RED);
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

static void cmd_tree(BaseSequentialStream *chp, int argc, char *argv[]) {
  FRESULT err;
  uint32_t clusters;
  FATFS *fsp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: tree\r\n");
    return;
  }
  if (!fs_ready) {
    chprintf(chp, "File System not mounted\r\n");
    return;
  }
  err = f_getfree("/", &clusters, &fsp);
  if (err != FR_OK) {
    chprintf(chp, "FS: f_getfree() failed\r\n");
    return;
  }
  chprintf(chp,
           "FS: %lu free clusters, %lu sectors per cluster, %lu bytes free\r\n",
           clusters, (uint32_t)MMC_FS.csize,
           clusters * (uint32_t)MMC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE);
  fbuff[0] = 0;
  scan_files(chp, (char *)fbuff);
}

/*
 * CAN transmitter thread.
 */
static WORKING_AREA(can_tx_wa, 256);

static msg_t can_tx(void * p) {
  CANTxFrame txmsg_can1;
  (void)p;
  chRegSetThreadName("transmitter");
  txmsg_can1.IDE = CAN_IDE_STD;
  txmsg_can1.EID = 0x412;
  txmsg_can1.RTR = CAN_RTR_DATA;
  
  // read the trimpot via ADC to decide what step commands to send out
  // uses low-level access to implement the simplest possible polled readout

#define AD0CR_PDN                   (1UL << 21)
#define AD0CR_START_NOW             (1UL << 24)
#define AD0CR_CHANNEL5              (1UL << 5)
#define LPC17xx_ADC_CLKDIV          12
#define AD0CR_START_MASK            (7UL << 24)
  
  // set up the ADC and pin
  LPC_SC->PCONP |= (1UL << 12);       // enable ADC power
  LPC_ADC->CR = AD0CR_PDN | (LPC17xx_ADC_CLKDIV << 8) | AD0CR_CHANNEL5;
  LPC_PINCON->PINSEL3 |= 0b11 << 30;  // enable P1.31 as AD0.5

  uint8_t lastCmd = 0;
  while (!chThdShouldTerminate()) {
    // perform one ADC conversion
    LPC_ADC->CR &= ~AD0CR_START_MASK;
    LPC_ADC->CR |= AD0CR_START_NOW;
    while ((LPC_ADC->GDR & (1<<31)) == 0)
      ;
    int sample = ((LPC_ADC->GDR >> 4) & 0xFFF) - 0x800; // signed
    
    uint8_t cmd = 0x04; // enable
    if (sample < 0) {
      cmd ^= 0x02; // direction
      sample = -sample;
    }
    if (sample < 100)
      continue; // dead zone, no stepping
    if (cmd != lastCmd) {
      // transmit a 1-byte "command" packet with the enable and direction bits
      txmsg_can1.DLC = 1;
      txmsg_can1.data8[0] = lastCmd = cmd;
    } else {
      sample /= 2; // >= 50
      // transmit a 0-byte "step" packet, but delay 1..950 ms before doing so
      if (sample > 999)
        sample = 999;
      chThdSleepMilliseconds(1000-sample);
      txmsg_can1.DLC = 0;
    }
    canTransmit(&CAND1, CAN_ANY_MAILBOX, &txmsg_can1, MS2ST(1000));
  }
  return 0;
}

/*
 * MMC card insertion event.
 */
static void InsertHandler(eventid_t id) {
  FRESULT err;

  (void)id;
  // buzzPlayWait(1000, MS2ST(100));
  // buzzPlayWait(2000, MS2ST(100));
  chprintf(chp1, "MMC: inserted\r\n");
  /*
   * On insertion MMC initialization and FS mount.
   */
  chprintf(chp1, "MMC: initialization ");
  if (mmcConnect(&MMCD1)) {
    chprintf(chp1, "failed\r\n");
    return;
  }
  chprintf(chp1, "ok\r\n");
  chprintf(chp1, "FS: mount ");
  err = f_mount(0, &MMC_FS);
  if (err != FR_OK) {
    chprintf(chp1, "failed\r\n");
    mmcDisconnect(&MMCD1);
    return;
  }
  fs_ready = TRUE;
  chprintf(chp1, "ok\r\n");
  // buzzPlay(440, MS2ST(200));
}

/*
 * MMC card removal event.
 */
static void RemoveHandler(eventid_t id) {

  (void)id;
  chprintf(chp1, "MMC: removed\r\n");
  mmcDisconnect(&MMCD1);
  fs_ready = FALSE;
  // buzzPlayWait(2000, MS2ST(100));
  // buzzPlayWait(1000, MS2ST(100));
}

/*
 * Application entry point.
 */
int main(void) {
  static const evhandler_t evhndl[] = {
    InsertHandler,
    RemoveHandler
  };
  struct EventListener el0, el1;

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
   * Initializes the MMC driver to work with SSP1.
   */
  mmcObjectInit(&MMCD1);
  mmcStart(&MMCD1, &mmccfg);
  
  /*
   * Activates the card insertion monitor.
   */
  tmr_init(&MMCD1);
  
  /*
   * Creates the LWIP threads (it changes priority internally).
   */
  chThdCreateStatic(wa_lwip_thread, LWIP_THREAD_STACK_SIZE, NORMALPRIO + 1,
                    lwip_thread, NULL);

  /*
   * Creates the HTTP thread (it changes priority internally).
   */
  chThdCreateStatic(wa_http_server, sizeof(wa_http_server), NORMALPRIO + 1,
                    http_server, NULL);

  chprintf(chp1, "\r\n[netdemo]\r\n");

  static const ShellCommand commands[] = {
    // {"pwrdown", cmd_pwrdown},
    {"mem", cmd_mem},
    {"threads", cmd_threads},
    {"test", cmd_test},
    {"tree", cmd_tree},
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
   * sleeping in a loop and listen for events.
   */
  chEvtRegister(&inserted_event, &el0, 0);
  chEvtRegister(&removed_event, &el1, 1);
  while (TRUE)
    chEvtDispatch(evhndl, chEvtWaitOne(ALL_EVENTS));
}
