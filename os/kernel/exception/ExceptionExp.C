/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionExp.C,v 1.76 2005/02/22 16:02:37 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: expedient-C implementations of ExceptionLocalAsm stuff.
 * **************************************************************************/
#include <kernIncs.H>
#include <sys/ppccore.H>
#include <misc/expedient.H>
#include <sys/Dispatcher.H>

#include "proc/Process.H"
#include "ProcessAnnex.H"
#include "ExceptionLocal.H"
#include "ExceptionExp.H"
#include "DispatcherDefaultKern.H"
#include "KernelTimer.H"
#include "trace/traceException.h"

/*
 * All the static functions in ExceptionExp are called from assembly
 * language via the C-linkage wrapper functions defined here.
 */

#define WRAPPER(cls, func) \
    extern "C" void cls##_##func(ExceptionExpRegs *erp) {cls::func(erp);}

WRAPPER(ExceptionExp, NullGenericEntryException);
WRAPPER(ExceptionExp, NullRunEntryException);
WRAPPER(ExceptionExp, NullInterruptEntryException);
WRAPPER(ExceptionExp, NullTrapEntryException);
WRAPPER(ExceptionExp, NullPgfltEntryException);
WRAPPER(ExceptionExp, NullIPCCallEntryException);
WRAPPER(ExceptionExp, NullIPCReturnEntryException);
WRAPPER(ExceptionExp, NullIPCFaultEntryException);
WRAPPER(ExceptionExp, NullSVCEntryException);

WRAPPER(ExceptionExp, AcquireReservedThread);
WRAPPER(ExceptionExp, ReleaseReservedThread);

WRAPPER(ExceptionExp, ResumeUserProcess);

WRAPPER(ExceptionExp, TrapExceptionUser);

WRAPPER(ExceptionExp, HandleUserPgflt);
WRAPPER(ExceptionExp, HandleKernelPgflt);

WRAPPER(ExceptionExp, UserInterruptContinue);
WRAPPER(ExceptionExp, KernelInterruptContinue);

WRAPPER(ExceptionExp, NonnativeSyscall);

WRAPPER(ExceptionExp, IPCCallSyscall);
WRAPPER(ExceptionExp, IPCReturnSyscall);
WRAPPER(ExceptionExp, IPCSyscallRemoteOnThread);
WRAPPER(ExceptionExp, KernelReplyRemoteOnThread);
WRAPPER(ExceptionExp, AcceptRemoteIPC);
WRAPPER(ExceptionExp, PPCPrimitiveSyscall);
WRAPPER(ExceptionExp, IPCAsyncSyscall);
WRAPPER(ExceptionExp, IPCAsyncSyscallRemoteOnThread);
WRAPPER(ExceptionExp, InvalidSyscall);

static inline void
GotoExceptionLocalAsm(ExceptionExpRegs *erp, code &func)
{
    GotoLegitimateAsm((ExpRegs *) erp, func);
}

static inline void
CallExceptionLocalAsm(ExceptionExpRegs *erp, code &func)
{
    CallLegitimateAsm((ExpRegs *) erp, func);
}

static inline void
CallExceptionLocalC(ExceptionExpRegs *erp, SysStatus rc)
{
    CallLegitimateC((ExpRegs *) erp, rc);
}

#define GOTO_EXCEPTION_LOCAL_ASM(erp, label) \
    GotoExceptionLocalAsm(erp, ExceptionLocal_##label)

#define CALL_EXCEPTION_LOCAL_ASM(erp, label) \
    CallExceptionLocalAsm(erp, ExceptionLocal_##label)

/*static*/ void
ExceptionExp::NullGenericEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null generic entry point, erp %p.\n", erp);
    breakpoint();
}

/*static*/ void
ExceptionExp::NullRunEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null RUN entry point, erp %p.\n", erp);
    breakpoint();
}

/*static*/ void
ExceptionExp::NullInterruptEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null INTERRUPT entry point, erp %p.\n", erp);
    breakpoint();
}

/*static*/ void
ExceptionExp::NullTrapEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null TRAP entry point, erp %p.\n", erp);
    breakpoint();
}

/*static*/ void
ExceptionExp::NullPgfltEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null PGFLT entry point, erp %p.\n", erp);
    breakpoint();
}

