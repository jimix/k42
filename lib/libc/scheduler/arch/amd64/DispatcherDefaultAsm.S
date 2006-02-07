/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

/* VVV this file is derived from  x86 asm code. pdb XXX
 * The differences reflect besides the changes from 32 to 64 bit operands 
 * a change to take adavantage of a new feature of the amd64 architecture.
 * In legacy mode interrupts in user mode use save on the TSS ring 0 stack
 * but in kernel mode the current stack is use to save the state.
 * In long mode (i.e. for both 64 bit mode and compatibility mode) it is possible
 * to use an alternate stack mechanism. One of 7 stacks, numbered 1 to 7 may be specified
 * in IDT entries. When that field is 0 the traditional mechanism is used
 * otherwise one of 7 possible stack defined in the TSS is used.
 * This permits this architecture to be treated more like mips or power.
 */

#include <sys/kinclude.H>
#include <sys/config.H>
#include <misc/asm.h>
#include <sys/syscalls.H>
#include <sys/arch/amd64/asmConstants.H>
#include <misc/expedient.H>
#include <misc/volatileFrame.H>
#include <defines/use_expedient.H>


#ifndef CONFIG_SIMICS	// seems not implemented in Simics
/* LOAD_FCR loads the default value in the SIMD floating point control/status register
 */
#define LOAD_FCR(scratchReg)			\
	pushq	$0x1f80;			\
	ldmxcsr	(%rsp);				\
	popq	scratchReg
#else
#define LOAD_FCR(scratchReg) 
#endif

#define GET_DISPATCHER(dispatcherReg)\
	LOAD_C_DATA_UVAL(dispatcherReg,extRegsLocal,0);\
	leaq	XR_dispatcher(dispatcherReg), dispatcherReg

/* for DEBUG we use fno_omit_frame_pointer, for NDEBUG fomit_frame_pointer
 * and respect the 16 bytes alignment eequired bu both ABI and hardware to
 * save restore floating point.
 */
#ifndef NDEBUG

#define DISPATCHER_STACK(dispatcherReg)				\
	movq	DD_dispatcherStack(dispatcherReg), %rsp;	\
	pushq	$0;						\
	pushq	$0;						\
	movq	%rsp,%rbp

#define DEBUGGER_STACK(dispatcherReg)\
	movq	DD_currentDebugStack(dispatcherReg), %rsp;	\
	pushq	$0;\
	pushq	$0;\
	movq	%rsp,%rbp

#else
#define DISPATCHER_STACK(dispatcherReg)				\
	movq	DD_dispatcherStack(dispatcherReg), %rsp;	\
	pushq	$0

#define DEBUGGER_STACK(dispatcherReg)\
	movq	DD_currentDebugStack(dispatcherReg), %rsp;	\
	pushq	$0

#endif // NDEBUG


#define GET_CURRENT_THREAD(reg)				\
	LOAD_C_DATA_UVAL(reg, CurrentThread, 0)

#define SET_CURRENT_THREAD(reg)\
	STORE_C_DATA_UVAL(reg, CurrentThread, 0)

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

#define CURTHREAD_STACK(psReg)\
	movq	VS_rsp(psReg),%rsp

#define LOAD_USER_STATE_ADDR(reg,dispatcherReg)			\
	movq	D__userStateOffset(dispatcherReg), reg;		\
	addq	dispatcherReg,reg

#define LOAD_TRAP_STATE_ADDR(reg,dispatcherReg)			\
	movq	D__trapStateOffset(dispatcherReg), reg;		\
	addq	dispatcherReg,reg


