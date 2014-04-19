// bootstrap loader via the CAN bus, using Tosqa conventions
// jcw, 2014-04-19

#include "ch.h"
#include "hal.h"
#include <string.h>

// ROM-based CAN bus drivers
#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

#define BLINK() palTogglePad(GPIO2, 10) // LPC11C24-DK-A
// #define BLINK() palTogglePad(GPIO0, 7) // LPCxpresso 11C24

CCAN_MSG_OBJ_T rxMsg;
uint32_t       myUid [4];
uint8_t        codeBuf [4096];
uint8_t        shortId;

static void CAN_rxCallback (uint8_t msg_obj_num) {
    rxMsg.msgobj = msg_obj_num;
    LPC_CCAN_API->can_receive(&rxMsg);
}

static CCAN_CALLBACKS_T callbacks = {
    CAN_rxCallback, 0, 0, 0, 0, 0, 0, 0,
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

#define PREP_WRITE      50
#define COPY_TO_FLASH   51
#define ERASE_SECT      52
#define READ_UID        58

static const uint32_t* iapCall(uint32_t type, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    static uint32_t results[5];
    uint32_t params [5] = { type, a, b, c, d };
    chSysDisable();
    iap_entry((unsigned*) params, (unsigned*) results);
    chSysEnable();
    return results[0] == 0 ? results + 1 : 0;
}

// send first 8 bytes of this chip's UID out to 0x1F123400
static bool bootCheck () {
    CCAN_MSG_OBJ_T msgObj;
    
    // send own uid to a fixed CAN bus address to request a unique 1..255 id
    msgObj.msgobj  = 10;
    msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400;
    // msgObj.mode_id = 0x432;
    msgObj.mask    = 0x0;
    msgObj.dlc     = 8;
    memcpy(msgObj.data, myUid, 8);
    LPC_CCAN_API->can_transmit(&msgObj);

    // configure message object 1 to receive 29-bit messages to 0x1F123400..FF
    msgObj.msgobj = 1;
    msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400;
    msgObj.mask = 0x1FFFFF00;
    LPC_CCAN_API->config_rxmsgobj(&msgObj);

    // wait up to 100 ms to get a reply
    rxMsg.msgobj = 0;
    for (int i = 0; i < 100; ++i) {
        if (rxMsg.msgobj) {
            rxMsg.msgobj = 0;
            if (memcmp(rxMsg.data, myUid, 8) == 0) {
                shortId = rxMsg.mode_id; // use lower 8 bits of address as id

                // only listen to the assigned address from now on
                msgObj.msgobj = 1;
                msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400 | shortId;
                msgObj.mask = 0x1FFFFFFF;
                LPC_CCAN_API->config_rxmsgobj(&msgObj);
                msgObj.msgobj = 2;
                LPC_CCAN_API->config_rxmsgobj(&msgObj);
                msgObj.msgobj = 3;
                LPC_CCAN_API->config_rxmsgobj(&msgObj);

                return true;
            }
        }
        chThdSleepMilliseconds(1);
    }
    return false;
}

static bool download (uint8_t page) {
    // send out a request to receive a 4 KB page
    CCAN_MSG_OBJ_T msgObj;    
    msgObj.msgobj  = 11;
    msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400;
    // msgObj.mode_id = 0x321;
    msgObj.mask    = 0x0;
    msgObj.dlc     = 3;
    msgObj.data[0] = 1;         // boot request for a 4 KB page download
    msgObj.data[1] = shortId;   // own id
    msgObj.data[2] = page;      // page index (1..7 for LPC11C24)
    LPC_CCAN_API->can_transmit(&msgObj);
    
    // wait for entire page to come in, as 8-byte messages
    rxMsg.msgobj = 0;
    uint8_t *p = codeBuf;
    for (int i = 0; i < 100; ++i) {
        if (rxMsg.msgobj) {
            rxMsg.msgobj = 0;
            if (rxMsg.dlc == 8) {
                // BLINK();
                memcpy(p, rxMsg.data, 8);
                p += 8;
                if (p >= codeBuf + sizeof codeBuf)
                    return true; // page reception completed
            }
            i = 0; // restart timeout
        } else
            chThdSleepMilliseconds(1);
    }
    // download ends when messages are not received within 100 ms of each other
    return false;
}

static void saveToFlash (uint8_t page) {
    iapCall(PREP_WRITE, page, page, 0, 0);
    iapCall(ERASE_SECT, page, page, 0, 0);
    iapCall(PREP_WRITE, page, page, 0, 0);
    iapCall(COPY_TO_FLASH, page * sizeof codeBuf,
            (uint32_t) codeBuf, sizeof codeBuf, 48000);
}

int main () {
    halInit();
    chSysInit();
    canBusInit();

    memcpy(myUid, iapCall(READ_UID, 0, 0, 0, 0), sizeof myUid);
    
    if (bootCheck() > 0) {
        for (uint8_t page = 1; download(page); ++page) {
            BLINK();
            saveToFlash(page);
            BLINK();
        }
    }
    
    for (;;) {
        BLINK();
        chThdSleepMilliseconds(1000);
    }

    return 0;
}
