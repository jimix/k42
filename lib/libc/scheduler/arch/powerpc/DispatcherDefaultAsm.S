/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultAsm.S,v 1.130 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: support routines for hand-written assembler
 * **************************************************************************/
#include <sys/kinclude.H>
#include <sys/config.H>
#include <sys/syscalls.H>
#include <misc/asm.h>
#include <misc/hardware.H>
#include <misc/expedient.H>
#include <misc/volatileFrame.H>
#include <misc/arch/powerpc/asdef.h>
#include <misc/arch/powerpc/trap.h>
#include <sys/arch/powerpc/asmConstants.H>
#include <defines/use_expedient.H>

TOC_C_DATA_ENTRY(kernelInfoLocal)
TOC_C_DATA_ENTRY(extRegsLocal)
TOC_C_DATA_ENTRY(activeThrdCntLocal)

TOC_C_DATA_ENTRY(DispatcherDefaultExp_RunEntry)
TOC_C_DATA_ENTRY(DispatcherDefaultExp_PPCClient)
TOC_C_DATA_ENTRY(DispatcherDefaultExp_IPCCallEntry)
TOC_C_DATA_ENTRY(DispatcherDefaultExp_IPCReturnEntry)
TOC_C_DATA_ENTRY(DispatcherDefaultExp_PPCServerOnThread)
TOC_C_DATA_ENTRY(DispatcherDefaultExp_IPCFaultEntry)
TOC_C_DATA_ENTRY(DispatcherDefaultExp_IPCFaultOnThread)

#define GET_EXTREGS_AND_DISPATCHER(extRegsReg,dispatcherReg)\
	LOAD_C_DATA_ADDR(extRegsReg,extRegsLocal);\
	ld	dispatcherReg,XR_dispatcher(extRegsReg)

#define GET_DISPATCHER(dispatcherReg)\
	LOAD_C_DATA_UVAL(dispatcherReg,extRegsLocal,XR_dispatcher);\

#define LOAD_FPSCR(dispatcherReg,scratchFPR)\
	lfd	scratchFPR,DD_fpscr(dispatcherReg);\
	mtfsf	0xff,scratchFPR

#define DISPATCHER_STACK(dispatcherReg,scratchReg)\
	ld	r1,DD_dispatcherStack(dispatcherReg);\
	li	scratchReg,0;\
	stdu	scratchReg,-STK_SIZE(r1)

#define DEBUGGER_STACK(dispatcherReg,scratchReg)\
	ld	r1,DD_currentDebugStack(dispatcherReg);\
	li	scratchReg,0;\
	stdu	scratchReg,-STK_SIZE(r1)

#define THREAD_STACK(threadReg,reservation,scratchReg)\
	ld	r1,TH_startSP(threadReg);\
	li	scratchReg,0;\
	stdu    scratchReg,-(STK_SIZE+((reservation)*8))(r1)

#define GET_CURTHREAD_AND_STACK(vsReg)\
	ld	r13,VS_r13(vsReg);\
	ld	r1,VS_r1(vsReg)

#define LOAD_USER_STATE_ADDR(reg,dispatcherReg)\
	ld	reg,D__userStateOffset(dispatcherReg);\
	add	reg,dispatcherReg,reg

#define LOAD_TRAP_STATE_ADDR(reg,dispatcherReg)\
	ld	reg,D__trapStateOffset(dispatcherReg);\
	add	reg,dispatcherReg,reg


ENTRY_POINT_DESC(DispatcherDefault_RunEntry_Desc, DispatcherDefault_RunEntry)
ENTRY_POINT_DESC(DispatcherDefault_InterruptEntry_Desc, DispatcherDefault_InterruptEntry)
ENTRY_POINT_DESC(DispatcherDefault_TrapEntry_Desc, DispatcherDefault_TrapEntry)
ENTRY_POINT_DESC(DispatcherDefault_PgfltEntry_Desc, DispatcherDefault_PgfltEntry)
ENTRY_POINT_DESC(DispatcherDefault_IPCCallEntry_Desc, DispatcherDefault_IPCCallEntry)
ENTRY_POINT_DESC(DispatcherDefault_IPCReturnEntry_Desc, DispatcherDefault_IPCReturnEntry)
ENTRY_POINT_DESC(DispatcherDefault_IPCFaultEntry_Desc, DispatcherDefault_IPCFaultEntry)
ENTRY_POINT_DESC(DispatcherDefault_SVCEntry_Desc, DispatcherDefault_SVCEntry)

ENTRY_POINT_DESC(DispatcherDefault_SVCDirect_Desc, DispatcherDefault_SVCDirect)

#ifndef	NDEBUG
/*
 * In debugging builds we check for stack overflow when a thread is
 * suspended or interrupted.
 * When a thread is returned to the free list, we check whether it has
 * stepped on the last 16 words of its stack.
 * We check for a primitive PPC not guarded by allowPrimitivePPC.
 */
#define DEBUG_CHK_STK_SUSPEND(threadReg,scratchReg)\
	ld	scratchReg,TH_bottomSP(threadReg);\
	tdllt	r1,scratchReg /* test is okay even if bottomSP is 0 */
#define DEBUG_CHK_STK_INTERRUPT(dspReg,scratchReg)\
	ld	scratchReg,DD_sandboxShepherd(dspReg);\
	cmpldi	scratchReg,0;\
	bne-	1f;\
	LOAD_USER_STATE_ADDR(r1,dspReg);\
	ld	r13,VS_r13(r1);\
	ld	r1,VS_r1(r1);\
	DEBUG_CHK_STK_SUSPEND(r13,scratchReg);\
    1:
