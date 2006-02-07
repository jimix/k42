/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocalAsm.S,v 1.121 2005/02/22 16:02:40 rosnbrg Exp $
 *****************************************************************************/
#include <sys/kinclude.H>
#include <sys/config.H>
#include <sys/syscalls.H>
#include <misc/hardware.H>
#include <misc/asm.h>
#include <misc/expedient.H>
#include <misc/volatileFrame.H>
#include <misc/arch/powerpc/asdef.h>
#include <misc/arch/powerpc/trap.h>
#include <sys/arch/powerpc/asmConstants.H>
#include <defines/use_expedient.H>
#include <sys/hcall.h>

/*
 * When we're entered from user-mode and we're going to execute C code, we must
 * get (at least) the "control" part (bits 24-31) of fpscr to a known state.
 */
#define INIT_FPSCR()\
	mtfsfi	6,0;\
	mtfsfi	7,0

#define EXCEPTION_STACK(reservation,scratchReg)\
	LOAD_C_DATA_UVAL(r1,exceptionLocal,EL_exceptionStack);\
	li	scratchReg,0;\
	stdu	scratchReg,-(STK_SIZE+((reservation)*8))(r1)

#define STACK_TOC_AND_FPSCR(elReg,reservation)\
	ld	r1,EL_exceptionStack(elReg);\
	li	r2,0;\
	stdu	r2,-(STK_SIZE+((reservation)*8))(r1);\
	ld	r2,EL_toc(elReg);\
	INIT_FPSCR()

#define THREAD_STACK(threadReg,reservation,scratchReg)\
	ld	r1,TH_startSP(threadReg);\
	li	scratchReg,0;\
	stdu	scratchReg,-(STK_SIZE+((reservation)*8))(r1)

#define DEBUGGER_STACK(reservation,scratchReg)\
	LOAD_C_DATA_UVAL(r1,exceptionLocal,EL_currentDebugStack);\
	li	scratchReg,0;\
	stdu	scratchReg,-(STK_SIZE+((reservation)*8))(r1)

#define ENABLE_HW_INTERRUPTS(scratchReg)\
	mfmsr	scratchReg;\
	ori	scratchReg,scratchReg,PSL_EE;\
	mtmsrd	scratchReg

#define DISABLE_HW_INTERRUPTS(scratchReg)\
	mfmsr	scratchReg;\
	xori	scratchReg,scratchReg,PSL_EE;\
	mtmsrd	scratchReg

/*
 * In debugging builds, in order to catch inappropriate exception-level
 * extRegsLocal.dispatcher dereferences, we clobber it before calling
 * legitimate C code and put it back afterwards.
 */
#ifndef	NDEBUG
#define HIDE_DISPATCHER(xrReg,dspReg) \
	LOAD_C_DATA_ADDR(xrReg,extRegsLocal);\
	li	dspReg,-1;\
	std	dspReg,XR_dispatcher(xrReg)
#define RESTORE_DISPATCHER(xrReg,dspReg) \
	LOAD_C_DATA_UVAL(dspReg,exceptionLocal,EL_currentProcessAnnex);\
	ld	dspReg,PA_dispatcherUser(dspReg);\
	LOAD_C_DATA_ADDR(xrReg,extRegsLocal);\
	std	dspReg,XR_dispatcher(xrReg)
#else
#define HIDE_DISPATCHER(xrReg,dspReg)
#define RESTORE_DISPATCHER(xrReg,dspReg)
#endif	// !NDEBUG

#ifndef NDEBUG
/*
 * In debugging builds, when a thread is returned to the free list, we
 * check whether it has stepped on the last 16 words of its stack.
 */
#define DEBUG_CREATE_STK_FENCE(fenceReg,scratchReg)\
	li	scratchReg,0xbf;\
	rldimi	scratchReg,scratchReg,8,48;\
	rldimi	scratchReg,scratchReg,16,32;\
	rldimi	scratchReg,scratchReg,32,0;\
	std	scratchReg,( 0*8)(fenceReg);\
	std	scratchReg,( 1*8)(fenceReg);\
	std	scratchReg,( 2*8)(fenceReg);\
	std	scratchReg,( 3*8)(fenceReg);\
	std	scratchReg,( 4*8)(fenceReg);\
	std	scratchReg,( 5*8)(fenceReg);\
	std	scratchReg,( 6*8)(fenceReg);\
	std	scratchReg,( 7*8)(fenceReg);\
	std	scratchReg,( 8*8)(fenceReg);\
	std	scratchReg,( 9*8)(fenceReg);\
	std	scratchReg,(10*8)(fenceReg);\
	std	scratchReg,(11*8)(fenceReg);\
	std	scratchReg,(12*8)(fenceReg);\
	std	scratchReg,(13*8)(fenceReg);\
	std	scratchReg,(14*8)(fenceReg);\
	std	scratchReg,(15*8)(fenceReg)
