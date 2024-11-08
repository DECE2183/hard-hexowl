/* package hexocalc */

#include <stddef.h>

#ifndef GO_CGO_PROLOGUE_H
#define GO_CGO_PROLOGUE_H

typedef signed char GoInt8;
typedef unsigned char GoUint8;
typedef short GoInt16;
typedef unsigned short GoUint16;
typedef int GoInt32;
typedef unsigned int GoUint32;
typedef long long GoInt64;
typedef unsigned long long GoUint64;
typedef GoInt64 GoInt;
typedef GoUint64 GoUint;
typedef size_t GoUintptr;
typedef float GoFloat32;
typedef double GoFloat64;
typedef float _Complex GoComplex64;
typedef double _Complex GoComplex128;

#endif // GO_CGO_PROLOGUE_H

typedef struct { const char *p; size_t n; } _GoString_;
typedef _GoString_ GoString;
typedef void *GoMap;
typedef void *GoChan;
typedef struct { void *t; void *v; } GoInterface;
typedef struct { void *data; size_t len; size_t cap; } GoSlice;

/* End of boilerplate cgo prologue.  */

#ifdef __cplusplus
extern "C" {
#endif


/* Return type for HexowlCalculate */
typedef struct hexowl_calculate_return {
	GoUint8 success; /* success */
	GoString decVal; /* decVal */
	GoString hexVal; /* hexVal */
	GoString binVal; /* binVal */
	GoUint32 calcTime;	/* calcTime */
} hexowl_calculate_return_t;

typedef void (*hexowl_print_func_t)(GoString str);
typedef void (*hexowl_clear_func_t)(void);
typedef int (*hexowl_flist_func_t)(char *str);
typedef int (*hexowl_fopen_func_t)(GoString name, GoString mode);
typedef int (*hexowl_fclose_func_t)(void);
typedef int (*hexowl_fwrite_func_t)(const void *data, size_t size);
typedef int (*hexowl_fread_func_t)(void *data, size_t size);

//go:noinline
extern hexowl_calculate_return_t HexowlCalculate(const char *input);

//go:noinline
extern void HexowlInit(
	hexowl_print_func_t printfunc,
	GoUint32 limit,
	hexowl_clear_func_t clearfunc,
	hexowl_flist_func_t listfunc,
	hexowl_fopen_func_t openfunc,
	hexowl_fclose_func_t closefunc,
	hexowl_fwrite_func_t writefunc,
	hexowl_fread_func_t readfunc);

//go:noinline
extern GoUint64 GetFreeMem();

#ifdef __cplusplus
}
#endif
