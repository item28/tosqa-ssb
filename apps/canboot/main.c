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

#define BOOT_ADDR_BASE  (CAN_MSGOBJ_EXT | 0x1F123400)

// #define BLINK() (LPC_GPIO2->DATA ^= 1<<10) // LPC11C24-DK-A
// #define BLINK() palTogglePad(GPIO2, 10) // LPC11C24-DK-A
// #define BLINK() palTogglePad(GPIO0, 7) // LPCxpresso 11C24
#define BLINK()

CCAN_MSG_OBJ_T rxMsg;
uint32_t       myUid [4];
uint8_t        codeBuf [4096] __attribute__((aligned(4)));
uint8_t        shortId;
volatile int   ready;
int            blinkRate;

static void CAN_rxCallback (uint8_t msg_obj_num) {
    rxMsg.msgobj = msg_obj_num;
    LPC_CCAN_API->can_receive(&rxMsg);
    ready = 1;
}

static CCAN_CALLBACKS_T callbacks = {
    CAN_rxCallback, 0, 0, 0, 0, 0, 0, 0,
};

static void canBusInit (void) {
    LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 17; // SYSCTL_CLOCK_CAN

    static uint32_t clkInitTable[2] = { // see common/canRate.c
        0x00000000UL,                   // CANCLKDIV, running at 12 MHz
        0x000054C1UL                    // CAN_BTR, produces 500 Kbps
    };

    LPC_CCAN_API->init_can(clkInitTable, 0);
    LPC_CCAN_API->config_calb(&callbacks);
}

#define PREP_WRITE      50
#define COPY_TO_FLASH   51
#define ERASE_SECT      52
#define READ_UID        58

static const uint32_t* iapCall(uint32_t type, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    static uint32_t results[5];
    uint32_t params [5] = { type, a, b, c, d };
    iap_entry((unsigned*) params, (unsigned*) results);
    if (results[0] != 0)
        blinkRate = 100;
    return results[0] == 0 ? results + 1 : 0;
}

// there's a single "boot configuration byte" at the end of the boot flash area
static uint8_t bootConfigByte (void) {
    return 0x01;
    return *(const uint8_t*) 0x0FFF; // last byte of first 4 KB page in flash
}

// there is a single CAN bus address to which all boot requests are sent
// the address sent to includes bits 6..0 of the boot configuration byte
static int bootRequestAddr (void) {
    return BOOT_ADDR_BASE | 0x80 | bootConfigByte();
}

// send first 8 bytes of this chip's UID out to BOOT_ADDR_BASE
static int bootCheck (void) {
    CCAN_MSG_OBJ_T msgObj;
    
    // send own uid to a fixed CAN bus address to request a unique 1..127 id
    msgObj.msgobj  = 10;
    msgObj.mode_id = bootRequestAddr();
    msgObj.mask    = 0x0;
    msgObj.dlc     = 8;
    memcpy(msgObj.data, myUid, 8);
    LPC_CCAN_API->can_transmit(&msgObj);

    // configure msg object 1 to receive messages to BOOT_ADDR_BASE + 0..127
    // the lower 7 bits of the reply address will be the assigned unique id
    msgObj.msgobj = 1;
    msgObj.mode_id = BOOT_ADDR_BASE;
    msgObj.mask = 0x1FFFFF80;
    LPC_CCAN_API->config_rxmsgobj(&msgObj);

    // wait briefly to get a reply
    rxMsg.msgobj = 0;
    int i;
    for (i = 0; i < 50000; ++i) { // empirical value, tries for about 250 ms
        LPC_CCAN_API->isr();
        if (ready) {
            ready = 0;
            if (rxMsg.dlc == 8 && memcmp(rxMsg.data, myUid, 8) == 0) {
                shortId = rxMsg.mode_id; // use lower 7 bits of address as id

                // only listen to the assigned address from now on
                msgObj.msgobj = 1;
                msgObj.mode_id = BOOT_ADDR_BASE | shortId;
                msgObj.mask = 0x1FFFFFFF;
                LPC_CCAN_API->config_rxmsgobj(&msgObj);
                msgObj.msgobj = 2;
                LPC_CCAN_API->config_rxmsgobj(&msgObj);
                msgObj.msgobj = 3;
                LPC_CCAN_API->config_rxmsgobj(&msgObj);

                return 1;
            }
        }
    }
    return 0;
}

static int download (uint8_t page) {
    // send out a request to receive a 4 KB page
    CCAN_MSG_OBJ_T msgObj;    
    msgObj.msgobj  = 11;
    msgObj.mode_id = bootRequestAddr();
    msgObj.mask    = 0x0;
    msgObj.dlc     = 2;
    msgObj.data[0] = shortId;   // this unit's own assigned id
    msgObj.data[1] = page;      // page index (1..7 for LPC11C24)
    LPC_CCAN_API->can_transmit(&msgObj);
    
    // wait for entire page to come in, as 8-byte messages
    rxMsg.msgobj = 0;
    uint8_t *p = codeBuf;
    int timer;
    for (timer = 0; timer < 50000; ++timer) {
        LPC_CCAN_API->isr();
        if (ready) {
            ready = 0;
            if (rxMsg.dlc == 8) {
                memcpy(p, rxMsg.data, 8);
                p += 8;
                if (p >= codeBuf + sizeof codeBuf)
                    return 1; // page reception completed
                timer = 0; // reset the timeout timer
            }
        }
    }
    // download ends when messages are not received within a certain time
    return 0;
}

static void saveToFlash (uint8_t page) {
    iapCall(PREP_WRITE, page, page, 0, 0);
    iapCall(ERASE_SECT, page, page, 12000, 0);
    iapCall(PREP_WRITE, page, page, 0, 0);
    iapCall(COPY_TO_FLASH, page * sizeof codeBuf,
            (uint32_t) codeBuf, sizeof codeBuf, 12000);
}

int main (void) {
    // no clock setup, running on IRC @ 12 MHz
    
    LPC_GPIO2->DIR |= 1<<10;
    blinkRate = 1000;

    canBusInit();
    
    memcpy(myUid, iapCall(READ_UID, 0, 0, 0, 0), sizeof myUid);
    
    if (bootConfigByte() & 0x80) {
        // self-starting mode bit set, only wait up to one second
        int i;
        for (i = 0; i < 4; ++i)
            if (bootCheck())
                break;
    } else {
        // wait indefinitely for a reply from the boot master
        while (!bootCheck())
            ;
    }
    
    // only perform download check if an id has been given by the boot master
    if (shortId > 0) {
        uint8_t page = 0;
        while (download(++page)) {
            BLINK();
            saveToFlash(page);
            BLINK();
        }
    }
    
    // set stack pointer
    asm volatile("ldr r0, =0x1000");
    asm volatile("ldr r0, [r0]");
    asm volatile("mov sp, r0");
    // jump to start address
    asm volatile("ldr r0, =0x1004");
    asm volatile("ldr r0, [r0]");
    asm volatile("mov pc, r0");

    return 0;
}