#define DEBUG_CHK_STK_FENCE(fenceReg,regA,regB,regC)\
	li	regA,0xbf;\
	rldimi	regA,regA,8,48;\
	rldimi	regA,regA,16,32;\
	rldimi	regA,regA,32,0;\
	mr	regB,regA;\
	ld	regC,( 0*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 1*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 2*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 3*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 4*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 5*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 6*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 7*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 8*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,( 9*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,(10*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,(11*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,(12*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,(13*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,(14*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	ld	regC,(15*8)(fenceReg); or regA,regA,regC; and regB,regB,regC;\
	tdne	regA,regB
#define DEBUG_CHK_STK_FREE_THREAD(threadReg,fenceReg,regA,regB,regC)\
	ld	fenceReg,TH_bottomSP(threadReg);\
	DEBUG_CHK_STK_FENCE(fenceReg,regA,regB,regC)
#else
#define DEBUG_CREATE_STK_FENCE(fenceReg,regA,regB,regC)
#define DEBUG_CHK_STK_FENCE(fenceReg,regA,regB,regC)
#define DEBUG_CHK_STK_FREE_THREAD(threadReg,fenceReg,regA,regB,regC)
#endif	// !NDEBUG

TOC_C_DATA_ENTRY(exceptionLocal)
TOC_C_DATA_ENTRY(kernelInfoLocal)
TOC_C_DATA_ENTRY(extRegsLocal)
TOC_C_TEXT_ENTRY(ExceptionLocal_PseudoSimosBreakpoint)
TOC_C_DATA_ENTRY(GDBStub_KernelTrap)
TOC_C_DATA_ENTRY(ExceptionExp_ResumeUserProcess)
TOC_C_DATA_ENTRY(ExceptionExp_AcquireReservedThread)
TOC_C_DATA_ENTRY(ExceptionExp_ReleaseReservedThread)
TOC_C_DATA_ENTRY(ExceptionExp_NullGenericEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullRunEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullInterruptEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullTrapEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullPgfltEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullIPCCallEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullIPCReturnEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullIPCFaultEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_NullSVCEntryException)
TOC_C_DATA_ENTRY(ExceptionExp_TrapExceptionUser)
TOC_C_DATA_ENTRY(ExceptionExp_HandleUserPgflt)
TOC_C_DATA_ENTRY(ExceptionExp_HandleKernelPgflt)
TOC_C_DATA_ENTRY(ExceptionExp_UserInterruptContinue)
TOC_C_DATA_ENTRY(ExceptionExp_KernelInterruptContinue)
TOC_C_DATA_ENTRY(ExceptionExp_NonnativeSyscall)
TOC_C_DATA_ENTRY(ExceptionExp_IPCCallSyscall)
TOC_C_DATA_ENTRY(ExceptionExp_IPCReturnSyscall)
TOC_C_DATA_ENTRY(ExceptionExp_IPCSyscallRemoteOnThread)
TOC_C_DATA_ENTRY(ExceptionExp_KernelReplyRemoteOnThread)
TOC_C_DATA_ENTRY(ExceptionExp_AcceptRemoteIPC)
TOC_C_DATA_ENTRY(ExceptionExp_PPCPrimitiveSyscall)
TOC_C_DATA_ENTRY(ExceptionExp_IPCAsyncSyscall)
TOC_C_DATA_ENTRY(ExceptionExp_IPCAsyncSyscallRemoteOnThread)
TOC_C_DATA_ENTRY(ExceptionExp_InvalidSyscall)

/*
 * NOTE:  Any translation-on code that includes an rfid instruction must
 * ensure that there are no page faults, not even mapping faults, between
 * the instruction that loads the first srr and the rfid.  We gather all
 * such code sequences here on a single page, and we bolt a pagetable entry
 * that maps that page (see initKernelMappings.C).  We also bolt the first
 * page of exceptionLocal. Critical code sequences, if they make data
 * references at all, can only make them to that page.
 */
#define MAX_ALIGN 12
TEXT_ALIGN(MAX_ALIGN)
CODE_LABEL(ExceptionLocal_CriticalPage)

/*
 * All entry points are launched with extRegsLocal.disabled set to 1 and
 * extRegsLocal.dispatcher set to the user-mode dispatcher address.  r2 is
 * initialized to the value specified in the entry point descriptor, r11 is
 * set to &extRegsLocal, and r12 is set to extRegsLocal.dispatcher.
 */
#define LAUNCH_ENTRY(entry) \
    CODE_BASIC_ENTRY(ExceptionLocal_Launch_##entry); \
	/* r11 = currentProcessAnnex */; \
	LOAD_C_DATA_ADDR(r1,extRegsLocal); \
	ld	r12,PA_dispatcherUser(r11); \
	mtctr	r0; /* save temporarily */ \
	ld	r0,(PA_launcher+(entry*EPL_SIZE)+EPL_msr)(r11); \
	li	r2,1; \
	std	r12,XR_dispatcher(r1); \
	std	r2,XR_disabled(r1); \
	ld	r2,(PA_launcher+(entry*EPL_SIZE)+EPL_toc)(r11); \
	std	r0,PA_msr(r11); \
	ld	r11,(PA_launcher+(entry*EPL_SIZE)+EPL_iar)(r11); \
	/* no further data references */; \
	mtsrr1	r0; \
	mtsrr0	r11; \
	mr	r11,r1; \
	mfctr	r0; /* restore */ \
	rfid; \
    CODE_BASIC_END(ExceptionLocal_Launch_##entry)

LAUNCH_ENTRY(RUN_ENTRY)
LAUNCH_ENTRY(INTERRUPT_ENTRY)
LAUNCH_ENTRY(TRAP_ENTRY)
LAUNCH_ENTRY(PGFLT_ENTRY)
LAUNCH_ENTRY(IPC_CALL_ENTRY)
LAUNCH_ENTRY(IPC_RTN_ENTRY)
LAUNCH_ENTRY(IPC_FAULT_ENTRY)
LAUNCH_ENTRY(SVC_ENTRY)

CODE_ENTRY(SyscallReturn)
	ld	r11,PA_msr(r11)		// get msr to restore
	// no further data references
	mtsrr0	r12			// set iar for rfid
	mtsrr1  r11			// set msr for rfid
	rfid				// resume
CODE_END(SyscallReturn)

CODE_ENTRY(ExceptionLocal_UserResume)
    SaveAreaResume:
	LOAD_C_DATA_ADDR(r2,exceptionLocal)
	ld	r12,VS_r2(r3)		// copy r0-r2 to exceptionLocal for
	ld	r11,VS_r1(r3)		//     for safe access later
	ld	r10,VS_r0(r3)
	ld	r2, EL_boltedRfiState(r2)
	ld	r1,VS_iar(r3)		// retrieve iar, msr for rfid
	ld	r0,VS_msr(r3)
	std	r12,RfiState_r2(r2)
	std	r11,RfiState_r1(r2)
	std	r10,RfiState_r0(r2)
#if VS_iar == 0
	stdcx.	r1,0,r3			// clear any ll/sc reservation
#else
	li	r7,VS_iar
	stdcx.	r1,r7,r3		// clear any ll/sc reservation
#endif
	ld	r12,VS_cr(r3)		// retrieve special registers
	ld	r11,VS_ctr(r3)
	ld	r10,VS_lr(r3)
	ld	r9,VS_xer(r3)
	mtcr	r12			// restore special registers
	mtctr	r11
	mtlr	r10
	mtxer	r9
	lfd	f0,VS_fpscr(r3)		// restore fpscr
	mtfsf	0xff,f0
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
	ld	r13,VS_r13(r3)		// restore volatile gpr's
	ld	r12,VS_r12(r3)
	ld	r11,VS_r11(r3)
	ld	r10,VS_r10(r3)
	ld	r9,VS_r9(r3)
	ld	r8,VS_r8(r3)
	ld	r7,VS_r7(r3)
	ld	r6,VS_r6(r3)
	ld	r5,VS_r5(r3)
	ld	r4,VS_r4(r3)
	ld	r3,VS_r3(r3)		// done with r3 now
	// all further data accesses must be to exceptionLocal
	mtsrr1	r0			// restore iar, msr for rfid
	mtsrr0	r1
	ld	r0,RfiState_r0(r2)	// restore r0-r2
	ld	r1,RfiState_r1(r2)
	ld	r2,RfiState_r2(r2)
	rfid
CODE_END(ExceptionLocal_UserResume)

CODE_ENTRY(ExceptionLocal_UserRFISyscall)
	// lolita left r12 = &EL
	ld	r2,EL_toc(r12)
	ld	r11,EL_currentProcessAnnex(r12)	// r11 = PA
	ld	r1,PA_dispatcher(r11)		// r1 = dispatcher
	ld	r13,PA_userStateOffset(r11)
	add	r13,r1,r13			// r13 = PA.userStatePtr()
	// We're about to enable caller, so we have to check for interrupts.
	lwz	r0,D_flags(r1)			// if (interrupts pending)
	cmpwi	r0,0				//     goto InterruptResume
	bne-	CODE(InterruptResume)
	LOAD_C_DATA_ADDR(r1,extRegsLocal)	// r1 = &extRegsLocal
	std	r0,XR_disabled(r1)		// enable; (r0 == 0, now)
	ld	r2,EL_boltedRfiState(r12)       // processors bolted storage
	ld	r0,VS_r0(r13)			// copy user's r0-r2 to EL
	ld	r1,VS_r1(r13)			//     for safe access later
	std	r0,RfiState_r0(r2)              // not enough registers for
	ld	r0,VS_r2(r13)                   // this to be pretty
	std	r1,RfiState_r1(r2)
	std	r0,RfiState_r2(r2)
	ld	r0,EL_msrUserChange(r12)	// r0 = user-settable bits mask
	ld	r1,PA_msr(r11)			// r1 = current msr for process
	ld	r2,VS_msr(r13)			// r2 = requested msr
	andc	r1,r1,r0			// clear user-settable bits
	and	r2,r2,r0			// extract user-settable bits
	or	r0,r2,r1			// merge to form msr for rfid
	std	r0,PA_msr(r11)			// store new msr in PA
	ld	r1,VS_iar(r13)			// retrieve iar for rfid
	ld	r2,VS_cr(r13)			// retrieve cr
#if VS_iar == 0
	stdcx.	r1,0,r13			// clear any ll/sc reservation
#else
	li	r11,VS_iar
	stdcx.	r1,r11,r13			// clear any ll/sc reservation
#endif
	mtcr	r2				// restore cr
	ld	r2,EL_boltedRfiState(r12)       // processors bolted storage
	ld	r11,VS_r11(r13)			// restore r11-r13
	ld	r12,VS_r12(r13)
	ld	r13,VS_r13(r13)
	// all further data accesses must be to bolted storage
	mtsrr1	r0				// restore iar, msr for rfid
	mtsrr0	r1
	ld	r0,RfiState_r0(r2)		// restore r0-r2
	ld	r1,RfiState_r1(r2)
	ld	r2,RfiState_r2(r2)
	rfid
CODE_END(ExceptionLocal_UserRFISyscall)

CODE_ENTRY(ExceptionLocal_TrapRFISyscall)
	// lolita left r12 = &EL
	ld	r2,EL_toc(r12)
	ld	r11,EL_currentProcessAnnex(r12)	// r11 = PA
	ld	r1,PA_dispatcher(r11)		// r1 = dispatcher
	ld	r13,PA_trapStateOffset(r11)
	add	r13,r1,r13			// r13 = PA.trapStatePtr()
	// Restore disabled status.  If we're resuming enabled, we have
	// to check for pending interrupts.
	ld	r0,D_trapDisabledSave(r1)
	cmpldi	r0,0
	bne+	TrapRFISyscall_Resume
	lwz	r0,D_flags(r1)			// if (interrupts pending)
	cmpwi	r0,0				//     goto InterruptResume
	bne-	TrapRFISyscall_InterruptResume
	LOAD_C_DATA_ADDR(r1,extRegsLocal)	// r1 = &extRegsLocal
	std	r0,XR_disabled(r1)		// enable; (r0 == 0, now)
    TrapRFISyscall_Resume:
	ld	r2,EL_boltedRfiState(r12)       // processors bolted storage
	ld	r0,VS_r0(r13)			// copy user's r0-r2 to EL
	ld	r1,VS_r1(r13)			//     for safe access later
	std	r0,RfiState_r0(r2)
	ld	r0,VS_r2(r13)
	std	r1,RfiState_r1(r2)
	std	r0,RfiState_r2(r2)
	ld	r0,EL_msrUserChange(r12)	// r0 = user-settable bits mask
	ld	r1,PA_msr(r11)			// r1 = current msr for process
	ld	r2,VS_msr(r13)			// r2 = requested msr
	andc	r1,r1,r0			// clear user-settable bits
	and	r2,r2,r0			// extract user-settable bits
	or	r0,r2,r1			// merge to form msr for rfid
	std	r0,PA_msr(r11)			// store new msr in PA
	ld	r1,VS_iar(r13)			// retrieve iar for rfid
	ld	r2,VS_cr(r13)			// retrieve cr
#if VS_iar == 0
	stdcx.	r1,0,r13			// clear any ll/sc reservation
#else
	li	r11,VS_iar
	stdcx.	r1,r11,r13			// clear any ll/sc reservation
#endif
	mtcr	r2				// restore cr
	ld	r2,EL_boltedRfiState(r12)       // processors bolted storage
	ld	r11,VS_r11(r13)
	ld	r12,VS_r12(r13)			// restore r11-r13
	ld	r13,VS_r13(r13)
	// all further data accesses must be to bolted storage
	mtsrr1	r0				// restore iar, msr for rfid
	mtsrr0	r1
	ld	r0,RfiState_r0(r2)		// restore r0-r2
	ld	r1,RfiState_r1(r2)
	ld	r2,RfiState_r2(r2)
	rfid
    TrapRFISyscall_InterruptResume:
	// Swap user and trap states.
	ld	r0,PA_userStateOffset(r11)
	ld	r12,PA_trapStateOffset(r11)
	std	r0,PA_trapStateOffset(r11)
	std	r0,D__trapStateOffset(r1)
	std	r12,PA_userStateOffset(r11)
	std	r12,D__userStateOffset(r1)
	b	CODE(InterruptResume)
CODE_END(ExceptionLocal_TrapRFISyscall)

/*
 * Code fragment that anyone can use to execute an rfid safely.  The target
 * IAR and MSR are passed in r11 and r12, respectively.
 */
CODE_ENTRY(ExceptionLocal_SafeRFID)
	mtsrr0	r11		// target IAR
	mtsrr1	r12		// target MSR
	rfid
CODE_END(ExceptionLocal_SafeRFID)

/*
 * End of critical code sequences (sequences that include rfid instructions).
 */
CODE_LABEL(ExceptionLocal_CriticalPageEnd)

CODE_ENTRY(ExceptionLocal_NullGenericEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullGenericEntryException)
CODE_END(ExceptionLocal_NullGenericEntryException)

CODE_ENTRY(ExceptionLocal_NullRunEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullRunEntryException)
CODE_END(ExceptionLocal_NullRunEntryException)

CODE_ENTRY(ExceptionLocal_NullInterruptEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullInterruptEntryException)
CODE_END(ExceptionLocal_NullInterruptEntryException)

CODE_ENTRY(ExceptionLocal_NullTrapEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullTrapEntryException)
CODE_END(ExceptionLocal_NullTrapEntryException)

CODE_ENTRY(ExceptionLocal_NullPgfltEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullPgfltEntryException)
CODE_END(ExceptionLocal_NullPgfltEntryException)

CODE_ENTRY(ExceptionLocal_NullIPCCallEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullIPCCallEntryException)
CODE_END(ExceptionLocal_NullIPCCallEntryException)

CODE_ENTRY(ExceptionLocal_NullIPCReturnEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullIPCReturnEntryException)
CODE_END(ExceptionLocal_NullIPCReturnEntryException)

CODE_ENTRY(ExceptionLocal_NullIPCFaultEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullIPCFaultEntryException)
CODE_END(ExceptionLocal_NullIPCFaultEntryException)

CODE_ENTRY(ExceptionLocal_NullSVCEntryException)
	INIT_FPSCR()
	EXCEPTION_STACK(0, r11)
	GOTO_EXPEDIENT(ExceptionExp_NullSVCEntryException)
CODE_END(ExceptionLocal_NullSVCEntryException)

CODE_ENTRY(ExceptionLocal_CreateThread)
	VOLATILE_FRAME_ENTER()
	// CurrentThread = ExceptionCreateThread(dispatcher);
	mr	r3,r12			// r3 = dispatcher
	bl	C_TEXT(DispatcherDefaultKern_ExceptionCreateThread)
	mr	r13,r3			// CurrentThread = thread
	VOLATILE_FRAME_RETURN()
CODE_END(ExceptionLocal_CreateThread)

/*
 * All registers except r0, r3, and r11-r13 may be in use.  On return, r0
 * contains the old value of extRegsLocal.disabled, r3 contains srcProc,
 * we've context-switched to the kernel, allocated a reserved thread for
 * srcProc, and left r13 pointing to the reserved thread.  r11 and r12 are
 * not preserved.
 */
CODE_ENTRY(ExceptionLocal_AcquireReservedThread)
#ifdef	USE_EXPEDIENT_RESERVED_THREAD
	FRAME_ENTER(0, r11)
	CALL_EXPEDIENT(ExceptionExp_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
	FRAME_RETURN(0, r11)
#else
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	// erp->srcProc = exceptionLocal.currentProcessAnnex;
	ld	r3,EL_currentProcessAnnex(r12)
	// erp->curProc = exceptionLocal.kernelProcessAnnex;
	ld	r11,EL_kernelProcessAnnex(r12)
	// erp->curProc->switchContextKernel();
	std	r11,EL_currentProcessAnnex(r12)
	ld	r0,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r0
	// erp->dispatcher =
	//     (DispatcherDefaultKern *) erp->curProc->dispatcherUser;
	ld	r12,PA_dispatcherUser(r11)
	// erp->disabledSave = extRegsLocal.disabled;
	// extRegsLocal.disabled = 0;
	// extRegsLocal.dispatcher = erp->dispatcher;
	// CurrentThread = erp->dispatcher->freeList;
	LOAD_C_DATA_ADDR(r11,extRegsLocal)
	ld	r0,XR_disabled(r11)
	li	r13,0
	std	r13,XR_disabled(r11)
	std	r12,XR_dispatcher(r11)
	ld	r13,DD_freeList(r12)
	// if (CurrentThread == NULL) goto NoThread
	cmpldi	r13,0
	beq-	AcquireReservedThread_NoThread
	// erp->dispatcher->freeList = CurrentThread->next;
	ld	r11,TH_next(r13)
	std	r11,DD_freeList(r12)
	// erp->srcProc->reservedThread = CurrentThread;
	std	r13,PA_reservedThread(r3)
	blr				// return
    AcquireReservedThread_NoThread:
	FRAME_ENTER(0, r11)
	// CALL_EXCEPTION_LOCAL_ASM(erp, CreateThread);
	bl	CODE(ExceptionLocal_CreateThread)
	// if (CurrentThread != NULL) goto Panic
	cmpldi	r13,0
	beq-	AcquireReservedThread_Panic
	// erp->srcProc->reservedThread = CurrentThread;
	std	r13,PA_reservedThread(r3)
	FRAME_RETURN(0, r11)
    AcquireReservedThread_Panic:
	trap
#endif	// !USE_EXPEDIENT_RESERVED_THREAD
CODE_END(ExceptionLocal_AcquireReservedThread)

/*
 * On entry, r11 (curProc) contains the process annex to which we're
 * returning, and r13 (CurrentThread) contains the kernel thread that we're
 * releasing.  r11 is preserved, and r13 is restored to the value preserved
 * in curProc.  r0, r12, and r8-r10 are available and are not preserved.
 * All other registers may be in use.
 */
CODE_ENTRY(ExceptionLocal_ReleaseReservedThread)
#ifdef	USE_EXPEDIENT_RESERVED_THREAD
	FRAME_ENTER(0, r0)
	CALL_EXPEDIENT(ExceptionExp_ReleaseReservedThread)
	FRAME_RETURN(0, r0)
#else
	// erp->curProc->reservedThread = NULL;
	li	r0,0
	std	r0,PA_reservedThread(r11)
	// erp->dispatcher = DISPATCHER_KERN;
	LOAD_C_DATA_ADDR(r10,extRegsLocal)
	ld	r12,XR_dispatcher(r10)
	// erp->dispatcher->exceptionFreeThread(CurrentThread);
	ld	r0,DD_freeList(r12)
	std	r0,TH_next(r13)
	std	r13,DD_freeList(r12)
	DEBUG_CHK_STK_FREE_THREAD(r13,r12,r9,r8,r0)
	// return to interrupted process
	// extRegsLocal.dispatcher = erp->curProc->dispatcherUser;
	ld	r0,PA_dispatcherUser(r11)
	std	r0,XR_dispatcher(r10)
	// erp->curProc->switchContextUser();
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	std	r11,EL_currentProcessAnnex(r12)
	ld	r10,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r10
	ld	r10,PA_segmentTable(r11)
	ld	r0,EL_currentSegmentTable(r12)
	cmpld	r10,r0
	beqlr+				// return if already in target space
	std	r10,EL_currentSegmentTable(r12)
	ld	r0, EL_segLoad(r12)	// get next SLB index
	cmpldi	r0, 0			// compare with zero
	bne	ReleaseReservedThread_SoftwareSLB
	ld	r9,PA_asr(r11)
	isync
	mtasr	r9
	slbia
	isync
	blr				// return
	
	// r10 addresses new segment table - must take care after
	// slbia to avoid a segment fault on the address in r10 -
	// would be a performance bug
ReleaseReservedThread_SoftwareSLB:	
	ld	r8, ST_SLBCache+16(r10)	// ste maybe covering current seg table
	ld	r9, ST_SLBCache+24(r10)
	isync
	slbia				// invalidate all SLB entries except 0th
	slbmte	r8, r9			// reload 2nd SLB entry
	isync
	ld      r8, ST_cacheMax(r10)
	std     r8, ST_slbNext(r10)
	subic.  r8, r8, 2
	ble     2f
	la      r10, ST_SLBCache+24(r10)
	mtctr	r8
1:	
	ldu	r8, 8(r10)
	ldu	r9, 8(r10)
	slbmte  r8, r9
	bdnz	1b
2:	
	isync
	blr				// return
#endif	// !USE_EXPEDIENT_RESERVED_THREAD
CODE_END(ExceptionLocal_ReleaseReservedThread)

CODE_ENTRY(ExceptionLocal_MakeCurProcReady)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_MakeCurrentProcessAnnexReady();
	bl	C_TEXT(ExceptionLocal_MakeCurrentProcessAnnexReady)
	VOLATILE_FRAME_RETURN()
CODE_END(ExceptionLocal_MakeCurProcReady)

/*
 * On entry, r2 = toc, r11 = curProc, r13 = curProc->userStatePtr().
 * Registers r0-r2, r11-r13, cr, iar, and msr are already saved in userState.
 */
CODE_ENTRY(InterruptResume)
	std	r3,VS_r3(r13)		// save the volatile gpr's that
	std	r4,VS_r4(r13)		//     aren't already saved
	std	r5,VS_r5(r13)
	std	r6,VS_r6(r13)
	std	r7,VS_r7(r13)
	std	r8,VS_r8(r13)
	std	r9,VS_r9(r13)
	std	r10,VS_r10(r13)
	mfctr	r3			// retrieve special registers
	mflr	r4
	mfxer	r5
	std	r3,VS_ctr(r13)		// save special registers
	std	r4,VS_lr(r13)
	std	r5,VS_xer(r13)
	stfd	f0,VS_f0(r13)		// save volatile fpr's
	stfd	f1,VS_f1(r13)
	stfd	f2,VS_f2(r13)
	stfd	f3,VS_f3(r13)
	stfd	f4,VS_f4(r13)
	stfd	f5,VS_f5(r13)
	stfd	f6,VS_f6(r13)
	stfd	f7,VS_f7(r13)
	stfd	f8,VS_f8(r13)
	stfd	f9,VS_f9(r13)
	stfd	f10,VS_f10(r13)
	stfd	f11,VS_f11(r13)
	stfd	f12,VS_f12(r13)
	stfd	f13,VS_f13(r13)
	mffs	f0			// save fpscr
	stfd	f0,VS_fpscr(r13)
	// r2 = toc and r11 = curProc, still
	b	CODE(ExceptionLocal_Launch_INTERRUPT_ENTRY)
CODE_END(InterruptResume)

CODE_ENTRY(FloatingPointSave)
	stfd	f0,VS_f0(r1)		// save volatile fpr's
	stfd	f1,VS_f1(r1)
	stfd	f2,VS_f2(r1)
	stfd	f3,VS_f3(r1)
	stfd	f4,VS_f4(r1)
	stfd	f5,VS_f5(r1)
	stfd	f6,VS_f6(r1)
	stfd	f7,VS_f7(r1)
	stfd	f8,VS_f8(r1)
	stfd	f9,VS_f9(r1)
	stfd	f10,VS_f10(r1)
	stfd	f11,VS_f11(r1)
	stfd	f12,VS_f12(r1)
	stfd	f13,VS_f13(r1)
	mffs	f0			// save fpscr
	stfd	f0,VS_fpscr(r1)
	blr
CODE_END(FloatingPointSave)

CODE_ENTRY(ExceptionLocal_ResumeUserProcess)
#ifdef	USE_EXPEDIENT_USER_RESUME
	GOTO_EXPEDIENT(ExceptionExp_ResumeUserProcess)
#else
	/*
	 * On entry, r11 (curProc) contains exceptionLocal.currentProcessAnnex
	 * being resumed, and r12 (curDispatcher) contains curProc->dispatcher.
	 */
	// if (erp->curDispatcher->interrupts.pending()) goto InterruptsPending
	lwz	r0,D_flags(r12)
	cmpwi	r0,0
	bne-	ResumeUserProcess_InterruptsPending
    ResumeUserProcess_Resume:
	// erp->volatileState = erp->curProc->excStatePtr();
	ld	r0,PA_excStateOffset(r11)
	add	r3,r12,r0			// r3 is erp->volatileState
	// GOTO_EXCEPTION_LOCAL_ASM(erp, UserResume);
	b	CODE(ExceptionLocal_UserResume)
    ResumeUserProcess_InterruptsPending:
	// if (extRegsLocal.disabled) goto Resume
	LOAD_C_DATA_UVAL(r3,extRegsLocal,XR_disabled)
	cmpldi	r3,0
	bne-	ResumeUserProcess_Resume
	// erp->curProc->swapUserAndExcStates();
	ld	r3,PA_excStateOffset(r11)
	ld	r4,PA_userStateOffset(r11)
	std	r3,PA_userStateOffset(r11)
	std	r4,PA_excStateOffset(r11)
	std	r3,D__userStateOffset(r12)
	ld	r0,PA_dispatcherPhysAddr(r11)
	add	r0,r0,r4
	std	r0,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r0
	//      GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_INTERRUPT_ENTRY);
	b	CODE(ExceptionLocal_Launch_INTERRUPT_ENTRY)
#endif	// !USE_EXPEDIENT_USER_RESUME
CODE_END(ExceptionLocal_ResumeUserProcess)

CODE_ENTRY(ExceptionLocal_TrapExceptionUser)
	bl	FloatingPointSave
	INIT_FPSCR()
	EXCEPTION_STACK(0, r0)
	GOTO_EXPEDIENT(ExceptionExp_TrapExceptionUser)
CODE_END(ExceptionLocal_TrapExceptionUser)

CODE_ENTRY(KernelTrap)
	FULLSAVE_FRAME_ENTER(0, r0)
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	/*
	 * Kernel debugging is more sane if we do not switch threads after
	 * every breakpoint or single-step.  The decrementer will almost
	 * certainly expire while we're talking to gdb, and on modern
	 * processors we cannot cancel the interrupt that will be pending.
	 * Instead we make sure the interrupt will be pending, but we set a
	 * flag to cause the interrupt to be postponed.
	 */
	li	r0,1
	mtdec	r0
	std	r0,EL_trapSkipDecr(r12)
	// GDBStub_KernelTrap(trapNumber, trapInfo, trapAuxInfo,
	//		      volatileStatePtr, nonvolatileStatePtr);
	// r3-r5 already hold trapNumber, trapInfo, and trapAuxInfo
	// r6 = address of volatile state
	la	r6,EL_trapVolatileState(r12)
	// r7 = address of nonvolatile state in FULLSAVE_FRAME
	la	r7,FULLSAVE_FRAME_NVS_OFFSET(0)(r1)
	bl	C_TEXT(GDBStub_KernelTrap)
	FULLSAVE_FRAME_RETURN(0, r0)
CODE_END(KernelTrap)

CODE_ENTRY(KernelAlignmentHandler)
	FULLSAVE_FRAME_ENTER(0, r0)
	// rc = fix_alignment(trapNumber, trapInfo, trapAuxInfo,
	//		      volatileStatePtr, nonvolatileStatePtr);
	// r3-r5 already hold trapNumber, trapInfo, and trapAuxInfo
	// r6 = address of volatile state already
	// r7 = address of nonvolatile state in FULLSAVE_FRAME
	la	r7,FULLSAVE_FRAME_NVS_OFFSET(0)(r1)
	bl	C_TEXT(fix_alignment)
	FULLSAVE_FRAME_RETURN(0, r0)
CODE_END(KernelAlignmentHandler)

CODE_ENTRY(ExceptionLocal_TrapExceptionKernel)
	bl	FloatingPointSave
    TrapExceptionKernelInternal:
	cmpldi  r3,EXC_ALI
	bne	TrapExceptionKernel_NotAlignment
	DEBUGGER_STACK(3, r0)
	std	r3,STK_LOCAL0(r1)	// preserve trap{Number,Info,AuxInfo}
	std	r4,STK_LOCAL1(r1)
	std	r5,STK_LOCAL2(r1)
	LOAD_C_DATA_OFFSET(r6,exceptionLocal,EL_trapVolatileState)
	bl	CODE(KernelAlignmentHandler)
	cmpldi	r3,0		// resume if handler succeeded
	LOAD_C_DATA_OFFSET(r3,exceptionLocal,EL_trapVolatileState)
	beq+	SaveAreaResume
	ld	r3,STK_LOCAL0(r1)	// restore trap{Number,Info,AuxInfo}
	ld	r4,STK_LOCAL1(r1)
	ld	r5,STK_LOCAL2(r1)
    TrapExceptionKernel_NotAlignment:
	LOAD_C_DATA_UVAL(r6,kernelInfoLocal,KI_onSim)
	cmpldi	r6,0
	bne	TrapExceptionKernel_OnSim
	DEBUGGER_STACK(0, r0)
	bl	CODE(KernelTrap)
	LOAD_C_DATA_OFFSET(r3,exceptionLocal,EL_trapVolatileState)
	b	SaveAreaResume
    TrapExceptionKernel_OnSim:
	/*
	 * Under SIMOS we don't run the kernel debugger as a stub, so we can't
	 * really deliver a trap to gdb directly.  Instead, we make it look as
	 * though the client executed a "bl PseudoSimosBreakpoint()"
	 * immediately prior to the trapping instruction.  This isn't perfect,
	 * but it usually leaves us in a state in which we can find and look
	 * at the code that caused the trap.
	 */
	LOAD_C_DATA_OFFSET(r3,exceptionLocal,EL_trapVolatileState)
	ld	r11,VS_iar(r3)		// set lr to old iar
	std	r11,VS_lr(r3)
					// set iar to &PseudoSimosBreakpoint
	LOAD_C_TEXT_ADDR(r11,ExceptionLocal_PseudoSimosBreakpoint)
	std	r11,VS_iar(r3)
	b	SaveAreaResume
CODE_END(ExceptionLocal_TrapExceptionKernel)

C_TEXT_ENTRY(ExceptionLocal_PseudoSimosBreakpoint)
	.long 	0x7c0007ce	// SIMOS_BREAKPOINT
	b	C_TEXT(ExceptionLocal_PseudoSimosBreakpoint)	// loop
	blr			// to make gdb happy
C_TEXT_END(ExceptionLocal_PseudoSimosBreakpoint)

CODE_ENTRY(TrapFromKernelPgflt)
	/*
	 * Copy the VolatileState from the stack to exceptionLocal where
	 * TrapExceptionKernel expects it.
	 */
	la	r1,-8(r1)		// prepare addresses for copy loop
	LOAD_C_DATA_OFFSET(r11,exceptionLocal,EL_trapVolatileState-8)
	li	r0,(VS_SIZE/8)		// set ctr to word count
	mtctr	r0
    TrapFromKernelPgflt_CopyLoop:
	ldu	r0,8(r1)
	stdu	r0,8(r11)
	bdnz	TrapFromKernelPgflt_CopyLoop
	li	r3,EXC_DSI  // may have been ISI, but we can't tell now
	b	TrapExceptionKernelInternal
CODE_END(TrapFromKernelPgflt)

CODE_ENTRY(ExceptionLocal_PgfltExceptionUser)
	bl	FloatingPointSave
	INIT_FPSCR()
	/*
	 * Note: AcquireReservedThread() needs a stack only if there's no
	 *       thread on the free list.  If we inlined it here, we
	 *       could avoid the couple cycles and cache-line touches it
	 *       takes to get onto EXCEPTION_STACK when it's not necessary.
	 */
	EXCEPTION_STACK(0, r0)
	bl	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
#ifdef	USE_EXPEDIENT_USER_PGFLT
	THREAD_STACK(r13, 0, r11)
	GOTO_EXPEDIENT(ExceptionExp_HandleUserPgflt)
#else
	THREAD_STACK(r13, 5, r11)
	// uval disabledSave = erp->disabledSave;	// preserve disabledSv
	// ProcessAnnex *srcProc = erp->srcProc;	// preserve srcProc
	// uval faultInfo = erp->Pgflt_faultInfo;	// preserve faultInfo
	// uval faultAddr = erp->Pgflt_faultAddr;	// preserve faultAddr
	std	r0,STK_LOCAL0(r1)		// LOCAL0 is disabledSave
	std	r3,STK_LOCAL1(r1)		// LOCAL1 is srcProc
	std	r4,STK_LOCAL2(r1)		// LOCAL2 is faultInfo
	std	r5,STK_LOCAL3(r1)		// LOCAL3 is faultAddr
	// exceptionLocal.dispatchQueue.
	//     pushCDABorrower(exceptionLocal.kernelProcessAnnex);
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	ld	r11,EL_cdaBorrowersTop(r12)
	addi	r11,r11,1
	rldicl	r11,r11,0,64-LOG_CDA_BORROWERS_SIZE
	std	r11,EL_cdaBorrowersTop(r12)
	sldi	r11,r11,3
	add	r11,r11,r12
	ld	r12,EL_kernelProcessAnnex(r12)
	std	r12,EL_cdaBorrowers(r11)
	// erp->Pgflt_noReflection = erp->disabledSave;
	mr	r6,r0				// r6 is erp->noReflection
	// CallExceptionLocalC(erp,
	//    ExceptionLocal_PgfltHandler(erp->srcProc,
	//				  erp->Pgflt_faultInfo,
	//				  erp->Pgflt_faultAddr,
	//				  erp->Pgflt_noReflection));
	// (srcProc, faultInfo, faultAddr, and noReflection are already in
	// r3, r4, r5, and r6, respectively.)
	bl	C_TEXT(ExceptionLocal_PgfltHandler)
	// erp->curProc = exceptionLocal.currentProcessAnnex;
	// if (erp->curProc->dispatcher->hasWork && !erp->curProc->isReady()) {
	//     CALL_EXCEPTION_LOCAL_ASM(erp, MakeCurProcReady);
	// }
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	ld	r11,EL_currentProcessAnnex(r12)
	ld	r10,PA_dispatcher(r11)
	ld	r0,D_hasWork(r10)
	cmpdi	r0,0
	beq+	PgfltExceptionUser_SourceOkay
	ld	r0,PA_cpuDomainNext(r11)
	cmpdi	r0,0
	beql-	CODE(ExceptionLocal_MakeCurProcReady)
    PgfltExceptionUser_SourceOkay:
	// erp->curProc = srcProc;
	// exceptionLocal.dispatchQueue.popCDABorrower();
	// if (exceptionLocal.dispatchQueue.currentCDABorrower() !=
	//							erp->curProc) {
	//     goto Reschedule;
	// }
	ld	r11,STK_LOCAL1(r1)
	ld	r10,EL_cdaBorrowersTop(r12)
	subic.	r10,r10,1
	blt-	PgfltExceptionUser_Reschedule
	std	r10,EL_cdaBorrowersTop(r12)
	sldi	r10,r10,3
	add	r10,r10,r12
	ld	r10,EL_cdaBorrowers(r10)
	cmpld	r10,r11
	bne-	PgfltExceptionUser_Reschedule
    PgfltExceptionUser_MainPath:
	// CALL_EXCEPTION_LOCAL_ASM(erp, ReleaseReservedThread);
	bl	CODE(ExceptionLocal_ReleaseReservedThread)
	// if (erp->returnCode == 0) {
	cmpdi	r3,0
	bne	PgfltExceptionUser_NotYetHandled
	//    // in-core or disabled page fault:  resume process
	//    extRegsLocal.disabled = disabledSave;
	LOAD_C_DATA_ADDR(r12,extRegsLocal)
	ld	r0,STK_LOCAL0(r1)
	std	r0,XR_disabled(r12)
	//    erp->curDispatcher = erp->curProc->dispatcher;
	ld	r12,PA_dispatcher(r11)
	//    GOTO_EXCEPTION_LOCAL_ASM(erp, ResumeUserProcess);
	b	CODE(ExceptionLocal_ResumeUserProcess)
    PgfltExceptionUser_NotYetHandled:
	// } else if (erp->returnCode > 0) {
	//     // I/O page fault, enabled:  reflect page fault
	ble	PgfltExceptionUser_BadAddress
	//    erp->Pgflt_faultAddr = faultAddr;	// restore faultAddr
	//    erp->Pgflt_faultInfo = faultInfo;	// restore faultInfo
	ld	r5,STK_LOCAL3(r1)
	ld	r4,STK_LOCAL2(r1)
	//    erp->curProc->swapUserAndExcStates();
	ld	r6,PA_excStateOffset(r11)
	ld	r7,PA_userStateOffset(r11)
	ld	r12,PA_dispatcher(r11)
	std	r6,PA_userStateOffset(r11)
	std	r7,PA_excStateOffset(r11)
	std	r6,D__userStateOffset(r12)
	ld	r0,PA_dispatcherPhysAddr(r11)
	add	r0,r0,r7
	std	r0,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r0
	//      GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_PGFLT_ENTRY);
	b	CODE(ExceptionLocal_Launch_PGFLT_ENTRY)
    PgfltExceptionUser_BadAddress:
	// } else {
	//     // bad address page fault:  convert to trap
	//     erp->Trap_trapNumber = erp->PgfltTrapNumber();
	//     erp->Trap_trapInfo = faultInfo;
	//     erp->Trap_trapAuxInfo = faultAddr;
	li	r3,EXC_DSI
	ld	r4,STK_LOCAL2(r1)
	ld	r5,STK_LOCAL3(r1)
	//    erp->curProc->dispatcher->trapDisabledSave = disabledSave;
	ld	r12,PA_dispatcher(r11)
	ld	r0,STK_LOCAL0(r1)
	std	r0,D_trapDisabledSave(r12)
	//    erp->curProc->swapTrapAndExcStates();
	ld	r6,PA_excStateOffset(r11)
	ld	r7,PA_trapStateOffset(r11)
	std	r6,PA_trapStateOffset(r11)
	std	r7,PA_excStateOffset(r11)
	std	r6,D__trapStateOffset(r12)
	ld	r0,PA_dispatcherPhysAddr(r11)
	add	r0,r0,r7
	std	r0,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r0
	//    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_TRAP_ENTRY);
	b	CODE(ExceptionLocal_Launch_TRAP_ENTRY)
	// }
    PgfltExceptionUser_Reschedule:
	// exceptionLocal.dispatchQueue.clearCDABorrowers();
	li	r10,-1
	std	r10,EL_cdaBorrowersTop(r12)
	// SysStatus returnCode = erp->returnCode;	// preserve returnCode
	std	r3,STK_LOCAL4(r1)
	// CallExceptionLocalC(erp, ExceptionLocal_AwaitDispatch(erp->curProc));
	mr	r3,r11			// r3 = curProc
	bl	C_TEXT(ExceptionLocal_AwaitDispatch)
	// erp->returnCode = returnCode;		// restore returnCode
	ld	r3,STK_LOCAL4(r1)
	// erp->curProc = srcProc;			// re-fetch curProc
	ld	r11,STK_LOCAL1(r1)
	b	PgfltExceptionUser_MainPath
#endif	// !USE_EXPEDIENT_USER_PGFLT
CODE_END(ExceptionLocal_PgfltExceptionUser)

CODE_ENTRY(ExceptionLocal_PgfltExceptionKernel)
	bl	FloatingPointSave
	/*
	 * A VolatileState has been pushed on the stack (preserving the stack
	 * floor area).  We create a frame around it to give our callee a
	 * place to store a return address.
	 */
	la	r12,-(STK_FLOOR-VS_SIZE)(r1)	// reconstruct original stkptr
#ifdef	NDEBUG
	stdu	r12,-STK_SIZE(r1)		// create minimal frame
#else	// NDEBUG
	/*
	 * To help with debugging, we set things up to look as if the faulting
	 * thread called this code immediately prior to its next real
	 * instruction.  We store the address of that instruction in the
	 * return-address slot at the top of the old stack.  We have to
	 * preserve the current value in that slot because we can't be sure
	 * it's not live.
	 */
	ld	r11,VS_msr(r1)			// pick up fault-time MSR
	ld	r10,STK_LR(r12)			// retrieve saved LR
	ld	r9,VS_iar(r1)			// pick up fault-time IAR
	std	r9,STK_LR(r12)			// store fault IAR as saved LR
	stdu	r12,-(STK_SIZE+2*8)(r1)		// create frame w/ 2 data slots
	std	r10,STK_LOCAL0(r1)		// preserve old saved LR
	/*
	 * Check for page faults with interrupts disabled.  Shouldn't happen.
	 */
	ori	r10,r11,PSL_EE
	tdne	r10,r11
	/*
	 * We do some paranoid stack checking in debugging builds.
	 */
	ld	r12,TH_bottomSP(r13)		// retrieve current bottomSP
	tdllt	r1,r12				// make sure we're not past it
	DEBUG_CHK_STK_FENCE(r12,r11,r10,r9)	// check fence at bottomSP
	std	r12,STK_LOCAL1(r1)		// preserve old bottomSP
	la	r12,-KERN_PGFLT_STK_SPACE(r1)	// calculate new bottomSP
	ld	r11,TH_truebottomSP(r13)        // make sure new bottomSP
	tdllt	r12,r11				//      is still within stack
	DEBUG_CREATE_STK_FENCE(r12,r11)		// create fence at new bottomSP
	std	r12,TH_bottomSP(r13)		// store new bottomSP
#endif	// !NDEBUG
	GOTO_EXPEDIENT(ExceptionExp_HandleKernelPgflt)
CODE_END(ExceptionLocal_PgfltExceptionKernel)

CODE_ENTRY(KernelPgfltRestoreStack)
#ifdef	NDEBUG
	la	r1,STK_SIZE(r1)			// reconstruct VS address
#else	// NDEBUG
	ld	r12,TH_bottomSP(r13)		// check the fence at bottomSP
	DEBUG_CHK_STK_FENCE(r12,r11,r10,r9)
	ld	r12,STK_LOCAL1(r1)		// retrieve old bottomSP
	ld	r11,STK_LOCAL0(r1)		// retrieve old saved LR
	DEBUG_CREATE_STK_FENCE(r12,r10)		// restore fence at bottomSP
	std	r12,TH_bottomSP(r13)		// restore bottomSP
	la	r1,(STK_SIZE+2*8)(r1)		// reconstruct VS address
	la	r12,-(STK_FLOOR-VS_SIZE)(r1)	// reconstruct original stkptr
	std	r11,STK_LR(r12)			// restore saved LR
#endif	// !NDEBUG
	blr
CODE_END(KernelPgfltRestoreStack)

CODE_ENTRY(ExceptionLocal_KernelPgfltResume)
	bl	KernelPgfltRestoreStack
	mr	r3,r1				// r3 holds VS address
	b	SaveAreaResume
CODE_END(ExceptionLocal_KernelPgfltResume)

CODE_ENTRY(ExceptionLocal_KernelPgfltToTrap)
	bl	KernelPgfltRestoreStack
	b	TrapFromKernelPgflt
CODE_END(ExceptionLocal_KernelPgfltToTrap)

/*
 * This kernel page fault handler is installed while the kernel is in the
 * debugger, an environment in which we cannot attempt to resolve faults.
 * Instead we convert faults directly to traps.  The debugger expects a
 * trapNumber in r3.  We assert EXC_DSI, although we may be here because of
 * an ISI.  FaultInfo and faultAddr are already in r4 and r5, respectively.
 */
CODE_ENTRY(ExceptionLocal_PgfltExceptionKernelInDebugger)
	bl	FloatingPointSave
	li	r3,EXC_DSI
	b	TrapFromKernelPgflt
CODE_END(ExceptionLocal_PgfltExceptionKernelInDebugger)

CODE_ENTRY(ExceptionLocal_IOInterrupt)
	bl	FloatingPointSave
	andi.	r0,r5,PSL_PR		// branch if kernel-mode interrupt
	beq	IOInterruptKernel
    IOInterruptUser:
	INIT_FPSCR()
	EXCEPTION_STACK(0, r0)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(HWInterrupt_IOInterrupt)
	RESTORE_DISPATCHER(r11,r12)
	b	CODE(ExceptionLocal_UserInterruptContinue)
    IOInterruptKernel:
	EXCEPTION_STACK(0, r0)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(HWInterrupt_IOInterrupt)
	RESTORE_DISPATCHER(r11,r12)
	b	CODE(ExceptionLocal_KernelInterruptContinue)
CODE_END(ExceptionLocal_IOInterrupt)

CODE_ENTRY(ExceptionLocal_UserInterruptContinue)
#ifdef	USE_EXPEDIENT_INTERRUPT
	GOTO_EXPEDIENT(ExceptionExp_UserInterruptContinue)
#else
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	// if (exceptionLocal.dispatchQueue.cdaBorrowersEmpty()) {
	//     goto UserInterruptAwaitDispatch;
	// }
	ld	r11,EL_cdaBorrowersTop(r12)
	cmpdi	r11,0
	blt-	CODE(ExceptionLocal_UserInterruptAwaitDispatch)
	// erp->curProc = exceptionLocal.currentProcessAnnex;
	ld	r11,EL_currentProcessAnnex(r12)
	// erp->curDispatcher = erp->curProc->dispatcher;
	ld	r12,PA_dispatcher(r11)
	// GOTO_EXCEPTION_LOCAL_ASM(erp, ResumeUserProcess);
	b	CODE(ExceptionLocal_ResumeUserProcess)
#endif	// !USE_EXPEDIENT_INTERRUPT
CODE_END(ExceptionLocal_UserInterruptContinue)

CODE_ENTRY(ExceptionLocal_UserInterruptAwaitDispatch)
	bl	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
	THREAD_STACK(r13, 2, r11)
	std	r0,STK_LOCAL0(r1)	// preserve disabledSave
	std	r3,STK_LOCAL1(r1)	// preserve srcProc
	// ExceptionLocal_AwaitDispatch(srcProc);
	bl	C_TEXT(ExceptionLocal_AwaitDispatch)
	ld	r11,STK_LOCAL1(r1)	// curProc = srcProc
	LOAD_C_DATA_ADDR(r12,extRegsLocal)
	ld	r0,STK_LOCAL0(r1)	// extRegsLocal.disabled = disabledSave
	std	r0,XR_disabled(r12)
	bl	CODE(ExceptionLocal_ReleaseReservedThread)
	EXCEPTION_STACK(0, r0)
	b	CODE(ExceptionLocal_UserInterruptContinue)
CODE_END(ExceptionLocal_UserInterruptAwaitDispatch)

CODE_ENTRY(ExceptionLocal_KernelInterruptContinue)
#ifdef	USE_EXPEDIENT_INTERRUPT
	GOTO_EXPEDIENT(ExceptionExp_KernelInterruptContinue)
#else
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	// erp->curProc = exceptionLocal.currentProcessAnnex;
	ld	r11,EL_currentProcessAnnex(r12)
	// if (erp->curProc != exceptionLocal.kernelProcessAnnex)
	//						goto AbandonIdle;
	ld	r0,EL_kernelProcessAnnex(r12)
	cmpd	r11,r0
	bne-	KernelInterruptContinue_AbandonIdle
#if 1 // COMMENT THIS OUT IF YOU DON'T WANT KERNEL THREADS TO PREEMPT
      // turning this off is very valuable for debugging	
	// if (exceptionLocal.dispatchQueue.cdaBorrowersEmpty()) {
	//     erp->curProc->deliverInterrupt(SoftIntr::PREEMPT);
	// }
	ld	r0,EL_cdaBorrowersTop(r12)
	cmpdi	r0,0
	bge+	KernelInterruptContinue_NoPreempt
	ld	r10,PA_dispatcher(r11)
	addi	r10,r10,D_flags
    KernelInterruptContinue_SetPreemptLoop:
	lwarx	r0,0,r10
	ori	r0,r0,SHIFT_L(1,SOFTINTR_PREEMPT)
	stwcx.	r0,0,r10
	bne-	KernelInterruptContinue_SetPreemptLoop
	cmpwi	r0,SHIFT_L(1,SOFTINTR_PREEMPT)
	bne-	KernelInterruptContinue_NoPreempt
	ld	r0,PA_cpuDomainNext(r11)
	cmpdi	r0,0
	beql-	CODE(ExceptionLocal_MakeCurProcReady)
    KernelInterruptContinue_NoPreempt:
#endif	
	// erp->curDispatcher = erp->curProc->dispatcher;
	ld	r12,PA_dispatcher(r11)
	// if (!erp->curDispatcher->interrupts.pending() ||
	//			extRegsLocal.disabled) goto Resume;
	lwz	r0,D_flags(r12)
	LOAD_C_DATA_ADDR(r10,extRegsLocal)
	cmpwi	r0,0
	beq-	KernelInterruptContinue_Resume
	ld	r0,XR_disabled(r10)
	cmpdi	r0,0
	bne-	KernelInterruptContinue_Resume
    KernelInterruptContinue_SoftIntr:
	// extRegsLocal.disabled = 1;
	li	r0,1
	std	r0,XR_disabled(r10)
	// erp->curProc->swapUserAndExcStates();
	ld	r10,PA_excStateOffset(r11)
	ld	r9,PA_userStateOffset(r11)
	std	r10,PA_userStateOffset(r11)
	std	r9,PA_excStateOffset(r11)
	std	r10,D__userStateOffset(r12)
	ld	r0,PA_dispatcherPhysAddr(r11)
	add	r0,r0,r9
	std	r0,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r0
	// GOTO_EXCEPTION_LOCAL_ASM(erp, KernelReflectInterrupt);
	b	CODE(ExceptionLocal_KernelReflectInterrupt)
    KernelInterruptContinue_Resume:
	// erp->volatileState = erp->curProc->excStatePtr();
	ld	r0,PA_excStateOffset(r11)
	add	r3,r12,r0
	// GOTO_EXCEPTION_LOCAL_ASM(erp, KernelInterruptResume);
	b	CODE(ExceptionLocal_KernelInterruptResume)
    KernelInterruptContinue_AbandonIdle:
	// // currentProcessAnnex must be idle loop.  Abandon it without saving
	// // state.
	// GOTO_EXCEPTION_LOCAL_ASM(erp, IdleLoopYield);
	b	CODE(ExceptionLocal_IdleLoopYield);
#endif	// !USE_EXPEDIENT_INTERRUPT
CODE_END(ExceptionLocal_KernelInterruptContinue)

CODE_ENTRY(ExceptionLocal_KernelInterruptResume)
	b	SaveAreaResume
CODE_END(ExceptionLocal_KernelInterruptResume)

CODE_ENTRY(ExceptionLocal_KernelReflectInterrupt)
	ENABLE_HW_INTERRUPTS(r0)
	b	CODE(DispatcherDefault_InterruptEntry)
CODE_END(ExceptionLocal_KernelReflectInterrupt)

CODE_ENTRY(ExceptionLocal_IdleLoopYield)
	bl	C_TEXT(ExceptionLocal_GetNextProcess)
	mr	r11,r3			// curProc = new PA to run
	b	CODE(ExceptionLocal_Launch_RUN_ENTRY)
CODE_END(ExceptionLocal_IdleLoopYield)

CODE_ENTRY(ExceptionLocal_DecInterrupt)
	bl	FloatingPointSave
	andi.	r0,r5,PSL_PR		// branch if kernel-mode interrupt
	beq	DecInterruptKernel
    DecInterruptUser:
	INIT_FPSCR()
	EXCEPTION_STACK(0, r0)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(HWInterrupt_DecInterrupt)
	RESTORE_DISPATCHER(r11,r12)
	b	CODE(ExceptionLocal_UserInterruptContinue)
    DecInterruptKernel:
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	ld	r0,EL_trapSkipDecr(r12)	// postpone this interrupt if it
	cmpldi	r0,0			//     follows a trap
	bne-	DecInterruptKernel_TrapFixup
	EXCEPTION_STACK(0, r0)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(HWInterrupt_DecInterrupt)
	RESTORE_DISPATCHER(r11,r12)
	b	CODE(ExceptionLocal_KernelInterruptContinue)
    DecInterruptKernel_TrapFixup:
	lis	r0,0x10			// interrupt again in 2^20 ticks, long
	mtdec	r0			//     enough to handle an EXI
	li	r0,0			// clear the flag
	std	r0,EL_trapSkipDecr(r12)
	mr	r3,r1			// r1 still points to the save area
	b	SaveAreaResume
CODE_END(ExceptionLocal_DecInterrupt)

CODE_ENTRY(ExceptionLocal_PerfInterrupt)
	bl	FloatingPointSave
	andi.	r0,r5,PSL_PR		// branch if kernel-mode interrupt
	beq	PerfInterruptKernel
    PerfInterruptUser:
	INIT_FPSCR()
	EXCEPTION_STACK(0, r0)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(HWInterrupt_PerfInterrupt)
	RESTORE_DISPATCHER(r11,r12)
       	b	CODE(ExceptionLocal_UserInterruptContinue)
    PerfInterruptKernel:
	EXCEPTION_STACK(0, r0)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(HWInterrupt_PerfInterrupt)
	RESTORE_DISPATCHER(r11,r12)
       	b	CODE(ExceptionLocal_KernelInterruptContinue)
CODE_END(ExceptionLocal_PerfInterrupt)

CODE_ENTRY(ExceptionLocal_NonnativeSyscall)
	// lolita left r12 = &EL
#ifdef	USE_EXPEDIENT_SVC
	STACK_TOC_AND_FPSCR(r12, 0)
	GOTO_EXPEDIENT(ExceptionExp_NonnativeSyscall)
#else
	// erp->curProc = exceptionLocal.currentProcessAnnex;
	ld	r11,EL_currentProcessAnnex(r12)
	// erp->curProc->swapUserAndExcStates();
	ld	r2,PA_dispatcher(r11)
	ld	r1,PA_excStateOffset(r11)
	std	r1,D__userStateOffset(r2)
	ld	r2,PA_userStateOffset(r11)
	std	r1,PA_userStateOffset(r11)
	std	r2,PA_excStateOffset(r11)
	ld	r1,PA_dispatcherPhysAddr(r11)
	add	r1,r1,r2
	std	r1,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r1
	// GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_SVC_ENTRY);
	ld	r2,EL_toc(r12)
	b	CODE(ExceptionLocal_Launch_SVC_ENTRY)
#endif	// !USE_EXPEDIENT_SVC
CODE_END(ExceptionLocal_NonnativeSyscall)

CODE_ENTRY(ExceptionLocal_SetEntryPointSyscall)
	// lolita left r12 = &EL
	mr	r11,r1			// save stack pointer
	STACK_TOC_AND_FPSCR(r12, 2)
	mflr	r12			// save return address
	std	r11,STK_LOCAL0(r1)
	std	r12,STK_LOCAL1(r1)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(ExceptionLocal_SetEntryPoint)
	RESTORE_DISPATCHER(r11,r12)
	ld	r12,STK_LOCAL1(r1)
	ld	r1,STK_LOCAL0(r1)
	LOAD_C_DATA_UVAL(r11,exceptionLocal,EL_currentProcessAnnex)
	b	SyscallReturn
CODE_END(ExceptionLocal_SetEntryPointSyscall)

CODE_ENTRY(ExceptionLocal_ProcessYieldSyscall)
	// lolita left r12 = &EL
	STACK_TOC_AND_FPSCR(r12, 2)
	bl	C_TEXT(ExceptionLocal_GetNextProcess)
	mr	r11,r3			// curProc = new PA to run
	b	CODE(ExceptionLocal_Launch_RUN_ENTRY)
CODE_END(ExceptionLocal_ProcessYieldSyscall)

CODE_ENTRY(ExceptionLocal_ProcessHandoffSyscall)
	// lolita left r12 = &EL
	STACK_TOC_AND_FPSCR(r12, 2)
	// target commID already in r3
	bl	C_TEXT(ExceptionLocal_GetHandoffProcess)
	mr	r11,r3			// curProc = new PA to run
	b	CODE(ExceptionLocal_Launch_RUN_ENTRY)
CODE_END(ExceptionLocal_ProcessHandoffSyscall)

#ifndef	USE_EXPEDIENT_PPC
/*
 * r11 = curProc, r12 = &exceptionLocal, r1 and r2 available.
 */
CODE_ENTRY(IPC_SwitchContext)
	// erp->curProc->switchContext();
	std	r11,EL_currentProcessAnnex(r12)
	ld	r1,PA_excStatePhysAddr(r11)
	mtsprg	sprg1,r1
	lbz	r1,PA_isKernel(r11)
	cmpldi	r1,0
	bnelr-				// return if target space is kernel
	ld	r2,EL_currentSegmentTable(r12)
	ld	r1,PA_segmentTable(r11)
	cmpld	r1,r2
	beqlr-				// return if already in target space
	std	r1,EL_currentSegmentTable(r12)
	ld	r2,EL_segLoad(r12)	// get segment load hardware type
	cmpldi	r2,0			// 0 indicates hardware table
	bne	IPC_SwitchContext_SoftwareSLB
	ld	r2,PA_asr(r11)
	isync
	mtasr	r2
	slbia
	isync
	blr				// return
	
	// r1  addresses new segment table - must take care after
	// slbia to avoid a segment fault on the address in r10 -
	// would be a performance bug
	// we use r11 here, and restore it.  r2 is also available
IPC_SwitchContext_SoftwareSLB:	
	ld	r2, ST_SLBCache+16(r1)	// ste maybe covering current seg table
	ld	r11, ST_SLBCache+24(r1) // unless first entry did
	isync
	slbia				// invalidate all SLB entries except 0th
	slbmte	r2, r11			// reload 2nd SLB entry
	isync
	ld      r2, ST_cacheMax(r1)
	std     r2, ST_slbNext(r1)
	subic.  r2, r2, 2
	ble     2f
	la      r1, ST_SLBCache+24(r1)
	mtctr	r2
1:	
	ldu	r2, 8(r1)
	ldu	r11, 8(r1)
	slbmte  r2, r11
	bdnz	1b
2:	
	ld	r11,EL_currentProcessAnnex(r12)
	isync
	blr				// return
CODE_END(IPC_SwitchContext)

/*
 * r12 = &exceptionLocal, r11 = curProc, r1 = SERROR(?, ?, ?).
 */
CODE_ENTRY(IPC_Fault)
	// erp->curProc->dispatcher->ipcFaultReason = SERROR(?, ?, ?);
	ld	r2,PA_dispatcher(r11)
	std	r1,D_ipcFaultReason(r2)
	// GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_FAULT_ENTRY);
	ld	r2,EL_toc(r12)
	b	CODE(ExceptionLocal_Launch_IPC_FAULT_ENTRY)
CODE_END(IPC_Fault)
#endif	// !USE_EXPEDIENT_PPC

/*
 * r11 = target.
 */
CODE_ENTRY(ExceptionLocal_ReqRetryNotif)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_RequestRetryNotification(target);
	mr	r3,r11
	bl	C_TEXT(ExceptionLocal_RequestRetryNotification)
	VOLATILE_FRAME_RETURN()
CODE_END(ExceptionLocal_ReqRetryNotif)

CODE_ENTRY(ExceptionLocal_IPCCallSyscall)
	// lolita left r12 = &EL
#ifdef	USE_EXPEDIENT_PPC
	STACK_TOC_AND_FPSCR(r12, 2)	// stack space is reserved for possible
					//     use in IPCSyscallRemote
	GOTO_EXPEDIENT(ExceptionExp_IPCCallSyscall)
#else
	// source = exceptionLocal.currentProcessAnnex;
	ld	r2,EL_currentProcessAnnex(r12)
	// if (source->dispatcher->hasWork && !source->isReady()) {
	//     CALL_EXCEPTION_LOCAL_ASM(erp, MakeCurProcReady);
	// }
	ld	r11,PA_dispatcher(r2)
	ld	r1,D_hasWork(r11)
	cmpdi	r1,0
	beq+	IPCCallSyscall_SourceOkay
	ld	r1,PA_cpuDomainNext(r2)
	cmpdi	r1,0
	bne+	IPCCallSyscall_SourceOkay
	STACK_TOC_AND_FPSCR(r12, 0)
	bl	CODE(ExceptionLocal_MakeCurProcReady)
	ld	r2,EL_currentProcessAnnex(r12)	// re-fetch source
    IPCCallSyscall_SourceOkay:
	// erp->curProc =
	//    exceptionLocal.ipcTargetTable.lookupWild(erp->IPC_targetID);
	// if (erp->curProc == NULL) goto TargetNotFound
	rlwinm	r1,r14,ROT_RIGHT_32(COMMID_RD_SHIFT - (RD_HASH_OFFSET + 3)),\
			SHIFT_L(RD_MASK, RD_HASH_OFFSET + 3)
	srdi	r11,r14,(COMMID_PID_SHIFT - 3)
	xor	r11,r11,r1
	ld	r1,EL__tableOffsetMask(r12)
	and	r1,r11,r1
	ld	r11,EL__table(r12)
	ldx	r11,r11,r1
    IPCCallSyscall_HashChainLoop:
	cmpldi	r11,0
	beq-	IPCCallSyscall_TargetNotFound
	ld	r1,PA_commID(r11)
	ori	r1,r1,SHIFT_L(VP_WILD, COMMID_VP_SHIFT)	// assumes low bits
	cmpld	r1,r14
	beq+	IPCCallSyscall_TargetFound
	ld	r11,PA_ipcTargetNext(r11)
	b	IPCCallSyscall_HashChainLoop
    IPCCallSyscall_TargetFound:
	// if (erp->curProc->reservedThread != NULL) goto TargetDisabled
	ld	r1,PA_reservedThread(r11)
	cmpldi	r1,0
	bne-	IPCCallSyscall_TargetDisabled
	/*
	 * At this point we're committed to switching to the target.
	 */
	// exceptionLocal.dispatchQueue.pushCDABorrower(erp->curProc);
	ld	r1,EL_cdaBorrowersTop(r12)
	addi	r1,r1,1
	rldicl	r1,r1,0,64-LOG_CDA_BORROWERS_SIZE
	std	r1,EL_cdaBorrowersTop(r12)
	sldi	r1,r1,3
	add	r1,r1,r12
	std	r11,EL_cdaBorrowers(r1)
	// erp->IPC_callerID = source->commID;
	ld	r14,PA_commID(r2)
	// TraceOSExceptionPPCCall(erp->curProc->commID);
	ld	r1,EL_trcInfoMask(r12)
	rldicl.	r1,r1,ROT_RIGHT(TRC_EXCEPTION_MAJOR_ID),63
	bnel-	IPCCallSyscall_Trace
	// erp->curProc->switchContext();
	bl	CODE(IPC_SwitchContext)
	// GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_CALL_ENTRY);
	ld	r2,EL_toc(r12)
	b	CODE(ExceptionLocal_Launch_IPC_CALL_ENTRY)
    IPCCallSyscall_TargetDisabled:
	// CALL_EXCEPTION_LOCAL_ASM(erp, ReqRetryNotif);
	STACK_TOC_AND_FPSCR(r12, 0)
	bl	CODE(ExceptionLocal_ReqRetryNotif)
	// erp->curProc = exceptionLocal.currentProcessAnnex;
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	ld	r11,EL_currentProcessAnnex(r12)
	SERROR(r1, 1573, SYSCALL_IPC_CALL, ERRNO_AGAIN)
	b	CODE(IPC_Fault)
    IPCCallSyscall_TargetNotFound:
	STACK_TOC_AND_FPSCR(r12, 2)		// stack space is reserved for
						//  use in IPCSyscallRemote
	// erp->IPC_ipcType = SYSCALL_IPC_CALL;
	li	r12,SYSCALL_IPC_CALL
	// GOTO_EXCEPTION_LOCAL_ASM(erp, IPCSyscallRemote);
	b	CODE(ExceptionLocal_IPCSyscallRemote)
    IPCCallSyscall_Trace:
	// oldIndex = traceControl->index;
	ld	r1,EL_trcControl(r12)
	ld	r2,TC_index(r1)
	// newIndex = oldIndex + 2
	addi	r2,r2,2
	// if (TRACE_BUFFER_OFFSET_GET(newIndex) < 2) goto TraceSlow
	rldicl	r13,r2,0,64-TRC_BUFFER_OFFSET_BITS
	cmpldi	r13,2
	blt-	IPCCallSyscall_TraceSlow
	// now = Scheduler::SysTimeNow();
	// traceControl->index = newIndex;
	std	r2,TC_index(r1)
	// index = (newIndex - 2) & traceInfo->indexMask;
	ld	r13,EL_trcInfoIndexMask(r12)
	subi	r2,r2,2
	and	r2,r2,r13
	// traceControl->bufferCount[TRACE_BUFFER_NUMBER_GET(index)] += 2;
	rldicl	r13,r2,ROT_RIGHT(TRC_BUFFER_OFFSET_BITS),\
				64-TRC_BUFFER_NUMBER_BITS
	sldi	r13,r13,3
	add	r1,r1,r13
	ld	r13,TC_bufferCount(r1)
	addi	r13,r13,2
	std	r13,TC_bufferCount(r1)
	// traceArray[index] = traceFormFirstWord(now, 2, EXCEPTION, PPC_CALL);
	mftb	r13
	ld	r1,EL_trcArray(r12)
	sldi	r2,r2,3
	TRACE_FORM_FIRST_WORD(r13, 2, TRC_K42_LAYER_ID,
				TRC_EXCEPTION_MAJOR_ID,
				TRC_EXCEPTION_PPC_CALL)
	stdux	r13,r1,r2
	// traceArray[index+1] = erp->curProc->commID;
	ld	r13,PA_commID(r11)
	std	r13,8(r1)
	blr
    IPCCallSyscall_TraceSlow:
	STACK_TOC_AND_FPSCR(r12, 0)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_TracePPCCall(curProc->commID);
	ld	r3,PA_commID(r11)
	bl	C_TEXT(ExceptionLocal_TracePPCCall)
	VOLATILE_FRAME_RETURN()
#endif	// !USE_EXPEDIENT_PPC
CODE_END(ExceptionLocal_IPCCallSyscall)

CODE_ENTRY(ExceptionLocal_IPCReturnSyscall)
	// lolita left r12 = &EL
#ifdef	USE_EXPEDIENT_PPC
	STACK_TOC_AND_FPSCR(r12, 2)	// stack space is reserved for possible
					//     use in IPCSyscallRemote
	GOTO_EXPEDIENT(ExceptionExp_IPCReturnSyscall)
#else
	// source = exceptionLocal.currentProcessAnnex;
	ld	r2,EL_currentProcessAnnex(r12)
	// if (source->dispatcher->hasWork && !source->isReady()) {
	//     CALL_EXCEPTION_LOCAL_ASM(erp, MakeCurProcReady);
	// }
	ld	r11,PA_dispatcher(r2)
	ld	r1,D_hasWork(r11)
	cmpdi	r1,0
	beq+	IPCReturnSyscall_SourceOkay
	ld	r1,PA_cpuDomainNext(r2)
	cmpdi	r1,0
	bne+	IPCReturnSyscall_SourceOkay
	STACK_TOC_AND_FPSCR(r12, 0)
	bl	CODE(ExceptionLocal_MakeCurProcReady)
	ld	r2,EL_currentProcessAnnex(r12)	// re-fetch source
    IPCReturnSyscall_SourceOkay:
	// exceptionLocal.dispatchQueue.popCDABorrower();
	// erp->curProc = exceptionLocal.dispatchQueue.currentCDABorrower();
	// if ((erp->curProc == NULL) ||
	//          (erp->curProc->commID != erp->IPC_targetID)) {
	//     goto SlowPath;
	// }
	ld	r1,EL_cdaBorrowersTop(r12)
	subic.	r1,r1,1
	blt-	IPCReturnSyscall_SlowPath
	std	r1,EL_cdaBorrowersTop(r12)
	sldi	r1,r1,3
	add	r1,r1,r12
	ld	r11,EL_cdaBorrowers(r1)
	ld	r1,PA_commID(r11)
	cmpld	r1,r14
	bne-	IPCReturnSyscall_SlowPath
    IPCReturnSyscall_FastPath:
	// if (erp->curProc->reservedThread != NULL) goto CheckPrimitivePPC
	ld	r1,PA_reservedThread(r11)
	cmpldi	r1,0
	bne-	IPCReturnSyscall_CheckPrimitivePPC
	/*
	 * At this point we're committed to switching to the target.
	 */
	// erp->IPC_callerID = source->commID;
	ld	r14,PA_commID(r2)
	// TraceOSExceptionPPCReturn(erp->curProc->commID);
	ld	r1,EL_trcInfoMask(r12)
	rldicl.	r1,r1,ROT_RIGHT(TRC_EXCEPTION_MAJOR_ID),63
	bnel-	IPCReturnSyscall_Trace
	// erp->curProc->switchContext();
	bl	CODE(IPC_SwitchContext)
	// GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_RTN_ENTRY);
	ld	r2,EL_toc(r12)
	b	CODE(ExceptionLocal_Launch_IPC_RTN_ENTRY)
    IPCReturnSyscall_SlowPath:
	// exceptionLocal.dispatchQueue.clearCDABorrowers();
	li	r1,-1
	std	r1,EL_cdaBorrowersTop(r12)
	// erp->curProc =
	//    exceptionLocal.ipcTargetTable.lookupExact(erp->IPC_targetID);
	// if (erp->curProc == NULL) goto TargetNotFound
	rlwinm	r1,r14,ROT_RIGHT_32(COMMID_RD_SHIFT - (RD_HASH_OFFSET + 3)),\
			SHIFT_L(RD_MASK, RD_HASH_OFFSET + 3)
	srdi	r11,r14,(COMMID_PID_SHIFT - 3)
	xor	r11,r11,r1
	ld	r1,EL__tableOffsetMask(r12)
	and	r1,r11,r1
	ld	r11,EL__table(r12)
	ldx	r11,r11,r1
    IPCReturnSyscall_HashChainLoop:
	cmpldi	r11,0
	beq-	IPCReturnSyscall_TargetNotFound
	ld	r1,PA_commID(r11)
	cmpld	r1,r14
	beq+	IPCReturnSyscall_TargetFound
	ld	r11,PA_ipcTargetNext(r11)
	b	IPCReturnSyscall_HashChainLoop
    IPCReturnSyscall_TargetFound:
	// if (erp->curProc->isKernel) goto FastPath;
	lbz	r1,PA_isKernel(r11)
	cmpldi	r1,0
	bne-	IPCReturnSyscall_FastPath
	// if (erp->curProc->reservedThread != NULL) goto CheckPrimitivePPC
	ld	r1,PA_reservedThread(r11)
	cmpldi	r1,0
	bne-	IPCReturnSyscall_CheckPrimitivePPC
	/*
	 * At this point we're committed to switching to the target.
	 */
	// erp->IPC_callerID = source->commID;
	ld	r14,PA_commID(r2)
	// TraceOSExceptionPPCReturn(erp->curProc->commID);
	ld	r1,EL_trcInfoMask(r12)
	rldicl.	r1,r1,ROT_RIGHT(TRC_EXCEPTION_MAJOR_ID),63
	bnel-	IPCReturnSyscall_Trace
	// erp->curProc->switchContext();
	bl	CODE(IPC_SwitchContext)
	// GOTO_EXCEPTION_LOCAL_ASM(erp, AwaitDispatch_Launch_IPC_RTN_ENTRY);
	b	CODE(ExceptionLocal_AwaitDispatch_Launch_IPC_RTN_ENTRY)
    IPCReturnSyscall_CheckPrimitivePPC:
	// if ((erp->curProc->reservedThread != PPC_PRIMITIVE_MARKER) ||
	//         (source->commID != erp->curProc->ppcTargetID) ||
	//             (erp->IPC_threadID != erp->curProc->ppcThreadID)) {
	//     goto TargetDisabled;
	// }
	ld	r0,PA_reservedThread(r11)
	cmpdi	r0,PPC_PRMTV_MARKER
	bne+	IPCReturnSyscall_TargetDisabled
	ld	r1,PA_commID(r2)
	ld	r0,PA_ppcTargetID(r11)
	cmpld	r1,r0
	bne-	IPCReturnSyscall_TargetDisabled
	ld	r0,PA_ppcThreadID(r11)
	cmpld	r15,r0
	bne-	IPCReturnSyscall_TargetDisabled
	/*
	 * At this point we're committed to completing a primitive PPC.
	 */
	// erp->IPC_callerID = source->commID;
	mr	r14,r1
	// erp->curProc->reservedThread = NULL;
	li	r1,0
	std	r1,PA_reservedThread(r11)
	// TraceOSExceptionPPCReturn(erp->curProc->commID);
	ld	r1,EL_trcInfoMask(r12)
	rldicl.	r1,r1,ROT_RIGHT(TRC_EXCEPTION_MAJOR_ID),63
	bnel-	IPCReturnSyscall_Trace
	// erp->curProc->switchContext();
	bl	CODE(IPC_SwitchContext)
	// extRegsLocal.dispatcher = erp->curProc->dispatcherUser;
	ld	r2,EL_toc(r12)
	LOAD_C_DATA_ADDR(r1, extRegsLocal)
	ld	r0,PA_dispatcherUser(r11)
	std	r0,XR_dispatcher(r1)
	// extRegsLocal.disabled = 1;	// in case sender didn't disable
	li	r0,1
	std	r0,XR_disabled(r1)
	// if (exceptionLocal.dispatchQueue.currentCDABorrower() == NULL) {
	//     GOTO_EXCEPTION_LOCAL_ASM(erp, AwaitDispatch_PPCPrimitiveResume);
	// }
	ld	r0,EL_cdaBorrowersTop(r12)
	cmpdi	r0,0
	blt-	CODE(ExceptionLocal_AwaitDispatch_PPCPrimitiveResume)
	// GOTO_EXCEPTION_LOCAL_ASM(erp, PPCPrimitiveResume);
	b	CODE(ExceptionLocal_PPCPrimitiveResume)
    IPCReturnSyscall_TargetDisabled:
	// CALL_EXCEPTION_LOCAL_ASM(erp, ReqRetryNotif);
	STACK_TOC_AND_FPSCR(r12, 0)
	bl	CODE(ExceptionLocal_ReqRetryNotif)
	// erp->curProc = exceptionLocal.currentProcessAnnex;
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	ld	r11,EL_currentProcessAnnex(r12)
	SERROR(r1, 1576, SYSCALL_IPC_RTN, ERRNO_AGAIN);
	b	CODE(IPC_Fault)
    IPCReturnSyscall_TargetNotFound:
	// if (source->isKernel) goto KernelReplyRemote;
	lbz	r11,PA_isKernel(r2)
	cmpdi	r11,0
	bne	IPCReturnSyscall_KernelReplyRemote
	STACK_TOC_AND_FPSCR(r12, 2)		// stack space is reserved for
						//  use in IPCSyscallRemote
	// erp->IPC_ipcType = SYSCALL_IPC_RTN;
	li	r12,SYSCALL_IPC_RTN
	// GOTO_EXCEPTION_LOCAL_ASM(erp, IPCSyscallRemote);
	b	CODE(ExceptionLocal_IPCSyscallRemote)
    IPCReturnSyscall_KernelReplyRemote:
	// erp->curProc = source;
	mr	r11,r2
	ld	r2,EL_toc(r12)
	// erp->dispatcher = (DispatcherDefaultKern *) erp->curProc->dispatcher;
	ld	r12,PA_dispatcher(r11)
	// CurrentThread = erp->dispatcher->freeList;
	ld	r13,DD_freeList(r12)
	// erp->dispatcher->freeList = CurrentThread->next;
	ld	r1,TH_next(r13)
	std	r1,DD_freeList(r12)
	// GOTO_EXCEPTION_LOCAL_ASM(erp, KernelReplyRemote);
	b	CODE(ExceptionLocal_KernelReplyRemote)
    IPCReturnSyscall_Trace:
	// oldIndex = traceControl->index;
	ld	r1,EL_trcControl(r12)
	ld	r2,TC_index(r1)
	// newIndex = oldIndex + 2; 
	addi	r2,r2,2
	// if (TRACE_BUFFER_OFFSET_GET(newIndex) < 2) goto TraceSlow
	rldicl	r13,r2,0,64-TRC_BUFFER_OFFSET_BITS
	cmpldi	r13,2
	blt-	IPCReturnSyscall_TraceSlow
	// now = Scheduler::SysTimeNow();
	// traceControl->index = newIndex;
	std	r2,TC_index(r1)
	// index = (newIndex - 2) & traceInfo->indexMask;
	ld	r13,EL_trcInfoIndexMask(r12)
	subi	r2,r2,2
	and	r2,r2,r13
	// traceControl->bufferCount[TRACE_BUFFER_NUMBER_GET(index)] += 2;
	rldicl	r13,r2,ROT_RIGHT(TRC_BUFFER_OFFSET_BITS),\
				64-TRC_BUFFER_NUMBER_BITS
	sldi	r13,r13,3
	add	r1,r1,r13
	ld	r13,TC_bufferCount(r1)
	addi	r13,r13,2
	std	r13,TC_bufferCount(r1)
	// traceArray[index] = traceFormFirstWord(now, 2, EXCEPTION, PPC_RTN);
	mftb	r13
	ld	r1,EL_trcArray(r12)
	sldi	r2,r2,3
	TRACE_FORM_FIRST_WORD(r13, 2, TRC_K42_LAYER_ID,
				TRC_EXCEPTION_MAJOR_ID,
				TRC_EXCEPTION_PPC_RETURN)
	stdux	r13,r1,r2
	// traceArray[index+1] = erp->curProc->commID;
	ld	r13,PA_commID(r11)
	std	r13,8(r1)
	blr
    IPCReturnSyscall_TraceSlow:
	STACK_TOC_AND_FPSCR(r12, 0)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_TracePPCReturn(curProc->commID);
	ld	r3,PA_commID(r11)
	bl	C_TEXT(ExceptionLocal_TracePPCReturn)
	VOLATILE_FRAME_RETURN()
#endif	// !USER_EXPEDIENT_PPC
CODE_END(ExceptionLocal_IPCReturnSyscall)

CODE_ENTRY(IPCReturnAwaitDispatch)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_AwaitDispatch(srcProc);
	mr	r3,r11			// srcProc = curProc
	bl	C_TEXT(ExceptionLocal_AwaitDispatch)
	ld	r11,VOLATILE_FRAME_R11(r1)// recover curProc
	bl	CODE(ExceptionLocal_ReleaseReservedThread)
	VOLATILE_FRAME_RETURN()
CODE_END(IPCReturnAwaitDispatch)

CODE_ENTRY(ExceptionLocal_AwaitDispatch_Launch_IPC_RTN_ENTRY)
	/*
	 * All registers except r0, r1, r2, r11, and r13 are in use.
	 * r12 = &EL. We hop onto EXCEPTION_STACK just long enough to
	 * allocate and switch to a kernel thread.  AcquireReservedThread()
	 * preserves all registers except r0, r3, and r11-r13.
	 */
	STACK_TOC_AND_FPSCR(r12, 1)
	std	r3,STK_LOCAL0(r1)	// preserve r3
	bl	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
	mr	r11,r3			// curProc = srcProc
	ld	r3,STK_LOCAL0(r1)	// restore r3
	THREAD_STACK(r13, 0, r0)
	bl	CODE(IPCReturnAwaitDispatch)
	b	CODE(ExceptionLocal_Launch_IPC_RTN_ENTRY)
CODE_END(ExceptionLocal_AwaitDispatch_Launch_IPC_RTN_ENTRY)

CODE_ENTRY(ExceptionLocal_IPCSyscallRemote)
	/*
	 * All registers except r0, r11, and r13 are in use, but we're already
	 * on EXCEPTION_STACK with space for 2 data words reserved.  Free up
	 * the registers that are needed in allocating and switching to a
	 * kernel thread. AcquireReservedThread() preserves all registers
	 * except r0, r3, and r11-r13.
	 */
	std	r3,STK_LOCAL0(r1)	// preserve r3 (xhandle or returnCode)
	std	r12,STK_LOCAL1(r1)	// preserve r12 (ipcType)
	bl	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
	mr	r11,r3			// curProc = srcProc
	ld	r12,STK_LOCAL1(r1)	// restore r12	(ipcType)
	ld	r3,STK_LOCAL0(r1)	// restore r3 (xhandle or returnCode)
	THREAD_STACK(r13, 0, r0)
	GOTO_EXPEDIENT(ExceptionExp_IPCSyscallRemoteOnThread)
CODE_END(ExceptionLocal_IPCSyscallRemote)

CODE_ENTRY(ExceptionLocal_KernelReplyRemote)
	/*
	 * All registers except r10-r12 are in use, but a thread has been
	 * allocated.  All we need to do is switch to it and continue.
	 */
	THREAD_STACK(r13, 0, r12)
	GOTO_EXPEDIENT(ExceptionExp_KernelReplyRemoteOnThread)
CODE_END(ExceptionLocal_KernelReplyRemote)

/*
 * extern "C" void ExceptionLocal_AcceptRemoteIPC(IPCRegsArch *ipcRegsP,
 *                                                CommID callerID,
 *                                                uval ipcType,
 *                                                ProcessAnnex *curProc);
 */
C_TEXT_ENTRY(ExceptionLocal_AcceptRemoteIPC)
	FRAME_ENTER(0, r12)
	// ipcRegsP already in r3
	mr	r14,r4			// callerID in expected register
	mr	r12,r5			// ipcType in expected register
	mr	r11,r6			// curProc in expected register
	GOTO_EXPEDIENT(ExceptionExp_AcceptRemoteIPC)
C_TEXT_END(ExceptionLocal_AcceptRemoteIPC)

CODE_ENTRY(ExceptionLocal_AwaitDispatch_PPCPrimitiveResume)
	/*
	 * All registers except r0, r1, r2, r11, and r13 are in use.
	 * r12 = &EL. We hop onto EXCEPTION_STACK just long enough to
	 * allocate and switch to a kernel thread.  AcquireReservedThread()
	 * preserves all registers except r0, r3, and r11-r13.
	 */
	STACK_TOC_AND_FPSCR(r12, 1)
	std	r3,STK_LOCAL0(r1)	// preserve r3
	bl	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
	mr	r11,r3			// curProc = srcProc
	ld	r3,STK_LOCAL0(r1)	// restore r3
	THREAD_STACK(r13, 0, r0)
	bl	CODE(IPCReturnAwaitDispatch)
	// extRegsLocal.disabled = 1;
	LOAD_C_DATA_ADDR(r12, extRegsLocal)
	li	r0,1
	std	r0,XR_disabled(r12)
	b	CODE(ExceptionLocal_PPCPrimitiveResume)
CODE_END(ExceptionLocal_AwaitDispatch_PPCPrimitiveResume)

CODE_ENTRY(ExceptionLocal_PPCPrimitiveSyscall)
	// lolita left r12 = &EL
	ld	r11,EL_currentProcessAnnex(r12)
	mflr	r0
	std	r1,PA_syscallStackPtr(r11)
	std	r0,PA_syscallReturnAddr(r11)
    PPCPrimitiveSyscall_Retry:
	STACK_TOC_AND_FPSCR(r12, 1)	// stack space is reserved for possible
					//     use in PPCPrimitiveSyscallRetry
	GOTO_EXPEDIENT(ExceptionExp_PPCPrimitiveSyscall)
CODE_END(ExceptionLocal_PPCPrimitiveSyscall)

CODE_ENTRY(ExceptionLocal_PPCPrimitiveResume)
	ld	r1,PA_syscallStackPtr(r11)
	ld	r12,PA_syscallReturnAddr(r11)
	b	SyscallReturn
CODE_END(ExceptionLocal_PPCPrimitiveResume)

CODE_ENTRY(PPCPrimitiveAwaitRetry)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_PPCPrimitiveAwaitRetry(curProc, targetID);
	mr	r3,r11			// r3 = curProc
	mr	r4,r14			// r4 = targetID
	bl	C_TEXT(ExceptionLocal_PPCPrimitiveAwaitRetry)
	ld	r11,VOLATILE_FRAME_R11(r1)// recover curProc
	bl	CODE(ExceptionLocal_ReleaseReservedThread)
	VOLATILE_FRAME_RETURN()
CODE_END(PPCPrimitiveAwaitRetry)

CODE_ENTRY(ExceptionLocal_PPCPrimitiveSyscallRetry)
	/*
	 * All registers except r0 and r11-r13 are in use.  We're already on
	 * EXCEPTION_STACK with space for one data word reserved.  The original
	 * stack pointer and syscall return address have already been saved in
	 * currentProcessAnnex.  Free up the remaining registers that are
	 * needed in allocating and switching to a kernel thread.
	 * AcquireReservedThread() preserves all registers except r0, r3,
	 * and r11-r13.
	 */
	std	r3,STK_LOCAL0(r1)	// preserve r3
	bl	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
	mr	r11,r3			// curProc = srcProc
	ld	r3,STK_LOCAL0(r1)	// restore r3
	THREAD_STACK(r13, 0, r0)	// switch to thread stack
	bl	CODE(PPCPrimitiveAwaitRetry)
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	b	PPCPrimitiveSyscall_Retry
CODE_END(ExceptionLocal_PPCPrimitiveSyscallRetry)

CODE_ENTRY(ExceptionLocal_IPCAsyncSyscall)
	// lolita left r12 = &EL
	mr	r11,r1			// save stack pointer
	STACK_TOC_AND_FPSCR(r12, 4)	// stack space is reserved for possible
					//     use in IPCAsyncRemoteSyscall
	mflr	r12			// save return address
	CALL_EXPEDIENT(ExceptionExp_IPCAsyncSyscall)
	mr	r1,r11			// restore stack pointer
	LOAD_C_DATA_UVAL(r11,exceptionLocal,EL_currentProcessAnnex)
	b	SyscallReturn
CODE_END(ExceptionLocal_IPCAsyncSyscall)

CODE_ENTRY(ExceptionLocal_IPCAsyncSyscallRemote)
	/*
	 * All registers except r0 are in use, but we're already on
	 * EXCEPTION_STACK with space for 4 data words reserved.  Free up the
	 * registers that are needed in allocating and switching to a kernel
	 * thread.  AcquireReservedThread() preserves all registers except
	 * r0, r3, and r11-r13.
	 */
	std	r11,STK_LOCAL0(r1)	// preserve original stack pointer
	std	r12,STK_LOCAL1(r1)	// preserve syscall return address
	std	r13,STK_LOCAL2(r1)	// preserve thread register
	std	r3,STK_LOCAL3(r1)	// preserve r3
	bl	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, r0 = disabledSave, r3 = srcProc, r13 = thread
	mr	r11,r3			// curProc = srcProc
	ld	r3,STK_LOCAL3(r1)	// restore r3
	mr	r12,r1			// hang onto EXCEPTION_STACK pointer
	THREAD_STACK(r13, 3, r0)
	ld	r0,STK_LOCAL0(r12)	// copy original stack pointer
	std	r0,STK_LOCAL0(r1)
	ld	r0,STK_LOCAL1(r12)	// copy syscall return address
	std	r0,STK_LOCAL1(r1)
	ld	r0,STK_LOCAL2(r12)	// copy thread register
	std	r0,STK_LOCAL2(r1)
	CALL_EXPEDIENT(ExceptionExp_IPCAsyncSyscallRemoteOnThread)
	ld	r13,STK_LOCAL2(r1)	// restore thread register
	ld	r12,STK_LOCAL1(r1)	// recover syscall return address
	ld	r1,STK_LOCAL0(r1)	// restore stack pointer
	b	SyscallReturn
CODE_END(ExceptionLocal_IPCAsyncSyscallRemote)

CODE_ENTRY(ExceptionLocal_IPCAsyncSyscallInterruptReturn)
	ld	r12,PA_dispatcher(r11)	// r12 = PA.dispatcher
	ld	r0,PA_userStateOffset(r11)
	add	r12,r12,r0		// r12 = PA.userStatePtr()
	// save everything in userState that's expected in InterruptResume
	li	r0,0
	std	r0,VS_r0(r12)		// set r0 = 0
	std	r0,VS_r2(r12)		// set r2 = 0
	mfcr	r0			// save cr
	std	r0,VS_cr(r12)
	ld	r0,STK_LOCAL0(r1)	// save original stack pointer
	std	r0,VS_r1(r12)
	ld	r0,STK_LOCAL1(r1)	// retrieve syscall return address
	std	r0,VS_iar(r12)		// save as IAR
	std	r0,VS_r12(r12)		// also save as r12 ala SyscallReturn
	ld	r0,STK_LOCAL2(r1)	// save thread register
	std	r0,VS_r13(r12)
	ld	r0,PA_msr(r11)		// retrieve msr for this vp
	std	r0,VS_msr(r12)		// save as MSR
	std	r0,VS_r11(r12)		// also save as r11 ala SyscallReturn
	mr	r13,r12			// r13 = userStatePtr for IntrResume
	b	CODE(InterruptResume)
CODE_END(ExceptionLocal_IPCAsyncSyscallInterruptReturn)

CODE_ENTRY(ExceptionLocal_TimerRequestSyscall)
	// lolita left r12 = &EL
	mr	r11,r1			// save stack pointer
	STACK_TOC_AND_FPSCR(r12, 2)
	mflr	r12			// save return address
	std	r11,STK_LOCAL0(r1)
	std	r12,STK_LOCAL1(r1)
	HIDE_DISPATCHER(r11,r12)
	bl	C_TEXT(KernelTimer_TimerRequest)
	RESTORE_DISPATCHER(r11,r12)
	ld	r12,STK_LOCAL1(r1)
	ld	r1,STK_LOCAL0(r1)
	LOAD_C_DATA_UVAL(r11,exceptionLocal,EL_currentProcessAnnex)
	b	SyscallReturn
CODE_END(ExceptionLocal_TimerRequestSyscall)

CODE_ENTRY(ExceptionLocal_InvalidSyscall)
	// lolita left r12 = &EL
	mr	r11,r1			// save stack pointer
	STACK_TOC_AND_FPSCR(r12, 0)
	mflr	r12			// save return address
	CALL_EXPEDIENT(ExceptionExp_InvalidSyscall)
	mr	r1,r11			// restore stack pointer
	LOAD_C_DATA_UVAL(r11,exceptionLocal,EL_currentProcessAnnex)
	b	SyscallReturn
CODE_END(ExceptionLocal_InvalidSyscall)

ENTRY_POINT_DESC(IdleLoopDesc, IdleLoop)
CODE_ENTRY(IdleLoop)
	LOAD_C_DATA_UVAL(r11,kernelInfoLocal,KI_onHV)
	cmpldi	r11,0
	bne	IdleLoopHV
	LOAD_C_DATA_UVAL(r11,kernelInfoLocal,KI_onSim)
	cmpldi	r11,ONSIM_MAMBO
	beq	IdleLoopMambo
	cmpldi	r11,ONSIM_SIMOSPPC
	beq	IdleLoopSimos
    IdleLoopHardware:
	b	.			// infinite loop
    IdleLoopSimos:	
	b	.			// infinite loop
    IdleLoopMambo:
	li	r3,126			// bogus idle request
	.long	0x000eaeb0
	b IdleLoopMambo			// SMP mambo may return
    IdleLoopHV:
	li	r4,-1
	li	r3,H_YIELD
	HSC
    ForceDecFire:	
	li	r3,-1			// force a timer interrupt
	mtdec	r3
	mfdec	r3
	b ForceDecFire
	trap				// should never get here
CODE_END(IdleLoop)

/*
 * Fast-path implementations for the system calls that the kernel itself
 * uses.  The kernel can simply disable interrupts and branch to the
 * appropriate system call handler (after loading r12 with &EL).
 */

CODE_ENTRY(DispatcherDefault_SyscallIPCReturn)
	DISABLE_HW_INTERRUPTS(r12)
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	b	CODE(ExceptionLocal_IPCReturnSyscall)
CODE_END(DispatcherDefault_SyscallIPCReturn)

CODE_ENTRY(DispatcherDefault_SyscallYieldProcessor)
	DISABLE_HW_INTERRUPTS(r12)
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	b	CODE(ExceptionLocal_ProcessYieldSyscall)
CODE_END(DispatcherDefault_SyscallYieldProcessor)

CODE_ENTRY(DispatcherDefault_SyscallHandoffProcessor)
	DISABLE_HW_INTERRUPTS(r12)
	LOAD_C_DATA_ADDR(r12,exceptionLocal)
	b	CODE(ExceptionLocal_ProcessHandoffSyscall)
CODE_END(DispatcherDefault_SyscallHandoffProcessor)

/*****************************************************************************/
/*
 * Safe reload of hardware SLB table
 *
 * extern "C" void ExceptionLocal_LoadSLB(SegmentTable_SLB* s)
 *
 * Carefully invalidate the SLB and reload from a cache.  This is written
 * in assembly to avoid accidental reference to memory whose address is not
 * covered by the first (0th) and second (1st) entry of the SLB cache.
 * This assume (but does not guard) that the cache is itself covered by one
 * of the first two entries of the SLB cache.  Since the first entry in not
 * invalidated no special procedure is needed to reload it.  The second
 * entry in the cache is saved in registers before invalidating the SLB
 * and is immediately reloaded after the invalidation.  After the SLB is
 * invalidated and after the second cached entry is reloaded, so that the
 * SLB should contain enough to cover the SLB cache itself, each remaining
 * entry is read from the SLB cache and loaded into the SLB. 
 */
C_TEXT_ENTRY(ExceptionLocal_LoadSLB)
	ld	r4,ST_SLBCache+16(r3)	// save VSID[1] into a register
	ld	r5,ST_SLBCache+24(r3)	// save ESID[1] into a register
	isync
	slbia				// invalidate all SLB entries except 0th
	slbmte	r4,r5			// reload 2nd SLB entry
	isync
	ld      r4,ST_cacheMax(r3)	// size of cache
	std     r4,ST_slbNext(r3)
	subic.  r4,r4, 2		// 2 entries have already been loaded
	ble     2f
	la      r3,(ST_SLBCache+32-8)(r3) // ldu will move foward to VSID[2]
	mtctr	r4
1:	
	ldu	r4,8(r3)		// VSID[n] 
	ldu	r5,8(r3)		// ESID[n]
	slbmte  r4,r5			// load the nth entry
	bdnz	1b
2:	
	isync
	blr
C_TEXT_END(ExceptionLocal_LoadSLB)
