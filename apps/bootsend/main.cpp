// boot send - send firmware via CAN @ 100 KHz using CANopen
// jcw, 2014-04-14

#include <string.h>

#include "LPC11xx.h"
#include "data.h"

#define IAP_ENTRY_LOCATION      0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC    0x1FFF1FF8
#define LPC_ROM_API             (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#define INLINE inline

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

CCAN_MSG_OBJ_T rxMsg, txMsg;
bool           ready;
uint8_t        codeBuf [4096] __attribute__((aligned(4)));
uint16_t       error;

static void delay (int ms) {
    volatile int i;
    while (--ms >= 0)
        for (i = 0; i < 3000; ++i)
            ;
}

// display N brief blips followed by a long pause
static void blinkLed (int count) {
    // disable CAN so that it no longer interferes with other CAN traffic
    LPC_SYSCON->SYSAHBCLKCTRL &= ~(1<<17); // SYSCTL_CLOCK_CAN

    if (count > 0) // display count error blips
        for (;;) {
            for (int i = 0; i < count; ++i) {
                LPC_GPIO0->DATA ^= 1<<7;
                delay(50);
                LPC_GPIO0->DATA ^= 1<<7;
                delay(250);
            }
            delay(2000);
        }
    else // no error, display a steady 0.5 Hz blink
        for (;;) {
            LPC_GPIO0->DATA ^= 1<<7;
            delay(1000);
        }
}

static void CAN_rxCallback (uint8_t msg_obj_num) {
    rxMsg.msgobj = msg_obj_num;
    LPC_CCAN_API->can_receive(&rxMsg);
    ready = true;
}

static void CAN_errorCallback (uint32_t error_info) {
    error |= error_info;
}

