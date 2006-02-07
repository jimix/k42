#ifndef __EXPEDIENT_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __EXPEDIENT_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: expedient.H,v 1.13 2004/01/06 12:32:10 jimix Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Machinery for implementing assembler functionality cleanly, if
 *    inefficiently, in C.
 * **************************************************************************/

#ifndef __ASSEMBLER__

struct ExpRegs {
    uval64	r0,            r3,  r4,  r5,  r6,  r7,
		r8,  r9,  r10, r11, r12,      r14, r15,
		r16, r17, r18, r19, r20, r21, r22, r23,
		r24, r25, r26, r27, r28, r29, r30, r31;
    uval64	f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,
		f8,  f9,  f10, f11, f12, f13, f14, f15,
		f16, f17, f18, f19, f20, f21, f22, f23,
		f24, f25, f26, f27, f28, f29, f30, f31;
    uval64	cr;	// really just 32 bits
};

#else /* #ifndef __ASSEMBLER__ */

#include <misc/asm.h>

#define GOTO_EXPEDIENT(symbol)\
	la	r1,-ER_SIZE(r1);\
	std	r3,ER_r3(r1);\
	LOAD_C_DATA_UVAL(r3,symbol,0);\
	bl	C_TEXT(GotoExpedientC);\
	tw	4,0,0

#define CALL_EXPEDIENT(symbol)\
	la	r1,-ER_SIZE(r1);\
	std	r3,ER_r3(r1);\
	LOAD_C_DATA_UVAL(r3,symbol,0);\
	bl	C_TEXT(CallExpedientC);\
	ld	r3,ER_r3(r1);\
	la	r1,ER_SIZE(r1)

#endif /* #ifndef __ASSEMBLER__ */