/*static*/ void
ExceptionExp::NullIPCCallEntryException(ExceptionExpRegs *erp)
{
    ProcessAnnex *source, *target;

    // curProc is the guy we were trying to launch, the IPC target.
    target = exceptionLocal.currentProcessAnnex;

    // find the sender.  It has to be in the table still.
    source = exceptionLocal.ipcTargetTable.lookupExact(erp->IPC_callerID);
    tassert(source != NULL, err_printf("Source not found.\n"));

    erp->IPC_targetID = SysTypes::WILD_COMMID(target->commID);

    // Clear at least the target that was just pushed.
    exceptionLocal.dispatchQueue.clearCDABorrowers();
    
    tassertWrn(0, "NULL IPC_CALL entry, source %lx, target %lx.\n",
	       source->commID, target->commID);

    // Switch back to the sender context.
    TraceOSExceptionIPCRefused(source->commID);
    source->switchContext();

    // Request a retry notification for the sender.
    erp->curProc = target;
    CALL_EXCEPTION_LOCAL_ASM(erp, ReqRetryNotif);

    source->dispatcher->ipcFaultReason =
				_SERROR(1721, SYSCALL_IPC_CALL, EAGAIN);

    erp->curProc = source;
    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_FAULT_ENTRY);
}

/*static*/ void
ExceptionExp::NullIPCReturnEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null IPC_RETURN entry point, erp %p.\n", erp);
    breakpoint();
}

/*static*/ void
ExceptionExp::NullIPCFaultEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null IPC_FAULT entry point, erp %p.\n", erp);
    breakpoint();
}

/*static*/ void
ExceptionExp::NullSVCEntryException(ExceptionExpRegs *erp)
{
    err_printf("Launched a null SVC entry point, erp %p.\n", erp);
    breakpoint();
}

/*
 * int disabled, sched enabled, exc stack; sets CurrentThread and erp->srcProc
 */
/*static*/ void
ExceptionExp::AcquireReservedThread(ExceptionExpRegs *erp)
{
    erp->srcProc = exceptionLocal.currentProcessAnnex;

    tassert(!erp->srcProc->isKernel,
	    err_printf("Acquiring reserved thread for kernel process!\n"));

    tassert(erp->srcProc->reservedThread == NULL,
	    err_printf("VP already has a reserved thread!\n"));

    erp->curProc = exceptionLocal.kernelProcessAnnex;
    erp->curProc->switchContextKernel();

    erp->dispatcher = (DispatcherDefaultKern *) erp->curProc->dispatcherUser;

    erp->disabledSave = extRegsLocal.disabled;
    extRegsLocal.disabled = 0;
    extRegsLocal.dispatcher = erp->dispatcher;

    CurrentThread = erp->dispatcher->freeList;
    if (CurrentThread != NULL) {
	erp->dispatcher->freeList = CurrentThread->next;
    } else {
	CALL_EXCEPTION_LOCAL_ASM(erp, CreateThread);
	passert(CurrentThread != NULL, err_printf("CreateThread failed\n"));
    }

    erp->srcProc->reservedThread = CurrentThread;
}

/* Int disabled, sched enabled, exp->curproc has proc to switch back to */
/*static*/ void
ExceptionExp::ReleaseReservedThread(ExceptionExpRegs *erp)
{
    erp->curProc->reservedThread = NULL;

    erp->dispatcher = DISPATCHER_KERN;
    erp->dispatcher->exceptionFreeThread(CurrentThread);

    // return to interrupted process
    extRegsLocal.dispatcher = erp->curProc->dispatcherUser;
    erp->curProc->switchContextUser();
}

/* From HandlerUserPgflt and UserInterruptContinue, int disabed, on borrowed
 * kernel thread stack, no return, user AS is current.
 * Registers:
 *     erp->curProc:        exceptionLocal.currentProcessAnnex
 *     erp->curDispatcher:  erp->curProc->dispatcher
 */
/*static*/ void
ExceptionExp::ResumeUserProcess(ExceptionExpRegs *erp)
{
    if (erp->curDispatcher->interrupts.pending()) {
	if (!extRegsLocal.disabled) {
	    erp->curProc->swapUserAndExcStates();
	    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_INTERRUPT_ENTRY);
	}
    }

    erp->volatileState = erp->curProc->excStatePtr();
    GOTO_EXCEPTION_LOCAL_ASM(erp, UserResume);
}

