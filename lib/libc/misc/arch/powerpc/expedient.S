/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: expedient.S,v 1.14 2000/08/07 14:10:21 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Machinery for implementing assembler functionality cleanly, if
 *    inefficiently, in C.
 * **************************************************************************/

#include <sys/kinclude.H>

#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <sys/arch/powerpc/asmConstants.H>

#define ER_v_r(n) (ER_r3+((n-3)*8))	// for volatile gpr's r3-r12
#define ER_nv_r(n) (ER_r14+((n-14)*8))	// for non-volatile gpr's r14-r31
#define ER_f(n) (ER_f0+((n-0)*8))

C_TEXT_ENTRY(GotoExpedientC)
	mtctr	r3			// move target address to ctr
	std	r0,ER_r0(r1)		// save r0
	std	r4,ER_v_r(4)(r1)	// save volatile gpr's r4-r12
	std	r5,ER_v_r(5)(r1)
	std	r6,ER_v_r(6)(r1)
	std	r7,ER_v_r(7)(r1)
	std	r8,ER_v_r(8)(r1)
	std	r9,ER_v_r(9)(r1)
	std	r10,ER_v_r(10)(r1)
	std	r11,ER_v_r(11)(r1)
	std	r12,ER_v_r(12)(r1)
	std	r14,ER_nv_r(14)(r1)	// save non-volatile gpr's r14-r31
	std	r15,ER_nv_r(15)(r1)
	std	r16,ER_nv_r(16)(r1)
	std	r17,ER_nv_r(17)(r1)
	std	r18,ER_nv_r(18)(r1)
	std	r19,ER_nv_r(19)(r1)
	std	r20,ER_nv_r(20)(r1)
	std	r21,ER_nv_r(21)(r1)
	std	r22,ER_nv_r(22)(r1)
	std	r23,ER_nv_r(23)(r1)
	std	r24,ER_nv_r(24)(r1)
	std	r25,ER_nv_r(25)(r1)
	std	r26,ER_nv_r(26)(r1)
	std	r27,ER_nv_r(27)(r1)
	std	r28,ER_nv_r(28)(r1)
	std	r29,ER_nv_r(29)(r1)
	std	r30,ER_nv_r(30)(r1)
	std	r31,ER_nv_r(31)(r1)
	stfd	f0,ER_f(0)(r1)		// save fpr's
	stfd	f1,ER_f(1)(r1)
	stfd	f2,ER_f(2)(r1)
	stfd	f3,ER_f(3)(r1)
	stfd	f4,ER_f(4)(r1)
	stfd	f5,ER_f(5)(r1)
	stfd	f6,ER_f(6)(r1)
	stfd	f7,ER_f(7)(r1)
	stfd	f8,ER_f(8)(r1)
	stfd	f9,ER_f(9)(r1)
	stfd	f10,ER_f(10)(r1)
	stfd	f11,ER_f(11)(r1)
	stfd	f12,ER_f(12)(r1)
	stfd	f13,ER_f(13)(r1)
	stfd	f14,ER_f(14)(r1)
	stfd	f15,ER_f(15)(r1)
	stfd	f16,ER_f(16)(r1)
	stfd	f17,ER_f(17)(r1)
	stfd	f18,ER_f(18)(r1)
	stfd	f19,ER_f(19)(r1)
	stfd	f20,ER_f(20)(r1)
	stfd	f21,ER_f(21)(r1)
	stfd	f22,ER_f(22)(r1)
	stfd	f23,ER_f(23)(r1)
	stfd	f24,ER_f(24)(r1)
	stfd	f25,ER_f(25)(r1)
	stfd	f26,ER_f(26)(r1)
	stfd	f27,ER_f(27)(r1)
	stfd	f28,ER_f(28)(r1)
	stfd	f29,ER_f(29)(r1)
	stfd	f30,ER_f(30)(r1)
	stfd	f31,ER_f(31)(r1)
	mfcr	r0			// save cr
	std	r0,ER_cr(r1)
	mflr	r0			// store return addr in caller's frame
	std	r0,(ER_SIZE+STK_LR)(r1)
	mr	r3,r1			// ExpRegs ptr is argument to target
	la	r0,ER_SIZE(r1)		// reconstruct original stack pointer
	stdu	r0,-STK_SIZE(r1)	// push minimal frame on stack
	bctrl				// "goto" target
	tw	31,0,0			// NOTREACHED
