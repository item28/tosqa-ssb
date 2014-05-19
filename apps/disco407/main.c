/*
    ChibiOS/RT - Copyright (C) 2006-2013 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string.h>

#include "ch.h"
#include "hal.h"
#include "chprintf.h"

// USB related stuff

// Endpoints to be used for USBD1.
#define USBD1_DATA_REQUEST_EP           1
#define USBD1_DATA_AVAILABLE_EP         1
#define USBD1_INTERRUPT_REQUEST_EP      2

// Serial over USB Driver structure.
static SerialUSBDriver SDU1;
BaseSequentialStream *serial = (BaseSequentialStream*)&SDU1;
  
// USB Device Descriptor.
static const uint8_t vcom_device_descriptor_data[18] = {
  USB_DESC_DEVICE       (0x0110,        /* bcdUSB (1.1).                    */
                         0x02,          /* bDeviceClass (CDC).              */
                         0x00,          /* bDeviceSubClass.                 */
                         0x00,          /* bDeviceProtocol.                 */
                         0x40,          /* bMaxPacketSize.                  */
                         0x0483,        /* idVendor (ST).                   */
                         0x5740,        /* idProduct.                       */
                         0x0200,        /* bcdDevice.                       */
                         1,             /* iManufacturer.                   */
                         2,             /* iProduct.                        */
                         3,             /* iSerialNumber.                   */
                         1)             /* bNumConfigurations.              */
};

// Device Descriptor wrapper.
static const USBDescriptor vcom_device_descriptor = {
  sizeof vcom_device_descriptor_data,
  vcom_device_descriptor_data
};

/* Configuration Descriptor tree for a CDC.*/
static const uint8_t vcom_configuration_descriptor_data[67] = {
  /* Configuration Descriptor.*/
  USB_DESC_CONFIGURATION(67,            /* wTotalLength.                    */
                         0x02,          /* bNumInterfaces.                  */
                         0x01,          /* bConfigurationValue.             */
                         0,             /* iConfiguration.                  */
                         0xC0,          /* bmAttributes (self powered).     */
                         50),           /* bMaxPower (100mA).               */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x00,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x01,          /* bNumEndpoints.                   */
                         0x02,          /* bInterfaceClass (Communications
                                           Interface Class, CDC section
                                           4.2).                            */
                         0x02,          /* bInterfaceSubClass (Abstract
                                         Control Model, CDC section 4.3).   */
                         0x01,          /* bInterfaceProtocol (AT commands,
                                           CDC section 4.4).                */
                         0),            /* iInterface.                      */
  /* Header Functional Descriptor (CDC section 5.2.3).*/
  USB_DESC_BYTE         (5),            /* bLength.                         */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x00),         /* bDescriptorSubtype (Header
                                           Functional Descriptor.           */
  USB_DESC_BCD          (0x0110),       /* bcdCDC.                          */
  /* Call Management Functional Descriptor. */
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x01),         /* bDescriptorSubtype (Call Management
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x00),         /* bmCapabilities (D0+D1).          */
  USB_DESC_BYTE         (0x01),         /* bDataInterface.                  */
  /* ACM Functional Descriptor.*/
  USB_DESC_BYTE         (4),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x02),         /* bDescriptorSubtype (Abstract
                                           Control Management Descriptor).  */
  USB_DESC_BYTE         (0x02),         /* bmCapabilities.                  */
  /* Union Functional Descriptor.*/
  USB_DESC_BYTE         (5),            /* bFunctionLength.                 */
  USB_DESC_BYTE         (0x24),         /* bDescriptorType (CS_INTERFACE).  */
  USB_DESC_BYTE         (0x06),         /* bDescriptorSubtype (Union
                                           Functional Descriptor).          */
  USB_DESC_BYTE         (0x00),         /* bMasterInterface (Communication
                                           Class Interface).                */
  USB_DESC_BYTE         (0x01),         /* bSlaveInterface0 (Data Class
                                           Interface).                      */
  /* Endpoint 2 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_INTERRUPT_REQUEST_EP|0x80,
                         0x03,          /* bmAttributes (Interrupt).        */
                         0x0008,        /* wMaxPacketSize.                  */
                         0xFF),         /* bInterval.                       */
  /* Interface Descriptor.*/
  USB_DESC_INTERFACE    (0x01,          /* bInterfaceNumber.                */
                         0x00,          /* bAlternateSetting.               */
                         0x02,          /* bNumEndpoints.                   */
                         0x0A,          /* bInterfaceClass (Data Class
                                           Interface, CDC section 4.5).     */
                         0x00,          /* bInterfaceSubClass (CDC section
                                           4.6).                            */
                         0x00,          /* bInterfaceProtocol (CDC section
                                           4.7).                            */
                         0x00),         /* iInterface.                      */
  /* Endpoint 3 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_DATA_AVAILABLE_EP,       /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00),         /* bInterval.                       */
  /* Endpoint 1 Descriptor.*/
  USB_DESC_ENDPOINT     (USBD1_DATA_REQUEST_EP|0x80,    /* bEndpointAddress.*/
                         0x02,          /* bmAttributes (Bulk).             */
                         0x0040,        /* wMaxPacketSize.                  */
                         0x00)          /* bInterval.                       */
};

