#ifndef __VOLATILE_FRAME_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __VOLATILE_FRAME_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: volatileFrame.H,v 1.5 2001/10/05 21:48:08 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Assembler machinery for creating a stack frame that preserves all the
 *    volatile gpr's and fpr's.
 * **************************************************************************/

#ifdef __ASSEMBLER__

#include <misc/asm.h>

#define VOLATILE_FRAME_ENTER()\
	std	r3,STK_PARAM0(r1);\
	mflr	r3;\
	bl	C_TEXT(VolatileFrameEnter)

#define VOLATILE_FRAME_RETURN()\
	b	C_TEXT(VolatileFrameReturn)

#define VOLATILE_FRAME_SIZE (STK_SIZE+((3+14)*8))

// Parameter gpr's are stored in space reserved in the previous stack frame.
#define VOLATILE_FRAME_R3  (VOLATILE_FRAME_SIZE+STK_PARAM0)
#define VOLATILE_FRAME_R4  (VOLATILE_FRAME_SIZE+STK_PARAM1)
#define VOLATILE_FRAME_R5  (VOLATILE_FRAME_SIZE+STK_PARAM2)
#define VOLATILE_FRAME_R6  (VOLATILE_FRAME_SIZE+STK_PARAM3)
#define VOLATILE_FRAME_R7  (VOLATILE_FRAME_SIZE+STK_PARAM4)
#define VOLATILE_FRAME_R8  (VOLATILE_FRAME_SIZE+STK_PARAM5)
#define VOLATILE_FRAME_R9  (VOLATILE_FRAME_SIZE+STK_PARAM6)
#define VOLATILE_FRAME_R10 (VOLATILE_FRAME_SIZE+STK_PARAM7)

// All other volatile gpr's are stored in the new frame.
#define VOLATILE_FRAME_R0  STK_LOCAL0
#define VOLATILE_FRAME_R11 STK_LOCAL1
#define VOLATILE_FRAME_R12 STK_LOCAL2

// Volatile fpr's are stored in the new frame.
#define VOLATILE_FRAME_F0  (STK_LOCAL3+(0*8))
#define VOLATILE_FRAME_F1  (STK_LOCAL3+(1*8))
#define VOLATILE_FRAME_F2  (STK_LOCAL3+(2*8))
#define VOLATILE_FRAME_F3  (STK_LOCAL3+(3*8))
#define VOLATILE_FRAME_F4  (STK_LOCAL3+(4*8))
#define VOLATILE_FRAME_F5  (STK_LOCAL3+(5*8))
#define VOLATILE_FRAME_F6  (STK_LOCAL3+(6*8))
#define VOLATILE_FRAME_F7  (STK_LOCAL3+(7*8))
#define VOLATILE_FRAME_F8  (STK_LOCAL3+(8*8))
#define VOLATILE_FRAME_F9  (STK_LOCAL3+(9*8))
#define VOLATILE_FRAME_F10 (STK_LOCAL3+(10*8))
#define VOLATILE_FRAME_F11 (STK_LOCAL3+(11*8))
#define VOLATILE_FRAME_F12 (STK_LOCAL3+(12*8))
#define VOLATILE_FRAME_F13 (STK_LOCAL3+(13*8))

#endif /* #ifdef __ASSEMBLER__ */