C_TEXT_END(GotoExpedientC)

C_TEXT_ENTRY(GotoLegitimateAsm)
	mtctr	r4			// move target address to ctr
	mr	r1,r3			// pop everything but ExpRegs from stk
	ld	r0,ER_cr(r1)		// restore cr from ExpRegs
	mtcr	r0
	lfd	f0,ER_f(0)(r1)		// restore fpr's
	lfd	f1,ER_f(1)(r1)
	lfd	f2,ER_f(2)(r1)
	lfd	f3,ER_f(3)(r1)
	lfd	f4,ER_f(4)(r1)
	lfd	f5,ER_f(5)(r1)
	lfd	f6,ER_f(6)(r1)
	lfd	f7,ER_f(7)(r1)
	lfd	f8,ER_f(8)(r1)
	lfd	f9,ER_f(9)(r1)
	lfd	f10,ER_f(10)(r1)
	lfd	f11,ER_f(11)(r1)
	lfd	f12,ER_f(12)(r1)
	lfd	f13,ER_f(13)(r1)
	lfd	f14,ER_f(14)(r1)
	lfd	f15,ER_f(15)(r1)
	lfd	f16,ER_f(16)(r1)
	lfd	f17,ER_f(17)(r1)
	lfd	f18,ER_f(18)(r1)
	lfd	f19,ER_f(19)(r1)
	lfd	f20,ER_f(20)(r1)
	lfd	f21,ER_f(21)(r1)
	lfd	f22,ER_f(22)(r1)
	lfd	f23,ER_f(23)(r1)
	lfd	f24,ER_f(24)(r1)
	lfd	f25,ER_f(25)(r1)
	lfd	f26,ER_f(26)(r1)
	lfd	f27,ER_f(27)(r1)
	lfd	f28,ER_f(28)(r1)
	lfd	f29,ER_f(29)(r1)
	lfd	f30,ER_f(30)(r1)
	lfd	f31,ER_f(31)(r1)
	ld	r0,ER_r0(r1)		// restore r0
	ld	r3,ER_v_r(3)(r1)	// restore gpr's r3-r12
	ld	r4,ER_v_r(4)(r1)
	ld	r5,ER_v_r(5)(r1)
	ld	r6,ER_v_r(6)(r1)
	ld	r7,ER_v_r(7)(r1)
	ld	r8,ER_v_r(8)(r1)
	ld	r9,ER_v_r(9)(r1)
	ld	r10,ER_v_r(10)(r1)
	ld	r11,ER_v_r(11)(r1)
	ld	r12,ER_v_r(12)(r1)
	ld	r14,ER_nv_r(14)(r1)	// restore non-volatile gpr's r14-r31
	ld	r15,ER_nv_r(15)(r1)
	ld	r16,ER_nv_r(16)(r1)
	ld	r17,ER_nv_r(17)(r1)
	ld	r18,ER_nv_r(18)(r1)
	ld	r19,ER_nv_r(19)(r1)
	ld	r20,ER_nv_r(20)(r1)
	ld	r21,ER_nv_r(21)(r1)
	ld	r22,ER_nv_r(22)(r1)
	ld	r23,ER_nv_r(23)(r1)
	ld	r24,ER_nv_r(24)(r1)
	ld	r25,ER_nv_r(25)(r1)
	ld	r26,ER_nv_r(26)(r1)
	ld	r27,ER_nv_r(27)(r1)
	ld	r28,ER_nv_r(28)(r1)
	ld	r29,ER_nv_r(29)(r1)
	ld	r30,ER_nv_r(30)(r1)
	ld	r31,ER_nv_r(31)(r1)
	la	r1,ER_SIZE(r1)		// pop ExpRegs from stack
	bctr				// goto target
C_TEXT_END(GotoLegitimateAsm)