// Configuration Descriptor wrapper.
static const USBDescriptor vcom_configuration_descriptor = {
  sizeof vcom_configuration_descriptor_data,
  vcom_configuration_descriptor_data
};

// U.S. English language identifier.
static const uint8_t vcom_string0[] = {
  USB_DESC_BYTE(4),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  USB_DESC_WORD(0x0409)                 /* wLANGID (U.S. English).          */
};

// Vendor string.
static const uint8_t vcom_string1[] = {
  USB_DESC_BYTE(38),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0,
  'l', 0, 'e', 0, 'c', 0, 't', 0, 'r', 0, 'o', 0, 'n', 0, 'i', 0,
  'c', 0, 's', 0
};

// Device Description string.
static const uint8_t vcom_string2[] = {
  USB_DESC_BYTE(56),                    /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  'C', 0, 'h', 0, 'i', 0, 'b', 0, 'i', 0, 'O', 0, 'S', 0, '/', 0,
  'R', 0, 'T', 0, ' ', 0, 'V', 0, 'i', 0, 'r', 0, 't', 0, 'u', 0,
  'a', 0, 'l', 0, ' ', 0, 'C', 0, 'O', 0, 'M', 0, ' ', 0, 'P', 0,
  'o', 0, 'r', 0, 't', 0
};

// Serial Number string.
static const uint8_t vcom_string3[] = {
  USB_DESC_BYTE(8),                     /* bLength.                         */
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING), /* bDescriptorType.                 */
  '0' + CH_KERNEL_MAJOR, 0,
  '0' + CH_KERNEL_MINOR, 0,
  '0' + CH_KERNEL_PATCH, 0
};

// Strings wrappers array.
static const USBDescriptor vcom_strings[] = {
  {sizeof vcom_string0, vcom_string0},
  {sizeof vcom_string1, vcom_string1},
  {sizeof vcom_string2, vcom_string2},
  {sizeof vcom_string3, vcom_string3}
};

/*
 * Handles the GET_DESCRIPTOR callback. All required descriptors must be
 * handled here.
 */
static const USBDescriptor *get_descriptor(USBDriver *usbp,
                                           uint8_t dtype,
                                           uint8_t dindex,
                                           uint16_t lang) {
  (void)usbp;
  (void)lang;
  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:
    return &vcom_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION:
    return &vcom_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex < 4)
      return &vcom_strings[dindex];
  }
  return NULL;
}

static USBInEndpointState ep1instate; // IN EP1 state.
static USBOutEndpointState ep1outstate; // OUT EP1 state.

// EP1 initialization structure (both IN and OUT).
static const USBEndpointConfig ep1config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  sduDataTransmitted,
  sduDataReceived,
  0x0040,
  0x0040,
  &ep1instate,
  &ep1outstate,
  2,
  NULL
};

static USBInEndpointState ep2instate; // IN EP2 state.

// EP2 initialization structure (IN only).
static const USBEndpointConfig ep2config = {
  USB_EP_MODE_TYPE_INTR,
  NULL,
  sduInterruptTransmitted,
  NULL,
  0x0010,
  0x0000,
  &ep2instate,
  NULL,
  1,
  NULL
};

// Handles the USB driver global events.
static void usb_event(USBDriver *usbp, usbevent_t event) {

  switch (event) {
  case USB_EVENT_RESET:
    return;
  case USB_EVENT_ADDRESS:
    return;
  case USB_EVENT_CONFIGURED:
    chSysLockFromIsr();

    /* Enables the endpoints specified into the configuration.
       Note, this callback is invoked from an ISR so I-Class functions
       must be used.*/
    usbInitEndpointI(usbp, USBD1_DATA_REQUEST_EP, &ep1config);
    usbInitEndpointI(usbp, USBD1_INTERRUPT_REQUEST_EP, &ep2config);

    /* Resetting the state of the CDC subsystem.*/
    sduConfigureHookI(&SDU1);

    chSysUnlockFromIsr();
    return;
  case USB_EVENT_SUSPEND:
    return;
  case USB_EVENT_WAKEUP:
    return;
  case USB_EVENT_STALLED:
    return;
  }
  return;
}

// USB driver configuration.
static const USBConfig usbcfg = {
  usb_event,
  get_descriptor,
  sduRequestsHook,
  NULL
};

// Serial over USB driver configuration.
static const SerialUSBConfig serusbcfg = {
  &USBD1,
  USBD1_DATA_REQUEST_EP,
  USBD1_DATA_AVAILABLE_EP,
  USBD1_INTERRUPT_REQUEST_EP
};

