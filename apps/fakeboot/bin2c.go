// Convert a .bin file to data which can be included in a C program.
// -jcw, 2014-04-12
//
// compile and run, then send output to file: go run bin2c.go >data.h

package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io/ioutil"
	"os"
)

const filename = "../minimal/build/min.bin"

func main() {
	data, err := ioutil.ReadFile(filename)
	if err != nil {
		panic(err)
	}
	
	// fix the checksum to mark code as valid (see UM10398, p.416)
	buf := bytes.NewReader(data)
	values := [7]uint32{}
	err = binary.Read(buf, binary.LittleEndian, values[:])
	if err != nil {
		panic(err)
	}
	var sum uint32
	for _, v := range values {
		sum -= v
		// fmt.Println(v, sum)
	}
	data[28] = byte(sum)
	data[29] = byte(sum >> 8)
	data[30] = byte(sum >> 16)
	data[31] = byte(sum >> 24)
	
	fmt.Fprintf(os.Stderr, "%s: %d bytes, checksum 0x%04X @ 0x1C\n",
		filename, len(data), sum)

	fmt.Println("// generated by bin2c.go from", filename)
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
	fmt.Println("  0,0,0,0,0,0,0,0,") // extra bytes for the boot loader (why?)
	fmt.Println("};")
}
