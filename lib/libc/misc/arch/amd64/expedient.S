/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 *    Machinery for implementing assembler functionality cleanly, if
 *    inefficiently, in C.
 * **************************************************************************/

// VVV


#include <sys/kinclude.H>
#include <misc/asm.h>			
#include "misc/arch/amd64/fpr.h"          /* floating point register save area size */
#include <misc/expedient.H>
#include <sys/arch/amd64/asmConstants.H>

/* ABI requires rsp before a call to C to be be aligned 16 bytes, and hardware 
 * specs require  FPU/XMM save/restore to be 16 bytes aligned.
 * The ExpRegs structure which is saved-into and restored-from is built on 
 * the stack and must be 16 bytes aligned to satisfy the hardware specification 
 * (we always save an even number of gpr's in addition to the floating point
 * save).
 */

// r11 contains target address and rdi points to ExpRegs
C_TEXT_ENTRY(GotoExpedientC)
        FRAME_ENTER();
        call    *%r11;            // "goto" target
        int     $3

/* Before calling the actual assembler code the logical registers
 * are transfered into the machine corresponding real registers.
 * On entry rdi contains pointer to ExpRegs and rsi contains address of target.
 */

// rsi contains target address and rdi points to ExpRegs
C_TEXT_ENTRY(GotoLegitimateAsm)
	movq	%rsi, EXPREG_RSP(%rdi);	// save target address (from rsi)
	movq	%rdi,%rsp		// move expregs pointer to rsp for restore
	RESTORE_EXP_STATE();		// restore all Exp registers
        ret				// goto target, rsp back to before CALL_EXPEDIENT


// r11 contains target address and rdi points to ExpRegs
C_TEXT_ENTRY(CallExpedientC)
        FRAME_ENTER();
        call    *%r11;            // call target
        FRAME_RETURN()

/* Before calling the actual assembler code the logical registers
 * are transfered into the machine corresponding real registers.
 * but even before we must save the non volatile state for returning to C.
 * rdi contains ExpRegs address and rsi	the target function address
 */
C_TEXT_ENTRY(CallLegitimateAsm)
	FULLSAVE_FRAME_ENTER();	
	pushq	%rdi;			// save ExpRegs pointer for later use on return
	pushq	%rsi;			// save target pointer for the call
	movq	%rsp, EXPREG_RSP(%rdi);	// save rsp temporarily, needed for restoring from expregs
	movq	%rdi,%rsp		// move expregs pointer to rsp for restore
	RESTORE_EXP_STATE();		// restore all Exp registers
	popq	%rsp;			// restore rsp 
	call	*(%rsp);		// call target (from %rsi put on stack)
	leaq	8(%rsp),%rsp		// remove target address from stack
	pushq	%rdi			// save rdi on stack
	movq	8(%rsp),%rdi		// load rdi from stack saved value of expregs pointer
	movq	%rsp, EXPREG_RSP(%rdi);	// save rsp
	popq	%rdi			// restore rdi from stack 
	popq	%rsp			// set rsp to expregs pointer for save
	SAVE_EXP_STATE();		// save all pdated exp register values in ExpRegs
	movq	EXPREG_RSP(%rsp),%rsp;	// restore rsp
	leaq	16(%rsp),%rsp		// restore the stack where it was after FULLSAVE_FRAME_ENTER
	FULLSAVE_FRAME_RETURN()

/*      
 * CallLegitimateC(ExpRegs *erp, uval rc)
 *    Entered after a call from expedient-C to legitimate-C.  Save the return
 *    code in the appropriate slot and trash all the other VOLATILE registers.
 *    Floating point are (very) carefully trashed.
 */     
C_TEXT_ENTRY(CallLegitimateC)
	leaq	FPR_SAVE_AREA_SIZE(%rdi), %rdi;	// skip floating point registers
	movq	%rsi, 0(%rdi);		// store return code in rax
					// skip rbx
	movq	$0, 16(%rdi);		// rcx
	movq	$0, 24(%rdi);		// rdx
					// skip rbp
	movq	$0, 40(%rdi);		// rsi
	movq	$0, 48(%rdi);		// rdi
	movq	$0, 56(%rdi);		// r8
	movq	$0, 64(%rdi);		// r9
	movq	$0, 72(%rdi);		// r10
	movq	$0, 80(%rdi);		// r11
	
	ret