ENTRY_POINT_DESC(DispatcherDefault_RunEntry_Desc, DispatcherDefault_RunEntry)
ENTRY_POINT_DESC(DispatcherDefault_InterruptEntry_Desc, DispatcherDefault_InterruptEntry)
ENTRY_POINT_DESC(DispatcherDefault_TrapEntry_Desc, DispatcherDefault_TrapEntry)
ENTRY_POINT_DESC(DispatcherDefault_PgfltEntry_Desc, DispatcherDefault_PgfltEntry)
ENTRY_POINT_DESC(DispatcherDefault_IPCCallEntry_Desc, DispatcherDefault_IPCCallEntry)
ENTRY_POINT_DESC(DispatcherDefault_IPCReturnEntry_Desc, DispatcherDefault_IPCReturnEntry)
ENTRY_POINT_DESC(DispatcherDefault_IPCFaultEntry_Desc, DispatcherDefault_IPCFaultEntry)
ENTRY_POINT_DESC(DispatcherDefault_SVCEntry_Desc, DispatcherDefault_SVCEntry)

#ifndef	NDEBUG
/*
 * In debugging builds we check for stack overflow when a thread is
 * suspended or interrupted.
 * DEBUG_CHK_STK_SUSPEND code works only for positive addresses XXX
 * When a thread is returned to the free list, we check whether it has
 * stepped on the last 16 words of its stack.
 * We check for a primitive PPC not guarded by allowPrimitivePPC.
 */
#define DEBUG_CHK_STK_SUSPEND(threadReg,scratchReg)	\
	movq	TH_bottomSP(threadReg), scratchReg;	\
	cmpq	$0, scratchReg;				\
	je	1f;					\
	cmpq	scratchReg, %rsp; /* compare if rsp > scratchReg  yes I know gas is backward here ... sigh */			\
	jge	1f; /* hope it means rsp >= scratchReg	XXX */					\
	int	$3;					\
1:	nop

#define DEBUG_CHK_STK_INTERRUPT(dspReg,threadReg,scratchReg)	\
	LOAD_USER_STATE_ADDR(%rsp,dspReg);			\
	movq	VS_rsp(%rsp), %rsp;				\
	GET_CURRENT_THREAD(threadReg);				\
	DEBUG_CHK_STK_SUSPEND(threadReg,scratchReg)

/* amd64 can use 64 bit immediate (with a REX prefix because the default size is 32) and generally
 * sign extends the 32bit immediate... except for a couple of cases which can use real 64 bit
 * immediate, e.g. mov to register.
 * This code will be useful when running w/o Expedient.
 */
