/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: volatileFrame.S,v 1.6 2000/08/07 14:10:21 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Machinery for implementing assembler functionality cleanly, if
 *    inefficiently, in C.
 * **************************************************************************/

#include <sys/kinclude.H>
#include <misc/asm.h>
#include <misc/volatileFrame.H>
#include <misc/arch/powerpc/asdef.h>

/*
 * On entry, r3 has been saved in STK_PARAM0(r1) where it belongs, and the
 * original link register contents have been moved to r3.
 */
C_TEXT_ENTRY(VolatileFrameEnter)
	std	r3,STK_LR(r1)
	stdu	r1,-VOLATILE_FRAME_SIZE(r1)
	std	r0,VOLATILE_FRAME_R0(r1)
	std	r4,VOLATILE_FRAME_R4(r1)
	std	r5,VOLATILE_FRAME_R5(r1)
	std	r6,VOLATILE_FRAME_R6(r1)
	std	r7,VOLATILE_FRAME_R7(r1)
	std	r8,VOLATILE_FRAME_R8(r1)
	std	r9,VOLATILE_FRAME_R9(r1)
	std	r10,VOLATILE_FRAME_R10(r1)
	std	r11,VOLATILE_FRAME_R11(r1)
	std	r12,VOLATILE_FRAME_R12(r1)
	stfd	f0,VOLATILE_FRAME_F0(r1)
	stfd	f1,VOLATILE_FRAME_F1(r1)
	stfd	f2,VOLATILE_FRAME_F2(r1)
	stfd	f3,VOLATILE_FRAME_F3(r1)
	stfd	f4,VOLATILE_FRAME_F4(r1)
	stfd	f5,VOLATILE_FRAME_F5(r1)
	stfd	f6,VOLATILE_FRAME_F6(r1)
	stfd	f7,VOLATILE_FRAME_F7(r1)
	stfd	f8,VOLATILE_FRAME_F8(r1)
	stfd	f9,VOLATILE_FRAME_F9(r1)
	stfd	f10,VOLATILE_FRAME_F10(r1)
	stfd	f11,VOLATILE_FRAME_F11(r1)
	stfd	f12,VOLATILE_FRAME_F12(r1)
	stfd	f13,VOLATILE_FRAME_F13(r1)
	blr
C_TEXT_END(VolatileFrameEnter)

C_TEXT_ENTRY(VolatileFrameReturn)
	lfd	f13,VOLATILE_FRAME_F13(r1)
	lfd	f12,VOLATILE_FRAME_F12(r1)
	lfd	f11,VOLATILE_FRAME_F11(r1)
	lfd	f10,VOLATILE_FRAME_F10(r1)
	lfd	f9,VOLATILE_FRAME_F9(r1)
	lfd	f8,VOLATILE_FRAME_F8(r1)
	lfd	f7,VOLATILE_FRAME_F7(r1)
	lfd	f6,VOLATILE_FRAME_F6(r1)
	lfd	f5,VOLATILE_FRAME_F5(r1)
	lfd	f4,VOLATILE_FRAME_F4(r1)
	lfd	f3,VOLATILE_FRAME_F3(r1)
	lfd	f2,VOLATILE_FRAME_F2(r1)
	lfd	f1,VOLATILE_FRAME_F1(r1)
	lfd	f0,VOLATILE_FRAME_F0(r1)
	ld	r12,VOLATILE_FRAME_R12(r1)
	ld	r11,VOLATILE_FRAME_R11(r1)
	ld	r10,VOLATILE_FRAME_R10(r1)
	ld	r9,VOLATILE_FRAME_R9(r1)
	ld	r8,VOLATILE_FRAME_R8(r1)
	ld	r7,VOLATILE_FRAME_R7(r1)
	ld	r6,VOLATILE_FRAME_R6(r1)
	ld	r5,VOLATILE_FRAME_R5(r1)
	ld	r4,VOLATILE_FRAME_R4(r1)
	ld	r0,VOLATILE_FRAME_R0(r1)
	la	r1,VOLATILE_FRAME_SIZE(r1)
	ld	r3,STK_LR(r1)
	mtlr	r3
	ld	r3,STK_PARAM0(r1)
	blr
C_TEXT_END(VolatileFrameReturn)