#define DEBUG_CHK_STK_FREE_THREAD(threadReg,regA,regB,regC,regD)\
	li	regC,0xbf;\
	rldimi	regC,regC,8,48;\
	rldimi	regC,regC,16,32;\
	rldimi	regC,regC,32,0;\
	mr	regD,regC;\
	ld	regA,TH_bottomSP(threadReg);\
	cmpdi	regA,0;\
	beq	$ + (16*3 + 2)*4;\
	ld	regB,( 0*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 1*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 2*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 3*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 4*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 5*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 6*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 7*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 8*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,( 9*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,(10*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,(11*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,(12*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,(13*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,(14*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	ld	regB,(15*8)(regA); or regC,regC,regB; and regD,regD,regB;\
	tdne	regC,regD
#define DEBUG_CHK_THREAD_GROUPS(threadReg,scratchReg)\
	lhz	scratchReg,TH_groups(threadReg);\
	/*tdnei	scratchReg,0*/
#define DEBUG_CHK_ALLOW_PRIMITIVE_PPC(dspReg,scratchReg)\
	ld	scratchReg,DD_allowPrimitivePPCFlag(dspReg);\
       	cmpldi	scratchReg,0; /* FIXME: for simos breakpoint only*/ \
	bne	$+8;          /* FIXME: for simos breakpoint only*/ \
        .long 0x7C0007CE;     /* FIXME: for simos breakpoint only*/ \
	tdeqi	scratchReg,0
#else	// NDEBUG
#define DEBUG_CHK_STK_SUSPEND(threadReg,scratchReg)
#define DEBUG_CHK_STK_INTERRUPT(dspReg,scratchReg)
#define DEBUG_CHK_STK_FREE_THREAD(threadReg,regA,regB,regC,regD)
#define DEBUG_CHK_THREAD_GROUPS(threadReg,scratchReg)
#define DEBUG_CHK_ALLOW_PRIMITIVE_PPC(dspReg,scratchReg)
#endif	// NDEBUG

/*
 * extern "C" void DispatcherDefault_InitThread(Thread *thread,
 * 						Scheduler::ThreadFunction fct,
 * 						uval data);
 */
C_TEXT_ENTRY(DispatcherDefault_InitThread)
	LEAF_ENTER(r0)
	ld	r6,TH_startSP(r3)	// r6 is thread's simulated stack ptr
	li	r0,0			// create minimal frame
	stdu	r0,-STK_SIZE(r6)
	std	r4,STK_PARAM0(r6)	// store fct arg in frame
	std	r5,STK_PARAM1(r6)	// store data arg in frame
	bl	InitThread_SuspendCore
	// new thread will start here after return from SuspendCore_Continue
	// (NORETURN) ThreadBase(dispatcher, fct, data);
	ld	r5,STK_PARAM1(r1)	// r5 = data
	ld	r4,STK_PARAM0(r1)	// r4 =  fct
	mr	r3,r12			// r3 = dispatcher
	li	r0,0			// clear lr since "call" can't return
	mtlr	r0
	b	C_TEXT(DispatcherDefault_ThreadBase) // "call" ThreadBase
	// NOTREACHED
    InitThread_SuspendCore:
	// mimic SuspendCore
	mflr	r0			// mimic LEAF_ENTER()
	std	r0,STK_LR(r6)
	std	r6,TH_curSP(r3) 	// save stack ptr in thread object
	// now return to our caller, rather than branching to RunEntry
	LEAF_RETURN(r0)
C_TEXT_END(DispatcherDefault_InitThread)

/*
 * extern "C" void DispatcherDefault_InitThreadGeneral(
 *					Thread *thread,
 * 					Scheduler::ThreadFunctionGeneral fct,
 * 					uval len, char *data);
 */
C_TEXT_ENTRY(DispatcherDefault_InitThreadGeneral)
	LEAF_ENTER(r0)
	ld	r7,TH_startSP(r3)	// r7 is thread's simulated stack ptr
	cmpldi	r5,0			// skip data copy if len == 0
	beq-	InitThreadGeneral_CopyDone
	addi	r8,r5,7			// round len up to uval boundary
	rldicr	r8,r8,0,60
	sub	r7,r7,r8		// buy space on stack for data
	la	r8,(-1)(r6)		// setup pointers and ctr for copy
	la	r9,(-1)(r7)
	mtctr	r5
    InitThreadGeneral_CopyLoop:		// copy data a byte at a time
	lbzu	r0,1(r8)
	stbu	r0,1(r9)
	bdnz	InitThreadGeneral_CopyLoop
    InitThreadGeneral_CopyDone:
	mr	r6,r7			// data is now on stack
	li	r0,0			// create minimal frame
	stdu	r0,-STK_SIZE(r7)
	std	r4,STK_PARAM0(r7)	// store fct arg in frame
	std	r5,STK_PARAM1(r7)	// store len arg in frame
	std	r6,STK_PARAM2(r7)	// store data arg in frame
	bl	InitThreadGeneral_SuspendCore
	// new thread will start here after return from SuspendCore_Continue
	// (NORETURN) ThreadBaseGeneral(dispatcher, fct, len, data);
	ld	r6,STK_PARAM2(r1)	// r6 = data
	ld	r5,STK_PARAM1(r1)	// r5 = len
	ld	r4,STK_PARAM0(r1)	// r4 = fct
	mr	r3,r12			// r3 = dispatcher
	li	r0,0			// clear lr since "call" can't return
	mtlr	r0
	b	C_TEXT(DispatcherDefault_ThreadBaseGeneral) // "call" ThreadBase
	// NOTREACHED
    InitThreadGeneral_SuspendCore:
	// mimic SuspendCore
	mflr	r0			// mimic LEAF_ENTER()
	std	r0,STK_LR(r7)
	std	r7,TH_curSP(r3) 	// save stack ptr in thread object
	// now return to our caller, rather than branching to RunEntry
	LEAF_RETURN(r0)
C_TEXT_END(DispatcherDefault_InitThreadGeneral)

/*
 * extern "C" void
 *	DispatcherDefault_InterruptThread(Thread *thread,
 *					  Scheduler::InterruptFunction fct,
 *					  uval data);
 */
C_TEXT_ENTRY(DispatcherDefault_InterruptThread)
	LEAF_ENTER(r0)
	ld	r6,TH_curSP(r3)		// r6 is thread's simulated stack ptr
	stdu	r6,-STK_SIZE(r6)	// create minimal frame
	std	r4,STK_PARAM0(r6)	// store fct arg in frame
	std	r5,STK_PARAM1(r6)	// store data arg in frame
	bl	InterruptThread_SuspendCore
	// new thread will start here after return from SuspendCore_Continue
	// call fct(data)
	ld	r3,STK_PARAM1(r1)	// r3 = data
	ld	r4,STK_PARAM0(r1)	// r4 =  fct
	std	r2,STK_TOC(r1)		// preserve TOC
	ld	r0,0(r4)		// fetch function IAR
	mtlr	r0
	ld	r2,8(r4)		// load function TOC
	blrl				// call function
	ld	r2,STK_TOC(r1)		// restore TOC
	GET_DISPATCHER(r12)		// re-fetch dispatcher (the code we are
					//     resuming expects it)
	/*
	 * Now proceed as in SuspendCore_Continue, except that our continuation
	 * stack pointer is on our stack rather than in the thread structure.
	 */
	ld	r1,STK_BACKCHAIN(r1)	// pop frame from stack
	LEAF_RETURN(r0)			// mimic SuspendCore_Continue
	// NOTREACHED
    InterruptThread_SuspendCore:
	// mimic SuspendCore
	mflr	r0			// mimic LEAF_ENTER()
	std	r0,STK_LR(r6)
	std	r6,TH_curSP(r3) 	// save stack ptr in thread object
	// now return to our caller, rather than branching to RunEntry
	LEAF_RETURN(r0)
C_TEXT_END(DispatcherDefault_InterruptThread)

/*
 * extern "C" void DispatcherDefault_Suspend(DispatcherDefault *dispatcher);
 */
C_TEXT_ENTRY(DispatcherDefault_Suspend)
	FULLSAVE_FRAME_ENTER(0, r0)
	mr	r12,r3			// move dispatcher to expected register
	bl	CODE(DispatcherDefault_SuspendCore)
	FULLSAVE_FRAME_RETURN(0, r0)
C_TEXT_END(DispatcherDefault_Suspend)

CODE_ENTRY(DispatcherDefault_SuspendCore)
	LEAF_ENTER(r0)
	std	r1,TH_curSP(r13)
	DEBUG_CHK_STK_SUSPEND(r13,r11)
	b	RunEntryInternal
CODE_END(DispatcherDefault_SuspendCore)

CODE_ENTRY(DispatcherDefault_SuspendCore_Continue)
	ld	r1,TH_curSP(r13)
	LEAF_RETURN(r0)
CODE_END(DispatcherDefault_SuspendCore_Continue)

/*
 * extern "C" void DispatcherDefault_GotoRunEntry(DispatcherDefault
 * 							 *dispatcher);
 */
C_TEXT_ENTRY(DispatcherDefault_GotoRunEntry)
	mr	r12,r3			// move dispatcher to expected register
	b	RunEntryInternal
C_TEXT_END(DispatcherDefault_GotoRunEntry)

/*
 * extern "C" SysStatus
 *     DispatcherDefault_SetEntryPoint(EntryPointNumber entryPoint,
 *                                     EntryPointDesc entry);
 */
C_TEXT_ENTRY(DispatcherDefault_SetEntryPoint)
	LEAF_ENTER(r0)
	std	r2,STK_TOC(r1)
	bl	L0
	ld	r2,STK_TOC(r1)
	LEAF_RETURN(r0)
    L0:	lis	r0,SYSCALL_SET_ENTRY_POINT
	sc
C_TEXT_END(DispatcherDefault_SetEntryPoint)

/*
 * extern "C" void DispatcherDefault_HandoffProcessor(CommID targetID);
 */
C_TEXT_ENTRY(DispatcherDefault_HandoffProcessor)
	FULLSAVE_FRAME_ENTER(0, r0)
	// targetID stays in r3
	bl	CODE(DispatcherDefault_HandoffProcessorCore)
	FULLSAVE_FRAME_RETURN(0, r0)
C_TEXT_END(DispatcherDefault_HandoffProcessor)

CODE_ENTRY(DispatcherDefault_HandoffProcessorCore)
	LEAF_ENTER(r12)
	std	r1,TH_curSP(r13)
	DEBUG_CHK_STK_SUSPEND(r13,r12)
	b	CODE(DispatcherDefault_SyscallHandoffProcessor)
	// does not return here;  current thread is on ready queue and will
	// wake up in SuspendCore_Continue.
CODE_END(DispatcherDefault_HandoffProcessorCore)

#ifndef	NDEBUG
	/*
	 * To help with debugging, we set things up to look as if the
	 * interrupted thread called this entry-point code immediately prior to
	 * its next real instruction.  We store the address of that instruction
	 * in the return-address slot at the top of the old stack.  We have to
	 * preserve the current value in that slot because we can't be sure
	 * it's not live.  We assume that the frame was created with a
	 * PUSH_VOLATILE_STATE_FRAME() with the given "floor", and that
	 * "vsOffset" and "saveOffset" are the stack pointer offsets of the
	 * VolatileState and a data slot to be used for preserving the old
	 * return address, respectively.  The original stack pointer can be
	 * reconstucted as ((floor)-((vsOffset)+VS_SIZE))(r1).
	 *
	 * None of this work is necessary for correctness, so we define two
	 * versions of the macros.
	 */

#define DEBUG_FIXUP(floor,vsOffset,saveOffset,scratchReg)\
	ld	scratchReg,-((floor)-((vsOffset)+VS_SIZE))+STK_LR(r1);\
	std	scratchReg,(saveOffset)(r1);\
	ld	scratchReg,((vsOffset)+VS_iar)(r1);\
	std	scratchReg,-((floor)-((vsOffset)+VS_SIZE))+STK_LR(r1)

#define DEBUG_RESTORE(floor,vsOffset,saveOffset,scratchReg)\
	ld	scratchReg,(saveOffset)(r1);\
	std	scratchReg,-((floor)-((vsOffset)+VS_SIZE))+STK_LR(r1)

#else	// NDEBUG

#define DEBUG_FIXUP(floor,vsOffset,saveOffset,scratchReg)
#define DEBUG_RESTORE(floor,vsOffset,saveOffset,scratchReg)

#endif	// NDEBUG

/*
 * Push a frame on the stack with "reservation" data slots and room for a
 * VolatileState (which will follow the data slots in the frame).  vsReg points
 * to a VolatileState; copy it into the frame.  "floor" is an offset from the
 * current stack pointer specifying how much space beyond the current stack
 * must be preserved.
 */
#define PUSH_VOLATILE_STATE_FRAME(vsReg,floor,reservation,scratchReg,ptrReg)\
	stdu	r1,(floor)-((STK_SIZE+((reservation)*8))+VS_SIZE)(r1);\
	li	scratchReg,(VS_SIZE/8);\
	mtctr	scratchReg;\
	la	vsReg,-8(vsReg);\
	la	ptrReg,((STK_SIZE+((reservation)*8))-8)(r1);\
    /*loop:*/\
	ldu	scratchReg,8(vsReg);\
	stdu	scratchReg,8(ptrReg);\
	bdnz	$-8

/*
 * Pop a VolatileState frame from the stack, copying the state to the address
 * contained in vsReg.
 */
#define POP_VOLATILE_STATE_FRAME(vsReg,floor,reservation,scratchReg,ptrReg)\
	li	scratchReg,(VS_SIZE/8);\
	mtctr	scratchReg;\
	la	vsReg,-8(vsReg);\
	la	ptrReg,((STK_SIZE+((reservation)*8))-8)(r1);\
    /*loop:*/\
	ldu	scratchReg,8(ptrReg);\
	stdu	scratchReg,8(vsReg);\
	bdnz	$-8;\
	la	r1,-((floor)-((STK_SIZE+((reservation)*8))+VS_SIZE))(r1)

/*
 * Continue execution from an in-memory volatileState area whose address is
 * in r3. On powerpc, the final steps of the resume are accomplished via a
 * USER_RFI or TRAP_RFI syscall.  The syscall expects the values for r0-r2,
 * r11-r13, cr, msr, and iar to be in the userState or trapState area of the
 * dispatcher, so we have to copy the values there from the in-memory area.
 */
CODE_ENTRY(ResumeFromMemory)
TrapResumeFromMemory:
	GET_DISPATCHER(r12)
	LOAD_TRAP_STATE_ADDR(r4,r12)	// r4 = current trap-state address
	lis	r0,SYSCALL_TRAP_RFI
	b	ResumeFromMemoryCommon
UserResumeFromMemory:
	GET_DISPATCHER(r12)
	LOAD_USER_STATE_ADDR(r4,r12)	// r4 = current user-state address
	lis	r0,SYSCALL_USER_RFI
ResumeFromMemoryCommon:
	ld	r13,VS_r0(r3)		// retrieve r0-r2, r11-r13 from stack
	ld	r12,VS_r1(r3)
	ld	r11,VS_r2(r3)
	ld	r9,VS_r11(r3)
	ld	r8,VS_r12(r3)
	ld	r7,VS_r13(r3)
	std	r13,VS_r0(r4)		// store r0-r2, r11-r13 in save area
	std	r12,VS_r1(r4)
	std	r11,VS_r2(r4)
	std	r9,VS_r11(r4)
	std	r8,VS_r12(r4)
	std	r7,VS_r13(r4)
	ld	r13,VS_cr(r3)		// retrieve cr, iar, msr from stack
	ld	r12,VS_iar(r3)
	ld	r11,VS_msr(r3)
	std	r13,VS_cr(r4)		// store cr, iar, msr in save area
	std	r12,VS_iar(r4)
	std	r11,VS_msr(r4)
	b	ResumeCommon
CODE_END(ResumeFromMemory)

/*
 * Continue execution from a volatileState area whose address is in r3.  The
 * necessary values are already in the userState or trapState area of the
 * dispatcher, either because r3 itself points into the dispatcher or because
 * we got here from ResumeFromMemory which copied the values.
 */
CODE_ENTRY(Resume)
TrapResume:
	lis	r0,SYSCALL_TRAP_RFI
	b	ResumeCommon
UserResume:
	lis	r0,SYSCALL_USER_RFI
ResumeCommon:
	ld	r13,VS_ctr(r3)		// retrieve special registers
	ld	r12,VS_lr(r3)
	ld	r11,VS_xer(r3)
	mtctr	r13			// restore special registers
	mtlr	r12
	mtxer	r11
	lfd	f0,VS_fpscr(r3)		// restore fpscr
	mtfsf	0xfc,f0			// We only restore the "status" part
					//     of fpscr (bits 0-23) so that we
					//     don't miss global changes to the
					//     "control" part (bits 24-31).
	lfd	f13,VS_f13(r3)		// restore volatile fpr's
	lfd	f12,VS_f12(r3)
	lfd	f11,VS_f11(r3)
	lfd	f10,VS_f10(r3)
	lfd	f9,VS_f9(r3)
	lfd	f8,VS_f8(r3)
	lfd	f7,VS_f7(r3)
	lfd	f6,VS_f6(r3)
	lfd	f5,VS_f5(r3)
	lfd	f4,VS_f4(r3)
	lfd	f3,VS_f3(r3)
	lfd	f2,VS_f2(r3)
	lfd	f1,VS_f1(r3)
	lfd	f0,VS_f0(r3)
	ld	r10,VS_r10(r3)		// restore volatile gpr's r3-r10
	ld	r9,VS_r9(r3)
	ld	r8,VS_r8(r3)
	ld	r7,VS_r7(r3)
	ld	r6,VS_r6(r3)
	ld	r5,VS_r5(r3)
	ld	r4,VS_r4(r3)
	ld	r3,VS_r3(r3)
					// RFI syscall will restore r0-r2 and
					//     r11-r13 directly from dispatcher
	sc
CODE_END(Resume)

#if !defined(USE_EXPEDIENT_SCHEDULER) || !defined(USE_EXPEDIENT_PPC)
CODE_ENTRY(TraceCurThread)
	LOAD_C_DATA_UVAL(r10,kernelInfoLocal,KI_TI_mask)
	rldicl.	r10,r10,ROT_RIGHT(TRC_SCHEDULER_MAJOR_ID),63
	beqlr+
	// oldIndex = traceControl->index;
	LOAD_C_DATA_UVAL(r10,kernelInfoLocal,KI_TI_traceControl)
	addi	r10,r10,TC_index
    TraceCurThread_UpdateIndexLoop:
	ldarx	r0,0,r10
	// newIndex = oldIndex + 2
	addic	r0,r0,2		// addi doesn't work for r0
	// if (TRACE_BUFFER_OFFSET_GET(newIndex) < 2) goto TraceSlow
	rldicl	r1,r0,0,64-TRC_BUFFER_OFFSET_BITS
	cmpldi	r1,2
	blt-	TraceCurThreadSlow
	// now = Scheduler::SysTimeNow();
	mftb	r1
	// traceControl->index = newIndex;
	stdcx.	r0,0,r10
	bne-	TraceCurThread_UpdateIndexLoop
	// index = (newIndex - 2) & traceInfo->indexMask;
	LOAD_C_DATA_UVAL(r10,kernelInfoLocal,KI_TI_indexMask)
	subic	r0,r0,2		// subi doesn't work for r0
	and	r0,r0,r10
	// traceArray[index] = traceFormFirstWord(now, 2, SCHEDULER, CUR_THRD);
	LOAD_C_DATA_UVAL(r10,kernelInfoLocal,KI_TI_traceArray)
	sldi	r0,r0,3
	TRACE_FORM_FIRST_WORD(r1, 2, TRC_K42_LAYER_ID,
				TRC_SCHEDULER_MAJOR_ID,
				TRC_SCHEDULER_CUR_THREAD)
	stdux	r1,r10,r0
	// traceArray[index+1] = CurrentThread;
	std	r13,8(r10)
	// traceControl->bufferCount[TRACE_BUFFER_NUMBER_GET(index)] += 2;
	LOAD_C_DATA_UVAL(r10,kernelInfoLocal,KI_TI_traceControl)
	addi	r10,r10,TC_bufferCount
	rldicl	r0,r0,ROT_RIGHT(TRC_BUFFER_OFFSET_BITS+3),\
				64-TRC_BUFFER_NUMBER_BITS
	sldi	r0,r0,3
    TraceCurThread_UpdateCountLoop:
	ldarx	r1,r10,r0
	addi	r1,r1,2
	stdcx.	r1,r10,r0
	bne-	TraceCurThread_UpdateCountLoop
	blr
    TraceCurThreadSlow:
	DISPATCHER_STACK(r12,r0)
	VOLATILE_FRAME_ENTER()
	// DispatcherDefault_TraceCurThread();
	bl	C_TEXT(DispatcherDefault_TraceCurThread)
	VOLATILE_FRAME_RETURN()
CODE_END(TraceCurThread)
#endif	// !USE_EXPEDIENT_SCHEDULER || !USE_EXPEDIENT_PPC

/*
 * On entry, r12 = dispatcher and r11 = &extRegsLocal.  All other registers
 * may be in use.
 */
CODE_ENTRY(VolatileEnabledSelfInterrupt)
	VOLATILE_FRAME_ENTER()
    VolatileEnabledSelfInterrupt_Loop:
	// Disable()
	li	r0,1
	std	r0,XR_disabled(r11)
	// SelfInterrupt(dispatcher);
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_SelfInterrupt)
	// Enable()
	GET_EXTREGS_AND_DISPATCHER(r11,r12)
	li	r0,0
	std	r0,XR_disabled(r11)
	lwz	r0,D_flags(r12)
	cmplwi	r0,0
	bne-	VolatileEnabledSelfInterrupt_Loop
	VOLATILE_FRAME_RETURN()
CODE_END(VolatileEnabledSelfInterrupt)

/*
 * On entry, we're already disabled and r12 = dispatcher.  All other registers
 * may be in use.
 */
CODE_ENTRY(VolatileSelfInterrupt)
	VOLATILE_FRAME_ENTER()
	// SelfInterrupt(dispatcher);
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_SelfInterrupt)
	VOLATILE_FRAME_RETURN()
CODE_END(VolatileSelfInterrupt)

/*
 * On entry, r13 = CurrentThread.  r0 and r11 are available.  All other
 * registers may be in use.
 */
CODE_ENTRY(ActivateSelf)
	LOAD_C_DATA_ADDR(r11,activeThrdCntLocal)
#if ATC_genIndexAndActivationCnt != 0
#    error genIndexAndActivationCnt offset in ActiveThrdCnt not 0
#endif
    ActivateSelf_Loop:
	ldarx	r0,0,r11
	addic	r0,r0,2		// addi doesn't work for r0
	stdcx.	r0,0,r11
	bne-	ActivateSelf_Loop
	srdi	r0,r0,ATC_COUNT_BITS	// extract index
	sldi	r0,r0,3			// convert to byte offset
	la	r11,ATC_activeCnt(r11)
	add	r11,r11,r0
	std	r11,TH_activeCntP(r13)
	blr
CODE_END(ActivateSelf)

/*
 * On entry, r13 = CurrentThread.  r0 and r11 are available.  All other
 * registers may be in use.
 */
CODE_ENTRY(DeactivateSelf)
	ld	r11,TH_activeCntP(r13)
    DeactivateSelf_Loop:
	ldarx	r0,0,r11
	subic	r0,r0,2		// subi doesn't work for r0
	stdcx.	r0,0,r11
	bne-	DeactivateSelf_Loop
	cmpdi	r0,1
	li	r0,0
	std	r0,TH_activeCntP(r13)
	bnelr+			// return if no notification requested
	VOLATILE_FRAME_ENTER()
	mr	r3,r11			// r3 = activeCntP
	bl	C_TEXT(ActiveThrdCnt_NotifyAdvance)
	VOLATILE_FRAME_RETURN()
CODE_END(DeactivateSelf)

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, and r12 = extRegsLocal.dispatcher.
 * All other registers are undefined.
 */
CODE_ENTRY(DispatcherDefault_RunEntry)
	LOAD_FPSCR(r12,f0)
RunEntryInternal:
#ifdef	USE_EXPEDIENT_SCHEDULER
	DISPATCHER_STACK(r12,r0)
	GOTO_EXPEDIENT(DispatcherDefaultExp_RunEntry)
#else	// USE_EXPEDIENT_SCHEDULER
	// if (erp->dispatcher->interrupts.pending()) goto ProcessInterrupts
	lwz	r0,D_flags(r12)
	cmplwi	r0,0
	bne-	RunEntry_ProcessInterrupts
    RunEntry_CheckForPreempt:
	// if (erp->dispatcher->preemptRequested) goto AllowPreempt
	ld	r0,DD_preemptRequested(r12)
	cmpldi	r0,0
	bne-	RunEntry_AllowPreempt
    RunEntry_GetNextThread:
	// CurrentThread = erp->dispatcher->getReadyThread();
	// if (CurrentThread == NULL) goto YieldProcessor
	ld	r13,D_hasWork(r12)
	cmpldi	r13,0
	beq-	RunEntry_YieldProcessor
	ld	r0,TH_next(r13)
	std	r0,D_hasWork(r12)
	// if ((CurrentThread->groups & erp->dispatcher->barredGroups) != 0)
	//							goto BarThread
	lhz	r0,TH_groups(r13)
	lhz	r11,DD_barredGroups(r12)
	and.	r0,r0,r11
	bne-	RunEntry_BarThread
	// traceScheduler1(TRACE_SCHEDULER_CUR_THREAD, uval64(CurrentThread));
	bl	CODE(TraceCurThread)
	// GotoAsm(erp, DispatcherDefault_SuspendCore_Continue);
	b	CODE(DispatcherDefault_SuspendCore_Continue)
    RunEntry_ProcessInterrupts:
	DISPATCHER_STACK(r12,r0)
	// CallAsm(erp, DispatcherDefault_ProcessInterrupts);
	bl	CODE(DispatcherDefault_ProcessInterrupts)
	b	RunEntry_CheckForPreempt
    RunEntry_AllowPreempt:
	// erp->dispatcher->preemptRequested = 0;
	li	r0,0
	std	r0,DD_preemptRequested(r12)
    RunEntry_YieldProcessor:
	// GotoAsm(erp, DispatcherDefault_SyscallYieldProcessor);
	b	CODE(DispatcherDefault_SyscallYieldProcessor)
    RunEntry_BarThread:
	// CurrentThread->next = erp->dispatcher->barredList;
	ld	r0,DD_barredList(r12)
	std	r0,TH_next(r13)
	// erp->dispatcher->barredList = CurrentThread;
	std	r13,DD_barredList(r12)
	// goto GetNextThread;
	b	RunEntry_GetNextThread
#endif	// USE_EXPEDIENT_SCHEDULER
CODE_END(DispatcherDefault_RunEntry)

CODE_ENTRY(SandboxUpcall)
	FULLSAVE_FRAME_ENTER(0, r0)
	// SandboxUpcall(volatileStatePtr, nonvolatileStatePtr);
	// r3 = address of volatile state already
	// r4 = address of nonvolatile state in FULLSAVE_FRAME
	la	r4,FULLSAVE_FRAME_NVS_OFFSET(0)(r1)
	bl	C_TEXT(K42Linux_SandboxUpcall)
	FULLSAVE_FRAME_RETURN(0, r0)
CODE_END(SandboxUpcall)

CODE_ENTRY(ReturnToSandbox)
	ld	r0,TH_upcallNeeded(r13)
	cmpldi	r0,0
	beq+	ReturnToSandbox_Resume
	la	r3,STK_LOCAL0(r1)	// address of VolatileState
	bl	CODE(SandboxUpcall)
    ReturnToSandbox_Resume:
	GET_DISPATCHER(r12)
	std	r13,DD_sandboxShepherd(r12) // shepherd = CurrentThread
	la	r3,STK_LOCAL0(r1)	// r3 = addr of volatile state on stack
	b	UserResumeFromMemory
CODE_END(ReturnToSandbox)

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, and r12 = extRegsLocal.dispatcher.
 * All other volatile registers are undefined.  CurrentThread's volatile state
 * is stored in dispatcher->userStatePtr().  Non-volatile state is still in
 * the registers.
 */
CODE_ENTRY(DispatcherDefault_InterruptEntry)
	LOAD_FPSCR(r12,f0)
	DEBUG_CHK_STK_INTERRUPT(r12,r11)
	DISPATCHER_STACK(r12,r0)
	bl	CODE(DispatcherDefault_ProcessInterrupts)
	LOAD_USER_STATE_ADDR(r3,r12)
	ld	r0,DD_rescheduleNeeded(r12)
	cmpldi	r0,0			// test for reschedule needed
	beq	UserResume		// resume if no reschedule needed
	// yield requested
	li	r0,0			// clear reschedule needed
	std	r0,DD_rescheduleNeeded(r12)
	ld	r13,DD_sandboxShepherd(r12)
	cmpldi	r13,0			// if (shepherd thread is non-NULL)
	bne-	InterruptEntry_InSandbox//     goto InSandbox
	GET_CURTHREAD_AND_STACK(r3)
	PUSH_VOLATILE_STATE_FRAME(r3,STK_FLOOR,1,r0,r4)
	DEBUG_FIXUP(STK_FLOOR,STK_LOCAL1,STK_LOCAL0,r0)
	// InterruptYield(dispatcher);
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_InterruptYield)
	DEBUG_RESTORE(STK_FLOOR,STK_LOCAL1,STK_LOCAL0,r0)
	la	r3,STK_LOCAL1(r1)	// r3 = addr of volatile state on stack
	b	UserResumeFromMemory
    InterruptEntry_InSandbox:
	THREAD_STACK(r13, 0, r0)	// get on shepherd's stack
	PUSH_VOLATILE_STATE_FRAME(r3,0,0,r0,r4)
	li	r0,0			// clear shepherd thread
	std	r0,DD_sandboxShepherd(r12)
	// InterruptYield(dispatcher);
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_InterruptYield)
	b	CODE(ReturnToSandbox)
CODE_END(DispatcherDefault_InterruptEntry)

CODE_ENTRY(NativeTrap)
	FULLSAVE_FRAME_ENTER(0, r0)
	// GDBStub_UserTrap(trapNumber, trapInfo, trapAuxInfo,
	//		    nonvolatileStatePtr);
	// r3-r5 already hold trapNumber, trapInfo, and trapAuxInfo
	// r6 = address of nonvolatile state in FULLSAVE_FRAME
	la	r6,FULLSAVE_FRAME_NVS_OFFSET(0)(r1)
	bl	C_TEXT(GDBStub_UserTrap)
	FULLSAVE_FRAME_RETURN(0, r0)
CODE_END(NativeTrap)

CODE_ENTRY(SandboxTrap)
	FULLSAVE_FRAME_ENTER(0, r0)
	// K42Linux_SandboxTrap(trapNumber, trapInfo, trapAuxInfo,
	//			volatileStatePtr, nonvolatileStatePtr);
	// r3-r5 already hold trapNumber, trapInfo, and trapAuxInfo
	// r6 = address of volatile state already
	// r7 = address of nonvolatile state in FULLSAVE_FRAME
	la	r7,FULLSAVE_FRAME_NVS_OFFSET(0)(r1)
	bl	C_TEXT(K42Linux_SandboxTrap)
	FULLSAVE_FRAME_RETURN(0, r0)
CODE_END(SandboxTrap)

CODE_ENTRY(AlignmentHandler)
	FULLSAVE_FRAME_ENTER(0, r0)
	std	r1,DD_alignHdlrStkPtr(r12) // mark that we're in handler and
					   // record stack pointer for longjmp
	// rc = fix_alignment(trapNumber, trapInfo, trapAuxInfo,
	//		      volatileStatePtr, nonvolatileStatePtr);
	// r3-r5 already hold trapNumber, trapInfo, and trapAuxInfo
	// r6 = address of volatile state already
	// r7 = address of nonvolatile state in FULLSAVE_FRAME
	la	r7,FULLSAVE_FRAME_NVS_OFFSET(0)(r1)
	bl	C_TEXT(fix_alignment)
	GET_DISPATCHER(r12)
    AlignmentHandler_Return:
	li	r0,0			// clear handler mark
	std	r0,DD_alignHdlrStkPtr(r12)
	FULLSAVE_FRAME_RETURN(0, r0)
CODE_END(AlignmentHandler)

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, r12 = extRegsLocal.dispatcher,
 * r3 = trapNumber, r4 = trapInfo, and r5 = trapAuxInfo.  All other volatile
 * registers are undefined.  The interrupt-time volatile state is stored in
 * dispatcher->trapStatePtr().  Non-volatile state is still in the registers.
 */
CODE_ENTRY(DispatcherDefault_TrapEntry)
	LOAD_FPSCR(r12,f0)
	/*
	 * A non-zero alignHdlrStkPtr means that this trap occurred inside
	 * the alignment handler.  We short-circuit the handler (essentially a
	 * longjmp).  The current trap{Number,Info,AuxInfo} will replace the
	 * originals and we'll proceed as with a regular trap.
	 */
	ld	r1,DD_alignHdlrStkPtr(r12)
	cmpldi	r1,0
	bne-	CODE(AlignmentHandler_Return)
	cmpldi	r3,EXC_ALI		// try to handle an alignment trap
	bne+	TrapEntry_NotAlignment
	DEBUGGER_STACK(r12,r0)
	LOAD_TRAP_STATE_ADDR(r6,r12)	// address of VolatileState
	PUSH_VOLATILE_STATE_FRAME(r6,0,4,r0,r7)
	std	r3,STK_LOCAL0(r1)	// preserve trap{Number,Info,AuxInfo}
	std	r4,STK_LOCAL1(r1)
	std	r5,STK_LOCAL2(r1)
	ld	r0,D_trapDisabledSave(r12) // preserve trapDisabledSave
	std	r0,STK_LOCAL3(r1)
	la	r6,STK_LOCAL4(r1)	// address of VolatileState
	bl	CODE(AlignmentHandler)
	ld	r0,STK_LOCAL3(r1)	// restore trapDisabledSave
	std	r0,D_trapDisabledSave(r12)
	/*
	 * The return value from AlignmentHandler will be 0 if it succeeded in
	 * emulating the trapping operation.  In that case we simply resume
	 * from the trap.  The return value will be negative if the handler
	 * could not perform the required operation.  In that case we restore
	 * faultNumber, faultInfo, and faultAuxInfo and proceed as with any
	 * other trap.  If the handler was aborted because of a recursive trap,
	 * the new trapNumber, trapInfo, and trapAuxInfo will be in r3-r5.
	 * We can distinguish this case because trapNumbers are positive.  In
	 * this case we proceed with the new trap parameters, but passing along
	 * the machine state from the original trap.
	 */
	cmpdi	r3,0
	bne-	TrapEntry_AlignmentNotHandled
	la	r3,STK_LOCAL4(r1)	// r3 = addr of volatile state on stack
	b	TrapResumeFromMemory	// handled -- resume from trap
    TrapEntry_AlignmentNotHandled:
	bgt-	TrapEntry_ConvertAlignmentTrap
	ld	r3,STK_LOCAL0(r1)	// restore trap{Number,Info,AuxInfo},
	ld	r4,STK_LOCAL1(r1)	//     but only if there was no
	ld	r5,STK_LOCAL2(r1)	//     recursive trap
    TrapEntry_ConvertAlignmentTrap:
	LOAD_TRAP_STATE_ADDR(r6,r12)	// copy VS back to dispatcher trapState
	POP_VOLATILE_STATE_FRAME(r6,0,4,r0,r7)
    TrapEntry_NotAlignment:
	ld	r13,DD_sandboxShepherd(r12)
	cmpldi	r13,0			// if (shepherd thread is non-NULL)
	bne-	TrapEntry_InSandbox	//     goto InSandbox
	DEBUGGER_STACK(r12,r0)
	bl	CODE(NativeTrap)
	GET_DISPATCHER(r12)
	LOAD_TRAP_STATE_ADDR(r3,r12)
	b	TrapResume
    TrapEntry_InSandbox:
	THREAD_STACK(r13, 0, r0)	// get on shepherd's stack
	LOAD_TRAP_STATE_ADDR(r6,r12)
	PUSH_VOLATILE_STATE_FRAME(r6,0,0,r0,r7)
	li	r0,0			// clear shepherd thread
	std	r0,DD_sandboxShepherd(r12)
	la	r6,STK_LOCAL0(r1)	// address of VolatileState
	bl	CODE(SandboxTrap)
	b	CODE(ReturnToSandbox)
CODE_END(DispatcherDefault_TrapEntry)

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, r12 = extRegsLocal.dispatcher,
 * r3 = faultID, r4 = faultInfo, and r5 = faultAddr.  All other volatile
 * registers are undefined.  CurrentThread's volatile state is stored in
 * dispatcher->userStatePtr().  Non-volatile state is still in the registers.
 */
CODE_ENTRY(DispatcherDefault_PgfltEntry)
	LOAD_FPSCR(r12,f0)
	LOAD_USER_STATE_ADDR(r11,r12)	// r11 = userStatePtr()
	ld	r13,DD_sandboxShepherd(r12)
	cmpldi	r13,0			// if (shepherd thread is non-NULL)
	bne-	PgfltEntry_InSandbox	//     goto InSandbox
	GET_CURTHREAD_AND_STACK(r11)
					// r3-r5 have defined values
	PUSH_VOLATILE_STATE_FRAME(r11,STK_FLOOR,1,r0,r6)
	DEBUG_FIXUP(STK_FLOOR,STK_LOCAL1,STK_LOCAL0,r0)
	// PgfltBlock(dispatcher, faultInfo, faultAddr, faultID);
	mr	r6,r3			// r6 = faultID
					// r5 = faultAddr (already)
					// r4 = faultInfo (already)
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_PgfltBlock)
	DEBUG_RESTORE(STK_FLOOR,STK_LOCAL1,STK_LOCAL0,r0)
	la	r3,STK_LOCAL1(r1)	// r3 = addr of volatile state on stack
	b	UserResumeFromMemory
    PgfltEntry_InSandbox:
	THREAD_STACK(r13, 0, r0)	// get on shepherd's stack
					// r3-r5 have defined values
	PUSH_VOLATILE_STATE_FRAME(r11,0,0,r0,r6)
	li	r0,0			// clear shepherd thread
	std	r0,DD_sandboxShepherd(r12)
	// PgfltBlock(dispatcher, faultInfo, faultAddr, faultID);
	mr	r6,r3			// r6 = faultID
					// r5 = faultAddr (already)
					// r4 = faultInfo (already)
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_PgfltBlock)
	b	CODE(ReturnToSandbox)
CODE_END(DispatcherDefault_PgfltEntry)

CODE_ENTRY(DispatcherDefault_ProcessInterrupts)
	FRAME_ENTER(1, r0)
	std	r12,STK_LOCAL0(r1)	// save dispatcher
	// HandleInterrupts(dispatcher);
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_HandleInterrupts)
	ld	r12,STK_LOCAL0(r1)	// restore dispatcher
	FRAME_RETURN(1, r0)