/*static*/ void
ExceptionExp::TrapExceptionUser(ExceptionExpRegs *erp)
{
    erp->curProc = exceptionLocal.currentProcessAnnex;

    erp->curProc->dispatcher->trapDisabledSave = extRegsLocal.disabled;
    erp->curProc->swapTrapAndExcStates();

    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_TRAP_ENTRY);
}

/* From local::HandleUserPgflt, int disabled, on kernel thread, state in PS,
 * faultaddr/info + srcproc in erp reg
 */
/*static*/ void
ExceptionExp::HandleUserPgflt(ExceptionExpRegs *erp)
{
    uval disabledSave = erp->disabledSave;	// preserve disabledSave
    ProcessAnnex *srcProc = erp->srcProc;	// preserve srcProc
    uval faultInfo = erp->Pgflt_faultInfo;	// preserve faultInfo
    uval faultAddr = erp->Pgflt_faultAddr;	// preserve faultAddr

    exceptionLocal.dispatchQueue.
	pushCDABorrower(exceptionLocal.kernelProcessAnnex);

    /*
     * User page fault reflection is allowed if and only if the process
     * was not disabled when the fault occurred.
     */
    erp->Pgflt_noReflection = erp->disabledSave;

    CallExceptionLocalC(erp,
	ExceptionLocal_PgfltHandler(erp->srcProc,
				    erp->Pgflt_faultInfo,
				    erp->Pgflt_faultAddr,
				    erp->Pgflt_noReflection));

    erp->curProc = exceptionLocal.currentProcessAnnex;
    if (erp->curProc->dispatcher->hasWork && !erp->curProc->isReady()) {
	CALL_EXCEPTION_LOCAL_ASM(erp, MakeCurProcReady);
    }

    erp->curProc = srcProc;
    exceptionLocal.dispatchQueue.popCDABorrower();
    if (exceptionLocal.dispatchQueue.currentCDABorrower() != erp->curProc) {
	goto Reschedule;
    }

MainPath:
    CALL_EXCEPTION_LOCAL_ASM(erp, ReleaseReservedThread);

    if (erp->returnCode == 0) {
	// in-core or disabled page fault:  resume process
	extRegsLocal.disabled = disabledSave;
	erp->curDispatcher = erp->curProc->dispatcher;
	GOTO_EXCEPTION_LOCAL_ASM(erp, ResumeUserProcess);
    } else if (erp->returnCode > 0) {
	// I/O page fault, enabled:  reflect page fault
	erp->Pgflt_faultAddr = faultAddr;	// restore faultAddr
	erp->Pgflt_faultInfo = faultInfo;	// restore faultInfo
	erp->curProc->swapUserAndExcStates();
	GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_PGFLT_ENTRY);
    } else {
	// bad address page fault:  convert to trap
	erp->Trap_trapNumber = erp->PgfltTrapNumber();
	erp->Trap_trapInfo = faultInfo;
	erp->Trap_trapAuxInfo = faultAddr;
	erp->curProc->dispatcher->trapDisabledSave = disabledSave;
	erp->curProc->swapTrapAndExcStates();
	GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_TRAP_ENTRY);
    }
    // NOTREACHED

Reschedule:
    exceptionLocal.dispatchQueue.clearCDABorrowers();
    SysStatus returnCode = erp->returnCode;	// preserve returnCode
    // AwaitDispatch() is a void function, so give it a dummy return value.
    CallExceptionLocalC(erp, (ExceptionLocal_AwaitDispatch(erp->curProc),0));
    erp->returnCode = returnCode;		// restore returnCode
    erp->curProc = srcProc;			// re-fetch curProc
    goto MainPath;
}

/* From local::pgfltExceptionKernel, int disabled, state in PS on stack,
 * on faulting kernel thread's stack, no return, erp has faultinfo/addr
 */
