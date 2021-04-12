/*
 *  The file is a replacement for 'Base.h' from Edk2
 *  required by the 'fwimage' utility.
 */

#pragma once

#include <stddef.h>
#include <inttypes.h>

#define IN
#define OUT

#define TRUE	1
#define FALSE	0
typedef unsigned char BOOLEAN;

typedef size_t UINTN;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;

typedef uint16_t CHAR16;
typedef void VOID;

typedef struct _GUID {
	UINT32	Data1;
	UINT16	Data2;
	UINT16	Data3;
	UINT8	Data4[8];
} GUID;

#define SIGNATURE_16(a, b)		\
	((a) | ((UINT16) (b) << 8))
#define SIGNATURE_32(a, b, c, d)	\
	(SIGNATURE_16(a, b) | ((UINT32) SIGNATURE_16(c, d) << 16))