CODE_END(DispatcherDefault_ProcessInterrupts)

/*
 * rc = DispatcherDefault_PPCCall(xhandle, methnum, targetID);
 */
C_TEXT_ENTRY(DispatcherDefault_PPCCall)
	FRAME_ENTER(2, r0)
	std	r14,STK_LOCAL0(r1)	// save r14
	std	r15,STK_LOCAL1(r1)	// save r15
	// xhandle already in r3
	mr	r10,r4			// r10 = methnum
	mr	r14,r5			// r14 = targetID
	bl	CODE(DispatcherDefault_PPCClient)
	ld	r15,STK_LOCAL1(r1)	// restore r15
	ld	r14,STK_LOCAL0(r1)	// restore r14
	FRAME_RETURN(2, r0)
C_TEXT_END(DispatcherDefault_PPCCall)

/*
 * rc = DispatcherDefault_PPCAsync(xhandle, methnum, targetID);
 */
C_TEXT_ENTRY(DispatcherDefault_PPCAsync)
	LEAF_ENTER(r0)
	std	r2,STK_TOC(r1)
	// xhandle already in r3
	mr	r10,r4			// r10 = methnum
	mr	r9,r5			// r9 = targetID
	bl	L1
	ld	r2,STK_TOC(r1)
	LEAF_RETURN(r0)
    L1:	lis	r0,SYSCALL_IPC_ASYNC
	sc
