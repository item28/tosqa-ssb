/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#ifndef GDISP_LLD_BOARD_H
#define GDISP_LLD_BOARD_H

#define SET_CS    palSetPad(GPIOD, GPIOD_LCD_CS)
#define CLR_CS    palClearPad(GPIOD, GPIOD_LCD_CS)
#define SET_RS    palSetPad(GPIOD, GPIOD_LCD_RS)
#define CLR_RS    palClearPad(GPIOD, GPIOD_LCD_RS)
#define SET_WR    palSetPad(GPIOD, GPIOD_LCD_WR)
#define CLR_WR    palClearPad(GPIOD, GPIOD_LCD_WR)
#define SET_RD    palSetPad(GPIOD, GPIOD_LCD_RD)
#define CLR_RD    palClearPad(GPIOD, GPIOD_LCD_RD)

static inline void init_board(GDisplay *g) {
	g->board = 0;

  SET_RS;
  SET_RD;
  SET_WR;
  SET_CS;
}

static inline void post_init_board(GDisplay *g) {
	(void) g;
}

static inline void setpin_reset(GDisplay *g, bool_t state) {
	(void) g;
  (void) state;
}

static inline void set_backlight(GDisplay *g, uint8_t percent) {
	(void) g;
	if(percent)
		palSetPad(GPIOB, GPIOB_TFT_LIGHT);
	else
		palClearPad(GPIOB, GPIOB_TFT_LIGHT);
}

static inline void acquire_bus(GDisplay *g) {
	(void) g;
  CLR_CS;
}

static inline void release_bus(GDisplay *g) {
	(void) g;
  SET_CS;
}

static inline void write_index(GDisplay *g, uint16_t index) {
	(void) g;
  CLR_RS;
  uint16_t dv = ((index&0x0003)<<14)|(((index&0x000C)>>2))|((index&0xE000)>>5);
  uint16_t ev = (index&0x1FF0)<<3;
  GPIOD->ODR = (GPIOD->ODR&~0xC703) | dv;
  GPIOE->ODR = (GPIOE->ODR&~0xFF80) | ev;
  CLR_WR; SET_WR; SET_RS;
}

static inline void write_data(GDisplay *g, uint16_t data) {
	(void) g;
  uint16_t dv = ((data&0x0003)<<14)|(((data&0x000C)>>2))|((data&0xE000)>>5);
  uint16_t ev = (data&0x1FF0)<<3;
  GPIOD->ODR = (GPIOD->ODR&~0xC703) | dv;
  GPIOE->ODR = (GPIOE->ODR&~0xFF80) | ev;
  CLR_WR; SET_WR;
}

static inline void setreadmode(GDisplay *g) {
	(void) g;
  // #define VAL_GPIODCRL            0x34334433      /*  PD7...PD0 */
  // #define VAL_GPIODCRH            0x33883333      /* PD15...PD8 */
  GPIOD->CRL = 0x34334444;
  GPIOD->CRH = 0x44883444;
  // #define VAL_GPIOECRL            0x34444444      /*  PE7...PE0 */
  // #define VAL_GPIOECRH            0x33333333      /* PE15...PE8 */
  GPIOE->CRL = 0x44444444;
  GPIOE->CRH = 0x44444444;
}

static inline void setwritemode(GDisplay *g) {
	(void) g;
  GPIOD->CRL = VAL_GPIODCRL;
  GPIOD->CRH = VAL_GPIODCRH;
  GPIOE->CRL = VAL_GPIOECRL;
  GPIOE->CRH = VAL_GPIOECRH;
}

static inline uint16_t read_data(GDisplay *g) {
	(void) g;
  CLR_RD;
  uint16_t dv = GPIOD->IDR;
  uint16_t ev = GPIOE->IDR;
  SET_RD;
  uint16_t c = ((dv&0xC000)>>14)|((dv&0x0003)<<2)|((ev&0xFF80)>>3)|((dv&0x0700)<<5);
	return ((c & 0x1F) << 11) | (c & 0x07E0) | ((c>>11) & 0x1F);
}

#endif /* GDISP_LLD_BOARD_H */
