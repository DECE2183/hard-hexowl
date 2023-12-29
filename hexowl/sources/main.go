package main

import (
	"fmt"
	"io"
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
typedef struct { void *data; long long int len; long long int cap; } _GoSlice_;

typedef void (*print_func_t)(_GoString_ str);
typedef void (*hexowl_clear_func_t)(void);
typedef int (*hexowl_flist_func_t)(char *str);
typedef int (*fopen_func_t)(_GoString_ name, _GoString_ mode);
typedef int (*fclose_func_t)(void);
typedef int (*fwrite_func_t)(const void* data, size_t size);
typedef int (*fread_func_t)(void* data, size_t size);

void ExtPrint(uintptr_t func, _GoString_ str)
{
	if (func == 0) return;
	((print_func_t)func)(str);
}

void ExtClear(uintptr_t func)
{
	if (func == 0) return;
	((hexowl_clear_func_t)func)();
}

int ExtOpenFile(uintptr_t func, _GoString_ name, _GoString_ mode)
{
	if (func == 0) return -8;
	return ((fopen_func_t)func)(name, mode);
}

int ExtCloseFile(uintptr_t func)
{
	if (func == 0) return -8;
	return ((fclose_func_t)func)();
}

int ExtWriteFile(uintptr_t func, _GoSlice_ data)
{
	if (func == 0) return -8;
	return ((fwrite_func_t)func)(data.data, data.len);
}

int ExtReadFile(uintptr_t func, _GoSlice_ data)
{
	if (func == 0) return -8;
	return ((fread_func_t)func)(data.data, data.len);
}
*/
import "C"

const (
	ERR_NONE               = 0
	ERR_SD_NOT_INSERTED    = -1
	ERR_SD_MOUNT_FAIL      = -2
	ERR_SD_NOT_EXISTS      = -3
	ERR_SD_WRITE_FAIL      = -4
	ERR_SD_READ_FAIL       = -5
	ERR_SD_LONG_NAME       = -6
	ERR_SD_MKDIR_ERR       = -7
	ERR_SD_NOT_IMPLEMENTED = -8
)

type displayWriter struct{}

type envWriter struct{}

type envReader struct{}

var funcsDescriptor struct {
	printFunc  uintptr
	printLimit int
	clearFunc  uintptr
	listFunc   uintptr
	openFunc   uintptr
	closeFunc  uintptr
	writeFunc  uintptr
	readFunc   uintptr
}

var errorMessages = map[int]error{
	ERR_SD_NOT_INSERTED:    fmt.Errorf("SD card not inserted"),
	ERR_SD_MOUNT_FAIL:      fmt.Errorf("SD card mount failure"),
	ERR_SD_NOT_EXISTS:      fmt.Errorf("SD card file not exists"),
	ERR_SD_WRITE_FAIL:      fmt.Errorf("SD card write failure"),
	ERR_SD_READ_FAIL:       fmt.Errorf("SD card read failure"),
	ERR_SD_LONG_NAME:       fmt.Errorf("too long file name"),
	ERR_SD_MKDIR_ERR:       fmt.Errorf("make dir error"),
	ERR_SD_NOT_IMPLEMENTED: fmt.Errorf("not implemented"),
}

var stdOut displayWriter

func toCstr(str string) C._GoString_ {
	return *(*C._GoString_)(unsafe.Pointer(&str))
}

func toCslice(slc []byte) C._GoSlice_ {
	return *(*C._GoSlice_)(unsafe.Pointer(&slc))
}

func removeAnsi(arr []byte) string {
	retArr := make([]byte, 0, len(arr))
	escDet := false
	for _, c := range arr {
		if c == '\u001B' {
			escDet = true
			continue
		}
		if escDet && c != 'm' {
			continue
		}
		if escDet && c == 'm' {
			escDet = false
			continue
		}
		retArr = append(retArr, c)
	}
	return string(retArr)
}

func (w *displayWriter) Write(arr []byte) (n int, err error) {
	if funcsDescriptor.printFunc == 0 || funcsDescriptor.printLimit == 0 {
		return 0, fmt.Errorf("print function does not defined")
	}

	var strToPrint string

	sizeLeft := len(arr)
	for sizeLeft > funcsDescriptor.printLimit {
		// skip ASNI ESC color code
		strToPrint = removeAnsi(arr[n : n+funcsDescriptor.printLimit])

		C.ExtPrint(funcsDescriptor.printFunc, toCstr(strToPrint))
		sizeLeft -= funcsDescriptor.printLimit
		n += funcsDescriptor.printLimit
	}

	// skip ASNI ESC color code
	strToPrint = removeAnsi(arr[n : n+sizeLeft])
	C.ExtPrint(funcsDescriptor.printFunc, toCstr(strToPrint))

	return n, nil
}

func (e *envWriter) Write(data []byte) (n int, err error) {
	if funcsDescriptor.closeFunc == 0 {
		return 0, fmt.Errorf("not implemented write function")
	}

	n = int(C.ExtWriteFile(funcsDescriptor.writeFunc, toCslice(data)))
	if n < 0 {
		err = errorMessages[n]
		n = 0
	} else if n == 0 {
		err = io.EOF
	}

	return
}

func (e *envWriter) Close() error {
	if funcsDescriptor.closeFunc == 0 {
		return fmt.Errorf("not implemented close function")
	}

	C.ExtCloseFile(funcsDescriptor.closeFunc)
	return nil
}

func (e *envReader) Read(data []byte) (n int, err error) {
	if funcsDescriptor.closeFunc == 0 {
		return 0, fmt.Errorf("not implemented read function")
	}

	n = int(C.ExtReadFile(funcsDescriptor.readFunc, toCslice(data)))
	if n < 0 {
		err = errorMessages[n]
		n = 0
	} else if n == 0 {
		err = io.EOF
	}

	return
}

func (e *envReader) Close() error {
	if funcsDescriptor.closeFunc == 0 {
		return fmt.Errorf("not implemented close function")
	}

	C.ExtCloseFile(funcsDescriptor.closeFunc)
	return nil
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

func clearOutput() {
	C.ExtClear(funcsDescriptor.clearFunc)
}

func envRead(name string) (io.ReadCloser, error) {
	errCode := C.ExtOpenFile(funcsDescriptor.openFunc, toCstr(name), toCstr("r"))
	if errCode < 0 {
		return nil, errorMessages[int(errCode)]
	}

	return &envReader{}, nil
}

func envWrite(name string) (io.WriteCloser, error) {
	errCode := C.ExtOpenFile(funcsDescriptor.openFunc, toCstr(name), toCstr("w"))
	if errCode < 0 {
		return nil, errorMessages[int(errCode)]
	}

	return &envWriter{}, nil
}

//export HexowlInit
//go:noinline
func HexowlInit(printfunc uintptr, printlimit uint32, clearfunc, listfunc, openfunc, closefunc, writefunc, readfunc uintptr) {
	fmt.Printf("print func addr: 0x%x\nlimit: %d\n", printfunc, printlimit)

	funcsDescriptor.printFunc = printfunc
	funcsDescriptor.printLimit = int(printlimit)
	funcsDescriptor.clearFunc = clearfunc
	funcsDescriptor.listFunc = listfunc
	funcsDescriptor.openFunc = openfunc
	funcsDescriptor.closeFunc = closefunc
	funcsDescriptor.writeFunc = writefunc
	funcsDescriptor.readFunc = readfunc

	sysDesc := builtin.System{
		Stdout:           &stdOut,
		ClearScreen:      clearOutput,
		WriteEnvironment: envWrite,
		ReadEnvironment:  envRead,
	}

	builtin.SystemInit(sysDesc)
}

//export GetFreeMem
//go:noinline
func GetFreeMem() uint64 {
	runtime.GC()
	var stats runtime.MemStats
	runtime.ReadMemStats(&stats)
	return stats.Sys - stats.HeapInuse
}
