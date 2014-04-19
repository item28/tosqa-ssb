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
    msg_t          rxBuffer [1];
    CCAN_MSG_OBJ_T rxMessage;
    int            rxOverruns;
    int            errorCount;
    uint32_t       myUid [4];
} canBus;

// configure message object 1 to receive all 11-bit messages addr..addr+7
static void listenAddressRange (int addr) {
    CCAN_MSG_OBJ_T msgObj;
    msgObj.msgobj = 1;
    msgObj.mode_id = addr;
    msgObj.mask = 0x7FC;
    LPC_CCAN_API->config_rxmsgobj(&msgObj);
}

static void CAN_rxCallback (uint8_t msg_obj_num) {
    CCAN_MSG_OBJ_T msgObj;
    msgObj.msgobj = msg_obj_num;
    LPC_CCAN_API->can_receive(&msgObj);
    
    switch (msg_obj_num) {
        case 1: // incoming request
            chSysLockFromIsr();
            if (chMBPostI(&canBus.rxPending, 0) == RDY_OK)
                canBus.rxMessage = msgObj;
            else
                ++canBus.rxOverruns; // rx packet lost, no room in msg queue
            chSysUnlockFromIsr();
            break;
        case 2: // listen address change request
            // if uid matches, adjust listening address range as instructed
            // the lower 8 bits of the msg id are bits 10..3 to listen to
            // this uses extended addressing, so there are enough spare bits
            if (canBus.rxMessage.dlc == 8 &&
                    memcmp(msgObj.data, canBus.myUid, 8) == 0)
                listenAddressRange((msgObj.mode_id & 0xFF) << 3);
            break;
        default:
            ++canBus.errorCount;
    }
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

    // configure message object 1 to receive all 11-bit messages 0x420-0x423
    listenAddressRange(0x420);

    // configure message object 2 to receive 29-bit messages to 0x1F1234xx
    CCAN_MSG_OBJ_T msgObj;    
    msgObj.msgobj = 2;
    msgObj.mode_id = CAN_MSGOBJ_EXT | 0x1F123400;
    msgObj.mask = 0x1FFFFF00;
    LPC_CCAN_API->config_rxmsgobj(&msgObj);

    readUid(canBus.myUid);

    // send first 8 bytes of this chip's UID out to 0x1F123210
    CCAN_MSG_OBJ_T txMsg;    
    txMsg.msgobj  = 20;
    txMsg.mode_id = CAN_MSGOBJ_EXT | 0x1F123210;
    txMsg.mask    = 0x0;
    txMsg.dlc     = 8;
    memcpy(txMsg.data, canBus.myUid, 8);
    LPC_CCAN_API->can_transmit(&txMsg);
}
