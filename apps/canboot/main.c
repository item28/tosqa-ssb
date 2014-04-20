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
        for (i = 0; i < 750; ++i)
            ;
}

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
    for (i = 0; i < 100000; ++i) {
        LPC_CCAN_API->isr();
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
    for (timer = 0; timer < 250000; ++timer) {
        LPC_CCAN_API->isr();
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
            (uint32_t) codeBuf, sizeof codeBuf, 12000);
}

int main (void) {
    // no clock setup, running on IRC @ 12 MHz
    // __disable_irq();
    
    LPC_GPIO2->DIR |= 1<<10;
    blinkRate = 1000;

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

void NMIVector(void) __attribute__ (( naked ));
void HardFaultVector(void) __attribute__ (( naked ));
void MemManageVector(void) __attribute__ (( naked ));
void BusFaultVector(void) __attribute__ (( naked ));
void UsageFaultVector(void) __attribute__ (( naked ));
void SVCallVector(void) __attribute__ (( naked ));
void DebugMonitorVector(void) __attribute__ (( naked ));
void PendSVVector(void) __attribute__ (( naked ));
void SysTickVector(void) __attribute__ (( naked ));

void NMIVector (void) {
	asm volatile("ldr r0, =0x1008");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void HardFaultVector (void) {
	asm volatile("ldr r0, =0x100C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void MemManageVector (void) {
	asm volatile("ldr r0, =0x1010");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void BusFaultVector (void) {
	asm volatile("ldr r0, =0x1014");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void UsageFaultVector (void) {
	asm volatile("ldr r0, =0x1018");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void SVCallVector (void) {
	asm volatile("ldr r0, =0x102C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void DebugMonitorVector (void) {
	asm volatile("ldr r0, =0x1030");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void PendSVVector (void) {
	asm volatile("ldr r0, =0x1038");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void SysTickVector (void) {
	asm volatile("ldr r0, =0x103C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector20(void) __attribute__ (( naked ));
void Vector24(void) __attribute__ (( naked ));
void Vector28(void) __attribute__ (( naked ));
void Vector34(void) __attribute__ (( naked ));

void Vector20 (void) {
	asm volatile("ldr r0, =0x1020");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector24 (void) {
	asm volatile("ldr r0, =0x1024");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector28 (void) {
	asm volatile("ldr r0, =0x1028");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector34 (void) {
	asm volatile("ldr r0, =0x1034");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector40(void) __attribute__ (( naked ));
void Vector44(void) __attribute__ (( naked ));
void Vector48(void) __attribute__ (( naked ));
void Vector4C(void) __attribute__ (( naked ));
void Vector50(void) __attribute__ (( naked ));
void Vector54(void) __attribute__ (( naked ));
void Vector58(void) __attribute__ (( naked ));
void Vector5C(void) __attribute__ (( naked ));
void Vector60(void) __attribute__ (( naked ));
void Vector64(void) __attribute__ (( naked ));
void Vector68(void) __attribute__ (( naked ));
void Vector6C(void) __attribute__ (( naked ));
void Vector70(void) __attribute__ (( naked ));
void Vector74(void) __attribute__ (( naked ));
void Vector78(void) __attribute__ (( naked ));
void Vector7C(void) __attribute__ (( naked ));
void Vector80(void) __attribute__ (( naked ));
void Vector84(void) __attribute__ (( naked ));
void Vector88(void) __attribute__ (( naked ));
void Vector8C(void) __attribute__ (( naked ));
void Vector90(void) __attribute__ (( naked ));
void Vector94(void) __attribute__ (( naked ));
void Vector98(void) __attribute__ (( naked ));
void Vector9C(void) __attribute__ (( naked ));
void VectorA0(void) __attribute__ (( naked ));
void VectorA4(void) __attribute__ (( naked ));
void VectorA8(void) __attribute__ (( naked ));
void VectorAC(void) __attribute__ (( naked ));
void VectorB0(void) __attribute__ (( naked ));
void VectorB4(void) __attribute__ (( naked ));
void VectorB8(void) __attribute__ (( naked ));
void VectorBC(void) __attribute__ (( naked ));

void Vector40 (void) {
	asm volatile("ldr r0, =0x1040");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector44 (void) {
	asm volatile("ldr r0, =0x1044");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector48 (void) {
	asm volatile("ldr r0, =0x1048");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector4C (void) {
	asm volatile("ldr r0, =0x104C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector50 (void) {
	asm volatile("ldr r0, =0x1050");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector54 (void) {
	asm volatile("ldr r0, =0x1054");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector58 (void) {
	asm volatile("ldr r0, =0x1058");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector5C (void) {
	asm volatile("ldr r0, =0x105C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector60 (void) {
	asm volatile("ldr r0, =0x1060");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector64 (void) {
	asm volatile("ldr r0, =0x1064");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector68 (void) {
	asm volatile("ldr r0, =0x1068");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector6C (void) {
	asm volatile("ldr r0, =0x106C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector70 (void) {
	asm volatile("ldr r0, =0x1070");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector74 (void) {
	asm volatile("ldr r0, =0x1074");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector78 (void) {
	asm volatile("ldr r0, =0x1078");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector7C (void) {
	asm volatile("ldr r0, =0x107C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector80 (void) {
	asm volatile("ldr r0, =0x1080");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector84 (void) {
	asm volatile("ldr r0, =0x1084");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector88 (void) {
	asm volatile("ldr r0, =0x1088");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector8C (void) {
	asm volatile("ldr r0, =0x108C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector90 (void) {
	asm volatile("ldr r0, =0x1090");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector94 (void) {
	asm volatile("ldr r0, =0x1094");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector98 (void) {
	asm volatile("ldr r0, =0x1098");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void Vector9C (void) {
	asm volatile("ldr r0, =0x109C");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorA0 (void) {
	asm volatile("ldr r0, =0x10A0");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorA4 (void) {
	asm volatile("ldr r0, =0x10A4");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorA8 (void) {
	asm volatile("ldr r0, =0x10A8");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorAC (void) {
	asm volatile("ldr r0, =0x10AC");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorB0 (void) {
	asm volatile("ldr r0, =0x10B0");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorB4 (void) {
	asm volatile("ldr r0, =0x10B4");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorB8 (void) {
	asm volatile("ldr r0, =0x10B8");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}

void VectorBC (void) {
	asm volatile("ldr r0, =0x10BC");
	asm volatile("ldr r0, [r0]");
	asm volatile("mov pc, r0");
}
