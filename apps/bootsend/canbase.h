static void CAN_rxCallback (uint8_t msg_obj_num) {
  msg_obj.msgobj = msg_obj_num;
  LPC_CCAN_API->can_receive(&msg_obj);
  palTogglePad(GPIO3, GPIO3_MOTOR_EN); // LED0 on Open11C14 board
}

static void CAN_txCallback (uint8_t /* msg_obj_num */) {}

static void CAN_errorCallback (uint32_t error_info) {
  if (error_info & 0x0002)
    palTogglePad(GPIO3, GPIO3_MOTOR_SLEEP); // LED3 on Open11C14 board
}

extern "C" void Vector74 ();
CH_IRQ_HANDLER(Vector74)  {
  CH_IRQ_PROLOGUE();
  LPC_CCAN_API->isr();
  CH_IRQ_EPILOGUE();
}

static uint32_t clkInitTable[2] = {
  0x00000000UL, // CANCLKDIV, running at 48 MHz
  // 0x000076C5UL  // CAN_BTR, 500 Kbps CAN bus rate
  0x000076DDUL  // CAN_BTR, 100 Kbps CAN bus rate
};

static CCAN_CALLBACKS_T callbacks = {
  CAN_rxCallback,
  CAN_txCallback,
  CAN_errorCallback,
	CANopen_SDOS_Exp_Read,	/* callback for expedited read access */
	CANopen_SDOS_Exp_Write,	/* callback for expedited write access */
	CANopen_SDOS_Seg_Read,	/* callback for segmented read access */
	CANopen_SDOS_Seg_Write,	/* callback for segmented write access */
  NULL,
};

volatile uint8_t SDOS_2200_Data[255] = "Boot-up value of SDO 2200h";

typedef struct _SDOS_Buffer_t {
	uint8_t* data;					/* pointer to buffer */
	uint32_t length;				/* length in buffer */
} SDOS_Buffer_t;

/* Application variables used in variable OD */
uint8_t  error_register;
uint32_t BlinkPattern;
uint32_t StepControl;

volatile SDOS_Buffer_t SDOS_2200 = {
  (uint8_t*)SDOS_2200_Data,
  sizeof(SDOS_2200_Data),
};

/* CANopen read-only (constant) Object Dictionary (OD) entries
   Used with Expedited SDO only. Lengths = 1/2/4 bytes */
static CCAN_ODCONSTENTRY myConstOD [] = {
  /* idx, subidx, len, value */
  { 0x1000, 0x00, 4, 0x00000000UL },
  { 0x1018, 0x00, 1, 0x00000001UL },     /* only vendor ID is specified */
  { 0x1018, 0x01, 4, 0x000002DCUL },     /* NXP vendor ID for CANopen */
};

const uint32_t myConstODsize = sizeof(myConstOD)/sizeof(myConstOD[0]);

/* CANopen list of variable Object Dictionary (OD) entries
   Expedited SDO with length=1/2/4 bytes and segmented SDO */
static CCAN_ODENTRY myOD [] = {
/* idx, subidx, access_type | len, value_pointer */
  { 0x1001, 0x00, OD_EXP_RO | 1, (uint8_t*) &error_register },
  { 0x2000, 0x00, OD_EXP_RW | 4, (uint8_t*) &BlinkPattern},
  { 0x2100, 0x00, OD_EXP_RW | 4, (uint8_t*) &StepControl},
  { 0x2200, 0x00, OD_SEG_RW,     (uint8_t*) &SDOS_2200},
};

const uint32_t myODsize = sizeof(myOD)/sizeof(myOD[0]);

#define CAN_NODE_ID 1

/* CANopen configuration structure */
static CCAN_CANOPENCFG myCANopen = {
	CAN_NODE_ID,					/* node_id */
	1,										/* msgobj_rx */
	2,										/* msgobj_tx */
	1,										/* isr_handled */
	myConstODsize,
	myConstOD,
	myODsize,
	myOD,
};

static void initCan () {
  LPC_SYSCON->SYSAHBCLKCTRL |= 1 << 17; // SYSCTL_CLOCK_CAN

  LPC_CCAN_API->init_can(clkInitTable, true);
  LPC_CCAN_API->config_calb(&callbacks);
  LPC_CCAN_API->config_canopen(&myCANopen);  
  
  NVIC_EnableIRQ(CAN_IRQn);
}