C_TEXT_END(DispatcherDefault_PPCAsync)

CODE_ENTRY(DispatcherDefault_PPCClient)
	GET_EXTREGS_AND_DISPATCHER(r11,r12)
#ifdef	USE_EXPEDIENT_PPC
	FRAME_ENTER(0, r0)
	CALL_EXPEDIENT(DispatcherDefaultExp_PPCClient)
	FRAME_RETURN(0, r0)
#else
	FRAME_ENTER(1, r0)	// one slot needed for PrimitivePPC
	// if (DispatcherDefault::IsDisabled()) goto PrimitivePPC
	ld	r0,XR_disabled(r11)
	cmpldi	r0,0
	bne-	PPCClient_PrimitivePPC
	// DispatcherDefault::Disable();
	li	r0,1
	std	r0,XR_disabled(r11)
	// tassertSilent(CurrentThread->groups == 0, BREAKPOINT);
	DEBUG_CHK_THREAD_GROUPS(r13,r0)
	// CurrentThread->targetID = erp->PPC_calleeID;
	std	r14,TH_targetID(r13)
	// erp->PPC_threadID = CurrentThread->threadID;
	ld	r15,TH_threadID(r13)
	// CallAsm(erp, DispatcherDefault_PPCClientCore);
	bl	CODE(DispatcherDefault_PPCClientCore)
	// CurrentThread->targetID = SysTypes::COMMID_NULL;
	li	r0,-1
	std	r0,TH_targetID(r13)
	// DispatcherDefault::Enable();
	li	r0,0
	std	r0,XR_disabled(r11)
	lwz	r0,D_flags(r12)
	cmplwi	r0,0
	bnel-	CODE(VolatileEnabledSelfInterrupt)
	FRAME_RETURN(1, r0)
    PPCClient_PrimitivePPC:
        // tassertSilent(erp->dispatcher->allowPrimitivePPCFlag, BREAKPOINT);
	DEBUG_CHK_ALLOW_PRIMITIVE_PPC(r12,r11)
	// Thread *const savedCurrentThread = CurrentThread;
	std	r13,STK_LOCAL0(r1)
	// CallAsm(erp, DispatcherDefault_PPCPrimitiveClientCore);
	bl	CODE(DispatcherDefault_PPCPrimitiveClientCore)
	// CurrentThread = savedCurrentThread;
	ld	r13,STK_LOCAL0(r1)
	FRAME_RETURN(1, r11)
