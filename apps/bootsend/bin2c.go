// Convert a .bin file to data which can be included in a C program.
// -jcw, 2014-04-12
//
// compile and run, then send output to file: go run bin2c.go >data.h

package main

import (
	"fmt"
	"io/ioutil"
)

func main() {
	data, err := ioutil.ReadFile("../canuid/build/ssb.bin")
	if err != nil {
		panic(err)
	}
	// fmt.Println(len(data))

	fmt.Println("const uint8_t bootData [] = {")
	s := "  "
	for _, b := range data {
		t := fmt.Sprintf("%d,", b)
		if len(s+t) > 80 {
			fmt.Println(s)
			s = "  "
		}
		s += t
	}
	fmt.Println(s)
	fmt.Println("};")
}
