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

/*
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <string.h>

typedef struct { const char *p; ptrdiff_t n; } _GoString_;
typedef void (*print_func_t)(_GoString_ str);

static char buf[512];

void ExtPrint(uintptr_t func, _GoString_ str)
{
	print_func_t f = (print_func_t)func;
	if (f != NULL)
	{
		memset(buf, 0, 512);
		strncpy(buf, str.p, str.n);
		printf("str: %s\n", buf);
		f(str);
	}
}
*/
import "C"

type displayWriter struct {
	printFunc  uintptr
	printLimit int
}

var stdOut displayWriter

//go:noinline
func (w displayWriter) Write(arr []byte) (n int, err error) {
	fmt.Printf("writing to stdout: %+v\n", w)

	if w.printFunc == 0 || w.printLimit == 0 {
		fmt.Println("no print function")
		return 0, fmt.Errorf("print function does not defined")
	}

	var strToPrint string

	sizeLeft := len(arr)
	for sizeLeft > w.printLimit {
		strToPrint = string(arr[n : n+w.printLimit])
		fmt.Printf("to stdout: %s\n", strToPrint)
		C.ExtPrint(w.printFunc, *(*C._GoString_)(unsafe.Pointer(&strToPrint)))
		sizeLeft -= w.printLimit
		n += w.printLimit
	}

	strToPrint = string(arr[n : n+sizeLeft])
	fmt.Printf("to stdout: %s\n", strToPrint)
	C.ExtPrint(w.printFunc, *(*C._GoString_)(unsafe.Pointer(&strToPrint)))

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
func HexowlInit(printfunc uintptr, limit uint32) {
	fmt.Printf("print func addr: 0x%x\nlimit: %d\n", printfunc, limit)
	stdOut.printFunc = printfunc
	stdOut.printLimit = int(limit)
	builtin.FuncsInit(&stdOut)
}

//export GetFreeMem
//go:noinline
func GetFreeMem() uint64 {
	runtime.GC()
	var stats runtime.MemStats
	runtime.ReadMemStats(&stats)
	return stats.Sys - stats.HeapInuse
}