/*static*/ void
ExceptionExp::HandleKernelPgflt(ExceptionExpRegs *erp)
{
    tassertSilent(DISPATCHER_KERN ==
		    exceptionLocal.kernelProcessAnnex->dispatcher,
		  BREAKPOINT);	// can't print if running with
				// non-kernel dispatcher
    tassertMsg(!extRegsLocal.disabled,
	       "Kernel page fault (address %lx) while disabled.\n",
	       erp->Pgflt_faultAddr);

    uval faultInfo = erp->Pgflt_faultInfo;	// preserve faultInfo
    uval faultAddr = erp->Pgflt_faultAddr;	// preserve faultAddr

    /*
     * Kernel page fault reflection is never allowed.
     */
    erp->Pgflt_noReflection = 1;

    erp->srcProc = exceptionLocal.kernelProcessAnnex;

    CallExceptionLocalC(erp,
	ExceptionLocal_PgfltHandler(erp->srcProc,
				    erp->Pgflt_faultInfo,
				    erp->Pgflt_faultAddr,
				    erp->Pgflt_noReflection));

    if (erp->returnCode == 0) {
	GOTO_EXCEPTION_LOCAL_ASM(erp, KernelPgfltResume);
    } else {
	// bad address page fault:  convert to trap
	erp->Trap_trapNumber = erp->PgfltTrapNumber();
	erp->Trap_trapInfo = faultInfo;
	erp->Trap_trapAuxInfo = faultAddr;

	GOTO_EXCEPTION_LOCAL_ASM(erp, KernelPgfltToTrap);
    }
}

/*static*/ void
ExceptionExp::UserInterruptContinue(ExceptionExpRegs *erp)
{
    if (exceptionLocal.dispatchQueue.cdaBorrowersEmpty()) {
	GOTO_EXCEPTION_LOCAL_ASM(erp, UserInterruptAwaitDispatch);
    }

    erp->curProc = exceptionLocal.currentProcessAnnex;
    erp->curDispatcher = erp->curProc->dispatcher;
    GOTO_EXCEPTION_LOCAL_ASM(erp, ResumeUserProcess);
}

/*static*/ void
ExceptionExp::KernelInterruptContinue(ExceptionExpRegs *erp)
{
    erp->curProc = exceptionLocal.currentProcessAnnex;
    if (erp->curProc != exceptionLocal.kernelProcessAnnex) {
	// currentProcessAnnex must be idle loop.  Abandon it without saving
	// state.
	GOTO_EXCEPTION_LOCAL_ASM(erp, IdleLoopYield);
    } else {
	if (exceptionLocal.dispatchQueue.cdaBorrowersEmpty()) {
	    erp->curProc->deliverInterrupt(SoftIntr::PREEMPT);
	}
	erp->curDispatcher = erp->curProc->dispatcher;
	if (erp->curDispatcher->interrupts.pending() &&
					!extRegsLocal.disabled) {
	    extRegsLocal.disabled = 1;
	    erp->curProc->swapUserAndExcStates();
	    GOTO_EXCEPTION_LOCAL_ASM(erp, KernelReflectInterrupt);
	} else {
	    erp->volatileState = erp->curProc->excStatePtr();
	    GOTO_EXCEPTION_LOCAL_ASM(erp, KernelInterruptResume);
	}
    }
}

/* From local::NonnativeSyscall, on exc stack, int disabled, original syscall
 * args in erp registers, no return.
 */
/*static*/ void
ExceptionExp::NonnativeSyscall(ExceptionExpRegs *erp)
{
    erp->curProc = exceptionLocal.currentProcessAnnex;
    erp->curProc->swapUserAndExcStates();
    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_SVC_ENTRY);
}

/* From local::IPCCallSyscall, on exc stack, int disabled, original ipc args in
 * erp registers, no return.
 */
/*static*/ void
ExceptionExp::IPCCallSyscall(ExceptionExpRegs *erp)
{
    ProcessAnnex *source = exceptionLocal.currentProcessAnnex;

    if (source->dispatcher->hasWork && !source->isReady()) {
	CALL_EXCEPTION_LOCAL_ASM(erp, MakeCurProcReady);
    }

    erp->curProc = exceptionLocal.ipcTargetTable.lookupWild(erp->IPC_targetID);

    if (erp->curProc == NULL) goto TargetNotFound;

    if (erp->curProc->reservedThread != NULL) goto TargetDisabled;

    exceptionLocal.dispatchQueue.pushCDABorrower(erp->curProc);

    erp->IPC_callerID = source->commID;

    TraceOSExceptionPPCCall(erp->curProc->commID);
    erp->curProc->switchContext();

    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_CALL_ENTRY);