C_TEXT_ENTRY(CallExpedientC)
	mtctr	r3			// move target address to ctr
	std	r0,ER_r0(r1)		// save r0
	std	r4,ER_v_r(4)(r1)	// save volatile gpr's r4-r12
	std	r4,ER_v_r(4)(r1)
	std	r5,ER_v_r(5)(r1)
	std	r6,ER_v_r(6)(r1)
	std	r7,ER_v_r(7)(r1)
	std	r8,ER_v_r(8)(r1)
	std	r9,ER_v_r(9)(r1)
	std	r10,ER_v_r(10)(r1)
	std	r11,ER_v_r(11)(r1)
	std	r12,ER_v_r(12)(r1)
	std	r14,ER_nv_r(14)(r1)	// save non-volatile gpr's r14-r31
	std	r15,ER_nv_r(15)(r1)
	std	r16,ER_nv_r(16)(r1)
	std	r17,ER_nv_r(17)(r1)
	std	r18,ER_nv_r(18)(r1)
	std	r19,ER_nv_r(19)(r1)
	std	r20,ER_nv_r(20)(r1)
	std	r21,ER_nv_r(21)(r1)
	std	r22,ER_nv_r(22)(r1)
	std	r23,ER_nv_r(23)(r1)
	std	r24,ER_nv_r(24)(r1)
	std	r25,ER_nv_r(25)(r1)
	std	r26,ER_nv_r(26)(r1)
	std	r27,ER_nv_r(27)(r1)
	std	r28,ER_nv_r(28)(r1)
	std	r29,ER_nv_r(29)(r1)
	std	r30,ER_nv_r(30)(r1)
	std	r31,ER_nv_r(31)(r1)
	stfd	f0,ER_f(0)(r1)		// save fpr's
	stfd	f1,ER_f(1)(r1)
	stfd	f2,ER_f(2)(r1)
	stfd	f3,ER_f(3)(r1)
	stfd	f4,ER_f(4)(r1)
	stfd	f5,ER_f(5)(r1)
	stfd	f6,ER_f(6)(r1)
	stfd	f7,ER_f(7)(r1)
	stfd	f8,ER_f(8)(r1)
	stfd	f9,ER_f(9)(r1)
	stfd	f10,ER_f(10)(r1)
	stfd	f11,ER_f(11)(r1)
	stfd	f12,ER_f(12)(r1)
	stfd	f13,ER_f(13)(r1)
	stfd	f14,ER_f(14)(r1)
	stfd	f15,ER_f(15)(r1)
	stfd	f16,ER_f(16)(r1)
	stfd	f17,ER_f(17)(r1)
	stfd	f18,ER_f(18)(r1)
	stfd	f19,ER_f(19)(r1)
	stfd	f20,ER_f(20)(r1)
	stfd	f21,ER_f(21)(r1)
	stfd	f22,ER_f(22)(r1)
	stfd	f23,ER_f(23)(r1)
	stfd	f24,ER_f(24)(r1)
	stfd	f25,ER_f(25)(r1)
	stfd	f26,ER_f(26)(r1)
	stfd	f27,ER_f(27)(r1)
	stfd	f28,ER_f(28)(r1)
	stfd	f29,ER_f(29)(r1)
	stfd	f30,ER_f(30)(r1)
	stfd	f31,ER_f(31)(r1)
	mfcr	r0			// save cr
	std	r0,ER_cr(r1)
	mflr	r0			// store return addr in caller's frame
	std	r0,(ER_SIZE+STK_LR)(r1)
	mr	r3,r1			// ExpRegs ptr is argument to target
	la	r0,ER_SIZE(r1)		// reconstruct original stack pointer
	stdu	r0,-STK_SIZE(r1)	// push minimal frame on stack
	bctrl				// call target
	la	r1,STK_SIZE(r1)		// pop frame from stack
					// recover return address
	ld	r0,(ER_SIZE+STK_LR)(r1)
	mtlr	r0
	ld	r0,ER_cr(r1)		// restore cr
	mtcr	r0
	lfd	f0,ER_f(0)(r1)		// restore fpr's
	lfd	f1,ER_f(1)(r1)
	lfd	f2,ER_f(2)(r1)
	lfd	f3,ER_f(3)(r1)
	lfd	f4,ER_f(4)(r1)
	lfd	f5,ER_f(5)(r1)
	lfd	f6,ER_f(6)(r1)
	lfd	f7,ER_f(7)(r1)
	lfd	f8,ER_f(8)(r1)
	lfd	f9,ER_f(9)(r1)
	lfd	f10,ER_f(10)(r1)
	lfd	f11,ER_f(11)(r1)
	lfd	f12,ER_f(12)(r1)
	lfd	f13,ER_f(13)(r1)
	lfd	f14,ER_f(14)(r1)
	lfd	f15,ER_f(15)(r1)
	lfd	f16,ER_f(16)(r1)
	lfd	f17,ER_f(17)(r1)
	lfd	f18,ER_f(18)(r1)
	lfd	f19,ER_f(19)(r1)
	lfd	f20,ER_f(20)(r1)
	lfd	f21,ER_f(21)(r1)
	lfd	f22,ER_f(22)(r1)
	lfd	f23,ER_f(23)(r1)
	lfd	f24,ER_f(24)(r1)
	lfd	f25,ER_f(25)(r1)
	lfd	f26,ER_f(26)(r1)
	lfd	f27,ER_f(27)(r1)
	lfd	f28,ER_f(28)(r1)
	lfd	f29,ER_f(29)(r1)
	lfd	f30,ER_f(30)(r1)
	lfd	f31,ER_f(31)(r1)
	ld	r4,ER_v_r(4)(r1)	// restore volatile gpr's r4-r12
	ld	r5,ER_v_r(5)(r1)
	ld	r6,ER_v_r(6)(r1)
	ld	r7,ER_v_r(7)(r1)
	ld	r8,ER_v_r(8)(r1)
	ld	r9,ER_v_r(9)(r1)
	ld	r10,ER_v_r(10)(r1)
	ld	r11,ER_v_r(11)(r1)
	ld	r12,ER_v_r(12)(r1)
	ld	r14,ER_nv_r(14)(r1)	// restore non-volatile gpr's r14-r31
	ld	r15,ER_nv_r(15)(r1)
	ld	r16,ER_nv_r(16)(r1)
	ld	r17,ER_nv_r(17)(r1)
	ld	r18,ER_nv_r(18)(r1)
	ld	r19,ER_nv_r(19)(r1)
	ld	r20,ER_nv_r(20)(r1)
	ld	r21,ER_nv_r(21)(r1)
	ld	r22,ER_nv_r(22)(r1)
	ld	r23,ER_nv_r(23)(r1)
	ld	r24,ER_nv_r(24)(r1)
	ld	r25,ER_nv_r(25)(r1)
	ld	r26,ER_nv_r(26)(r1)
	ld	r27,ER_nv_r(27)(r1)
	ld	r28,ER_nv_r(28)(r1)
	ld	r29,ER_nv_r(29)(r1)
	ld	r30,ER_nv_r(30)(r1)
	ld	r31,ER_nv_r(31)(r1)
	ld	r0,ER_r0(r1)		// restore r0
	blr				// return
