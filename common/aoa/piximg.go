// Convert a BMP from row-major to column-major order for the oled display
// -jcw, 2014-04-12
//
// compile and run, then paste into oled_data.h: go run piximg.go

package main

import (
	"fmt"
	"io/ioutil"
)

func main() {
	data, err := ioutil.ReadFile("tosqa-128-64-bw.bmp")
	if err != nil {
		panic(err)
	}
	// fmt.Println(len(data))
	data = data[len(data)-1024:]

	pix := func(x, y int) byte {
		return (data[x/8+16*y]>>uint(7-x%8)) & 1
	}

	fmt.Println("const uint8_t logo [] = {")
	s := "  "
	for r := 0; r < 8; r++ {
		for c := 0; c < 128; c++ {
			var b byte;
			for j := 0; j < 8; j++ {
				x := c
				y := 63 - (8 * r + (7-j))
				b <<= 1
				b |= pix(x, y)
			}
			t := fmt.Sprintf("%d,", b)
			if len(s+t) > 80 {
				fmt.Println(s)
				s = "  "
			}
			s += t
		}
	}
	fmt.Println(s)
	fmt.Println("};")
}