#endif
CODE_END(DispatcherDefault_PPCClient)

CODE_ENTRY(DispatcherDefault_PPCPrimitiveClientCore)
	LEAF_ENTER(r11)
	std	r2,STK_TOC(r1)
	bl	L2
	ld	r2,STK_TOC(r1)
	LEAF_RETURN(r11)
    L2:	lis	r0,SYSCALL_PPC_PRIMITIVE
	sc
CODE_END(DispatcherDefault_PPCPrimitiveClientCore)

CODE_ENTRY(DispatcherDefault_PPCClientCore)
	LEAF_ENTER(r0)
	std	r1,TH_curSP(r13)
	DEBUG_CHK_STK_SUSPEND(r13,r0)
	lis	r0,SYSCALL_IPC_CALL
	sc
CODE_END(DispatcherDefault_PPCClientCore)

CODE_ENTRY(DispatcherDefault_PPCClientCore_Continue)
	ld	r1,TH_curSP(r13)
	LEAF_RETURN(r0)
CODE_END(DispatcherDefault_PPCClientCore_Continue)

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, r12 = extRegsLocal.dispatcher,
 * r3 = xHandle, r10 = methodNum, r14 = callerID, and r15 = threadID.
 * Registers r0, r1, and r13 are undefined.  All other registers have been
 * passed through intact from the caller.
 */
CODE_ENTRY(DispatcherDefault_IPCCallEntry)
	LOAD_FPSCR(r12,f0)
#ifdef	USE_EXPEDIENT_PPC
	DISPATCHER_STACK(r12,r0)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCCallEntry)
#else
	// CurrentThread = erp->dispatcher->allocThread();
	ld	r13,DD_freeList(r12)
	cmpldi	r13,0
	beq-	IPCCallEntry_NoThread
	ld	r0,TH_next(r13)
	std	r0,DD_freeList(r12)
    IPCCallEntry_HaveThread:
	// GotoAsm(erp, DispatcherDefault_PPCServerOnThread);
	b	CODE(DispatcherDefault_PPCServerOnThread)
    IPCCallEntry_NoThread:
	DISPATCHER_STACK(r12,r0)
	bl	CODE(PPC_CreateThread)
	// if (CurrentThread != NULL)
	//         GotoAsm(erp, DispatcherDefault_PPCServerOnThread);
	cmpldi	r13,0
	bne+	IPCCallEntry_HaveThread
	// erp->returnCode = _SERROR(?, 0, ENOMEM);
	SERROR(r3, 1582, 0, ERRNO_NOMEM)
	// RESET_PPC_LENGTH();
	li	r0,0
	std	r0,XR_ppcPageLength(r11)
	// GotoAsm(erp, DispatcherDefault_SyscallIPCReturn);
	b	CODE(DispatcherDefault_SyscallIPCReturn)
#endif
CODE_END(DispatcherDefault_IPCCallEntry)

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, r12 = extRegsLocal.dispatcher,
 * r14 = calleeID, and r15 = threadID.  Registers r0, r1, and r13 are
 * undefined.  All other registers have been passed back intact from the
 * callee.
 */
CODE_ENTRY(DispatcherDefault_IPCReturnEntry)
	LOAD_FPSCR(r12,f0)
#ifdef	USE_EXPEDIENT_PPC
	DISPATCHER_STACK(r12,r0)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCReturnEntry)