TargetDisabled:
    CALL_EXCEPTION_LOCAL_ASM(erp, ReqRetryNotif);
    erp->curProc = exceptionLocal.currentProcessAnnex;	// re-fetch source
    erp->curProc->dispatcher->ipcFaultReason =
				_SERROR(1561, SYSCALL_IPC_CALL, EAGAIN);
    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_FAULT_ENTRY);

TargetNotFound:
    /*
     * This IPC call can't be delivered locally, so we have to go remote.
     * To do that we have to get out of exception level and up on the
     * kernel thread reserved for this vp.  ALL registers in erp are in
     * use at this point, so we can't legitimately do the thread
     * allocation and switching here.  (AcquireReservedThread()
     * assumes certain registers are available.)  Instead we bail out back
     * to assembly language where we can start over.
     */
    erp->IPC_ipcType = SYSCALL_IPC_CALL;
    GOTO_EXCEPTION_LOCAL_ASM(erp, IPCSyscallRemote);
}

/* From local::IPCReturnSyscall, on exc stack, int disabled, original ipc args
 * in erp registers, no return.
 */
/*static*/ void
ExceptionExp::IPCReturnSyscall(ExceptionExpRegs *erp)
{
    ProcessAnnex *source = exceptionLocal.currentProcessAnnex;

    if (source->dispatcher->hasWork && !source->isReady()) {
	CALL_EXCEPTION_LOCAL_ASM(erp, MakeCurProcReady);
    }

    exceptionLocal.dispatchQueue.popCDABorrower();
    erp->curProc = exceptionLocal.dispatchQueue.currentCDABorrower();
    if ((erp->curProc == NULL) ||
	    (erp->curProc->commID != erp->IPC_targetID)) {
	goto SlowPath;
    }

FastPath:
    if (erp->curProc->reservedThread != NULL) goto CheckPrimitivePPC;

    erp->IPC_callerID = source->commID;

    TraceOSExceptionPPCReturn(erp->curProc->commID);
    erp->curProc->switchContext();

    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_RTN_ENTRY);

SlowPath:
    exceptionLocal.dispatchQueue.clearCDABorrowers();
    erp->curProc = exceptionLocal.ipcTargetTable.lookupExact(erp->IPC_targetID);

    if (erp->curProc == NULL) goto TargetNotFound;

    // If this is a return TO the kernel process, we just do it.  We can't
    // use a reserved thread for the kernel process.
    if (erp->curProc->isKernel) goto FastPath;

    if (erp->curProc->reservedThread != NULL) goto CheckPrimitivePPC;

    erp->IPC_callerID = source->commID;
    TraceOSExceptionPPCReturn(erp->curProc->commID);
    erp->curProc->switchContext();
    GOTO_EXCEPTION_LOCAL_ASM(erp, AwaitDispatch_Launch_IPC_RTN_ENTRY);

CheckPrimitivePPC:
    if ((erp->curProc->reservedThread != PPC_PRIMITIVE_MARKER) ||
	    (source->commID != erp->curProc->ppcTargetID) ||
		(erp->IPC_threadID != erp->curProc->ppcThreadID)) {
	goto TargetDisabled;
    }
    erp->IPC_callerID = source->commID;
    erp->curProc->reservedThread = NULL;
    TraceOSExceptionPPCReturn(erp->curProc->commID);
    erp->curProc->switchContext();
    extRegsLocal.dispatcher = erp->curProc->dispatcherUser;
    extRegsLocal.disabled = 1;	// in case sender didn't disable
    if (exceptionLocal.dispatchQueue.currentCDABorrower() == NULL) {
	GOTO_EXCEPTION_LOCAL_ASM(erp, AwaitDispatch_PPCPrimitiveResume);
    }
    GOTO_EXCEPTION_LOCAL_ASM(erp, PPCPrimitiveResume);

TargetDisabled:
    CALL_EXCEPTION_LOCAL_ASM(erp, ReqRetryNotif);
    erp->curProc = exceptionLocal.currentProcessAnnex;	// re-fetch source
    erp->curProc->dispatcher->ipcFaultReason =
				_SERROR(1564, SYSCALL_IPC_RTN, EAGAIN);
    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_FAULT_ENTRY);