C_TEXT_END(CallExpedientC)

C_TEXT_ENTRY(CallLegitimateAsm)
	FULLSAVE_FRAME_ENTER(2,r0)
	mtctr	r4			// move target address to ctr
	std	r3,STK_LOCAL0(r1)	// save ExpRegs ptr in frame
	ld	r0,ER_cr(r3)		// restore cr from ExpRegs
	mtcr	r0
	lfd	f0,ER_f(0)(r3)		// restore fpr's
	lfd	f1,ER_f(1)(r3)
	lfd	f2,ER_f(2)(r3)
	lfd	f3,ER_f(3)(r3)
	lfd	f4,ER_f(4)(r3)
	lfd	f5,ER_f(5)(r3)
	lfd	f6,ER_f(6)(r3)
	lfd	f7,ER_f(7)(r3)
	lfd	f8,ER_f(8)(r3)
	lfd	f9,ER_f(9)(r3)
	lfd	f10,ER_f(10)(r3)
	lfd	f11,ER_f(11)(r3)
	lfd	f12,ER_f(12)(r3)
	lfd	f13,ER_f(13)(r3)
	lfd	f14,ER_f(14)(r3)
	lfd	f15,ER_f(15)(r3)
	lfd	f16,ER_f(16)(r3)
	lfd	f17,ER_f(17)(r3)
	lfd	f18,ER_f(18)(r3)
	lfd	f19,ER_f(19)(r3)
	lfd	f20,ER_f(20)(r3)
	lfd	f21,ER_f(21)(r3)
	lfd	f22,ER_f(22)(r3)
	lfd	f23,ER_f(23)(r3)
	lfd	f24,ER_f(24)(r3)
	lfd	f25,ER_f(25)(r3)
	lfd	f26,ER_f(26)(r3)
	lfd	f27,ER_f(27)(r3)
	lfd	f28,ER_f(28)(r3)
	lfd	f29,ER_f(29)(r3)
	lfd	f30,ER_f(30)(r3)
	lfd	f31,ER_f(31)(r3)
	ld	r0,ER_r0(r3)		// restore r0
	ld	r4,ER_v_r(4)(r3)	// restore volatile gpr's r4-r12
	ld	r5,ER_v_r(5)(r3)
	ld	r6,ER_v_r(6)(r3)
	ld	r7,ER_v_r(7)(r3)
	ld	r8,ER_v_r(8)(r3)
	ld	r9,ER_v_r(9)(r3)
	ld	r10,ER_v_r(10)(r3)
	ld	r11,ER_v_r(11)(r3)
	ld	r12,ER_v_r(12)(r3)
	ld	r14,ER_nv_r(14)(r3)	// restore non-volatile gpr's r14-r31
	ld	r15,ER_nv_r(15)(r3)
	ld	r16,ER_nv_r(16)(r3)
	ld	r17,ER_nv_r(17)(r3)
	ld	r18,ER_nv_r(18)(r3)
	ld	r19,ER_nv_r(19)(r3)
	ld	r20,ER_nv_r(20)(r3)
	ld	r21,ER_nv_r(21)(r3)
	ld	r22,ER_nv_r(22)(r3)
	ld	r23,ER_nv_r(23)(r3)
	ld	r24,ER_nv_r(24)(r3)
	ld	r25,ER_nv_r(25)(r3)
	ld	r26,ER_nv_r(26)(r3)
	ld	r27,ER_nv_r(27)(r3)
	ld	r28,ER_nv_r(28)(r3)
	ld	r29,ER_nv_r(29)(r3)
	ld	r30,ER_nv_r(30)(r3)
	ld	r31,ER_nv_r(31)(r3)
	ld	r3,ER_v_r(3)(r3)	// restore r3 last
	bctrl				// call target
	std	r3,STK_LOCAL1(r1)	// save r3 temporarily
	ld	r3,STK_LOCAL0(r1)	// retrieve ExpRegs ptr from frame
	std	r0,ER_r0(r3)		// save r0 in ExpRegs
	mfcr	r0			// save cr
	std	r0,ER_cr(r3)
	ld	r0,STK_LOCAL1(r1)	// retrieve r3
	std	r0,ER_v_r(3)(r3)	// save r3
	std	r4,ER_v_r(4)(r3)	// save volatile gpr's r4-r12
	std	r5,ER_v_r(5)(r3)
	std	r6,ER_v_r(6)(r3)
	std	r7,ER_v_r(7)(r3)
	std	r8,ER_v_r(8)(r3)
	std	r9,ER_v_r(9)(r3)
	std	r10,ER_v_r(10)(r3)
	std	r11,ER_v_r(11)(r3)
	std	r12,ER_v_r(12)(r3)
	std	r14,ER_nv_r(14)(r3)	// save non-volatile gpr's r14-r31
	std	r15,ER_nv_r(15)(r3)
	std	r16,ER_nv_r(16)(r3)
	std	r17,ER_nv_r(17)(r3)
	std	r18,ER_nv_r(18)(r3)
	std	r19,ER_nv_r(19)(r3)
	std	r20,ER_nv_r(20)(r3)
	std	r21,ER_nv_r(21)(r3)
	std	r22,ER_nv_r(22)(r3)
	std	r23,ER_nv_r(23)(r3)
	std	r24,ER_nv_r(24)(r3)
	std	r25,ER_nv_r(25)(r3)
	std	r26,ER_nv_r(26)(r3)
	std	r27,ER_nv_r(27)(r3)
	std	r28,ER_nv_r(28)(r3)
	std	r29,ER_nv_r(29)(r3)
	std	r30,ER_nv_r(30)(r3)
	std	r31,ER_nv_r(31)(r3)
	stfd	f0,ER_f(0)(r3)		// save fpr's
	stfd	f1,ER_f(1)(r3)
	stfd	f2,ER_f(2)(r3)
	stfd	f3,ER_f(3)(r3)
	stfd	f4,ER_f(4)(r3)
	stfd	f5,ER_f(5)(r3)
	stfd	f6,ER_f(6)(r3)
	stfd	f7,ER_f(7)(r3)
	stfd	f8,ER_f(8)(r3)
	stfd	f9,ER_f(9)(r3)
	stfd	f10,ER_f(10)(r3)
	stfd	f11,ER_f(11)(r3)
	stfd	f12,ER_f(12)(r3)
	stfd	f13,ER_f(13)(r3)
	stfd	f14,ER_f(14)(r3)
	stfd	f15,ER_f(15)(r3)
	stfd	f16,ER_f(16)(r3)
	stfd	f17,ER_f(17)(r3)
	stfd	f18,ER_f(18)(r3)
	stfd	f19,ER_f(19)(r3)
	stfd	f20,ER_f(20)(r3)
	stfd	f21,ER_f(21)(r3)
	stfd	f22,ER_f(22)(r3)
	stfd	f23,ER_f(23)(r3)
	stfd	f24,ER_f(24)(r3)
	stfd	f25,ER_f(25)(r3)
	stfd	f26,ER_f(26)(r3)
	stfd	f27,ER_f(27)(r3)
	stfd	f28,ER_f(28)(r3)
	stfd	f29,ER_f(29)(r3)
	stfd	f30,ER_f(30)(r3)
	stfd	f31,ER_f(31)(r3)
	FULLSAVE_FRAME_RETURN(2,r0)
