/*
 * defines.h
 */
#ifndef __DEFINES_H_94b054518dfc475a9d5b532207a60b61
#define __DEFINES_H_94b054518dfc475a9d5b532207a60b61 1

//YOUR GLOBAL DEFINES HERE:
#include "libraries/API/tinyusbboard.h"

#ifndef DUMMY_PORTONE
#   define DUMMY_PORTONE   B,0
#endif
#ifndef DUMMY_PORTTWO
#   define DUMMY_PORTTWO   B,0
#endif
#ifndef DUMMY_PORTTHREE
#   define DUMMY_PORTTHREE B,0
#endif
#ifndef DUMMY_PORTFOUR
#   define DUMMY_PORTFOUR  B,0
#endif


/* extend iocomfort */
typedef unsigned char  u8;
struct __bits 
	{ 
		u8 b0:1; u8 b1:1; u8 b2:1; u8 b3:1; u8 b4:1; u8 b5:1; u8 b6:1; u8 b7:1;
	}
	 __attribute__((__packed__));

#define	_internal_SBIT_OF(letter, bit)		DEFCONCAT(b, bit)
#define SBIT(pin) ((*(volatile struct __bits*)&_internal_PORT_OF(pin))._internal_SBIT_OF(pin))

 /* tell the compiler to block the registers and avoid generating code with them */
 /* check for corruptions via "cat release/main.asm | egrep -in r2[^0123456789]\|r2$" */
#define __HWCLKTMP "r10"
register u8 __register2 asm ("r2");
register u8 __register3 asm ("r3");
register u8 __register4 asm ("r4");
register u8 __register5 asm ("r5");
register u8 __register6 asm ("r6");
#define CONFIG_CPUCONTEXT_NO_R2
#define CONFIG_CPUCONTEXT_NO_R3
#define CONFIG_CPUCONTEXT_NO_R4
#define CONFIG_CPUCONTEXT_NO_R5
#define CONFIG_CPUCONTEXT_NO_R6
#endif