TargetNotFound:
    if (source->isKernel) goto KernelReplyRemote;

    /*
     * This IPC return can't be delivered locally, so we have to go remote.
     * To do that we have to get out of exception level and up on the
     * kernel thread reserved for this vp.  ALL registers in erp are in
     * use at this point, so we can't legitimately do the thread
     * allocation and switching here.  (AcquireReservedThread()
     * assumes certain registers are available.)  Instead we bail out back
     * to assembly language where we can start over.
     */
    erp->IPC_ipcType = SYSCALL_IPC_RTN;
    GOTO_EXCEPTION_LOCAL_ASM(erp, IPCSyscallRemote);

KernelReplyRemote:
    /*
     * The kernel is doing a non-local reply (either because the target has
     * migrated or (more likely) because the target has terminated.  We can't
     * use the normal remote IPC path because we can't acquire a reserved
     * thread for the kernel.  However, we can re-use the thread that just
     * generated the reply.  It's guaranteed to be on the free list at this
     * point.
     */
    erp->curProc = source;
    erp->dispatcher = (DispatcherDefaultKern *) erp->curProc->dispatcher;
    CurrentThread = erp->dispatcher->freeList;
    tassertMsg(CurrentThread != NULL, "Replying thread not on free list.\n");
    erp->dispatcher->freeList = CurrentThread->next;
    GOTO_EXCEPTION_LOCAL_ASM(erp, KernelReplyRemote);
}

/*static*/ void
ExceptionExp::IPCSyscallRemoteOnThread(ExceptionExpRegs *erp)
{
    IPCRegsArch ipcRegs;
    ProcessAnnex *curProcSave;

    erp->saveIPCRegs(&ipcRegs);

    curProcSave = erp->curProc;

    CallExceptionLocalC(erp,
	ExceptionLocal_IPCRemote(&ipcRegs, erp->IPC_targetID,
				 erp->IPC_ipcType, erp->curProc));

    erp->curProc = curProcSave;

    CALL_EXCEPTION_LOCAL_ASM(erp, ReleaseReservedThread);

    if (_SUCCESS(erp->returnCode)) {
	// Nothing else to do.  Return to curProc at its RUN entry point.
	GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_RUN_ENTRY);
    }

    erp->curProc->dispatcher->ipcFaultReason = erp->returnCode;
    erp->restoreIPCRegs(&ipcRegs);

    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_FAULT_ENTRY);
}

/*static*/ void
ExceptionExp::KernelReplyRemoteOnThread(ExceptionExpRegs *erp)
{
    IPCRegsArch ipcRegs;

    erp->saveIPCRegs(&ipcRegs);

    CallExceptionLocalC(erp,
	ExceptionLocal_IPCRemote(&ipcRegs, erp->IPC_targetID,
				 SYSCALL_IPC_RTN, erp->curProc));

    // We don't care whether the reply succeeded or failed.  Free the PPC
    // page in case it failed.
    RESET_PPC();
    enableHardwareInterrupts();
    Scheduler::Exit();
    // NOTREACHED
}

/* From local::AcceptRemoteIPC, curProc holds current ProcessAnnex, ipcBuf
 * holds IPC details, on exc stack, int disabled, no return.
 */
/*static*/ void
ExceptionExp::AcceptRemoteIPC(ExceptionExpRegs *erp)
{
    // PPC page has already been filled in.
    // callerID is already set.

    erp->restoreIPCRegs(erp->IPC_ipcRegsP);

    if (erp->IPC_ipcType == SYSCALL_IPC_CALL) {
	GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_CALL_ENTRY);
    } else {
	GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_RTN_ENTRY);
    }
}

/* From local::PPCPRimitiveSyscall, erp has PPC parms + resume info, on
 * exc stack, int disabled, no return.
 */
