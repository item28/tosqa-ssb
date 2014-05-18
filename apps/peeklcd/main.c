#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"
#include "test.h"
#include "gfx.h"

#define GPIOB_CAN_RX  8   // alt mode 0b10
#define GPIOB_CAN_TX  9   // alt mode 0b10

int devcode, d1, d2;

static WORKING_AREA(waBlinker, 64);
static msg_t Blinker (void *arg) {
  (void)arg;
  chRegSetThreadName("blinker1");
  while (TRUE) {
    palTogglePad(GPIOB, GPIOB_LED1);
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

static WORKING_AREA(waShowCan, 1024);
static msg_t ShowCan (void *arg) {
  (void)arg;
  chRegSetThreadName("showcan");
  
  gdispSetOrientation(GDISP_ROTATE_270);
  coord_t width = 320;
  coord_t height = 240;

  font_t font1 = gdispOpenFont("fixed*");
  int pos = 0, x = 0;
  const coord_t fheight1 = 8;
  int colour = White;
  
  CANRxFrame rxMsg;
  char buf [30];
  
  for (;;) {
    if (canReceive(&CAND1, CAN_ANY_MAILBOX, &rxMsg, 10000) == RDY_OK) {
      uint32_t id = rxMsg.EID;
      if (!rxMsg.IDE)
        id &= 0x7FF;
      chsnprintf(buf, sizeof buf, "%9x ", id);
      char* p = buf + strlen(buf);
      int i;
      for (i = 0; i < rxMsg.DLC; ++i)
        p = appendHex(p, rxMsg.data8[i]);
      *p = 0;
    } else
      strcpy(buf, ".");

    gdispFillStringBox(x, pos, width/2+1, fheight1, buf, font1, colour, Black, justifyLeft);
    pos += fheight1;
    if (pos >= height) {
        pos = 0;
        x = 160 - x;
        if (x == 0)
            colour = colour == White ? Yellow : White;
    }
  }
  
  return 0;
}

// Application entry point.
int main (void) {
  halInit();
  chSysInit();

  palSetGroupMode(GPIOB, PAL_PORT_BIT(12), 0, PAL_MODE_INPUT);
  palSetGroupMode(GPIOB, PAL_PORT_BIT(13), 0, PAL_MODE_INPUT);

  AFIO->MAPR = (AFIO->MAPR & ~(0b11<<13)) | (0b10<<13); // CAN RX/TX on PB8/PB9
  palSetGroupMode(GPIOB, PAL_PORT_BIT(GPIOB_CAN_RX), 0, PAL_MODE_INPUT);
  palSetGroupMode(GPIOB, PAL_PORT_BIT(GPIOB_CAN_TX), 0, PAL_MODE_STM32_ALTERNATE_PUSHPULL);
  
  // Activates CAN driver.
  static const CANConfig cancfg = {
    CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
    CAN_BTR_SJW(1) | CAN_BTR_TS2(2) |
    CAN_BTR_TS1(7) | CAN_BTR_BRP(5)
  };
  canStart(&CAND1, &cancfg);
  
  // Initialize and clear the display
  gfxInit();
  
  chThdCreateStatic(waBlinker, sizeof(waBlinker), NORMALPRIO, Blinker, NULL);
  chThdCreateStatic(waShowCan, sizeof(waShowCan), NORMALPRIO-2, ShowCan, NULL);

  for (;;) {
    // if (!palReadPad(GPIO2, GPIO2_KEY1))
    //   palTogglePad(GPIO1, GPIO1_LCD_BLED);
    // if (!palReadPad(GPIOC, GPIOC_BUTTONA))
    //   TestThread(&SD1);
    chThdSleepMilliseconds(500);
  }
}
