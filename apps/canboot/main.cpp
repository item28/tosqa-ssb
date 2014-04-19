// bootstrap loader via the CAN bus, using Tosqa conventions
// jcw, 2014-04-19

#include "ch.h"
#include "hal.h"
#include <string.h>

// Olimex LPC-P11C24 board has green/yellow LEDs, but on different pins
#if 1
    #define GPIO1_OLI_LED1    GPIO1_DIGIPOT_UD  // PIO1_11, green
    #define GPIO1_OLI_LED2    GPIO1_POWER_TERM  // PIO1_10, yellow
#endif

#include "../driver/blinker.cpp"

// ROM-based CAN bus drivers
#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T rxMsg;
uint32_t       myUid [4];
uint8_t        codeBuf [4096];

static void CAN_rxCallback (uint8_t msg_obj_num) {
    rxMsg.msgobj = msg_obj_num;
    LPC_CCAN_API->can_receive(&rxMsg);
}

static void CAN_txCallback (uint8_t /* msg_obj_num */) {}

static void CAN_errorCallback (uint32_t error_info) {
    // if (error_info & 0x0002)
    //     palTogglePad(GPIO1, GPIO1_OLI_LED1);
    if (error_info & 0x0004)
        palTogglePad(GPIO1, GPIO1_OLI_LED1);
}

static CCAN_CALLBACKS_T callbacks = {
    CAN_rxCallback,
    CAN_txCallback,
    CAN_errorCallback,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

extern "C" void Vector74 ();
// CH_FAST_IRQ_HANDLER(Vector74)  {
//     LPC_CCAN_API->isr();
// }
CH_IRQ_HANDLER(Vector74)  {
    CH_IRQ_PROLOGUE();
    LPC_CCAN_API->isr();
    CH_IRQ_EPILOGUE();
}

static void canBusInit () {
    LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 17; // SYSCTL_CLOCK_CAN

    static uint32_t clkInitTable[2] = { // see common/canRate.c
        0x00000000UL,                   // CANCLKDIV, running at 48 MHz
        0x000076C5UL                    // CAN_BTR, produces 500 Kbps
    };

    LPC_CCAN_API->init_can(clkInitTable, true);
    LPC_CCAN_API->config_calb(&callbacks);
    NVIC_EnableIRQ(CAN_IRQn);
}

static const uint32_t* iapCall(int type, int a, int b, int c, int d) {
    static uint32_t results[5];
    uint32_t params [5] = { type, a, b, c, d };
    iap_entry(params, results);
    return results[0] == 0 ? results + 1 : 0;
}

// see http://www.lpcware.com/content/forum/read-serial-number
static void readUid (uint32_t* uid) {
  unsigned param_table[5];
  unsigned result_table[5];
  param_table[0] = 58; // IAP command
  iap_entry(param_table, result_table);
  if (result_table[0] == 0) {
    uid[0] = result_table[1];
    uid[1] = result_table[2];
    uid[2] = result_table[3];
    uid[3] = result_table[4];
  } else {
    uid[0] = uid[1] = uid[2] = uid[3] = 0;
  }
}

// send first 8 bytes of this chip's UID out to 0x1F123400
static uint8_t bootCheck () {
    // configure message object 1 to receive 29-bit messages to 0x1F123400..FF
    CCAN_MSG_OBJ_T msgObj;    
    msgObj.msgobj = 1;
    msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400;
    msgObj.mask = 0x1FFFFF00;
    LPC_CCAN_API->config_rxmsgobj(&msgObj);

    msgObj.msgobj  = 2;
    msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400;
    // msgObj.mode_id = 0x432;
    msgObj.mask    = 0x0;
    msgObj.dlc     = 8;
    memcpy(msgObj.data, myUid, 8);
    LPC_CCAN_API->can_transmit(&msgObj);

    // wait up to 250 ms to get a reply
    for (int i = 0; i < 250; ++i) {
        if (rxMsg.msgobj == 1 && memcmp(rxMsg.data, myUid, 8) == 0) {
            rxMsg.msgobj = 0;
            uint8_t myAddr = rxMsg.mode_id; // use lower 8 bits of address
            // got reply, only listen to the specified address from now on
            if (myAddr > 0) {
                msgObj.msgobj = 1;
                msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400 | myAddr;
                msgObj.mask = 0x1FFFFFFF;
                LPC_CCAN_API->config_rxmsgobj(&msgObj);
                return myAddr;
            }
        }
        chThdSleepMilliseconds(1);
    }
    return 0;
}

static bool download (uint8_t page) {
    // send out a request to receive a 4 KB page
    CCAN_MSG_OBJ_T msgObj;    
    msgObj.msgobj  = 3;
    msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400;
    // msgObj.mode_id = 0x321;
    msgObj.mask    = 0x0;
    msgObj.dlc     = 2;
    msgObj.data[0] = 1; // boot request for a 4 KB page download
    msgObj.data[1] = page;
    LPC_CCAN_API->can_transmit(&msgObj);
    
    // wait entire page to come in, as 8-byte messages
    rxMsg.msgobj = 0;
    uint8_t *p = codeBuf;
    for (int i = 0; i < 250; ++i) {
        if (rxMsg.msgobj > 0 && rxMsg.dlc == 8) {
            rxMsg.msgobj = 0;
            // palTogglePad(GPIO1, GPIO1_OLI_LED1); // toggle on each receive
            memcpy(p, rxMsg.data, 8);
            p += 8;
            if (p >= codeBuf + sizeof codeBuf)
                return true; // page reception ok
            i = 0; // reset timeout
        }
        chThdSleepMilliseconds(1);
    }
    // download ends when messages are not received within 250 ms of each other
    return false;
}

int main () {
    halInit();
    chSysInit();
    canBusInit();
    blinkerInit();

    readUid(myUid);
    
    chThdSleepMilliseconds(1000);
    
    if (bootCheck() > 0) {
        for (uint8_t page = 1; download(page); ++page) {
            #ifdef GPIO1_OLI_LED2
                palTogglePad(GPIO1, GPIO1_OLI_LED2);
            #else
                palTogglePad(GPIO1, GPIO1_LED1);
            #endif
        }
    }
    
    for (;;) {
        chThdSleepMilliseconds(500);
    }

    return 0;
}
