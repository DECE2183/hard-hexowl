package main

import (
	"fmt"
	"runtime"
	"time"

	"github.com/dece2183/hexowl/builtin"
	"github.com/dece2183/hexowl/operators"
	"github.com/dece2183/hexowl/utils"
)

import "C"

//export CalculatePrompt
//go:noinline
func CalculatePrompt(inputStr *C.char) (success bool, decVal, hexVal, binVal string, calcTime uint32) {
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

//export HexocalcInit
//go:noinline
func HexocalcInit() {
	builtin.FuncsInit()
	fmt.Println("Hexocalc initialization complete.")
}

//export GetFreeMem
//go:noinline
func GetFreeMem() uint64 {
	runtime.GC()
	var stats runtime.MemStats
	runtime.ReadMemStats(&stats)
	return stats.Sys - stats.HeapInuse
}
