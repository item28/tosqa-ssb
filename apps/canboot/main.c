// bootstrap loader via the CAN bus, using Tosqa conventions
// jcw, 2014-04-19

#include "LPC11xx.h"
#include <string.h>

// ROM-based CAN bus drivers
#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#define INLINE inline

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

#define BLINK() (LPC_GPIO2->DATA ^= 1<<10) // LPC11C24-DK-A
// #define BLINK() palTogglePad(GPIO2, 10) // LPC11C24-DK-A
// #define BLINK() palTogglePad(GPIO0, 7) // LPCxpresso 11C24
// #define BLINK()

#define DELAY(ms) delay(ms)

static void delay (int ms) {
    volatile int i;
    while (--ms >= 0)
        for (i = 0; i < 3000; ++i)
            ;
}

CCAN_MSG_OBJ_T rxMsg;
uint32_t       myUid [4];
uint8_t        codeBuf [4096] __attribute__((aligned(4)));
uint8_t        shortId;
volatile int   ready;
int            blinkRate = 1000;

static void CAN_rxCallback (uint8_t msg_obj_num) {
    rxMsg.msgobj = msg_obj_num;
    LPC_CCAN_API->can_receive(&rxMsg);
    ready = 1;
}

static CCAN_CALLBACKS_T callbacks = {
    CAN_rxCallback, 0, 0, 0, 0, 0, 0, 0,
};

void Vector74 (void) {
    LPC_CCAN_API->isr();
}

static void canBusInit (void) {
    LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 17; // SYSCTL_CLOCK_CAN

    static uint32_t clkInitTable[2] = { // see common/canRate.c
        0x00000000UL,                   // CANCLKDIV, running at 48 MHz
        0x000076C5UL                    // CAN_BTR, produces 500 Kbps
    };

    LPC_CCAN_API->init_can(clkInitTable, 1);
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
    __disable_irq();
    iap_entry((unsigned*) params, (unsigned*) results);
    __enable_irq();
    if (results[0] != 0)
        blinkRate = 100;
    return results[0] == 0 ? results + 1 : 0;
}

// send first 8 bytes of this chip's UID out to 0x1F123400
static int bootCheck (void) {
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
    int i;
    for (i = 0; i < 1000000; ++i) {
        if (ready) {
            ready = 0;
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

                return 1;
            }
        }
        // DELAY(1);
    }
    return 0;
}

static int download (uint8_t page) {
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
    int timer;
    for (timer = 0; timer < 2500000; ++timer) {
        if (ready) {
            ready = 0;
            if (rxMsg.dlc == 8) {
                // BLINK();
                memcpy(p, rxMsg.data, 8);
                p += 8;
                if (p >= codeBuf + sizeof codeBuf)
                    return 1; // page reception completed
            }
            timer = 0; // reset the timeout timer
        }
    }
    // download ends when messages are not received within 100 ms of each other
    return 0;
}

static void saveToFlash (uint8_t page) {
    iapCall(PREP_WRITE, page, page, 0, 0);
    iapCall(ERASE_SECT, page, page, 0, 0);
    iapCall(PREP_WRITE, page, page, 0, 0);
    iapCall(COPY_TO_FLASH, page * sizeof codeBuf,
            (uint32_t) codeBuf, sizeof codeBuf, 48000);
}

// void SysTickVector () __attribute__((naked));
// void SysTickVector (void) {
//     asm volatile("ldr r0, =0x103C");
//     asm volatile("ldr r0, [r0]");
//     asm volatile("mov pc, r0");
// }

int main (void) {
    LPC_GPIO2->DIR |= (1 << 10);

    canBusInit();

    memcpy(myUid, iapCall(READ_UID, 0, 0, 0, 0), sizeof myUid);
    
    while (!bootCheck())
        DELAY(1000);

    uint8_t page = 0;
    while (download(++page)) {
        BLINK();
        saveToFlash(page);
        BLINK();
    }
    
    if (page > 1) {
        // set stack pointer
        asm volatile("ldr r0, =0x1000");
        asm volatile("ldr r0, [r0]");
        asm volatile("mov sp, r0");
        // jump to start address
        asm volatile("ldr r0, =0x1004");
        asm volatile("ldr r0, [r0]");
        asm volatile("mov pc, r0");
    }
    
    for (;;) {
        BLINK();
        DELAY(blinkRate);
    }

    return 0;
}