C_TEXT_END(CallLegitimateAsm)

/*
 * CallLegitimateC(ExpRegs *erp, uval rc)
 *    Entered after a call from expedient-C to legitimate-C.  Save the return
 *    code in the appropriate slot and trash all the other volatile registers.
 */
C_TEXT_ENTRY(CallLegitimateC)
	// store return code in erp->r3
	std	r4,ER_v_r(3)(r3)	// store return code in erp->r3
	// trash other volatile gpr's in *erp
	li	r0,0
	std	r0,ER_r0(r3)		// trash other volatile gpr's
	std	r0,ER_v_r(4)(r3)
	std	r0,ER_v_r(5)(r3)
	std	r0,ER_v_r(6)(r3)
	std	r0,ER_v_r(7)(r3)
	std	r0,ER_v_r(8)(r3)
	std	r0,ER_v_r(9)(r3)
	std	r0,ER_v_r(10)(r3)
	std	r0,ER_v_r(11)(r3)
	std	r0,ER_v_r(12)(r3)
	// trash volatile fpr's in *erp
	std	r0,ER_f(0)(r3)		// trash volatile fpr's
	std	r0,ER_f(1)(r3)
	std	r0,ER_f(2)(r3)
	std	r0,ER_f(3)(r3)
	std	r0,ER_f(4)(r3)
	std	r0,ER_f(5)(r3)
	std	r0,ER_f(6)(r3)
	std	r0,ER_f(7)(r3)
	std	r0,ER_f(8)(r3)
	std	r0,ER_f(9)(r3)
	std	r0,ER_f(10)(r3)
	std	r0,ER_f(11)(r3)
	std	r0,ER_f(12)(r3)
	std	r0,ER_f(13)(r3)
	blr
C_TEXT_END(CallLegitimateC)