/*static*/ void
ExceptionExp::PPCPrimitiveSyscall(ExceptionExpRegs *erp)
{
    ProcessAnnex *source = erp->curProc;

    erp->curProc = exceptionLocal.ipcTargetTable.lookupWild(erp->IPC_targetID);
    
    if ((erp->curProc == NULL) || (erp->curProc == source)) {
	tassertWrn(0, "PPCPrimitive target not found or is self, "
					    "source 0x%lx, target 0x%lx.\n",
		    source->commID, erp->IPC_targetID);
	erp->returnCode = _SERROR(1566, 0, EINVAL);
	erp->curProc = source;
	GOTO_EXCEPTION_LOCAL_ASM(erp, PPCPrimitiveResume);
    }
    
    if (erp->curProc->reservedThread != NULL) {
	/*
	 * Target is disabled.  We can't reflect a primitive PPC back to the
	 * sender, so we have to delay and retry in the kernel. To do that we
	 * have to get out of exception level and up on the kernel thread
	 * reserved for this vp.  ALL registers in erp are in use at this
	 * point, so we can't legitimately do the thread allocation and
	 * switching here.  (AcquireReservedThread() assumes certain
	 * registers are available.)  Instead we bail out back to assembly
	 * language where we can start over.
	 */
	tassertWrn(0,
		   "Primitive PPC from %lx to disabled target %lx.\n",
		   source->commID, erp->curProc->commID);
	GOTO_EXCEPTION_LOCAL_ASM(erp, PPCPrimitiveSyscallRetry);
    }

    source->reservedThread = PPC_PRIMITIVE_MARKER;
    source->ppcTargetID = erp->curProc->commID;
    source->ppcThreadID = erp->IPC_threadID;

    exceptionLocal.dispatchQueue.pushCDABorrower(erp->curProc);

    erp->IPC_callerID = source->commID;

    TraceOSExceptionPPCCall(erp->curProc->commID);
    erp->curProc->switchContext();

    GOTO_EXCEPTION_LOCAL_ASM(erp, Launch_IPC_CALL_ENTRY);
}

/* From local::IPCAsyncSyscall, ppc parms + resumeinfo in erp, on exc stack,
 * int disabled, returns.
 */
/*static*/ void
ExceptionExp::IPCAsyncSyscall(ExceptionExpRegs *erp)
{
    ProcessAnnex *source = exceptionLocal.currentProcessAnnex;

    ProcessAnnex *target = exceptionLocal.ipcTargetTable.
	lookupWild(erp->IPCAsync_targetID);

    if (target == NULL) {
	/*
	 * This async IPC can't be delivered locally, so we have to go remote.
	 * To do that we have to get out of exception level and up on the
	 * kernel thread reserved for this vp.  ALL registers in erp are in
	 * use at this point, so we can't legitimately do the thread
	 * allocation and switching here.  (AcquireReservedThread()
	 * assumes certain registers are available.)  Instead we bail out back
	 * to assembly language where we can start over.
	 */
	GOTO_EXCEPTION_LOCAL_ASM(erp, IPCAsyncSyscallRemote);
    }

    erp->returnCode =
	target->dispatcher->asyncBufferLocal.storeMsg(source->commID,
						      erp->IPC_xHandle,
						      erp->IPC_methodNum,
						      GET_PPC_LENGTH(),
						      PPCPAGE_DATA);
    TraceOSExceptionPPCAsyncLocal(
		    target->commID, erp->returnCode);
    RESET_PPC_LENGTH();
    if (_SUCCESS(erp->returnCode)) {
	target->deliverInterrupt(SoftIntr::ASYNC_MSG);
    }
}

/*static*/ void
ExceptionExp::IPCAsyncSyscallRemoteOnThread(ExceptionExpRegs *erp)
{
    uval const length = GET_PPC_LENGTH();
    uval buf[AsyncBuffer::MAX_LENGTH];
    memcpy(buf, PPCPAGE_DATA, MIN(length, sizeof(buf)));
    RESET_PPC_LENGTH();			// done with PPC_PAGE

    ProcessAnnex *curProcSave = erp->curProc;

    CallExceptionLocalC(erp,
	ExceptionLocal_PPCAsyncRemote(erp->curProc,
				      erp->IPCAsync_targetID,
				      erp->IPC_xHandle,
				      erp->IPC_methodNum,
				      length, buf));

    erp->curProc = curProcSave;

    CALL_EXCEPTION_LOCAL_ASM(erp, ReleaseReservedThread);

    extRegsLocal.disabled = 0;
    if (erp->curProc->dispatcher->interrupts.pending()) {
	GOTO_EXCEPTION_LOCAL_ASM(erp, IPCAsyncSyscallInterruptReturn);
    }
}

/*static*/ void
ExceptionExp::InvalidSyscall(ExceptionExpRegs *erp)
{
    err_printf("Invalid syscall, erp = %p\n", erp);
    breakpoint();
}
