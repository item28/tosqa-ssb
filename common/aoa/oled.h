// Interface to 0.96" OLED screen via I2C.

#include "oled_data.h"

static void sendByte (uint8_t byte) { 
  static uint8_t cmd[] = {0x40, 0};
  cmd[1] = byte;
  i2cMasterTransmitTimeout(&I2CD1, 0x3C, cmd, sizeof(cmd), 0, 0, TIME_INFINITE);
}

static void sendCmd(uint8_t byte) {
  static uint8_t cmd[] = {0x80, 0};
  cmd[1] = byte;
  i2cMasterTransmitTimeout(&I2CD1, 0x3C, cmd, sizeof(cmd), 0, 0, TIME_INFINITE);
}

static void setXY (uint8_t row, uint8_t col) {
  sendCmd(0xB0 + row);                    // set page address
  sendCmd(0x00 + (8*col & 0x0F));         // set low col address
  sendCmd(0x10 + ((8*col >> 4) & 0x0F));  // set high col address
}

void clearDisplay () {
  for (int k = 0; k < 8; ++k) {
    setXY(k, 0);    
    for (int i = 0; i < 128; ++i)
      sendByte(0);
  }
}

void printBigDigit (uint8_t digit, int x, int y) {    
  for (int i = 0; i < 96; ++i) {
    if (i % 24 == 0) {
      setXY(x, y);
      ++x;
    }
    uint8_t bits = digit == ' ' ? 0 : bigNumbers[digit][i];
    sendByte(bits);
  }
}

static void sendStr (const char *string) {
  while (*string) {
    for(int i = 0; i < 8; ++i)
      sendByte(myFont[*string-' '][i]);
    ++string;
  }
}

void sendStrXY (const char *string, int X, int Y) {
  setXY(X,Y);
  sendStr(string);
}

static void init_OLED () {
  sendCmd(0xae);		    // display off
  sendCmd(0xa6);        // Set Normal Display (default)
                               
  // Adafruit Init sequence for 128x64 OLED module
  sendCmd(0xAE);        // DISPLAYOFF
  sendCmd(0xD5);        // SETDISPLAYCLOCKDIV
  sendCmd(0x80);        //  the suggested ratio 0x80
  sendCmd(0xA8);        // SSD1306_SETMULTIPLEX
  sendCmd(0x3F);           
  sendCmd(0xD3);        // SETDISPLAYOFFSET
  sendCmd(0x0);         // no offset
  sendCmd(0x40 | 0x0);  // SETSTARTLINE
  sendCmd(0x8D);        // CHARGEPUMP
  sendCmd(0x14);           
  sendCmd(0x20);        // MEMORYMODE
  sendCmd(0x00);        // 0x0 act like ks0108
  sendCmd(0xA0 | 0x1);  // SEGREMAP   //Rotate screen 180 deg
  //sendCmd(0xA0);         
  sendCmd(0xC8);        // COMSCANDEC  Rotate screen 180 Deg
  //sendCmd(0xC0);         
  sendCmd(0xDA);        // 0xDA
  sendCmd(0x12);        // COMSCANDEC
  sendCmd(0x81);        // SETCONTRAS
  sendCmd(0xCF);           
  sendCmd(0xd9);        // SETPRECHARGE 
  sendCmd(0xF1);           
  sendCmd(0xDB);        // SETVCOMDETECT                
  sendCmd(0x40);           
  sendCmd(0xA4);        // DISPLAYALLON_RESUME        
  sendCmd(0xA6);        // NORMALDISPLAY             
  // clearDisplay();
  sendCmd(0x2e);        // stop scroll
  //----------------------------REVERSE comments----------------------------//
  //  sendCmd(0xa0);		// seg re-map 0->127(default)
  //  sendCmd(0xa1);		// seg re-map 127->0
  //  sendCmd(0xc8);
  //  delay(1000);
  //----------------------------REVERSE comments----------------------------//
  // sendCmd(0xa7);     // Set Inverse Display  
  // sendCmd(0xae);		  // display off
  sendCmd(0x20);        // Set Memory Addressing Mode
  sendCmd(0x00);        // Set Memory Addressing Mode as Horizontal
  //  sendCmd(0x02);    // Page addressing mode(RESET)  
  
  setXY(0,0);
  for(int i = 0; i < 128 * 8; ++i)
    sendByte(logo[i]);
  
  sendCmd(0xaf);		//display on
}