// This is a periodic thread that does absolutely nothing except flashing a LED.
static WORKING_AREA(waBlinker, 128);
static msg_t Blinker(void *arg) {
  (void)arg;
  chRegSetThreadName("blinker");
  while (TRUE) {
    palSetPad(GPIOD, GPIOD_LED3);       /* Orange.  */
    chThdSleepMilliseconds(500);
    palClearPad(GPIOD, GPIOD_LED3);     /* Orange.  */
    chThdSleepMilliseconds(500);
  }
  return 0;
}

static char* appendHex (char *p, uint8_t v) {
  const char* hex = "0123456789ABCDEF";
  *p++ = hex[v>>4];
  *p++ = hex[v&0x0F];
  return p;
}

// CAN receive thread class.
static WORKING_AREA(waCanRecv, 256);
static msg_t CanRecv(void *arg) {
  (void) arg;
  chRegSetThreadName("canrecv");
  CANRxFrame rxMsg;

  while(true) {
    while (canReceive(&CAND1, CAN_ANY_MAILBOX, &rxMsg, 100) == RDY_OK) {
      char buf [30];
      uint32_t id = rxMsg.EID;
      if (!rxMsg.IDE)
        id &= 0x7FF;
      chsnprintf(buf, sizeof buf, "S%x#", id);
      char* p = buf + strlen(buf);
      int i;
      for (i = 0; i < rxMsg.DLC; ++i)
        p = appendHex(p, rxMsg.data8[i]);
      strcpy(p, "\r\n");
      chprintf(serial, buf);
    }
  }

  return 0;
}

// CAN send thread class.
static uint8_t hexDigit (char c) {
    if (c > '9')
        c -= 7;
    return c & 0xF;
}

static WORKING_AREA(waCanSend, 256);
static msg_t CanSend(void *arg) {
  (void) arg;
  chRegSetThreadName("cansend");
  CANTxFrame txmsg_can1;
  txmsg_can1.RTR = CAN_RTR_DATA;

  uint8_t nibble = 0;
  enum { sNONE, sADDR, sDATA } state = sNONE;

  while (true) {
    char ch = chSequentialStreamGet(serial);

    switch (state) {
    case sNONE:
        if (ch == 'S') {
            state = sADDR;
            txmsg_can1.EID = 0;
        }
        break;
    case sADDR:
        if (ch == '#') {
            state = sDATA;
            nibble = 0;
        } else {
            txmsg_can1.EID = (txmsg_can1.EID << 4) | hexDigit(ch);
        }
        break;
    case sDATA:
        if (ch >= '0') {
            uint8_t* p = txmsg_can1.data8 + nibble/2;
            *p = (*p << 4) | hexDigit(ch);
            ++nibble;
        } else {
            txmsg_can1.IDE = txmsg_can1.EID > 0x7FF ? CAN_IDE_EXT : CAN_IDE_STD;
            txmsg_can1.DLC = nibble / 2;
            canTransmit(&CAND1, 1, &txmsg_can1, 100);
            state = sNONE;
        }
    }
  }

  return 0;
}

// Application entry point.
int main(void) {
  halInit();
  chSysInit();

  // Initializes a serial-over-USB CDC driver.
  sduObjectInit(&SDU1);
  sduStart(&SDU1, &serusbcfg);
  chprintf(serial, "\r\n[disco407]\r\n");

  // Activates the USB driver and then the USB bus pull-up on D+.
  // Note, a delay is inserted in order to not have to disconnect the cable
  // after a reset.
  usbDisconnectBus(serusbcfg.usbp);
  // chThdSleepMilliseconds(1500);
  usbStart(serusbcfg.usbp, &usbcfg);
  usbConnectBus(serusbcfg.usbp);

  GPIOD->AFRL = (GPIOD->AFRL & ~0xFF) | 0x99; // CAN RX/TX on PD0/PD1
  // palSetGroupMode(GPIOD, PAL_PORT_BIT(0), 0, PAL_STM32_MODE_ALTERNATE);
  // palSetGroupMode(GPIOD, PAL_PORT_BIT(1), 0, PAL_STM32_MODE_ALTERNATE);
  GPIOD->MODER = (GPIOD->MODER & ~0xF) | 0b1010;

  // Activates CAN driver.
  static const CANConfig cancfg = {
    CAN_MCR_ABOM | CAN_MCR_AWUM | CAN_MCR_TXFP,
    // CAN_BTR_LBKM |
    CAN_BTR_SJW(1) | CAN_BTR_TS2(2) |
    CAN_BTR_TS1(7) | CAN_BTR_BRP(6)
  };
  canStart(&CAND1, &cancfg);
  
  chThdCreateStatic(waBlinker, sizeof(waBlinker), NORMALPRIO, Blinker, NULL);
  chThdCreateStatic(waCanRecv, sizeof(waCanRecv), NORMALPRIO, CanRecv, NULL);
  chThdCreateStatic(waCanSend, sizeof(waCanSend), NORMALPRIO, CanSend, NULL);

  while (TRUE) {
    chThdSleepMilliseconds(500);
  }
}