#define DEBUG_CHK_STK_FREE_THREAD(threadReg,regA,regB,regC,regD)	\
	movq	0xbfbfbfbfbfbfbfbf, RegC;				\
	movq	RegC, RegD;						\
	movq	TH_bottomSP(threadReg), RegA;				\
	cmpq	$0, regA;						\
	je	1f;							\
	movq	( 0*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 1*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 2*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 3*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 4*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 5*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 6*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 7*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 8*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	( 9*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	(10*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	(11*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	(12*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	(13*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	(14*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	movq	(15*8)(regA), RegB; or RegB, RegC; and RegB, RegD;	\
	cmpq	RegC, RegD;						\
	je	1f;							\
	int	$3;							\
1:	nop

#define DEBUG_CHK_ALLOW_PRIMITIVE_PPC(dspReg,scratchReg)		\
	movq	DD_allowPrimitivePPCFlag(dspReg), scratchReg;		\
	cmpq	$0, scratchReg;						\
	jne	1f;							\
	int	$3;							\
1:	nop

#else	// NDEBUG
#define DEBUG_CHK_STK_SUSPEND(threadReg,scratchReg)
#define DEBUG_CHK_STK_INTERRUPT(dspReg,threadReg,scratchReg)
#define DEBUG_CHK_STK_FREE_THREAD(threadReg,regA,regB,regC,regD)
#define DEBUG_CHK_ALLOW_PRIMITIVE_PPC(dspReg,scratchReg)
#endif	// NDEBUG


/*
 * extern "C" void DispatcherDefault_InitThread(Thread *thread,
 * 						Scheduler::ThreadFunction fct,
 * 						uval data);
 *		rdi	Thread *thread
 *		rsi 	Scheduler::ThreadFunction fct
 *		rdx	uval data
 *
 * this routine sets up a brand new thread to be ready to be
 * dispatched at a point where it will pickup from its stack the
 * function to run and arguments and jump to ThreadBase which
 * finally runs the intended function.  */
CODE_ENTRY(DispatcherDefault_InitThread)
	LEAF_ENTER()
	movq	TH_startSP(%rdi), %r8	// r8 contains new thread stack	top

	/* save parameters on the new thread stack
	 */
	movq	%rsi, -8(%r8)		// save funct
	movq	%rdx, -16(%r8)		// save data
	call	InitThread_SuspendCore
start_from_here:			// new thread will start here
	popq	%rdx			// 2nd arg data
	popq	%rsi			// 1st arg funct
	movq	%r11,%rdi		// get dispatcher (from exp reg)
	pushq	$0			// push a null frame
	jmp	C_TEXT(DispatcherDefault_ThreadBase) // "call" ThreadBase
// NOTREACHED
	int	$3

InitThread_SuspendCore:
	popq    %rsi			// get the rip we want to start from
	movq	%rsi,  -24(%r8)		// save it on the new thread stack
	lea	-24(%r8), %r8		// update new thread stack pointer
#ifndef NDEBUG
	movq	$0,  -8(%r8)		// save it on the new thread stack
	lea	-8(%r8), %r8		// update new thread base pointer
#endif
	movq	%r8, TH_curSP(%rdi)	// save it in new thread
	LEAF_RETURN()

/*
 * extern "C" void DispatcherDefault_InitThreadGeneral(
 *					Thread *thread,
 * 					Scheduler::ThreadFunctionGeneral fct,
 * 					uval len, char *data);
 *		rdi	Thread *thread
 *		rsi 	Scheduler::ThreadFunction fct
 *		rdx	uval len
 *		rcx	char *data
 */
CODE_ENTRY(DispatcherDefault_InitThreadGeneral)
	LEAF_ENTER()
	movq	TH_startSP(%rdi), %r8	// r8 contains new thread stack	top

	/* save parameters on the new thread stack
	 * skip data copy if len = 0
	 */
	cmpq	$0, %rdx
	je	no_data
	movq	%rdx, %r9		// len in r9
	addq	0x7, %r9		// round up to next uval
	shrq	$3, %r9
	shlq	$3, %r9			// r9 contains rounded up length
	subq	%r9, %r8		// buy space on stack for data
	movq	%rcx, %r9		// save source address in r9
	movq	%rdx, %rcx		// original len in rcx used to loop
InitThreadGeneral_CopyLoop:		// can go uval at a time w/o alignment XXX
	movb	0(%r9),%dl		// copy data a byte at a time
	movb	%dl, 0(%r8)
	incq	%r9			// increment source adddress
	incq	%r8			// increment target address
	loop	InitThreadGeneral_CopyLoop
	subq	%rdx, %r8		// back to beginning of copy on the new stack
no_data:
	movq	%rdx, -8(%r8)		// below copy save args len on the new thread stack
	movq	%rsi, -16(%r8)		// below copy save args fct on the new thread stack
	call	InitThread_SuspendCore1
start_from_here_also:
	movq	%rsp, %rdx		// 3rd argument will point to the copy on the stack
	addq	$16, %rdx		// adjust for len and fct to the beginning of data 
	popq	%rdi			// 1st arg funct
	popq	%rsi                    // 2nd arg len
	pushq	$0			// push a null frame
	jmp	C_TEXT(DispatcherDefault_ThreadBase) // "call" ThreadBase
// NOTREACHED
	int	$3

InitThread_SuspendCore1:
	popq	%rsi			// get the rip we want to start from
	movq	%rsi,  -24(%r8)		// save it on the new thread stack
	lea	-24(%r8), %r8		// update new thread stack pointer
	movq	%r8, TH_curSP(%rdi)	// save it in new thread
	LEAF_RETURN()

/*
 * extern "C" void
 *	DispatcherDefault_InterruptThread(Thread *thread,
 *					  Scheduler::InterruptFunction fct,
 *					  uval data);
 *		rdi	Thread *thread
 *		rsi 	Scheduler::ThreadFunction fct
 *		rdx	uval data
 *
 * this routine fiddles with the target thread to make it run a specified
 * function next time it runs. */
CODE_ENTRY(DispatcherDefault_InitThread)
	LEAF_ENTER()
	// Not yet implemented
	int	$3
	LEAF_RETURN()


/*
 * extern "C" void DispatcherDefault_Suspend(DispatcherDefault *dispatcher);
 *
 * saves all non volatile state and calls DispatcherDefault_SuspendCore leaving
 * rip on the current stack. DispatcherDefault_SuspendCore will simply save
 * the stack pointer in the thread and go do...
 */
CODE_ENTRY(DispatcherDefault_Suspend)
	FULLSAVE_FRAME_ENTER()
	call	CODE(DispatcherDefault_SuspendCore)
	FULLSAVE_FRAME_RETURN()


CODE_ENTRY(DispatcherDefault_SuspendCore)
	LEAF_ENTER()
	movq	%rdi, %r11		// move dispatcher to r11 where RunEntry needs it in ExpRegs
	GET_CURRENT_THREAD(%rcx) 	// load thead pointer in rcx
	movq	%rsp,TH_curSP(%rcx)	// save current stack pointer (pointing to rip) in thread
	DEBUG_CHK_STK_SUSPEND(%rcx, %rdx)
	jmp	CODE(RunEntryInternal)


CODE_ENTRY(DispatcherDefault_SuspendCore_Continue)
	GET_CURRENT_THREAD(%rcx); 	// load thead pointer in rcx
	movq	TH_curSP(%rcx),%rsp	// reload stack pointer
	LEAF_RETURN()

/*
 * extern "C" void DispatcherDefault_GotoRunEntry(DispatcherDefault
 * 							 *dispatcher);
 */
CODE_ENTRY(DispatcherDefault_GotoRunEntry)
/* going to SchedulerLocal_RunEntry which will call an expedient
 * routine RunEntry which ONLY uses dispatcher in r11 for amd64.  */
	movq	%rdi, %r11		// move dispatcher to r11 where RunEntry needs it in ExpRegs
	jmp	CODE(RunEntryInternal)

/*
 * extern "C" SysStatus
 *     DispatcherDefault_SetEntryPoint(EntryPointNumber entryPoint,
 *                                     EntryPointDesc entry);
 */
C_TEXT_ENTRY(DispatcherDefault_SetEntryPoint)
#ifndef NDEBUG
	pushq	%rbp
	movq	%rsp,%rbp
#endif
	int $(SYSCALL_SET_ENTRY_POINT)
#ifndef NDEBUG
	movq	%rbp,%rsp
	popq	%rbp
#endif
	ret
C_TEXT_END(DispatcherDefault_SetEntryPoint)

/*
 * extern "C" void DispatcherDefault_HandoffProcessor(CommID targetID);
 */
C_TEXT_ENTRY(DispatcherDefault_HandoffProcessor)
	FULLSAVE_FRAME_ENTER()
	// targetID stays in %rax
	call	CODE(DispatcherDefault_HandoffProcessorCore)
	FULLSAVE_FRAME_RETURN()
C_TEXT_END(DispatcherDefault_HandoffProcessor)

CODE_ENTRY(DispatcherDefault_HandoffProcessorCore)
	LEAF_ENTER()
	GET_CURRENT_THREAD(%rcx) 	// load thead pointer in rcx
	movq	%rsp,TH_curSP(%rcx)	// save current stack pointer (pointing to rip) in thread
	DEBUG_CHK_STK_SUSPEND(%rcx, %rdx)
	int	$SYSCALL_PROC_HANDOFF
	// does not return here;  current thread is on ready queue and will
	// wake up in SuspendCore_Continue.
CODE_END(DispatcherDefault_HandoffProcessorCore)

#ifndef	NDEBUG
	/*
	 * To help with debugging, we set things up to look as if the
	 * interrupted thread called this entry-point code immediately prior
	 * to its next real instruction.  On amd64 there's not much to do, and
	 * it's probably silly to worry about avoiding the extra cycle in
	 * an NDEBUG build.
	 * when rbp is the frame pointer it point to the top of the current
	 * stack, just below the saved next real instruction.
	 * We set rbp to point 8 below the saved next real instruction.
	 * check w/ Bryan XXX useless XXX
	 */
#define DEBUG_FIXUP() lea VS_r11(%rsp),%rbp
#else	// NDEBUG
#define DEBUG_FIXUP()
#endif	// NDEBUG
#define DEBUG_RESTORE()

/*
 * Push the VolatileState to which psReg points onto the stack.
 * On amd64 even non-level changing iret pop ss rsp fields.  But
 * on amd64 restoring FPU also requires a 16-byte aligned save
 * area (lest GP exception) which must be accomodated. We first
 * align the stack to a 16 byte boundary, then copy the
 * VolatileState onto the stack including rsp which may not be
 * aligned.  */
#define PUSH_VOLATILE_STATE_FRAME(psReg,scratchReg)			\
	movq	VS_ss(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_rsp(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_rflags(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_cs(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_rip(psReg),scratchReg;	pushq scratchReg;	\
									\
	movq	VS_r11(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_r10(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_r9(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_r8(psReg),scratchReg;	pushq scratchReg;	\
									\
	movq	VS_rdi(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_rsi(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_rbp(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_rdx(psReg),scratchReg;	pushq scratchReg;	\
									\
	movq	VS_rcx(psReg),scratchReg;	pushq scratchReg;	\
	movq	VS_rax(psReg),scratchReg;	pushq scratchReg;	\
									\
	leaq	VS_float_save(psReg),psReg; 				\
	COPY_FLOAT_SAVES_TO_STACK(psReg)
	

/*
 * Resume thread from the volatile-state frame on the stack.  
 * We resume directly from the stack with a non-level-changing iret.
 * Note that the same code works for level changing iret on amd64.
 * All volatiles are available at this point.
 * this code is wrong, reenabling arbitrarily here is uncorrect
 * given the Expedient code this is transferred from. XXX pdb
 * Furthermore as pointed out by Jim and Bryan after reenabling the dispatcher
 * we need to test again if there is no intervening pending interrupts
 * and take appropriate action if and when. XXX TODO 
 */

CODE_ENTRY(UserResumeFromStack)
1:	LOAD_C_DATA_ADDR(%r11,extRegsLocal)
	movq	$0, XR_disabled(%r11)	// reset disabled in the extRegs	XXX check for interrupt pending and handle XXX
	PART_VOLATILE_FRAME_RESTORE()
	popq	%r10
	popq	%r11
	iretq

/*
 * Resume thread from the user state-save area.
 */
UserResume:
	LOAD_USER_STATE_ADDR(%rsp,%r11)	// %rsp points to VS 
	jmp	UserResumeFromStack

/* ENTRY upcall, disabled, no reg, no stack
 */
CODE_ENTRY(DispatcherDefault_RunEntry)
	GET_DISPATCHER(%r11)		// set up dispatcher in %r11 where DispatcherDefaultExp_RunEntry will need it 
	LOAD_FCR(%r8)	 		// initialize to a known set floating point control
RunEntryInternal:
#ifdef USE_EXPEDIENT_SCHEDULER
	DISPATCHER_STACK(%r11)
	// this expedient code requires ExpRegs dispatcher set up (%r11 on amd64)
	// DispatcherDefaultExp_RunEntry does not return and calls/goto
	//        CallAsm(erp, DispatcherDefault_ProcessInterrupts);
	//        GotoAsm(erp, DispatcherDefault_SyscallYieldProcessor);
	//        GotoAsm(erp, DispatcherDefault_SuspendCore_Continue);
	GOTO_EXPEDIENT(DispatcherDefaultExp_RunEntry)
	int	$3

#else
#endif	// USE_EXPEDIENT_SCHEDULER

/* ENTRY upcall, hw-enabled, no reg, no stack, have swapped save area Exc/User
 * called from ExceptionLocal_KernelReflectInterrupt
 */
CODE_ENTRY(DispatcherDefault_InterruptEntry)
	GET_DISPATCHER(%r11)		// set up dispatcher in %r11 (where RunEntry may need it) 
	LOAD_FCR(%r8)	 		// initialize to a known set floating point control
	DEBUG_CHK_STK_INTERRUPT(%r11, %r8, %r9)
	DISPATCHER_STACK(%r11)
	call	CODE(DispatcherDefault_ProcessInterrupts)
	// this call does not clobber DISPATCHER (%r11)
	LOAD_USER_STATE_ADDR(%r8,%r11)	// %r8 points to VS as swapped from Exc
					// test for yield request
	movq	DD_rescheduleNeeded(%r11), %r9
	cmpq    $0, %r9
	// %r8 points to VS, %r11 points to dispatcher
	je	UserResume		// resume if no reschedule needed

	// reschedule needed
					// clear reschedule needed
	movq	$0, DD_rescheduleNeeded(%r11)
	CURTHREAD_STACK(%r8)
	PUSH_VOLATILE_STATE_FRAME(%r8,%r9)
	DEBUG_FIXUP()
	// InterruptYield(dispatcher);
	movq	%r11, %rdi	// dispatcher arg1
	call	C_TEXT(DispatcherDefault_InterruptYield)
	DEBUG_RESTORE()
	jmp	CODE(UserResumeFromStack)


/* Trap ENTRY upcall, state in trap state in dispatcher,
 * disabled, no stack, no return
 */
CODE_ENTRY(DispatcherDefault_TrapEntry)
	GET_DISPATCHER(%r11)
	LOAD_FCR(%r8)	 		// initialize to a known set floating point control
	DEBUGGER_STACK(%r11)
	CALL_EXPEDIENT(C_TEXT(GdbUserService))
	jmp UserResume			// go resume from the user state save

/* PGFLT upcall, disabled, state in user statepage + non-volatile reg
 * fault addr and info in exp reg, does not return
 * %rax contains the returned faultID
 */
CODE_ENTRY(DispatcherDefault_PgfltEntry)
	GET_DISPATCHER(%r11)
	LOAD_FCR(%r8)	 		// initialize to a known set floating point control
	LOAD_USER_STATE_ADDR(%r8,%r11)	// %r8 points to VS as swapped from Exc
	CURTHREAD_STACK(%r8)
	PUSH_VOLATILE_STATE_FRAME(%r8,%r9)
	DEBUG_FIXUP()
	// calling Expedient SchedulerExp_PgfltBlock(dispatcher, faultInfo, faultAddr, faultID);
	movq	%rax, %rcx		// arg4 = faultID
					// arg3 = faultAddr (already)
					// arg2 = faultInfo (already)
	movq	%r11, %rdi		// arg1 = dispatcher
	CALL_EXPEDIENT(DispatcherDefault_PgfltBlock)
	DEBUG_RESTORE()
	jmp	UserResumeFromStack


/* From exp::runEntry, disabled, sched stack, no reg, returns */
CODE_ENTRY(DispatcherDefault_ProcessInterrupts)
	FRAME_ENTER()
	pushq	%r11			// save dispatcher
	movq	%r11, %rdi		// dispatcher arg1
	call C_TEXT(DispatcherDefault_HandleInterrupts)
	popq	%r11			// restore dispatvher
	FRAME_RETURN()

/* From exp::runEntry, no reg, no return, disabled, sched stack */
CODE_ENTRY(DispatcherDefault_SyscallYieldProcessor)
	int	$SYSCALL_PROC_YIELD

/* From PPC_CALL macro, enabled, on thread stack, returns */
CODE_ENTRY(DispatcherDefault_PPCClient)
	GET_DISPATCHER(%r11)
#ifdef USE_EXPEDIENT_PPC
	FRAME_ENTER()
	CALL_EXPEDIENT(DispatcherDefaultExp_PPCClient)
	FRAME_RETURN()
#else	// USE_EXPEDIENT_PPC
#endif	// USE_EXPEDIENT_PPC



/* From exp::PPCClient for disabled calls, returns, no thread key, other
 * ppc reg for syscall pre-defined
 */
CODE_ENTRY(DispatcherDefault_PPCPrimitiveClientCore)
	LEAF_ENTER()
	int	$SYSCALL_PPC_PRIMITIVE
	LEAF_RETURN()



/* Called from exp::PPCClient, disabled, on thread stack, 3 ipc parms + key
 * in reg, returns in PPCClientCore_Continue, and back through ra in stack
 * frame
 */
CODE_ENTRY(DispatcherDefault_PPCClientCore)
	LEAF_ENTER()
	GET_CURRENT_THREAD(%rcx); 	// load thead pointer in rcx
PPCClientCore_Internal:
	movq	%rsp,TH_curSP(%rcx)	// save stack pointer in thread
	DEBUG_CHK_STK_SUSPEND(%rcx,%rdx)
	int	$SYSCALL_IPC_CALL
// NOTREACHED
	int	$3

/* From exp::IPCCallEntry for handling ipc returns, disabled, thread object in
 * SCHED_THREAD register, no other registers defined; returns through
 * threads stack to caller of PPCClientCore
 */
CODE_ENTRY(DispatcherDefault_PPCClientCore_Continue)
	GET_CURRENT_THREAD(%rcx); 	// load thead pointer in rcx
PPCClientCore_Continue_Internal:
	movq	TH_curSP(%rcx),%rsp	// reload  stack pointer
	LEAF_RETURN()

/* IPC Entry point for ppc calls and returns, disabled, xhandle + methodnum +
 * threadkey + callerid in exp defined reg, no return
 */
CODE_ENTRY(DispatcherDefault_IPCCallEntry)
	GET_DISPATCHER(%r11)
	LOAD_FCR(%r8)	 		// initialize to a known set floating point control
#ifdef USE_EXPEDIENT_PPC
	DISPATCHER_STACK(%r11)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCCallEntry)
// NOTREACHED
	int	$3
#else	// USE_EXPEDIENT_PPC
#endif  // USE_EXPEDIENT_PPC

/* IPC Entry point for ppc calls and returns, disabled, xhandle + methodnum +
 * threadkey + callerid in exp defined reg, no return
 */
CODE_ENTRY(DispatcherDefault_IPCReturnEntry)
	GET_DISPATCHER(%r11)
	LOAD_FCR(%r8)	 		// initialize to a known set floating point control
#ifdef USE_EXPEDIENT_PPC
	DISPATCHER_STACK(%r11)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCReturnEntry)
// NOTREACHED
	int	$3
#else	// USE_EXPEDIENT_PPC
#endif  // USE_EXPEDIENT_PPC

/* From exp::IPCCallEntry, disabled, on sched stack, ipc call handler, thread
 * for ppc in SCHED_THREAD, func + methodnum + xobj in exp reg
 */
CODE_ENTRY(DispatcherDefault_PPCServerOnThread)
	GET_CURRENT_THREAD(%rcx); 	// load thead pointer in rcx
PPCServerOnThread_Internal:
	THREAD_STACK(%rcx)
#ifdef	USE_EXPEDIENT_PPC
	GOTO_EXPEDIENT(DispatcherDefaultExp_PPCServerOnThread)
#else	// USE_EXPEDIENT_PPC
#endif	// USE_EXPEDIENT_PPC

/* From exp::ipcEntry, ipc call reg defined, enabled, on thread, returns
 * with returncode reg set, and callerid assumed to be calleeid for return
 * ipc call.
 */
CODE_ENTRY(DispatcherDefault_InvokeXObjMethod)
	FRAME_ENTER()
	pushq	%r11			// save dispatcher
	// rc = (PPC_function)(PPC_xObj, PPC_callerID);
//	move	jp, SCHED_PPC_FUNTION_V1	// for future dynlib
						// arg1 = xObj (already)
	movq	%r12, %rsi			// arg2 = callerID
	call	*%r9				// call PPC_function
	popq	%r11				// restore dispatcher
	FRAME_RETURN()

/* IPC fault Entry point for IPC target failures and refusals, all ipc
 * parameters still in defined regs, no return
 */
CODE_ENTRY(DispatcherDefault_IPCFaultEntry)
	GET_DISPATCHER(%r11)
	LOAD_FCR(%r8)	 		// initialize to a known set floating point control
	DISPATCHER_STACK(%r11)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCFaultEntry)
// NOTREACHED
	int	$3



/* From exp::IPCFaultEntry, disabled, on sched stack, thread for
 * retry in CurrentThread, all ppc parameters in exp regs
 */
CODE_ENTRY(DispatcherDefault_IPCFaultOnThread)
	GET_CURRENT_THREAD(%rcx); 	// load thead pointer in rcx
	THREAD_STACK(%rcx)
	GOTO_EXPEDIENT(DispatcherDefaultExp_IPCFaultOnThread)
// CODE_END(DispatcherDefault_IPCFaultOnThread)

/* From exp::IPCFaultOnThread, disabled, on borrowed thread stack, 3 reg
 * for ipc call set, no return.
 */
CODE_ENTRY(DispatcherDefault_SyscallIPCCall)
	int	$SYSCALL_IPC_CALL
// NOTREACHED
	int	$3


/* From exp::PPCServerOnThread or exp::IPCFaultOnThread, disabled, on borrowed
 * thread stack, 3 reg for ipc call set, no return.
 */
CODE_ENTRY(DispatcherDefault_SyscallIPCReturn)
	int	$SYSCALL_IPC_RTN
// NOTREACHED
	int	$3


/*
 * rc = DispatcherDefault_PPCCall(xhandle, methnum, targetID);
 */
C_TEXT_ENTRY(DispatcherDefault_PPCCall)
        FRAME_ENTER()
	pushq	%r12			// save r12
        // xhandle already in rdi
	movq	%rsi, %r9		// put PP_methodNUM in r9 
        movq    %rdx, %r12              // put targetID in r12
        call     CODE(DispatcherDefault_PPCClient)
	popq	%r12
        FRAME_RETURN()

/*
 * rc = DispatcherDefault_PPCAsync(xhandle, methnum, targetID);
 */
C_TEXT_ENTRY(DispatcherDefault_PPCAsync)
        LEAF_ENTER()
        // xhandle already in rdi
	movq	%rsi, %r9		// put PP_methodNUM in r9 
        movq    %rdx, %r12              // put targetID in r12
        call    L1
        LEAF_RETURN()
    L1: int $SYSCALL_IPC_ASYNC


#ifdef notdefpdb

/*
 * This version of the interrupt entry point is only used in the kernel
 * process.  It lives here, rather than in a kernel-only file, because it uses
 * a lot of the same machinery as the regular interrupt entry point.  On entry,
 * a ProcessState has been pushed on the current stack.
 */
CODE_ENTRY(SchedulerLocal_InterruptEntryInternal)
	LINK_DISPATCHER_STACK(PS_ebp)
	CALL_EXPEDIENT(SchedulerExp_InterruptEntry)
	UNLINK_DISPATCHER_STACK(PS_ebp)
	cmpl	$0,%eax			// test rc for yield request
	je	UserResumeFromStack	// resume if no yield request
	// yield requested
	DEBUG_FIXUP()
	CALL_EXPEDIENT(SchedulerExp_DisabledYield)
	DEBUG_RESTORE()
	jmp	UserResumeFromStack
#endif // notdefpdb

/* SVC Entry point for reflected svc's, all svc
 * parameters still in defined regs, no return
 */
CODE_ENTRY(DispatcherDefault_SVCEntry)
	// FIXME: Implement this
	int	$3
