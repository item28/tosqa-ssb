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
#include "evtimer.h"
#include "chprintf.h"
#include "ff.h"
#include "data.h"
#include "usbconf.h"

BaseSequentialStream* serio;

// Card insertion monitor

#define POLLING_INTERVAL                10
#define POLLING_DELAY                   10

// Card monitor timer.
static VirtualTimer tmr;

// Debounce counter.
static unsigned cnt;

// Card event sources.
static EventSource inserted_event, removed_event;

// Insertion monitor timer callback function.
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

// Polling monitor start.
static void tmr_init(void *p) {

  chEvtInit(&inserted_event);
  chEvtInit(&removed_event);
  chSysLock();
  cnt = POLLING_INTERVAL;
  chVTSetI(&tmr, MS2ST(POLLING_DELAY), tmrfunc, p);
  chSysUnlock();
}

// FatFs related

// FS object.
FATFS MMC_FS;

// MMC driver instance.
MMCDriver MMCD1;

/* FS mounted and ready.*/
static bool_t fs_ready = FALSE;

/* Maximum speed SPI configuration (18MHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig hs_spicfg = {NULL, IOPORT2, GPIOB_SPI2NSS, 0};

/* Low speed SPI configuration (281.250kHz, CPHA=0, CPOL=0, MSb first).*/
static SPIConfig ls_spicfg = {NULL, IOPORT2, GPIOB_SPI2NSS,
                              SPI_CR1_BR_2 | SPI_CR1_BR_1};

/* MMC/SD over SPI driver configuration.*/
static MMCConfig mmccfg = {&SPID2, &ls_spicfg, &hs_spicfg};

/* Generic large buffer.*/
uint8_t fbuff[256]; // was 1024

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

// Command line related

static void cmd_mem(void) {
  size_t n, size;
  n = chHeapStatus(NULL, &size);
  chprintf(serio, "\r\ncore free memory : %u bytes\r\n", chCoreStatus());
  chprintf(serio, "heap fragments   : %u\r\n", n);
  chprintf(serio, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(void) {
  static const char *states[] = {THD_STATE_NAMES};
  chprintf(serio, "\r\n    addr    stack prio refs     state time\r\n");
  Thread *tp = chRegFirstThread();
  do {
    chprintf(serio, "%.8lx %.8lx %4lu %4lu %9s %lu\r\n",
            (uint32_t)tp, (uint32_t)tp->p_ctx.r13,
            (uint32_t)tp->p_prio, (uint32_t)(tp->p_refs - 1),
            states[tp->p_state], (uint32_t)tp->p_time);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static void cmd_tree(void) {
  FRESULT err;
  uint32_t clusters;
  FATFS *fsp;
  if (!fs_ready) {
    chprintf(serio, "File System not mounted\r\n");
    return;
  }
  err = f_getfree("/", &clusters, &fsp);
  if (err != FR_OK) {
    chprintf(serio, "FS: f_getfree() failed\r\n");
    return;
  }
  chprintf(serio,
           "\r\nFS: %lu free clusters, %lu sectors per cluster, %lu bytes free\r\n",
           clusters, (uint32_t)MMC_FS.csize,
           clusters * (uint32_t)MMC_FS.csize * (uint32_t)MMCSD_BLOCK_SIZE);
  fbuff[0] = 0;
  scan_files(serio, (char *)fbuff);
}

#include "forth.h"

// Main and generic code

// MMC card insertion event.
static void InsertHandler(eventid_t id) {
  (void)id;
  // On insertion MMC initialization and FS mount.
  if (mmcConnect(&MMCD1)) {
    return;
  }
  FRESULT err = f_mount(0, &MMC_FS);
  if (err != FR_OK) {
    mmcDisconnect(&MMCD1);
    return;
  }
  fs_ready = TRUE;
}

// MMC card removal event.
static void RemoveHandler(eventid_t id) {
  (void)id;
  mmcDisconnect(&MMCD1);
  fs_ready = FALSE;
}

// Forth command thread

static WORKING_AREA(waCommand, 1024);
static msg_t command (void *arg) {
  (void)arg;
  chRegSetThreadName("command");
  while (TRUE) {
    chThdSleepMilliseconds(500); // avoid startup stutter and runaway loops
    runForth(bootData);
  }
  return 0;
}

// Application entry point.
int main(void) {
  static const evhandler_t evhndl[] = {
    InsertHandler,
    RemoveHandler
  };
  struct EventListener el0, el1;

  halInit();
  chSysInit();

  // a delay is inserted to not have to disconnect the cable after a reset.
  // usbDisconnectBus(serusbcfg.usbp);
  // chThdSleepMilliseconds(1500);
  // usbStart(serusbcfg.usbp, &usbcfg);
  // usbConnectBus(serusbcfg.usbp);

  // Activates the serial drivers using the driver default configuration.
  sdStart(&SD1, NULL);
  sdStart(&SD2, NULL);

  // fix incorrect init in board.h
  // #define VAL_GPIOACRH            0x88844888      /* PA15...PA8 */
  GPIOA->CRH = 0x888448B8; // PA9 = B, not 8!
    
  // chprintf((BaseSequentialStream *)&SDU1, "hello USB!\r\n");
  // chprintf((BaseSequentialStream *)&SD1, "hello 1!\r\n");
  // chprintf((BaseSequentialStream *)&SD2, "hello 2!\r\n");

  serio = (BaseSequentialStream *)&SD1;

  // Initializes the MMC driver to work with SPI2.
  palSetPadMode(IOPORT2, GPIOB_SPI2NSS, PAL_MODE_OUTPUT_PUSHPULL);
  palSetPad(IOPORT2, GPIOB_SPI2NSS);
  mmcObjectInit(&MMCD1);
  mmcStart(&MMCD1, &mmccfg);

  // Activates the card insertion monitor.
  tmr_init(&MMCD1);

  // Creates the command thread.
  chThdCreateStatic(waCommand, sizeof(waCommand), NORMALPRIO, command, NULL);

  // sleep in a loop, listen for events, blink led, and check button
  chEvtRegister(&inserted_event, &el0, 0);
  chEvtRegister(&removed_event, &el1, 1);
  int led = 0;
  while (TRUE) {
    chEvtDispatch(evhndl, chEvtWaitOneTimeout(ALL_EVENTS, MS2ST(100)));

    if (++led >= 5) {
      led = 0;
      palTogglePad(IOPORT3, GPIOC_LED);
    }
    
    if (palReadPad(IOPORT1, GPIOA_BUTTON)) {
      palClearPad(IOPORT1, GPIOA_BUTTON);
      if (serio == (BaseSequentialStream *)&SD1) {        // SD1 -> SD2
        serio = (BaseSequentialStream *)&SD2;
      } else if (serio == (BaseSequentialStream *)&SD2) { // SD2 -> SDU1
        palSetPad(IOPORT1, GPIOA_BUTTON);
        // Initializes a serial-over-USB CDC driver.
        sduObjectInit(&SDU1);
        sduStart(&SDU1, &serusbcfg);
        usbStart(serusbcfg.usbp, &usbcfg);
        usbConnectBus(serusbcfg.usbp);
        serio = (BaseSequentialStream *)&SDU1;
      } else {                                            // SDU1 -> SD1
        usbStop(serusbcfg.usbp);
        usbDisconnectBus(serusbcfg.usbp);
        sduStop(&SDU1);
        serio = (BaseSequentialStream *)&SD1;
      }
      chprintf(serio, "[ifex] OK\r\n");
      
      while (palReadPad(IOPORT1, GPIOA_BUTTON))
        chThdYield();
    }
  }
  return 0;
}