#else
	// uval key = Thread::GetKey(erp->PPC_threadID);
	rldicl	r13,r15,ROT_RIGHT(COMMID_PID_SHIFT),64-PID_BITS
	// if (key >= erp->dispatcher->threadArraySize) goto Error
	ld	r1,DD_threadArraySize(r12)
	cmpld	r13,r1
	bge-	IPCReturnEntry_Error
	// CurrentThread = erp->dispatcher->threadArray[key];
	ld	r1,DD_threadArray(r12)
	sldi	r13,r13,3
	ldx	r13,r1,r13
	// if (CurrentThread == NULL) goto Error
	cmpldi	r13,0
	beq-	IPCReturnEntry_Error
	// if (!SysTypes::COMMID_PID_MATCH(CurrentThread->targetID,
	//				   erp->PPC_callerID)) goto Error
	ld	r1,TH_targetID(r13)
	xor	r1,r1,r14
	rldicl.	r1,r1,ROT_RIGHT(COMMID_PID_SHIFT),64-PID_BITS
	bne-	IPCReturnEntry_Error
	// traceScheduler1(TRACE_SCHEDULER_CUR_THREAD, uval64(CurrentThread));
	bl	CODE(TraceCurThread)
	// GotoAsm(erp, DispatcherDefault_PPCClientCore_Continue);
	b	CODE(DispatcherDefault_PPCClientCore_Continue)
IPCReturnEntry_Error:
	// RESET_PPC_LENGTH();
	li	r1,0
	std	r1,XR_ppcPageLength(r11)
	b	RunEntryInternal
#endif
CODE_END(DispatcherDefault_IPCReturnEntry)

/*
 * On entry, r13 = new thread, r12 = dispatcher, r11 = &extRegsLocal,
 * r10 = method number, r3 = xhandle.  r0, r1, cr, lr, and ctr are available.
 * All other registers are passed through from caller.
 */
CODE_ENTRY(DispatcherDefault_PPCServerOnThread)
	THREAD_STACK(r13, 0, r0)
#ifdef	USE_EXPEDIENT_PPC
	GOTO_EXPEDIENT(DispatcherDefaultExp_PPCServerOnThread)
#else
	// DispatcherDefault::Enable();
	li	r0,0
	std	r0,XR_disabled(r11)
	lwz	r0,D_flags(r12)
	cmplwi	r0,0
	bnel-	CODE(VolatileEnabledSelfInterrupt)
	bl	CODE(ActivateSelf)
	// uval seqNo = XHANDLE_SEQNO(erp->PPC_xHandle);
	rldicl	r0,r3,ROT_RIGHT(SEQNO_SHIFT),64-SEQNO_BITS
	// uval idx   = XHANDLE_IDX(erp->PPC_xHandle);
	rldicl	r3,r3,ROT_RIGHT(IDX_SHIFT),64-IDX_BITS
	// if (idx >= erp->dispatcher->published.xhandleTableLimit)
	//						goto BadXHandle
	ld	r11,DD_xhandleTableLimit(r12)
	cmpld	r3,r11
	bge-	PPCServerOnThread_BadXHandle
	// erp->PPC_xObj = &erp->dispatcher->published.xhandleTable[idx];
	ld	r11,DD_xhandleTable(r12)
	sldi	r3,r3,XBO_LOG_SIZE_IN_UVALS+3
	add	r3,r11,r3
	// if (erp->PPC_methodNum < XBaseObj::FIRST_METHOD) goto BadMethod
	cmpldi	r10,XBO_FIRST_METHOD
	blt-	PPCServerOnThread_BadMethod
	// if (erp->PPC_methodNum >= erp->PPC_xObj->__nummeth) goto BadMethod
	lbz	r11,XBO___nummeth(r3)
	// SyncAfterAcquire();
	isync
	cmpld	r10,r11
	bge-	PPCServerOnThread_BadMethod
	// if (seqNo != erp->PPC_xObj->seqNo) goto BadXHandle
	lbz	r11,XBO_seqNo(r3)
	cmpld	r0,r11
	bne-	PPCServerOnThread_BadXHandle
	// erp->PPC_function =
	//  (XBaseObj::GetFTable(erp->PPC_xObj))[erp->PPC_methodNum].getFunc();
	ld	r11,0(r3)
	sldi	r10,r10,VTE_LOG_SIZE_IN_UVALS+3
	add	r11,r11,r10
	ld	r11,VTE__func(r11)
	// traceScheduler2(TRACE_SCHEDULER_PPC_XOBJ_FCT,
	//                 uval64(CurrentThread), uval64(erp->PPC_function));
	LOAD_C_DATA_ADDR(r10,kernelInfoLocal)
	ld	r0,KI_TI_mask(r10)
	rldicl.	r0,r0,ROT_RIGHT(TRC_SCHEDULER_MAJOR_ID),63
	bnel-	CODE(PPC_TraceXObjFct)
	// CallAsm(erp, DispatcherDefault_InvokeXObjMethod);
	bl	CODE(DispatcherDefault_InvokeXObjMethod)
    PPCServerOnThread_NormalReturn:
	bl	CODE(DeactivateSelf)
	// DispatcherDefault::Disable();
	// erp->dispatcher = DISPATCHER;
	GET_EXTREGS_AND_DISPATCHER(r11,r12)
	li	r0,1
	std	r0,XR_disabled(r11)
	// erp->dispatcher->freeThread(CurrentThread);
	ld	r1,DD_freeList(r12)
	std	r1,TH_next(r13)
	std	r13,DD_freeList(r12)
	DEBUG_CHK_STK_FREE_THREAD(r13,r12,r11,r1,r0)
	// GotoAsm(erp, DispatcherDefault_SyscallIPCReturn);
	b	CODE(DispatcherDefault_SyscallIPCReturn)
    PPCServerOnThread_BadXHandle:
	// erp->returnCode = _SERROR(?, 0, EINVAL);
	SERROR(r3, 1583, 0, ERRNO_INVAL)
	b	PPCServerOnThread_ErrorReturn
    PPCServerOnThread_BadMethod:
	// erp->returnCode = _SERROR(?, 0, ENOSYS);
	SERROR(r3, 1584, 0, ERRNO_PERM)
    PPCServerOnThread_ErrorReturn:
	// RESET_PPC_LENGTH();
	LOAD_C_DATA_ADDR(r11,extRegsLocal)
	li	r0,0
	std	r0,XR_ppcPageLength(r11)
	// goto NormalReturn
	b	PPCServerOnThread_NormalReturn
#endif
CODE_END(DispatcherDefault_PPCServerOnThread)

CODE_ENTRY(DispatcherDefault_InvokeXObjMethod)
	FRAME_ENTER(0, r0)
	// rc = (PPC_function)(PPC_xObj, PPC_callerID);
	ld	r0,0(r11)
	mtlr	r0
	std	r2,STK_TOC(r1)
	ld	r2,8(r11)
					// r3 = xObj (already)
	mr	r4,r14			// r4 = callerID
	blrl
	ld	r2,STK_TOC(r1)
	FRAME_RETURN(0, r0)
CODE_END(DispatcherDefault_InvokeXObjMethod)

#ifndef	USE_EXPEDIENT_PPC
CODE_ENTRY(PPC_CreateThread)
	VOLATILE_FRAME_ENTER()
	// CurrentThread = CreateThread(dispatcher);
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefault_CreateThread)
	mr	r13,r3
	VOLATILE_FRAME_RETURN()
CODE_END(PPC_CreateThread)

/*
 * On entry, r0 is available, r11 has the XObj function pointer, and r10
 * holds &KernelInfo.  All other registers are in use.
 */
CODE_ENTRY(PPC_TraceXObjFct)
	std	r9,-8(r1)	// preserve r9
	std	r8,-16(r1)	// preserve r8
	// oldIndex = traceControl->index;
	ld	r9,KI_TI_traceControl(r10)
	addi	r9,r9,TC_index
    PPC_TraceXObjFct_UpdateIndexLoop:
	ldarx	r8,0,r9
	// newIndex = oldIndex + 3
	addi	r8,r8,3
	// if (TRACE_BUFFER_OFFSET_GET(newIndex) < 3) goto TraceSlow
	rldicl	r0,r8,0,64-TRC_BUFFER_OFFSET_BITS
	cmpldi	r0,3
	blt-	PPC_TraceXObjFct_Slow
	// now = Scheduler::SysTimeNow();
	mftb	r0
	// traceControl->index = newIndex;
	stdcx.	r8,0,r9
	bne-	PPC_TraceXObjFct_UpdateIndexLoop
	// index = (newIndex - 3) & traceInfo->indexMask;
	ld	r9,KI_TI_indexMask(r10)
	subi	r8,r8,3
	and	r8,r8,r9
	// traceArray[index] = traceFormFirstWord(now, 3, SCHEDULER, PPC_FCT);
	ld	r9,KI_TI_traceArray(r10)
	sldi	r8,r8,3
	TRACE_FORM_FIRST_WORD(r0, 3, TRC_K42_LAYER_ID,
				TRC_SCHEDULER_MAJOR_ID,
				TRC_SCHEDULER_PPC_XOBJ_FCT)
	stdux	r0,r9,r8
	// traceArray[index+1] = CurrentThread;
	std	r13,8(r9)
	// traceArray[index+2] = PPC_function;
	std	r11,16(r9)
	// traceControl->bufferCount[TRACE_BUFFER_NUMBER_GET(index)] += 3;
	ld	r9,KI_TI_traceControl(r10)
	addi	r9,r9,TC_bufferCount
	rldicl	r8,r8,ROT_RIGHT(TRC_BUFFER_OFFSET_BITS+3),\
				64-TRC_BUFFER_NUMBER_BITS
	sldi	r8,r8,3
    PPC_TraceXObjFct_UpdateCountLoop:
	ldarx	r0,r8,r9
	addic	r0,r0,3		// addi doesn't work for r0
	stdcx.	r0,r8,r9
	bne-	PPC_TraceXObjFct_UpdateCountLoop
	ld	r8,-16(r1)	// restore r8
	ld	r9,-8(r1)	// restore r9
	blr
    PPC_TraceXObjFct_Slow:
	ld	r8,-16(r1)	// restore r8
	ld	r9,-8(r1)	// restore r9
	VOLATILE_FRAME_ENTER()
	// DispatcherDefault_TracePPCXObjFct(PPC_function);
	mr	r3,r11		// r3 = PPC_function
	bl	C_TEXT(DispatcherDefault_TracePPCXObjFct)
	VOLATILE_FRAME_RETURN()
CODE_END(PPC_TraceXObjFct)
#endif

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, and r12 = extRegsLocal.dispatcher.
 * Registers r0, r1, and r13 are undefined.  All other registers have their
 * original values for the call or return.  The ipcFaultReason and ipcRetryID
 * are stored in the dispatcher structure.
 */
CODE_ENTRY(DispatcherDefault_IPCFaultEntry)
	LOAD_FPSCR(r12,f0)
	DISPATCHER_STACK(r12,r11)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCFaultEntry)
CODE_END(DispatcherDefault_IPCFaultEntry)

CODE_ENTRY(DispatcherDefault_IPCFaultOnThread)
	THREAD_STACK(r13, 0, r11)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCFaultOnThread)
