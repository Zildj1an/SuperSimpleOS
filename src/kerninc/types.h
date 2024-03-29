#pragma once

#define assert(x) if(!(x)) asm __volatile__("hlt");

typedef signed long long ssize_t;
typedef unsigned long long size_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define SIZE_MAX	0xFFFFFFFFFFFFFFFFULL
typedef long long intptr_t;
typedef unsigned long long uintptr_t;

#ifndef NULL
#define NULL ((void *) 0)
#endif
