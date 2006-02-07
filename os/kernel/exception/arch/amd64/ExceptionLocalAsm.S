/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocalAsm.S,v 1.38 2004/04/06 21:00:40 rosnbrg Exp $
 *****************************************************************************/

#include <sys/kinclude.H>
#include <sys/config.H>
#include <misc/asm.h>
#include <misc/expedient.H>
#include <misc/volatileFrame.H>
#include <sys/arch/amd64/asmConstants.H>
#include "init/kernel.H"
#include <defines/use_expedient.H>
#include <arch/amd64/segnum.h>              // KERNEL_CS, ...



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

CODE_ENTRY(IPCReturnAwaitDispatch)
CODE_END(IPCReturnAwaitDispatch)
	
/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

	
ENTRY_POINT_DESC(IdleLoopDesc, IdleLoop)

CODE_ENTRY(IdleLoop)
	jmp	CODE(IdleLoop)			// infinite loop
CODE_END(IdleLoop)



/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

/* 	cpuid(rdi, rsi, rdx, rcx, r8); */
/* put code from rdi into eax, issue the cpuid instruction. */
/* eax -> (rsi);  ebx -> (rdx);  ecx -> (rcx); edx -> (r8) */
	
C_TEXT_ENTRY(cpuid)
	pushq	%rcx
	pushq	%rdx	
        movl	%edi, %eax     
	cpuid
	movl	%eax, (%rsi)
	movl	%edx, (%r8)
	popq	%rsi                /* rdx */
	popq	%rdi		    /* rcx */
	movl	%ebx, (%rsi)
	movl	%ecx, (%rdi)
	ret
	


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */

	
	
/* On powerpc and mips64 there are dedicated registers for the
 * current_thread. This is not the case (yet ?) for amd64.  */

/*
 * When we're entered from user-mode and we're going to execute C
 * code, we must get (at least) the "control" part of fpscr to a
 * known state.  for amd64.
 * LOAD_FCR loads the default value in the SIMD floating point
 * control/status register */
#ifndef CONFIG_SIMICS
#define INIT_FCR()					\
	pushq	$0x1f80;				\
	ldmxcsr	(%rsp);					\
	lea	8(%rsp),%rsp
#else
#define INIT_FCR()					// SIMICS apparently does not support it, GP fault
#endif

#define GET_CURRENT_THREAD(reg)				\
	LOAD_C_DATA_UVAL(reg, CurrentThread, 0)

#define SET_CURRENT_THREAD(reg)				\
	STORE_C_DATA_UVAL(reg, CurrentThread, 0)


#ifndef NDEBUG
#define DEBUGGER_STACK()\
	LOAD_C_DATA_UVAL(%rsp,exceptionLocal,EL_currentDebugStack) \
	pushq   $0;					\
	pushq	$0;					\
	movq	%rsp,%rbp
#else
#define DEBUGGER_STACK()\
	LOAD_C_DATA_UVAL(%rsp,exceptionLocal,EL_currentDebugStack) \
	pushq   $0	
#endif // NDEBUG

#ifndef NDEBUG
#define EXCEPTION_STACK()\
	LOAD_C_DATA_UVAL(%rsp,exceptionLocal,EL_exceptionStack);\
	pushq	$0;\
	pushq	$0;\
	movq	%rsp,%rbp
#else  // NDEBUG
#define EXCEPTION_STACK()\
	LOAD_C_DATA_UVAL(%rsp,exceptionLocal,EL_exceptionStack);\
	pushq	$0
#endif // NDEBUG

#define LINK_EXCEPTION_STACK(ebpOffset)					\
	pushq	%rbp;							\
        lea    (ebpOffset)(%rsp),%rbp;					\
        LOAD_C_DATA_UVAL(%rsp,exceptionLocal,EL_exceptionStack)

#define UNLINK_EXCEPTION_STACK(ebpOffset)				\
        lea    -(ebpOffset)(%rbp),%rsp;					\
	popq	%rbp


#ifndef NDEBUG

#define THREAD_STACK(threadReg)\
	movq	TH_startSP(threadReg),%rsp;\
	pushq	$0;\
	pushq	$0;\
	movq	%rsp,%rbp
#else

#define THREAD_STACK(threadReg)\
	movq	TH_startSP(threadReg),%rsp;\
	pushq	$0

#endif  // NDEBUG

#define ENABLE_HW_INTERRUPTS()\
	sti

#define DISABLE_HW_INTERRUPTS()\
	cli

/*
 * In debugging builds, in order to catch inappropriate exception-level
 * extRegsLocal.dispatcher dereferences, we clobber it before calling
 * legitimate C code and put it back afterwards.
 * 
 */
#ifndef NDEBUG
#define HIDE_DISPATCHER(xrReg,dspReg)\
        LOAD_C_DATA_ADDR(xrReg,extRegsLocal);\
        movq	$0xffffffffffffffff,dspReg;\
        movq    dspReg,XR_dispatcher(xrReg)