CODE_END(DispatcherDefault_IPCFaultOnThread)

CODE_ENTRY(DispatcherDefault_SyscallIPCCall)
	lis	r0,SYSCALL_IPC_CALL
	sc
CODE_END(DispatcherDefault_SyscallIPCCall)

/*
 * On entry, r2 = toc, r11 = &extRegsLocal, and r12 = extRegsLocal.dispatcher.
 * The caller's original r1, r2, lr, iar, and msr have been saved in the
 * userState area of the dispatcher.  r0 contains the system call number and
 * r3-r8 contain the system call arguments.  r9 and r10 are available.
 */
CODE_ENTRY(DispatcherDefault_SVCEntry)
#define SVC_STK_SIZE (STK_SIZE-STK_FLOOR+(3*8)) // interrupt frame /w 3 slots
	LOAD_FPSCR(r12,f0)
	ld	r10,DD_sandboxShepherd(r12)	// r10 = shepherd
	cmpldi	r10,0				// if (shepherd is NULL)
	beq-	SVCEntry_Native			//     goto Native
    SVCEntry_Sandbox:
	THREAD_STACK(r10, 6, r9)		// get on shepherd's stack
						//     with six data slots
	std	r13,STK_LOCAL0(r1)		// preserve r13
	mr	r13,r10				// CurrentThread = shepherd
	LOAD_USER_STATE_ADDR(r10,r12)		// r10 = userStatePtr()
	std	r27,STK_LOCAL1(r1)		// preserve r27-r31
	std	r28,STK_LOCAL2(r1)
	std	r29,STK_LOCAL3(r1)
	std	r30,STK_LOCAL4(r1)
	std	r31,STK_LOCAL5(r1)
	ld	r27,VS_r1(r10)			// preserve caller's r1, r2,
	ld	r28,VS_r2(r10)			//     iar, msr, and lr
	ld	r29,VS_iar(r10)
	ld	r30,VS_msr(r10)
	ld	r31,VS_lr(r10)
	li	r10,0				// clear shepherd thread
	std	r10,DD_sandboxShepherd(r12)
	rldicr.	r10,r30,(63-PSL_SF_SHIFT),0	// test for 32-bit mode
	beq-	SVCEntry_InSandbox32
    SVCEntry_Sandbox_CallSVC64:	// SVCDirect_Sandbox joins this path here.
	mr	r9,r27				// r9 = caller's stkPtr
	mr	r10,r0				// r10 = syscall number
	// Enable();
	li	r0,0
	std	r0,XR_disabled(r11)
	lwz	r0,D_flags(r12)
	cmplwi	r0,0
	bnel-	CODE(VolatileEnabledSelfInterrupt)
	bl	CODE(ActivateSelf)
	// rc = SVC64(a0, a1, a2, a3, a4, a5, stkPtr, svcNum);
	// syscall parameters are already in r3-r8, stkPtr in r9, svcNum in r10
	bl	C_TEXT(K42Linux_SVC64)
	bl	CODE(DeactivateSelf)
	GET_EXTREGS_AND_DISPATCHER(r11,r12)
	// Disable();
	li	r0,1
	std	r0,XR_disabled(r11)
    SVCEntry_ReturnToSandbox:
	ld	r0,TH_upcallNeeded(r13)		// check for upcall request
	cmpldi	r0,0
	bne-	SVCEntry_DoUpcall
	std	r13,DD_sandboxShepherd(r12)	// shepherd = CurrentThread
	ld	r13,STK_LOCAL0(r1)		// restore r13
	mtlr	r31				// restore caller's lr
	// assume caller's msr is the same as current msr
	mtctr	r29				// we'll bctr to caller's iar
	mr	r2,r28				// restore caller's toc
	mr	r31,r1				// hang on to current stk ptr
	mr	r1,r27				// restore caller's stk ptr
	ld	r27,STK_LOCAL1(r31)		// restore r27-r31
	ld	r28,STK_LOCAL2(r31)
	ld	r29,STK_LOCAL3(r31)
	ld	r30,STK_LOCAL4(r31)
	ld	r31,STK_LOCAL5(r31)
	// Enable();
	li	r0,0
	std	r0,XR_disabled(r11)
	/*
	 * Warning - this sequence is delicate.  We've been disabled so we
	 * have to make the usual check for pending soft interrupts now that
	 * we've enabled.  The problem is that at this point, we're not only
	 * enabled, we're also in sandbox mode -- which means that we're
	 * subject to migration.  r12 points to the dispatcher on which we
	 * were disabled and for whose pending interrupts we may be
	 * responsible.  We check for interrupts there, but if we find any, by
	 * the time we can re-disable to process them, we may have migrated to
	 * a different dispatcher.  It's okay.  If we didn't migrate (by far
	 * the usual case) we'll have taken care of our responsibility.  And
	 * if we did migrate, we'll simply have yielded once on our new
	 * dispatcher unnecessarily.  We could only have migrated because of a
	 * software interrupt on the old dispatcher, and any pending interrupts
	 * for which we were responsible would have been handled at the same
	 * time as the one that initiated the migration.  In effect, migration
	 * absolves us of all responsibility for interrupts that arrived while
	 * we still had the original dispatcher disabled.  We just have to be
	 * careful not to use the value in r12 for anything other than reading
	 * the interrupt flags word from our recent dispatcher.
	 */
	lwz	r12,D_flags(r12)		// trash r12 right after use
	cmplwi	r12,0
	bne-	SVCEntry_SelfInterrupt
	/*
	 * The handler returns a non-negative value on success and a negative
	 * errno value on failure.  We have to return the success/failure
	 * indication in the SO bit of cr0, and on failure invert the errno.
	 * We accomplish this with a branchless absolute value (which sets
	 * cr0 as a side effect) followed by moving the appropriate cr0 bit.
	 */
	sradi.	r0,r3,63			// spread sign bit across word
	xor	r3,r3,r0			// ones complement (if r0 = -1)
	subf	r3,r0,r3			// twos complement (if r0 = -1)
	crmove	cr0*4+so,cr0*4+lt		// set cr0 SO bit from LT bit
	/*
	 * Semantically, r0 and r11-r12 can be left undefined at this point,
	 * but clearing them is cheap and makes it easier to get the state
	 * construction on the upcall path exactly right.
	 */
	li	r0,0
	li	r11,0
	li	r12,0
	bctr					// return to caller
	// NOTREACHED
	trap
    SVCEntry_SelfInterrupt:
	// Disable();
	li	r0,1
	std	r0,XR_disabled(r11)
	ld	r12,XR_dispatcher(r11)		// must re-acquire dispatcher
	/*
	 * Re-create the stack and register state as of the return from
	 * SVC64.  Then SelfInterrupt() and loop.
	 */
	ld	r11,DD_sandboxShepherd(r12)	// r11 = shepherd
	ld	r11,TH_startSP(r11)		// r11 = shepherd's stk ptr
	la	r11,-(STK_SIZE+(6*8))(r11)	//     frame with 6 data slots
	std	r13,STK_LOCAL0(r11)		// preserve caller's r13
	ld	r13,DD_sandboxShepherd(r12)	// CurrentThread = shepherd
	std	r27,STK_LOCAL1(r11)		// preserve r27-r31
	std	r28,STK_LOCAL2(r11)
	std	r29,STK_LOCAL3(r11)
	std	r30,STK_LOCAL4(r11)
	std	r31,STK_LOCAL5(r11)
	mr	r27,r1		/* r1 */	// preserve caller's r1, r2,
	mr	r28,r2		/* r2 */	//     "iar", msr, and lr
	mfctr	r29		/* "iar" */
	ld	r30,DD_msr(r12)	/* default msr */
	mflr	r31		/* lr */
	li	r1,0				// clear shepherd thread
	std	r1,DD_sandboxShepherd(r12)
	stdux	r1,r1,r11			// clear back chain and switch
						//     to shepherd stack :-)
	ld	r2,SVC_DIRECT_VEC_ADDR+EPD_toc(0) // load our toc
	bl	CODE(VolatileSelfInterrupt)
	GET_EXTREGS_AND_DISPATCHER(r11,r12)
	b	SVCEntry_ReturnToSandbox	// loop
	// NOTREACHED
	trap
    SVCEntry_InSandbox32:
	mr	r9,r27				// r9 = caller's stkPtr
	mr	r10,r0				// r10 = syscall number
	// Enable();
	li	r0,0
	std	r0,XR_disabled(r11)
	lwz	r0,D_flags(r12)
	cmplwi	r0,0
	bnel-	CODE(VolatileEnabledSelfInterrupt)
	bl	CODE(ActivateSelf)
	// rc = SVC32(a0, a1, a2, a3, a4, a5, stkPtr, svcNum);
	// syscall parameters are already in r3-r8, stkPtr in r9, svcNum in r10
	bl	C_TEXT(K42Linux_SVC32)
	bl	CODE(DeactivateSelf)
	GET_EXTREGS_AND_DISPATCHER(r11,r12)
	// Disable();
	li	r0,1
	std	r0,XR_disabled(r11)
	ld	r0,TH_upcallNeeded(r13)		// check for upcall request
	cmpldi	r0,0
	bne-	SVCEntry_DoUpcall
	GET_DISPATCHER(r12)
	std	r13,DD_sandboxShepherd(r12)	// shepherd = CurrentThread
	LOAD_USER_STATE_ADDR(r11,r12)		// r11 = userStatePtr()
	std	r27,VS_r1(r11)			// restore caller's r1, r2,
	std	r28,VS_r2(r11)			//     iar, and msr to
	std	r29,VS_iar(r11)			//     user-state area
	std	r30,VS_msr(r11)
	ld	r0,STK_LOCAL0(r1)		// restore caller's r13 to
	std	r0,VS_r13(r11)			//     user-state area
	// fixup return value as per comment above
	sradi.	r0,r3,63			// spread sign bit across word
	xor	r3,r3,r0			// ones complement (if r0 = -1)
	subf	r3,r0,r3			// twos complement (if r0 = -1)
	crmove	cr0*4+so,cr0*4+lt		// set cr0 SO bit from LT bit
	mfcr	r0				// copy cr to user-state area
	std	r0,VS_cr(r11)
	li	r0,0				// clear r0 and r11-r12 in
	std	r0,VS_r0(r11)			//     user-state area (to make
	std	r0,VS_r11(r11)			//     state construction on
	std	r0,VS_r12(r11)			//     upcall path easier)
	mtlr	r31				// restore caller's lr
	mtctr	r29				// ctr == iar, ala 64-bit path
	ld	r27,STK_LOCAL1(r1)		// restore r27-r31
	ld	r28,STK_LOCAL2(r1)
	ld	r29,STK_LOCAL3(r1)
	ld	r30,STK_LOCAL4(r1)
	ld	r31,STK_LOCAL5(r1)
	lis	r0,SYSCALL_USER_RFI		// return to sandbox
	sc
	// NOTREACHED
	trap
    SVCEntry_DoUpcall:
	/*
	 * Extend the current stack frame to include space for a VolatileState
	 * at STK_LOCAL0 (ala PUSH_VOLATILE_STATE_FRAME).  Fill in the
	 * VolatileState as if the sandbox were interrupted just prior to
	 * executing the instruction to which the system call returns.
	 */
	mr	r12,r1				// hang on to old stk ptr
	li	r0,0				// extend frame; back chain is
	stdu	r0,-(VS_SIZE)(r1)		//     still NULL
	la	r11,STK_LOCAL0(r1)		// r11 = &VolatileState
	std	r0,VS_r0(r11)			// clear r0 and r11-r12
	std	r0,VS_r11(r11)
	std	r0,VS_r12(r11)
	std	r27,VS_r1(r11)			// store preserved r1 and r2
	std	r28,VS_r2(r11)
	ld	r0,STK_LOCAL0(r12)		// store preserved r13
	std	r0,VS_r13(r11)
	// fixup return value as per comment above
	sradi.	r0,r3,63			// spread sign bit across word
	xor	r3,r3,r0			// ones complement (if r0 = -1)
	subf	r3,r0,r3			// twos complement (if r0 = -1)
	crmove	cr0*4+so,cr0*4+lt		// set cr0 SO bit from LT bit
	std	r3,VS_r3(r11)			// store r3-r10
	std	r4,VS_r4(r11)
	std	r5,VS_r5(r11)
	std	r6,VS_r6(r11)
	std	r7,VS_r7(r11)
	std	r8,VS_r8(r11)
	std	r9,VS_r9(r11)
	std	r10,VS_r10(r11)
	stfd	f0,VS_f0(r11)			// store f0-f13
	stfd	f1,VS_f1(r11)
	stfd	f2,VS_f2(r11)
	stfd	f3,VS_f3(r11)
	stfd	f4,VS_f4(r11)
	stfd	f5,VS_f5(r11)
	stfd	f6,VS_f6(r11)
	stfd	f7,VS_f7(r11)
	stfd	f8,VS_f8(r11)
	stfd	f9,VS_f9(r11)
	stfd	f10,VS_f10(r11)
	stfd	f11,VS_f11(r11)
	stfd	f12,VS_f12(r11)
	stfd	f13,VS_f13(r11)
	std	r29,VS_iar(r11)			// store preserved iar and msr
	std	r30,VS_msr(r11)
	mfcr	r0				// store cr
	std	r0,VS_cr(r11)
	std	r29,VS_ctr(r11)			// store ctr (ctr == iar)
	std	r31,VS_lr(r11)			// store preserved lr
	mfxer	r0				// store xer and fpscr
	std	r0,VS_xer(r11)
	mffs	f0
	stfd	f0,VS_fpscr(r11)
	ld	r27,STK_LOCAL1(r12)		// restore r27-r31
	ld	r28,STK_LOCAL2(r12)
	ld	r29,STK_LOCAL3(r12)
	ld	r30,STK_LOCAL4(r12)
	ld	r31,STK_LOCAL5(r12)
	mr	r3,r11				// address of VolatileState
	bl	CODE(SandboxUpcall)
	GET_DISPATCHER(r12)
	std	r13,DD_sandboxShepherd(r12)	// shepherd = CurrentThread
	la	r3,STK_LOCAL0(r1)	// r3 = addr of volatile state on stack
	b	UserResumeFromMemory
	// NOTREACHED
	trap
    SVCEntry_Native:
	LOAD_USER_STATE_ADDR(r10,r12)		// r10 = userStatePtr()
	ld	r1,VS_r1(r10)			// get back on caller's stack
	stdu	r1,-SVC_STK_SIZE(r1)		// push interrupt frame with
						//     three data slots
	std	r31,STK_LOCAL0(r1)		// preserve r31
	ld	r31,VS_iar(r10)			// retrieve caller's iar
	std	r30,STK_LOCAL1(r1)		// preserve r30
	ld	r30,SVC_STK_SIZE+STK_LR(r1)	// preserve old LR slot
	std	r29,STK_LOCAL2(r1)		// preserve r29
	ld	r29,VS_r2(r10)			// preserve caller's toc
	std	r31,SVC_STK_SIZE+STK_LR(r1)	// save iar in old LR slot
	ld	r31,VS_lr(r10)			// preserve caller's lr
    SVCEntry_Native_CallSVC64:	// SVCDirect_Native joins this path here.
	la	r9,SVC_STK_SIZE(r1)		// r9 = caller's stkPtr
	mr	r10,r0				// r10 = syscall number
	/*
	 * SVC64() is called enabled and returns enabled.  r11 and r12
	 * still hold &extRegsLocal and extRegsLocal.dispatcher, respectively.
	 */
	// Enable();
	li	r0,0
	std	r0,XR_disabled(r11)
	lwz	r0,D_flags(r12)
	cmplwi	r0,0
	bnel-	CODE(VolatileEnabledSelfInterrupt)
	// rc = SVC64(a0, a1, a2, a3, a4, a5, stkPtr, svcNum);
	// syscall parameters are already in r3-r8, stkPtr in r9, svcNum in r10
	bl	C_TEXT(K42Linux_SVC64)
	/*
	 * Restore state and return to caller.
	 */
	ld	r0,SVC_STK_SIZE+STK_LR(r1)	// retrieve caller's iar
	mtlr	r31				// restore caller's lr
	std	r30,SVC_STK_SIZE+STK_LR(r1)	// restore old LR slot
	mr	r2,r29				// restore caller's toc
	mtctr	r0				// prepare for bctr
	ld	r31,STK_LOCAL0(r1)		// restore r31
	ld	r30,STK_LOCAL1(r1)		// restore r30
	ld	r29,STK_LOCAL2(r1)		// restore r29
	la	r1,SVC_STK_SIZE(r1)		// pop frame
	// fixup return value as per comment above
	sradi.	r0,r3,63			// spread sign bit across word
	xor	r3,r3,r0			// ones complement (if r0 = -1)
	subf	r3,r0,r3			// twos complement (if r0 = -1)
	crmove	cr0*4+so,cr0*4+lt		// set cr0 SO bit from LT bit
	bctr					// return to caller
	// NOTREACHED
	trap
