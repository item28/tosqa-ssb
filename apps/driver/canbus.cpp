// interface to the CAN bus hardware

// ROM-based CAN bus drivers
#define IAP_ENTRY_LOCATION        0X1FFF1FF1
#define LPC_ROM_API_BASE_LOC      0x1FFF1FF8
#define LPC_ROM_API               (*(LPC_ROM_API_T**) LPC_ROM_API_BASE_LOC)

#include "nxp/romapi_11xx.h"
#include "nxp/ccand_11xx.h"

// internal state
static struct {
    Mailbox        rxPending;
    msg_t          rxBuffer[1];
    CCAN_MSG_OBJ_T rxMessage;
    int            rxOverruns;
    int            errorCount;
} canBus;

static void CAN_rxCallback (uint8_t msg_obj_num) {
    CCAN_MSG_OBJ_T msgObj;
    msgObj.msgobj = msg_obj_num;
    LPC_CCAN_API->can_receive(&msgObj);
    
    chSysLockFromIsr();
    if (chMBPostI(&canBus.rxPending, 0) == RDY_OK)
        canBus.rxMessage = msgObj;
    else
        ++canBus.rxOverruns; // rx packet lost, no room in message queue
    chSysUnlockFromIsr();
}

static void CAN_txCallback (uint8_t /* msg_obj_num */) {}

static void CAN_errorCallback (uint32_t error_info) {
    if (error_info & 0x0002)
        ++canBus.errorCount;
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
CH_IRQ_HANDLER(Vector74)  {
    CH_IRQ_PROLOGUE();
    LPC_CCAN_API->isr();
    CH_IRQ_EPILOGUE();
}

void canBusInit () {
    chMBInit(&canBus.rxPending, canBus.rxBuffer, 1);

    LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 17; // SYSCTL_CLOCK_CAN

    static uint32_t clkInitTable[2] = { // see common/canRate.c
        0x00000000UL,                   // CANCLKDIV, running at 48 MHz
        0x000076C5UL                    // CAN_BTR, produces 500 Kbps
    };

    LPC_CCAN_API->init_can(clkInitTable, true);
    LPC_CCAN_API->config_calb(&callbacks);
    NVIC_EnableIRQ(CAN_IRQn);

    // Configure message object 1 to receive all 11-bit messages 0x420-0x423
    CCAN_MSG_OBJ_T msg_obj;
    msg_obj.msgobj = 1;
    msg_obj.mode_id = 0x420;
    msg_obj.mask = 0x7FC;
    LPC_CCAN_API->config_rxmsgobj(&msg_obj);
}