#define RESTORE_DISPATCHER(xrReg,dspReg)\
        LOAD_C_DATA_UVAL(dspReg,exceptionLocal,EL_currentProcessAnnex);\
        movq	PA_dispatcherUser(dspReg),dspReg;\
        LOAD_C_DATA_ADDR(xrReg,extRegsLocal);\
        movq	dspReg,XR_dispatcher(xrReg)
#else
#define HIDE_DISPATCHER(xrReg,dspReg)
#define RESTORE_DISPATCHER(xrReg,dspReg)
#endif  // !NDEBUG

#ifndef NDEBUG
/*
 * In debugging builds, when a thread is returned to the free list, we
 * check whether it has stepped on the last 16 words of its stack.
 */
#define DEBUG_CREATE_STK_FENCE(fenceReg,scratchReg)\
	movq	$0xbfbfbfbfbfbfbfbf, scratchReg;				\
										\
	movq	scratchReg,( 0*8)(fenceReg);					\
	movq	scratchReg,( 1*8)(fenceReg);					\
	movq	scratchReg,( 2*8)(fenceReg);					\
	movq	scratchReg,( 3*8)(fenceReg);					\
	movq	scratchReg,( 4*8)(fenceReg);					\
	movq	scratchReg,( 5*8)(fenceReg);					\
	movq	scratchReg,( 6*8)(fenceReg);					\
	movq	scratchReg,( 7*8)(fenceReg);					\
	movq	scratchReg,( 8*8)(fenceReg);					\
	movq	scratchReg,( 9*8)(fenceReg);					\
	movq	scratchReg,(10*8)(fenceReg);					\
	movq	scratchReg,(11*8)(fenceReg);					\
	movq	scratchReg,(12*8)(fenceReg);					\
	movq	scratchReg,(13*8)(fenceReg);					\
	movq	scratchReg,(14*8)(fenceReg);					\
	movq	scratchReg,(15*8)(fenceReg)

