package main

import (
	"fmt"
	"runtime"
	"time"
	"unsafe"

	"github.com/dece2183/hexowl/builtin"
	"github.com/dece2183/hexowl/operators"
	"github.com/dece2183/hexowl/utils"
)

import "C"

type displayWriter struct {
	print      func(str string)
	printLimit int
}

var stdOut displayWriter

//go:noinline
func (w displayWriter) Write(arr []byte) (n int, err error) {
	fmt.Println("writing to stdout")

	if w.print == nil || stdOut.printLimit == 0 {
		fmt.Println("no print function")
		return 0, fmt.Errorf("print function does not defined")
	}

	sizeLeft := len(arr)
	for sizeLeft > w.printLimit {
		w.print(string(arr[n : n+w.printLimit]))
		sizeLeft -= w.printLimit
		n += w.printLimit
	}
	w.print(string(arr[n : n+sizeLeft]))

	return n, nil
}

//export HexowlCalculate
//go:noinline
func HexowlCalculate(inputStr *C.char) (success bool, decVal, hexVal, binVal string, calcTime uint32) {
	input := C.GoString(inputStr)

	calcBeginTime := time.Now()
	words := utils.ParsePrompt(input)

	operator, err := operators.Generate(words, make(map[string]interface{}))
	if err != nil {
		calcTime = uint32(time.Since(calcBeginTime).Milliseconds())
		decVal = fmt.Sprintf("%s", err)
		return
	}

	val, err := operators.Calculate(operator, make(map[string]interface{}))
	if err != nil {
		calcTime = uint32(time.Since(calcBeginTime).Milliseconds())
		decVal = fmt.Sprintf("%s", err)
		return
	}

	calcTime = uint32(time.Since(calcBeginTime).Milliseconds())
	success = true

	if val == nil {
		return
	}

	switch v := val.(type) {
	case string:
		decVal = v
	case bool:
		decVal = fmt.Sprintf("%v", v)
	default:
		decVal = fmt.Sprintf("%v", val)
		hexVal = fmt.Sprintf("0x%X", utils.ToNumber[uint64](val))
		binVal = fmt.Sprintf("0b%b", utils.ToNumber[uint64](val))
	}

	return
}

//export HexowlInit
//go:noinline
func HexowlInit(printfunc uintptr, limit int) {
	stdOut.print = *(*func(str string))(unsafe.Pointer(printfunc))
	stdOut.printLimit = limit
	builtin.FuncsInit(stdOut)
}

//export GetFreeMem
//go:noinline
func GetFreeMem() uint64 {
	runtime.GC()
	var stats runtime.MemStats
	runtime.ReadMemStats(&stats)
	return stats.Sys - stats.HeapInuse
}
