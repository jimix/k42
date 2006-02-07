#ifndef __EXPEDIENT_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __EXPEDIENT_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: expedient.H,v 1.10 2001/10/25 16:26:01 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    Machinery for implementing assembler functionality cleanly, if
 *    inefficiently, in C.
 * **************************************************************************/

// VVV

#include "misc/arch/amd64/fpr.h"          /* floating point register save area size */

#ifndef __ASSEMBLER__


/* KEEP THIS STRUCTURE the same as struct DispatcherDefaultExpRegs */
/* KEEP THIS STRUCTURE the same as struct ExceptionExpRegs */
struct ExpRegs {

    uval64 float_savearea[FPR_SAVE_AREA_SIZE/sizeof(uval64)];	// floating point save/restore area

    uval64  rax;
    uval64  rbx;
    uval64  rcx;
    uval64  rdx;
    uval64  rbp;
    uval64  rsi;
    uval64  rdi;
    uval64  r8;
    uval64  r9;
    uval64  r10;
    uval64  r11;
    uval64  r12;
    uval64  r13;
    uval64  r14;
    uval64  r15;
    // MUST BE THE LAST ELEMENT of the structure
    // we use this location to save rsp (very temporary) and also to
    // store the function address we may call.  We do the call by a "ret"
    uval64  rsp;  // temporary save for restoring sp for CallLegitimateAsm
};

#else /* #ifndef __ASSEMBLER__ */

#define EXPREG_RSP   (FPR_SAVE_AREA_SIZE+(15*8))
#define EXPREG_SIZE  (FPR_SAVE_AREA_SIZE+(16*8))

#include <misc/asm.h>

/* when  goto/call an "expedient C" replacement for a TBD assembler routine,
 * by convention the C routine will find in the ExpRegs structure the machine
 * registers saved before passing control to the C routine.
 * The ExpRegs structure is built on the stack before passing its address
 * to the C routine in %rdi ( ABI). It must be 16 byte-aligned for both ABI
 * and hardware reason (FPU save/restore).
 * The reason to include the floating point registers in the ExpRegs is to
 * be able to pass floating point parameters if and when applicable thru a
 * ppc call.
 * Expedient C routines are called from assembler... of course.
 * ExpRegs contains a floating point save structure of 512 bytes and 128 bytes
 * to save gprs for a total of 640 bytes.
 * r11 is a scratch register (ABI).
 */


#define GOTO_EXPEDIENT(symbol)						\
	leaq	-EXPREG_SIZE(%rsp),%rsp;  /* create space for ExpRegs structure */	\
	SAVE_EXP_STATE();		 /* create ExpRegs structure */	\
	movq	%rsp, %rdi;						\
	LOAD_C_TEXT_ADDR(%r11, symbol);					\
	call    C_TEXT(GotoExpedientC);					\
	int	$3

#define CALL_EXPEDIENT(symbol)		\
	leaq	-EXPREG_SIZE(%rsp),%rsp;  /* create space for ExpRegs structure */	\
	SAVE_EXP_STATE();		 /* create ExpRegs structure */	\
	movq	%rsp, %rdi;						\
	LOAD_C_TEXT_ADDR(%r11, symbol);					\
	call    C_TEXT(CallExpedientC);					\
	RESTORE_EXP_STATE();						\
	leaq	8(%rsp),%rsp		/* discard ExpRegs structure space */

/*
 *  SAVE_EXP_STATE and RESTORE_EXP_STATE MUST be used with %rsp pointing to
 *  the first byte of the ExpRegs structure.
 */

/*
 *  For SAVE, we then move it to the end (except for the rsp field which is
 *  a temporary save area), and push to fill it.  At the end rsp is again
 *  pointing to the first byte of the ExpRegs structure.
 */

#define SAVE_EXP_STATE()			\
	leaq	EXPREG_RSP(%rsp),%rsp;		\
	pushq	%r15;				\
	pushq	%r14;				\
	pushq	%r13;				\
	pushq	%r12;				\
	pushq	%r11;				\
	pushq	%r10;				\
	pushq	%r9;				\
	pushq	%r8;				\
	pushq	%rdi;				\
	pushq	%rsi;				\
	pushq	%rbp;				\
	pushq	%rdx;				\
	pushq	%rcx;				\
	pushq	%rbx;				\
	pushq	%rax;				\
	leaq	-FPR_SAVE_AREA_SIZE(%rsp), %rsp;	\
	SAVE_FLOAT_(%rsp,%rax)				\
	cld

/*
 *  For RESTORE, we just pop all the contents out of the ExpRegs structure,
 *  and leave rsp pointing to the last field (the %rsp field).
 */

#define RESTORE_EXP_STATE()				\
	RESTORE_FLOAT_(%rsp,%rax)			\
	leaq	FPR_SAVE_AREA_SIZE(%rsp), %rsp;		\
	popq	%rax;					\
	popq	%rbx;					\
	popq	%rcx;					\
	popq	%rdx;					\
	popq	%rbp;					\
	popq	%rsi;					\
	popq	%rdi;					\
	popq	%r8;					\
	popq	%r9;					\
	popq	%r10;					\
	popq	%r11;					\
	popq	%r12;					\
	popq	%r13;					\
	popq	%r14;					\
	popq	%r15       /* leave %rsp pointing to rsp field */



#endif /* #ifndef __ASSEMBLER__ */