#define DEBUG_CHK_STK_FENCE(fenceReg,regA,regB,regC)				\
	movq	$0xbfbfbfbfbfbfbfbf, regA;					\
										\
	movq	regA,regB;							\
	movq	( 0*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 1*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 2*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 3*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 4*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 5*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 6*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 7*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 8*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	( 9*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	(10*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	(11*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	(12*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	(13*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	(14*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	movq	(15*8)(fenceReg),regC; orq regC, regA; andq regC, regB;		\
	cmpq	regA,regB;							\
	je	1f;								\
	int	$3;								\
1:	nop

#define DEBUG_CHK_STK_FREE_THREAD(threadReg,fenceReg,regA,regB,regC)		\
	movq	TH_bottomSP(threadReg), fenceReg;				\
	DEBUG_CHK_STK_FENCE(fenceReg,regA,regB,regC)
#else

#define DEBUG_CREATE_STK_FENCE(fenceReg,regA,regB,regC)
#define DEBUG_CHK_STK_FENCE(fenceReg,regA,regB,regC)
#define DEBUG_CHK_STK_FREE_THREAD(threadReg,fenceReg,regA,regB,regC)

#endif	// !NDEBUG

/* in 64 bit mode ES, Ds and SS seggment registers are ignored and their
 * content unmodified.
 */
#define SWITCH_DATASEGS(ss)

#define RESTORE_DATASEGS(csOffset)

CODE_ENTRY(ExceptionLocal_NullGenericEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullGenericEntryException)

CODE_ENTRY(ExceptionLocal_NullRunEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullRunEntryException)

CODE_ENTRY(ExceptionLocal_NullInterruptEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullInterruptEntryException)

CODE_ENTRY(ExceptionLocal_NullTrapEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullTrapEntryException)

CODE_ENTRY(ExceptionLocal_NullPgfltEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullPgfltEntryException)

CODE_ENTRY(ExceptionLocal_NullIPCCallEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullIPCCallEntryException)

CODE_ENTRY(ExceptionLocal_NullIPCReturnEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullIPCReturnEntryException)

CODE_ENTRY(ExceptionLocal_NullIPCFaultEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullIPCFaultEntryException)

CODE_ENTRY(ExceptionLocal_NullSVCEntryException)
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_NullSVCEntryException)


CODE_ENTRY(ExceptionLocal_LaunchUserOrKernel)
	SWITCH_DATASEGS(USER_SS)
	iretq

/* launches use PA_launcher as offset of the launcher array in the
 * ProcessAnnex which as this stage should be in %r10.
 */  

CODE_ENTRY(ExceptionLocal_Launch_RUN_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(RUN_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go

CODE_ENTRY(ExceptionLocal_Launch_INTERRUPT_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(INTERRUPT_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go

CODE_ENTRY(ExceptionLocal_Launch_TRAP_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(TRAP_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go

CODE_ENTRY(ExceptionLocal_Launch_PGFLT_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(PGFLT_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go

CODE_ENTRY(ExceptionLocal_Launch_IPC_CALL_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(IPC_CALL_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go

CODE_ENTRY(ExceptionLocal_Launch_IPC_RTN_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(IPC_RTN_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go

CODE_ENTRY(ExceptionLocal_Launch_IPC_FAULT_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(IPC_FAULT_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go

CODE_ENTRY(ExceptionLocal_Launch_SVC_ENTRY)
	// set stackptr to launcher address
	lea	(PA_launcher+(SVC_ENTRY*EPL_SIZE))(%r10),%rsp
	ret				// go


/* From exp::AcquireReservedThread, on exception stack, dispatcher in
 * %r11, all volatiles to be preserved, new thread to be returned in %rax.
 */
CODE_ENTRY(ExceptionLocal_CreateThread)
	VOLATILE_FRAME_ENTER()
	// CurrentThread = ExceptionCreateThread(dispatcher);
	movq	%r11, %rdi	// move dispatcher in arg1 reg
	call 	C_TEXT(DispatcherDefaultKern_ExceptionCreateThread)
				// returns Thread* in %rax
	SET_CURRENT_THREAD(%rax)
	VOLATILE_FRAME_RETURN()

/* From PgfltExceptionUser, UserInterruptAwaitDispatch, AwaitDispatch_Launch_IPC_RTN,
 * AwaitDispatch_PPCPrimitiveResume, and IPCAsyncSyscallRemote.  Returns with
 * %rdi == srcProc and %rcx == CurrentThread == thread.  Preserves all other
 * registers except ...
 */
CODE_ENTRY(ExceptionLocal_AcquireReservedThread)
#ifdef	USE_EXPEDIENT_RESERVED_THREAD
	FRAME_ENTER()
	CALL_EXPEDIENT(ExceptionExp_AcquireReservedThread)
	// on return, %rax = disabledSave, %rdi = srcProc,
	// CurrentThread = thread
	GET_CURRENT_THREAD(%rcx)	// load current thread in %rcx
	FRAME_RETURN()

#else
	xxxxx
#endif	// !USE_EXPEDIENT_RESERVED_THREAD

/* From HandleUserPgflt, UserInterruptAwaitDispatch, IPCReturnAwaitDispatch,
 * and IPCAsyncSyscallRemoteOnThread.  Called on thread stack with
 * %r10 == curProc.  Preserves all registers 
 */
CODE_ENTRY(ExceptionLocal_ReleaseReservedThread)
#ifdef	USE_EXPEDIENT_RESERVED_THREAD
	FRAME_ENTER()
	CALL_EXPEDIENT(ExceptionExp_ReleaseReservedThread)
	FRAME_RETURN()
#else
	xxxxx
#endif	// !USE_EXPEDIENT_RESERVED_THREAD

/* From PgfltExceptionUser and IPC{Call,Return}Syscall, on exception stack,
 * all volatiles to be preserved.
 */
CODE_ENTRY(ExceptionLocal_MakeCurProcReady)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_MakeCurrentProcessAnnexReady();
	call	C_TEXT(ExceptionLocal_MakeCurrentProcessAnnexReady)
	VOLATILE_FRAME_RETURN()



/* From UserResumeProcess (and KernelPgfltResume, TrapExceptionKernel, etc),
 * volatile + special in exc save area, rest in reg, int disabled,
 * pointer to volatileState in %rdi, no return
 */

CODE_ENTRY(ExceptionLocal_UserResume)
SaveAreaResume:
	movq	%rdi,%rsp		// set stackptr to save-area address
	PART_VOLATILE_FRAME_RESTORE()
	popq	%r10
	popq	%r11
	iretq

CODE_ENTRY(ExceptionLocal_ResumeUserProcess)
#ifdef	USE_EXPEDIENT_USER_RESUME
	GOTO_EXPEDIENT(ExceptionExp_ResumeUserProcess)
#else
	xxxx
#endif	// !USE_EXPEDIENT_USER_RESUME


/* on entry we are on the current ITS stack, 
 * FaultFrame saved by hardware, including error code
 * (pushed NULL if not hardware present) and the trapNumber
 * pushed last on the stack.
 * error code and trapNumber are in VolatileState slot %r11, %r10
 * respectively.
 * On this machine the SS selector pushed is NULL if we are were kernel mode
 * (and obviously already 64 bit mode).
 */
CODE_ENTRY(ExceptionLocal_TrapException)
	// save the rest of VolatileState
	PART_VOLATILE_FRAME_SAVE()
 
	movq	VS_r10(%rsp),%rdi	// retrieve Trap_trapNumber in ExpRegs register
	movq	VS_r11(%rsp),%rsi	// retrieve Trap_trapInfo in ExpRegs register
	movq	%r11,VS_r11(%rsp)	// save %r11
	movq	%r10,VS_r10(%rsp)	// save %r10

//	testq	$0x3,VS_cs(%rsp)	// branch if kernel-mode trap
	testw	$0x3,VS_cs(%rsp)	// branch if kernel-mode trap
	jz	TrapExceptionKernel

	/* int disabled, state in trap save area, cause in
	 * trapinfo register, no return.
	 */
TrapExceptionUser:
	SWITCH_DATASEGS(KERNEL_SS)	// noop for amd64
	INIT_FCR()
	EXCEPTION_STACK()		// move to exception stack
	GOTO_EXPEDIENT(ExceptionExp_TrapExceptionUser)
// NOTREACHED
	int	$3

/* From local::KernelPgfltToTrap, int disabled, state in trap save area,
 * cause in trapinfo register, no return. Also from idt.C
 */
TrapExceptionKernel:
	/*
	 * Note:  If we wanted to support kernel-mode processes other than
	 *        the kernel process itself, we would have to recognize them
	 *        here.
	 */
	DEBUGGER_STACK()
	LOAD_C_DATA_OFFSET(%r8,exceptionLocal,EL_trapVolatileState)
//	LOAD_C_DATA_OFFSET(%r8,exceptionLocal,0)
//	lea EL_trapVolatileState(%r8), %r8		XXX this call may assume different parm that hte other branch later related to page fault ... XXX
	CALL_EXPEDIENT(GdbKernelService)
	movq	%r8, %rdi		// move back where it should be in ExceptionExpRegs
	jmp	SaveAreaResume
trapOnSim:

/* on entry we are on the current ITS stack, 
 * FaultFrame saved by hardware, including error code (faultInfo)
 * faultInfo is in VolatileState slot %r11
 */
CODE_ENTRY(ExceptionLocal_PgfltException)
	// save the rest of VolatileState
	pushq	%r10
	PART_VOLATILE_FRAME_SAVE()
 
	movq	VS_r11(%rsp),%rsi	// retrieve Pgflt_faultInfo in ExpRegs register
	movq	%r11,VS_r11(%rsp)	// save %r11
	movq	%cr2,%rdx		// retrieve fault address (faultAddr) to ExpRegs
	testw	$0x3,VS_cs(%rsp)	// branch if kernel-mode trap
	jz	PgfltExceptionKernel

PgfltExceptionUser:
	/* int disabled, no stack, no rturn, volatile in
	 * procstate, faultinfo/addr in exp reg
	 */
	INIT_FCR()
	/*
	 * Note: AcquireReservedThread() needs a stack only if
	 *       there's no thread on the free list.  If we inlined it here,
	 *       we could avoid the couple cycles and cache-line touches it
	 *       takes to get onto EXCEPTION_STACK when it's not necessary.
	 */
	EXCEPTION_STACK()
	call	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, %rax = disabledSave, %rdi = srcProc, %rcx = thread
#ifdef USE_EXPEDIENT_USER_PGFLT
	THREAD_STACK(%rcx)		// switch to thread stack
	GOTO_EXPEDIENT(ExceptionExp_HandleUserPgflt)
#else
	xxxx
#endif // USE_EXPEDIENT_USER_PGFLT

/* int disabled, on faulting kernel thread stack, special +
 * volatile in VS on stack, no return, faultaddr/info in exp reg.
 */
PgfltExceptionKernel:
#ifndef	NDEBUG
	/*
	 * To help with debugging, we set things up to look as if the
	 * interrupted thread called this entry-point code immediately prior
	 * to its next real instruction.
	 */
	/*
	 * We do some paranoid stack checking in debugging builds.
	 */
	GET_CURRENT_THREAD(%rcx)		// retrieve current bottomSP
	movq	TH_bottomSP(%rcx), %r8
	cmpq	%r8,%rsp 			// compare if rsp > r8  yes I know gas is backward here ... sigh
	jge	1f				// make sure we're not past it
	int	$3
    1:
						// check fence at bottomSP
	DEBUG_CHK_STK_FENCE(%r8, %r9, %r10, %r11)
	lea	-KERN_PGFLT_STK_SPACE(%rsp), %r9	// calculate new bottomSP		
	lea	-(KERN_THREAD_SIZE-TH_SIZE)(%rcx), %r10	// make sure new bottomSP is still within stack
	cmpq	%r10, %r9				// yes I know gas is backward here ... sigh
	jge     2f
	int	$3
    2:
	DEBUG_CREATE_STK_FENCE(%r9,%r10)		// create fence at new bottomSP
	movq	%r9, TH_bottomSP(%rcx)			// store new bottomSP
	pushq	%r8					// preserve old bottomSP
#endif	// !NDEBUG
	GOTO_EXPEDIENT(ExceptionExp_HandleKernelPgflt)



/* From exp::HandlerKernelPgflt, state in VS on stack + reg, disabled int,
 * on original kernel faulting stack, with procstate on stack, no return.
 */
CODE_ENTRY(ExceptionLocal_KernelPgfltResume)
#ifdef	NDEBUG
#else	// NDEBUG
	GET_CURRENT_THREAD(%rcx)		// check the fence at bottomSP
	movq	TH_bottomSP(%rcx), %r8
	DEBUG_CHK_STK_FENCE(%r8, %r9, %r10, %r11)
	popq	%r8				// retrieve old bottomSP
	DEBUG_CREATE_STK_FENCE(%r8,%r9)		// restore fence at bottomSP
	movq	%r8, TH_bottomSP(%rcx)		// restore bottomSP
#endif	// !NDEBUG
	movq	%rsp, %rdi			// to permit the branch, but could bypass to SaveAreaResume next instruction XXX
	jmp	SaveAreaResume			// %rdi should hold VS address XXX not obvious check pls XXX

/* From exp::HandlerKernelPgflt, state in VS on stack + reg, disabled int,
 * on original kernel faulting stack, with procstate on stack, no return.
 *	XXX check for consistency of parms w/ other paths because the mips64 code copies on the stack here XXX
 */
CODE_ENTRY(ExceptionLocal_KernelPgfltToTrap)
	jmp	TrapExceptionKernel



/* on entry irq has been pushed at VS_r11 (%r11 slot) on the current VolatileState.
 * Current stack is Stack Interrupt Table entry 1 in TSS.
 */
CODE_ENTRY(ExceptionLocal_IOInterrupt)
	// save the rest of VolatileState
	pushq	%r10
	PART_VOLATILE_FRAME_SAVE()
 
	movq	VS_r11(%rsp),%rdi	// retrieve interrupt number as arg1
	movq	%r11,VS_r11(%rsp)	// save %r11

	testq	$0x3,VS_cs(%rsp)	// branch if kernel-mode interrupt
	jz	IOInterruptKernel
IOInterruptUser:
	INIT_FCR()
	EXCEPTION_STACK()		// could use LINK_EXCEPTION_STACK(some) but would need saving %rbp first XXX
	HIDE_DISPATCHER(%rsi, %rdx)	// use arg2 arg3 unused arg registers
	call	C_TEXT(HWInterrupt_IOInterrupt)
	RESTORE_DISPATCHER(%rsi, %rdx)
	// This call to expedient does not need any set up in ExpRegs
	// goes to asm UserInterruptAwaitDispatch or ResumeUserProcess
	GOTO_EXPEDIENT(ExceptionExp_UserInterruptContinue)
IOInterruptKernel:
	EXCEPTION_STACK()
	HIDE_DISPATCHER(%rsi, %rdx)	// use arg2 arg3 unused arg registers
	call	C_TEXT(HWInterrupt_IOInterrupt)
	RESTORE_DISPATCHER(%rsi, %rdx)
	// This call to expedient does not need any set up in ExpRegs
	// goes to asm IdleLoopYield or KernelReflectInterrupt 
	// or KernelInterruptResume
	GOTO_EXPEDIENT(ExceptionExp_KernelInterruptContinue)


/* From exp::UserInterruptContinue, those comments hereafter may not be up to date XXX int disabled, 
 * kernel thread to switch to in exp reg, no return.
 */
CODE_ENTRY(ExceptionLocal_UserInterruptAwaitDispatch)
	call	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, %rax = disabledSave, %rdi = srcProc, %rcx = thread
	THREAD_STACK(%rcx)
	pushq	%rax				// preserve disabledSave
	pushq	%rdi				// preserve srcProc
	call	C_TEXT(ExceptionLocal_AwaitDispatch)
	popq	%r10				// curProc = srcProc
        LOAD_C_DATA_ADDR(%r11, extRegsLocal)	// extRegsLocal.disabled =
	popq 	%rax				//     disabledsave
	movq	%rax, XR_disabled(%r11)
	call	CODE(ExceptionLocal_ReleaseReservedThread)
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_UserInterruptContinue)
	
CODE_ENTRY(ExceptionLocal_KernelInterruptResume)
	jmp	SaveAreaResume

CODE_ENTRY(ExceptionLocal_KernelReflectInterrupt)
	ENABLE_HW_INTERRUPTS()
	jmp	CODE(DispatcherDefault_InterruptEntry)

CODE_ENTRY(ExceptionLocal_IdleLoopYield)
	call	C_TEXT(ExceptionLocal_GetNextProcess)
	// returns the new PA in %rax
	movq	%rax, %r10			// curProc = new PA to run
	jmp	CODE(ExceptionLocal_Launch_RUN_ENTRY)




/* state in savearea, no stack, int disabled.
 */
CODE_ENTRY(ExceptionLocal_TimerInterrupt)
	// save the rest of VolatileState
	pushq	%r11
	pushq	%r10
	PART_VOLATILE_FRAME_SAVE()
 
	testq	$0x3,VS_cs(%rsp)	// branch if kernel-mode interrupt
	jz	TimerInterruptKernel
TimerInterruptUser:
	INIT_FCR()
	EXCEPTION_STACK()
	HIDE_DISPATCHER(%r8,%r9)
	call	C_TEXT(HWInterrupt_TimerInterrupt)
	RESTORE_DISPATCHER(%r8,%r9)
	GOTO_EXPEDIENT(ExceptionExp_UserInterruptContinue)
TimerInterruptKernel:
	EXCEPTION_STACK()
	HIDE_DISPATCHER(%r8,%r9)
	call	C_TEXT(HWInterrupt_TimerInterrupt)
	RESTORE_DISPATCHER(%r8,%r9)
	GOTO_EXPEDIENT(ExceptionExp_KernelInterruptContinue)

CODE_ENTRY(ExceptionLocal_IPInterrupt)
	// save the rest of VolatileState
	pushq	%r11
	pushq	%r10
	PART_VOLATILE_FRAME_SAVE()
 
	testq	$0x3,VS_cs(%rsp)	// branch if kernel-mode interrupt
	jz	IPInterruptKernel
IPInterruptUser:
	INIT_FCR()
	EXCEPTION_STACK()
	HIDE_DISPATCHER(%r8,%r9)
	call	C_TEXT(HWInterrupt_IPInterrupt)
	RESTORE_DISPATCHER(%r8,%r9)
	GOTO_EXPEDIENT(ExceptionExp_UserInterruptContinue)
IPInterruptKernel:
	EXCEPTION_STACK()
	HIDE_DISPATCHER(%r8,%r9)
//	LINK_EXCEPTION_STACK(PS_ebp)	// can we XXX
	call	C_TEXT(HWInterrupt_IPInterrupt)
	RESTORE_DISPATCHER(%r8,%r9)
	GOTO_EXPEDIENT(ExceptionExp_KernelInterruptContinue)


// from lolita, no stack, syscall args in arg reg, returns
// extern "C" SysStatus SetEntryPoint(EntryPointNumber entryPoint,
//                                    EntryPointDesc entry)
CODE_ENTRY(ExceptionLocal_SetEntryPointSyscall)
	LINK_EXCEPTION_STACK(0)
        INIT_FCR()
        HIDE_DISPATCHER(%r10, %r11)
        call      C_TEXT(ExceptionLocal_SetEntryPoint)
        RESTORE_DISPATCHER(%r10, %r11)
	UNLINK_EXCEPTION_STACK(0)
	iretq				// resume client

CODE_ENTRY(ExceptionLocal_ProcessYieldSyscall)
	INIT_FCR()
	EXCEPTION_STACK()
	call	C_TEXT(ExceptionLocal_GetNextProcess)
	// return new PA in %rax
	movq	%rax, %r10			// curProc = new PA to run
	jmp	CODE(ExceptionLocal_Launch_RUN_ENTRY)

CODE_ENTRY(ExceptionLocal_ProcessHandoffSyscall)
	INIT_FCR()
	EXCEPTION_STACK()
	// target commID already in %rax
	call	C_TEXT(ExceptionLocal_HandoffProcess)
	// return new PA in %rax
	movq	%rax, %r10			// curProc = new PA to run
	jmp	CODE(ExceptionLocal_Launch_RUN_ENTRY)

/*
 * r10 = target.
 */
CODE_ENTRY(ExceptionLocal_ReqRetryNotif)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_RequestRetryNotification(target);
	movq	%r10, %rdi				// target as arg1
	call	C_TEXT(ExceptionLocal_RequestRetryNotification)
	VOLATILE_FRAME_RETURN()
CODE_END(ExceptionLocal_ReqRetryNotif)

/* ppc reg defined, int disabled, no stack, no return */
CODE_ENTRY(ExceptionLocal_IPCCallSyscall)
#ifdef USE_EXPEDIENT_PPC
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_IPCCallSyscall)
#else
#endif // USE_EXPEDIENT_PPC


/*  ppc reg defined, int disabled, no stack, no return */
CODE_ENTRY(ExceptionLocal_IPCReturnSyscall)
#ifdef USE_EXPEDIENT_PPC
	INIT_FCR()
	EXCEPTION_STACK()
	GOTO_EXPEDIENT(ExceptionExp_IPCReturnSyscall)
#else
#endif // USE_EXPEDIENT_PPC

/* From IPCReturnSyscall.
*/
CODE_ENTRY(ExceptionLocal_AwaitDispatch_Launch_IPC_RTN_ENTRY)
	/*
	 * All registers except %rsp, %r10, %r11 are in use
	 * We hop onto EXCEPTION_STACK just long enough to allocate and
	 * switch to a kernel thread.  AcquireReservedThread()
	 * preserves all registers except %r10 .....
	 */
	INIT_FCR()
	EXCEPTION_STACK()
	pushq	%rcx				// clobbered by ExceptionLocal_AcquireReservedThread
	pushq	%rdi				// clobbered by ExceptionLocal_AcquireReservedThread
	call	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, %rax = disabledSave, %rdi = srcProc, %rcx = thread
	movq	%rdi, %r10			// srcProc->curProc
	popq	%rdi				// restore %rdi
	THREAD_STACK(%rcx)			// switch to thread stack
	popq	%rcx				// restore %rcx
	call	CODE(IPCReturnAwaitDispatch)
	jmp	CODE(ExceptionLocal_Launch_IPC_RTN_ENTRY)
CODE_END(ExceptionLocal_AwaitDispatch_Launch_IPC_RTN_ENTRY)

/*
 * extern "C" void ExceptionLocal_AcceptRemoteIPC(IPCRegsArch *ipcRegsP,
 *                                                CommID callerID,
 *                                                uval ipcType,
 *                                                ProcessAnnex *curProc);
 */
C_TEXT_ENTRY(ExceptionLocal_AcceptRemoteIPC)
	FRAME_ENTER()
	// ipcRegsP already in %rdi
	movq	%rsi, %r12			// callerID in expected register
	movq	%rdx, %r11			// ipcType in expected register
	movq	%rcx, %r10			// curProc in expected register
	GOTO_EXPEDIENT(ExceptionExp_AcceptRemoteIPC)
C_TEXT_END(ExceptionLocal_AcceptRemoteIPC)

/* From IPCReturnSyscall.
*/
CODE_ENTRY(ExceptionLocal_AwaitDispatch_PPCPrimitiveResume)
	/*
	 * All registers except %rsp, %r10, %r11 are in use
	 * We hop onto EXCEPTION_STACK just long enough to allocate and
	 * switch to a kernel thread.  AcquireReservedThread()
	 * preserves all registers except %r10 .....
	 */
	INIT_FCR()
	EXCEPTION_STACK()
	pushq	%rcx				// clobbered by ExceptionLocal_AcquireReservedThread
	pushq	%rdi				// clobbered by ExceptionLocal_AcquireReservedThread
	call	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, %rax = disabledSave, %rdi = srcProc, %rcx = thread
	movq	%rdi, %r10			// srcProc->curProc
	popq	%rdi				// restore %rdi
	THREAD_STACK(%rcx)			// switch to thread stack
	popq	%rcx				// restore %rcx
	call	CODE(IPCReturnAwaitDispatch)
	jmp	CODE(ExceptionLocal_PPCPrimitiveResume)
CODE_END(ExceptionLocal_AwaitDispatch_PPCPrimitiveResume)

/* From lolita, ppc reg defined, int disabled, no stack, no return 
 * is the LINK_EXCEPTION_STACK apprpriate here XXX
 */

CODE_ENTRY(ExceptionLocal_PPCPrimitiveSyscall)
	INIT_FCR()
#ifndef NDEBUG
	LINK_EXCEPTION_STACK(0)
	// save frame pointer in process annex for later resume
	LOAD_C_DATA_UVAL(%r10,exceptionLocal,EL_currentProcessAnnex)
	movq	%rbp,PA_syscallFramePtr(%r10)
#else
	// save frame pointer in process annex for later resume
	LOAD_C_DATA_UVAL(%r10,exceptionLocal,EL_currentProcessAnnex)
	movq	%rsp,PA_syscallFramePtr(%r10)
	EXCEPTION_STACK()
#endif // NDEBUG
PPCPrimitiveSyscall_Retry:
	GOTO_EXPEDIENT(ExceptionExp_PPCPrimitiveSyscall)

/* %r10 contains srcProc to get back the frame to return to the caller.
 * int disabled, exc stack, rc exp reg set.  EXP_CURPROC has current
 */
CODE_ENTRY(ExceptionLocal_PPCPrimitiveResume)
#ifndef NDEBUG
	movq	PA_syscallFramePtr(%r10),%rbp
	UNLINK_EXCEPTION_STACK(0)
#else
	movq	PA_syscallFramePtr(%r10),%rsp
#endif // NDEBUG
	iretq				// resume client

// from PPCPrimitiveSyscallRetry.  %r10 = curProc.  Must preserve all registers.
CODE_ENTRY(PPCPrimitiveAwaitRetry)
	VOLATILE_FRAME_ENTER()
	// ExceptionLocal_PPCPrimitiveAwaitRetry(curProc, targetID);
	movq	%r10, %rdi				// curProc as arg1
	movq	%r12, %rsi				// targetID as arg2
	call	C_TEXT(ExceptionLocal_PPCPrimitiveAwaitRetry)
	movq	520(%rsp), %r10				// recover curProc
	call	CODE(ExceptionLocal_ReleaseReservedThread)
	VOLATILE_FRAME_RETURN()
CODE_END(PPCPrimitiveAwaitRetry)

/:/ From exp::PPCPrimitiveSyscall

CODE_ENTRY(ExceptionLocal_PPCPrimitiveSyscallRetry)
	/*
         * Target was disabled.  We can't reflect a primitive PPC back to the
         * sender, so we have to delay and retry in the kernel. To do that we
         * have to get out of exception level and up on the kernel thread
         * reserved for this vp.  ALL registers in erp are in use at this
         * point, so we could not legitimately do the thread allocation and
         * switching in the exp::PPCPrimitiveSyscall.  
	 * (AcquireReservedThread() assumes certain
         * registers are available.)  Instead we bail out back to assembly
         * language where we can start over... here.
	 * 
	 * Note that the faultFrame pointer (or equivalent in the DEBUG case)
	 * has been already saved in ExceptionLocal_PPCPrimitiveSyscall (above)
	 * because this syscall is not expected to finish immediately (conext
	 * switch ...). On the contrary in ExceptionLocal_IPCAsyncSyscall we
	 * normally expect to complete at exception level so that we save the minimum
	 * up front (only in register) and in the case where we have to acquire a reserved
	 * thread to complete the call (calling ExceptionLocal_IPCAsyncSyscall like here below)
	 * we have to do more temporary saving, see ExceptionLocal_IPCAsyncSyscallInterruptReturn
	 * hereafter.
	 *
	 * All registers except %r10 and  %r11 are in use.  We're already on
	 * EXCEPTION_STACK.  Free up the registers that are needed in
	 * allocating and switching to a kernel thread.  The original stack
	 * pointer and syscall return address have already been saved in
	 * currentProcessAnnex.  Free up the remaining registers that are
	 * needed in allocating and switching to a kernel thread.
	 * AcquireReservedThread() preserves all registers in ExpRegs except
	 * %rdi and %r10 %r11.	dubious statement I believe the only thing 
	 * in ExpRegs which have already been set is %r10 curproc
	 * and %r11 dispatcher, this one to KERN_DISPATCHER which would not change
	 * in ExceptionLocal_AcquireReservedThread. Hence I believe
	 * the only one needed is %r10.
	 */
	pushq	%r11				// needed as a scratch for %rsp
	pushq	%rcx				// destroyed by ExceptionLocal_AcquireReservedThread
	pushq	%rdi				// destroyed by ExceptionLocal_AcquireReservedThread w/srcProc
	call	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, %rax = disabledSave, %rdi = srcProc, %rcx = thread
	movq	%rsp, %r11			// stash away EXCEPTION_STACK ptr
	THREAD_STACK(%rcx)			// switch to thread stack
	movq	%rdi, %r10			// curProc <- srcproc in Expregs
	movq	0(%r11), %rdi			// restore %rdi
	movq	8(%r11), %rcx			// restore %rcx
	movq	16(%r11), %r11			// finally restore %r11 itself 
	call	CODE(PPCPrimitiveAwaitRetry)
	EXCEPTION_STACK()		
	jmp	PPCPrimitiveSyscall_Retry
	

/* From lolita, int disabled, no stack, ppc reg defined */
CODE_ENTRY(ExceptionLocal_IPCAsyncSyscall)
	movq	%rsp, %r11		// save temporarily frame pointer
	LINK_EXCEPTION_STACK(0)
	INIT_FCR()
	CALL_EXPEDIENT(ExceptionExp_IPCAsyncSyscall)
	UNLINK_EXCEPTION_STACK(0)
	iretq				// resume client


/* From exp::IPCAsyncSyscall.
 * original stack pointer in %r10.
 * see note in ExceptionLocal_PPCPrimitiveSyscall in ExceptionLocal_PPCPrimitiveSyscallRetry
 * above for an explanation.
 */
CODE_ENTRY(ExceptionLocal_IPCAsyncSyscallRemote)
	/*
	 * ALL registers may be in use, but we're already on EXCEPTION_STACK.
	 * Free up the registers that are needed in allocating and switching
	 * to a kernel thread.  AcquireReservedThread() preserves
	 * all registers except %rdi, %rcx, and %r10.  
	 * see note in ExceptionLocal_PPCPrimitiveSyscallRetry.
	 */
	pushq	%r11				// temporarily saved original frame pointer
//	pushq	%r10				// will need a scratch save register
	pushq	%rcx				// clobbered by ExceptionLocal_AcquireReservedThread
	pushq	%rdi				// clobbered by ExceptionLocal_AcquireReservedThread
	call	CODE(ExceptionLocal_AcquireReservedThread)
	// on return, %rax = disabledSave, %rdi = srcProc, %rcx = thread
	movq	%rsp, %r10			// keep EXCEPTION_STACK ptr
	THREAD_STACK(%rcx)			// switch to thread stack
	movq	24(%r10), %r10			// save original frame pointer on thread stack
	pushq	%r10
	movq	%rdi, %r10			// curProc <- srcProc
	movq	(%r10), %rdi			// restore %rdi
	movq	8(%r10), %rcx			// restore %rcx
	CALL_EXPEDIENT(ExceptionExp_IPCAsyncSyscallRemoteOnThread)
	popq	%rsp				// pop the original frame pointer
	iretq

/*
 * From exp::IPCAsyncSyscallRemoteOnThread when pending interrupts have been
 * detected.  We have to set things up to look as if we'd returned from the
 * IPCAsync syscall and then been interrupted.  curproc i.e. %r10 is valid.  Client's
 * original frame pointer is still on the stack.
 */
CODE_ENTRY(ExceptionLocal_IPCAsyncSyscallInterruptReturn)
	int $3					// TBD XXX


// from lolita, no stack, syscall args in arg reg, returns
// extern "C" SysTime _TIMER_REQUEST(SysTime when, TimerRequest::Kind kind)
CODE_ENTRY(ExceptionLocal_TimerRequestSyscall)
	LINK_EXCEPTION_STACK(0)
        INIT_FCR()
        HIDE_DISPATCHER(%r10, %r11)
        call      C_TEXT(KernelTimer_TimerRequest)
        RESTORE_DISPATCHER(%r10, %r11)
	UNLINK_EXCEPTION_STACK(0)
	iretq				// resume client

/* if we get a timer interrupt before we are ready to handle it we just eoi,
 * the real handler is installed by interrupt_fixup().
 * Until then this temporary handler is installed by initExceptionHandlers().
 */

.extern interrupt_eoi_timer;

CODE_ENTRY(ExceptionLocal_NullTimerInterrupt)
	pushq	%r11
	pushq	%r10
        pushq   %r9
        pushq   %r8
        pushq   %rdi
        pushq   %rsi
        pushq   %rbp
        pushq   %rdx
        pushq   %rcx
        pushq   %rax
        call C_TEXT(interrupt_eoi_timer)
        popq    %rax
        popq    %rcx
        popq    %rdx
        popq    %rbp
        popq    %rsi
        popq    %rdi
        popq    %r8
        popq    %r9
        popq    %r10
        popq    %r11
      	iretq

