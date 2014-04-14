// CAN bus register divider calculator, adapted from LPCOpen sample code
// -jcw, 2014-04-10
//
// compile and run on Mac OSX or Linux as: gcc canrate.c && ./a.out

#include <stdio.h>
#include <stdint.h>

uint32_t canTimingCfg[2];

void baudrateCalculate(uint32_t baud_rate, uint32_t pClk) {
  uint32_t clk_per_bit = pClk / baud_rate;
  uint32_t div, quanta, segs;
  for (div = 0; div <= 15; div++) {
    for (quanta = 1; quanta <= 32; quanta++) {
      for (segs = 3; segs <= 17; segs++) {
        if (clk_per_bit == (segs * quanta * (div + 1))) {
          segs -= 3;
          uint32_t seg1 = segs / 2;
          uint32_t seg2 = segs - seg1;
          uint32_t can_sjw = seg1 > 3 ? 3 : seg1;
          canTimingCfg[0] = div;
          canTimingCfg[1] = ((quanta - 1) & 0x3F) | (can_sjw & 0x03) << 6 |
                              (seg1 & 0x0F) << 8 | (seg2 & 0x07) << 12;
          return;
        }
      }
    }
  }
}

int main() {
  baudrateCalculate(500000, 48000000);
  printf("CLKDIV 0x%04X BTR 0x%04X \n", canTimingCfg[0], canTimingCfg[1]);
  return 0;
}