static CCAN_CALLBACKS_T callbacks = {
    CAN_rxCallback,
    NULL,
    CAN_errorCallback,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static void initCan () {
    LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 17; // SYSCTL_CLOCK_CAN

    static uint32_t clkInitTable[2] = {
        0x00000000UL, // CANCLKDIV, running at 48 MHz
        // 0x000076C5UL  // CAN_BTR, 500 Kbps CAN bus rate
        0x000076DDUL    // CAN_BTR, 100 Kbps CAN bus rate
    };

    LPC_CCAN_API->init_can(clkInitTable, false);
    LPC_CCAN_API->config_calb(&callbacks);
}

static void sendAndWait(uint8_t header, int length) {
    txMsg.mode_id = 0x67D;
    txMsg.mask = 0x00;
    txMsg.dlc = length;
    txMsg.msgobj = 8;
    txMsg.data[0] = header;

    ready = false;
    LPC_CCAN_API->can_transmit(&txMsg);
    for (int i = 0; i < 100000; ++i) {
        LPC_CCAN_API->isr();
        if (ready)
            return;
    }
    blinkLed(2);
}

static void sendReq(uint8_t header, uint16_t index, uint8_t subindex, const uint8_t* data, int length) {
    txMsg.data[1] = index;
    txMsg.data[2] = index >> 8;
    txMsg.data[3] = subindex;
    for (int i = 0; i < 4; ++i)
        txMsg.data[4+i] = i < length ? *data++ : 0;
    sendAndWait(header, 8);
    
    if (txMsg.data[7] & 0x08)
        blinkLed(3);
}

void sdoWriteExpedited(uint16_t index, uint8_t subindex, const uint32_t data, uint8_t length) {
    sendReq((1<<5) | ((4-length)<<2) | 0b11, index, subindex, (uint8_t*) &data, length);
}

uint32_t sdoReadSegmented(uint16_t index, uint8_t subindex) {
    uint32_t zero = 0;
    sendReq(0x40, index, subindex, (uint8_t*) &zero, 4);
    return (rxMsg.data[7] << 24) | (rxMsg.data[6] << 16) |
            (rxMsg.data[5] << 8) | rxMsg.data[4];
}

void sdoWriteSegmented(uint16_t index, uint8_t subindex, int length) {
    sendReq(0x21, index, subindex, (uint8_t*) &length, 4);
}

void sdoWriteDataSegment(uint8_t header, const uint8_t* data, int length) {
    for (int i = 0; i < 7; ++i)
        txMsg.data[1+i] = i < length ? *data++ : 0;
    sendAndWait(header, 8);
}

// 3 = P0_3/P0_5, 2 = P0_4/P3_3, 1 = P1_10/P1_11, 0 = P2_3/P2_6
// 7 = P0_7/P2_0, 6 = P2_1/P2_2, 5 = P0_11/P1_0, 4 = P1_1/P1_2

// the boot configuration is set with jumpers placed between different I/O pins
// see http://jeelabs.net/projects/tosca/wiki/Boot_installer
static uint8_t getBootConfigByte () {
    // avoid special pin modes, use GPIO for PIO0_11 and PIO1_1
    LPC_IOCON->R_PIO0_11 = 0xD1;
    LPC_IOCON->R_PIO1_1 = 0xD1;
    // inputs: P0_3, P1_0, P1_2, P1_11, P2_0, P2_2, P2_6, P3_3 
    LPC_GPIO0->DIR &= ~(1<<3);
    LPC_GPIO1->DIR &= ~(1<<0) & ~(1<<2) & ~(1<<11);
    LPC_GPIO2->DIR &= ~(1<<0) & ~(1<<2) & ~(1<<6);
    LPC_GPIO3->DIR &= ~(1<<3);
    // outputs: P0_4, P0_5, P0_7, P0_11, P1_1, P1_10, P2_1, P2_3
    LPC_GPIO0->DIR |= (1<<4) | (1<<5) | (1<<7) | (1<<11);
    LPC_GPIO1->DIR |= (1<<1) | (1<<10);
    LPC_GPIO2->DIR |= (1<<1) | (1<<3);
    // set all outputs low
    LPC_GPIO0->DATA &= ~(1<<4) & ~(1<<5) & ~(1<<7) & ~(1<<11);
    LPC_GPIO1->DATA &= ~(1<<1) & ~(1<<10);
    LPC_GPIO2->DATA &= ~(1<<1) & ~(1<<3);
    // read the input bits - will be high when not jumpered, due to pull-ups
    uint8_t b = 0;
    if (LPC_GPIO2->DATA & (1<<0))  b |= 1<<7;
    if (LPC_GPIO2->DATA & (1<<2))  b |= 1<<6;
    if (LPC_GPIO1->DATA & (1<<0))  b |= 1<<5;
    if (LPC_GPIO1->DATA & (1<<2))  b |= 1<<4;
    if (LPC_GPIO0->DATA & (1<<3))  b |= 1<<3;
    if (LPC_GPIO3->DATA & (1<<3))  b |= 1<<2;
    if (LPC_GPIO1->DATA & (1<<11)) b |= 1<<1;
    if (LPC_GPIO2->DATA & (1<<6))  b |= 1<<0;

    return ~b; // jumper set means pulled low, but should be returned as "1"
}

int main () {
    LPC_GPIO0->DIR |= 1<<7;     // LED is PIO0_7 on the LPCxpresso 11C24
    LPC_GPIO0->DATA &= ~(1<<7); // turn LED off

    delay(1000); // wait 1s after power-up before uploading to target

    initCan();

    /* Configure message object 1 to receive all 11-bit messages 0x5FD */
    static CCAN_MSG_OBJ_T msg;
    msg.msgobj = 1;
    msg.mode_id = 0x5FD;
    msg.mask = 0x7FF;
    LPC_CCAN_API->config_rxmsgobj(&msg);

    // read device type ("LPC1")
    uint32_t devType = sdoReadSegmented(0x1000, 0);
    if (memcmp(&devType, "LPC1", 4) != 0)
        blinkLed(4);

    // read 16-byte serial number
    // uint32_t uid[4];
    // uid[0] = sdoReadSegmented(0x5100, 1);
    // uid[1] = sdoReadSegmented(0x5100, 2);
    // uid[2] = sdoReadSegmented(0x5100, 3);
    // uid[3] = sdoReadSegmented(0x5100, 4);
    
    // unlock for upload
    uint16_t unlock = 23130;
    sdoWriteExpedited(0x5000, 0, unlock, sizeof unlock);    
    // prepare sectors
    sdoWriteExpedited(0x5020, 0, 0x0000, 2);
    // erase sectors
    sdoWriteExpedited(0x5030, 0, 0x0000, 2);

    if (getBootConfigByte() != 0) {
        // copy code to RAM buffer and add the boot config byte
        memset(codeBuf, 0xFF, sizeof codeBuf);
        memcpy(codeBuf, bootData, sizeof bootData);
        codeBuf[sizeof codeBuf-1] = getBootConfigByte();

        // start write to RAM
        sdoWriteExpedited(0x5015, 0, 0x10000800, 4); // ram address
        // upload code to RAM
        sdoWriteSegmented(0x1F50, 1, 0x1000);
        uint8_t toggle = 0;
        for (size_t i = 0; i < sizeof codeBuf; i += 7) {
            sdoWriteDataSegment(toggle, codeBuf + i, 7);
            toggle ^= 0x10;
        }

        // prepare sectors
        sdoWriteExpedited(0x5020, 0, 0x0000, 2);
        // copy ram to flash
        sdoWriteExpedited(0x5050, 1, 0x00000000, 4);    // flash address
        sdoWriteExpedited(0x5050, 2, 0x10000800, 4);    // ram address
        sdoWriteExpedited(0x5050, 3, 0x1000, 2);        // 4096 bytes
    }
    
    // G 0000
    sdoWriteExpedited(0x5070, 0, 0, 4);
    sdoWriteExpedited(0x1F51, 1, 1, 1);

    // 5 quick blips if there has been any CAN error, or steady blink if all ok
    blinkLed(error ? 5 : 0);

    return 0;
}