CODE_END(DispatcherDefault_SVCEntry)

/*
 * On entry, r12 = caller's original lr, lr = return address ("iar").
 * The caller's r1 and r2 are intact.  r0 contains the system call number and
 * r3-r8 contain the system call arguments.  r9-r11 are available.
 */
CODE_ENTRY(DispatcherDefault_SVCDirect)
	mr	r9,r2				// hang on to caller's r2
	ld	r2,SVC_DIRECT_VEC_ADDR+EPD_toc(0) // load our toc
	mr	r10,r12				// hang on to caller's lr
	LOAD_C_DATA_ADDR(r11,extRegsLocal)	// r11 = &extRegsLocal
	li	r12,1				// disable()
	std	r12,XR_disabled(r11)
	ld	r12,XR_dispatcher(r11)		// r12 = dispatcher
	ld	r11,DD_sandboxShepherd(r12)	// r11 = shepherd thread
	cmpldi	r11,0				// if (shepherd is NULL)
	beq-	SVCDirect_Native		//     goto Native
    SVCDirect_Sandbox:
	/*
	 * In sandbox.  Construct stack and register state to match
	 * that constructed on the corresponding path in SVCEntry, and then
	 * continue on that path.
	 */
	ld	r11,TH_startSP(r11)		// r11 = shepherd's stk ptr
	la	r11,-(STK_SIZE+(6*8))(r11)	// frame with 6 data slots
	std	r13,STK_LOCAL0(r11)		// preserve caller's r13
	ld	r13,DD_sandboxShepherd(r12)	// CurrentThread = shepherd
	std	r27,STK_LOCAL1(r11)		// preserve r27-r31
	std	r28,STK_LOCAL2(r11)
	std	r29,STK_LOCAL3(r11)
	std	r30,STK_LOCAL4(r11)
	std	r31,STK_LOCAL5(r11)
	mr	r27,r1		/* r1 */	// preserve caller's r1, r2,
	mr	r28,r9		/* r2 */	//     "iar", msr, and lr
	mflr	r29		/* "iar" */
	ld	r30,DD_msr(r12)	/* default msr */
	mr	r31,r10		/* lr */
	li	r1,0				// clear shepherd thread
	std	r1,DD_sandboxShepherd(r12)
	stdux	r1,r1,r11			// clear back chain and switch
						//     to shepherd stack :-)
	LOAD_C_DATA_ADDR(r11,extRegsLocal)	// retrieve &extRegsLocal
	b	SVCEntry_Sandbox_CallSVC64
	// NOTREACHED
	trap
    SVCDirect_Native:
	/*
	 * On a native thread.  Construct stack and register state to match
	 * that constructed on the corresponding path in SVCEntry, and then
	 * continue on that path.
	 */
	stdu	r1,-SVC_STK_SIZE(r1)		// push interrupt frame with
						//     three data slots
	std	r31,STK_LOCAL0(r1)		// preserve r31
	mflr	r31				// retrieve caller's "iar"
	std	r30,STK_LOCAL1(r1)		// preserve r30
	ld	r30,SVC_STK_SIZE+STK_LR(r1)	// preserve old LR slot
	std	r29,STK_LOCAL2(r1)		// preserve r29
	mr	r29,r9				// preserve caller's r2
	std	r31,SVC_STK_SIZE+STK_LR(r1)	// save iar in old LR slot
	mr	r31,r10				// preserve caller's lr
	LOAD_C_DATA_ADDR(r11,extRegsLocal)	// retrieve &extRegsLocal
	b	SVCEntry_Native_CallSVC64
	// NOTREACHED
	trap
CODE_END(DispatcherDefault_SVCDirect)

/*
 * extern "C" void
 *	DispatcherDefault_LaunchSandbox(VolatileState *vsp,
 *					NonvolatileState *nvsp);
 */
C_TEXT_ENTRY(DispatcherDefault_LaunchSandbox)
	NVS_RESTORE(r4,0)
	b	UserResumeFromMemory	// r3 = &VS, as required in Resume
	// NOTREACHED
	trap
C_TEXT_END(DispatcherDefault_LaunchSandbox)
