// boot send - send firmware via CAN @ 100 KHz using CANopen
// jcw, 2014-04-14

#include "ch.h"
#include "hal.h"
#include "data.h"
#include <string.h>

#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T msg_obj;

#include "canopen.h"
#include "canbase.h"

int main () {
  halInit();
  chSysInit();
  initCan();

  // brief red flash on startup
  palTogglePad(GPIO0, GPIO0_LED2);
  chThdSleepMilliseconds(100);
  palTogglePad(GPIO0, GPIO0_LED2);
  
  /* Configure message object 1 to receive all 11-bit messages 0x5FD */
  msg_obj.msgobj = 1;
  msg_obj.mode_id = 0x5FD;
  msg_obj.mask = 0x7FF;
  LPC_CCAN_API->config_rxmsgobj(&msg_obj);

  int i = 0;
  for (;;) {
    // ....
    msg_obj.msgobj  = 1;
    msg_obj.mode_id = 0x67D;
    msg_obj.mask    = 0x0;
    msg_obj.dlc     = 8;
    memcpy(msg_obj.data, bootData + 8 * i++, 8);

    LPC_CCAN_API->can_transmit(&msg_obj);
    chThdSleepMilliseconds(500);

    // palTogglePad(GPIO1, GPIO1_LED1);
    palTogglePad(GPIO3, GPIO3_MOTOR_MS1);
    // palTogglePad(GPIO2, GPIO2_HALL_MODE);
  }

  return 0;
}
