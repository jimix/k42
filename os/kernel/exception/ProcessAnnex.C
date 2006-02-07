/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessAnnex.C,v 1.44 2005/07/18 21:49:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the exception-level kernel structure that
 *                     represents a virtual processor.
 * **************************************************************************/

#include "kernIncs.H"
#include "proc/Process.H"
#include "ExceptionLocal.H"
#include "DispatcherDefaultKern.H"
#include "ProcessAnnex.H"
#include "mem/FCM.H"

void
ProcessAnnex::init(ProcessRef pref, ProcessID pid,
		   uval userModeArg, uval isKernelArg,
		   Dispatcher *dspUser, Dispatcher *dsp,
		   FCMRef dspFCMRef, uval dspOffset,
	           SegmentTable *segTable, DispatcherID dspid)
{
    machine.init(userModeArg, dsp, segTable);
    cpuDomainNext = NULL;
    ipcTargetNext = NULL;
    userMode = userModeArg;
    isKernel = isKernelArg;
    processRef = pref;
    commID = SysTypes::COMMID(pid, dspid);
    dispatcherUser = dspUser;
    dispatcher = dsp;
    segmentTable = segTable;
    excStateOffset = OFFSETOF(Dispatcher,_state[0]);
    trapStateOffset = OFFSETOF(Dispatcher,_state[1]);
    userStateOffset = OFFSETOF(Dispatcher,_state[2]);
    machine.setExcStateOffset(excStateOffset);
    dispatcher->_trapStateOffset = trapStateOffset;
    dispatcher->_userStateOffset = userStateOffset;
    reservedThread = NULL;
    awaitingDispatch = 0;
    terminator = Scheduler::NullThreadID;
    cpuDomainAnnex = NULL;
    dispatchTicks = 0;
    pulseTicks = 0;
    warningTicks = SysTime(-1);
    ppcTargetID = 0;
    ppcThreadID = 0;
    pp = NO_PHYS_PROC;
    ipcRetryTargets = 0;
    ipcRetrySources = 0;
    ipcDeferredRetrySources = 0;
    ipcRetryNext = NULL;
    ipcCallsEnabled = 0;
    pendingRemoteIPC = NULL;
    EntryPointDesc entry;
    entry.nullify();
    for (uval i = 0; i < NUM_ENTRY_POINTS; i++) {
	setEntryPoint((EntryPointNumber) i, entry);
    }
    dispatcherFCMRef = dspFCMRef;
    dispatcherOffset = dspOffset;
    fnMgr.init();
}

void
ProcessAnnex::attach(CPUDomainAnnex *cda)
{
    tassertMsg(cpuDomainAnnex == NULL, "ProcessAnnex already attached.\n");
    tassertMsg(cpuDomainNext == NULL, "ProcessAnnex already runnable.\n");
    tassertMsg(pp == NO_PHYS_PROC, "Wrong PP.\n");
    cpuDomainAnnex = cda;
    pp = cda->getPP();
    makeReady();
}

void
ProcessAnnex::detach()
{
    tassertMsg(cpuDomainAnnex != NULL, "ProcessAnnex not attached.\n");
    tassertMsg(pp != NO_PHYS_PROC, "Wrong PP.\n");
    makeNotReady();
    pp = NO_PHYS_PROC;
    cpuDomainAnnex = NULL;
}

void
ProcessAnnex::setEntryPoint(EntryPointNumber entryPoint, EntryPointDesc entry)
{
    if (!entry.isNull()) {
	launcher[entryPoint].set(userMode, entry);
	if (entryPoint == IPC_CALL_ENTRY) {
	    /*
	     * We have to keep explicit track of the status of the IPC_CALL
	     * entry point because the null-entry-point handler for IPC_CALLs
	     * can't deal with an IPC that arrived from a remote source.  See
	     * ExceptionLocal::AcceptRemoteIPC().
	     */
	    ipcCallsEnabled = 1;
	    /*
	     * Now that the IPC_CALL entry point is enabled, we need to trigger
	     * notifications for any IPCs that have been refused and we need to
	     * deliver any pending remote IPCs.  In either case we request a
	     * soft preempt, which will get the ball rolling.
	     */
	    if ((ipcDeferredRetrySources != 0) ||
		((pendingRemoteIPC != NULL) &&
		 (pendingRemoteIPC->ipcType == SYSCALL_IPC_CALL)))
	    {
		ipcRetrySources |= ipcDeferredRetrySources;
		ipcDeferredRetrySources = 0;
		deliverInterrupt(SoftIntr::PREEMPT);
	    }
	}
    } else {
	codeAddress handler;
	switch (entryPoint) {
	case RUN_ENTRY:
	    handler = &ExceptionLocal_NullRunEntryException;
	    break;
	case INTERRUPT_ENTRY:
	    handler = &ExceptionLocal_NullInterruptEntryException;
	    break;
	case TRAP_ENTRY:
	    handler = &ExceptionLocal_NullTrapEntryException;
	    break;
	case PGFLT_ENTRY:
	    handler = &ExceptionLocal_NullPgfltEntryException;
	    break;
	case IPC_CALL_ENTRY:
	    ipcCallsEnabled = 0;
	    handler = &ExceptionLocal_NullIPCCallEntryException;
	    break;
	case IPC_RTN_ENTRY:
	    handler = &ExceptionLocal_NullIPCReturnEntryException;
	    break;
	case IPC_FAULT_ENTRY:
	    handler = &ExceptionLocal_NullIPCFaultEntryException;
	    break;
	case SVC_ENTRY:
	    handler = &ExceptionLocal_NullSVCEntryException;
	    break;
	default:
	    handler = &ExceptionLocal_NullGenericEntryException;
	    break;
	}
	launcher[entryPoint].setException(handler);
    }
}

void
ProcessAnnex::switchProcess()
{
    exceptionLocal.currentProcessAnnex = this;
}

void
ProcessAnnex::waitForTerminate()
{
    tassert(!hardwareInterruptsEnabled(), err_printf("Enabled!\n"));
    if (reservedThread != NULL) {
	if (reservedThread == PPC_PRIMITIVE_MARKER) {
	    // Target is blocked in a primitive PPC.  No need to wait.
	    reservedThread = NULL;
	} else {
	    // Make sure reservedThread reschedules on the way back
	    // to its client.
	    exceptionLocal.dispatchQueue.clearCDABorrowers();

	    if (awaitingDispatch) {
		signalDispatch();
	    }

	    terminator = Scheduler::GetCurThread();
	    do {
		enableHardwareInterrupts();
		Scheduler::Block();
		disableHardwareInterrupts();
	    } while (terminator != Scheduler::NullThreadID);
	    tassert(reservedThread == NULL,
		    err_printf("Non-NULL reservedThread pointer\n"));
	}
    }
}

void
ProcessAnnex::waitForDispatch()
{
    tassert(!hardwareInterruptsEnabled(), err_printf("Enabled!\n"));
    tassert(reservedThread == Scheduler::GetCurThreadPtr(),
	    err_printf("Wrong thread!\n"));
    if (terminator == Scheduler::NullThreadID) {
	awaitingDispatch = 1;
	enableHardwareInterrupts();
	PRESERVE_PPC_PAGE();
	do {
	    Scheduler::Block();
	} while (awaitingDispatch);
	RESTORE_PPC_PAGE();
	disableHardwareInterrupts();
    }
}

void
ProcessAnnex::resumeTermination()
{
    tassert(!hardwareInterruptsEnabled(), err_printf("Enabled!\n"));
    tassert(reservedThread == Scheduler::GetCurThreadPtr(),
	    err_printf("Wrong thread!\n"));
    ThreadID const t = terminator;
    terminator = Scheduler::NullThreadID;
    reservedThread = NULL;
    enableHardwareInterrupts();
    RESET_PPC();
    Scheduler::Disable();
    Scheduler::DisabledUnblockOnThisDispatcher(t);
    Scheduler::DisabledExit();
    // NOTREACHED
}

void
ProcessAnnex::signalDispatch()
{
    tassert(!hardwareInterruptsEnabled(), err_printf("Enabled!\n"));
    /*
     * This routine is called at exception level, when kernelInfoLocal may
     * hold a pointer to some non-kernel process's dispatcher.  Therefore
     * we can't access the kernel dispatcher in the normal manner.
     */
    DispatcherDefaultKern *const disp =
	(DispatcherDefaultKern*) exceptionLocal.kernelProcessAnnex->dispatcher;
    awaitingDispatch = 0;
    disp->exceptionUnblock(reservedThread);
}

struct ProcessAnnex::MakeReadyMsg : MPMsgMgr::MsgAsync {
    CommID commID;

    virtual void handle() {
	ExceptionLocal::MakeReady(commID);
	free();
    }
};

void
ProcessAnnex::sendInterrupt(SoftIntr::IntrType intr)
{
    SysStatus rc;
    SoftIntr priorInts;
    VPNum targetPP;

    /*
     * The interrupt bit we're about to set may be a signal to the target
     * dispatcher telling it to look at some secondary data structure (page
     * fault completion vector, async IPC buffer, etc.).  We need to make sure
     * that changes to those secondary structures are committed before the
     * interrupt bit is set.
     */
    SyncBeforeRelease();
    priorInts = dispatcher->interrupts.fetchAndSet(intr);

    if (priorInts.pending()) {
	/*
	 * Dispatcher already had pending interrupts.  Someone else will have
	 * made (or will be making) it runnable.
	 */
	return;
    }

    /*
     * Dispatcher had no prior pending interrupts.  We're responsible for
     * making it runnable.  It may or may not be on this processor, and there's
     * nothing to keep it from migrating even as we look at it.  We pick up its
     * current physical processor and try to make it runnable there.  If it
     * moves, we can forget it because the migration machinery will make it
     * runnable on its new processor.  However, we have to make sure the
     * physical processor we pick up isn't stale with respect to the interrupt
     * bit we just set.  The scenario we have to guard against is our seeing an
     * old physical-processor value _and_ the dispatcher's new processor seeing
     * an old interrupts-vector value.
     */
    SyncBeforeRelease();	// FIXME: is this the proper sync?
    targetPP = VPNum(FetchAndNop(&pp));

    if (targetPP == Scheduler::GetVP()) {
	/*
	 * The dispatcher is local (or at least it was a moment ago).  Drop to
	 * exception level and make it runnable.
	 */
	InterruptState is;
	disableHardwareInterrupts(is);
	/*
	 * Make sure the dispatcher is still local.  If it is, it will not
	 * move now that interrupts are disabled.  If it's not, we can forget
	 * it.  Its new processor will make it runnable.
	 */
	if (VPNum(FetchAndNop(&pp)) == targetPP) {
	    makeReady();
	}
	enableHardwareInterrupts(is);
    } else if (targetPP == NO_PHYS_PROC) {
	/*
	 * The dispatcher is not attached to a physical processor.  It will be
	 * made runnable when it is attached to a new processor.  We needn't
	 * do anything here.
	 */
    } else {
	/*
	 * The dispatcher is remote.  Send a message to its processor to make
	 * it runnable.  We don't care whether it is still there when the
	 * message arrives.
	 */
	MakeReadyMsg *const msg =
	    new(exceptionLocal.getExceptionMsgMgr()) MakeReadyMsg;
	msg->commID = commID;
	rc = msg->send(SysTypes::DSPID(0, targetPP));
	tassertMsg(_SUCCESS(rc), "MakeReadyMsg send failed.\n");
    }
}

void
ProcessAnnex::notifyFaultCompletion(PageFaultNotification *pn)
{
    uval const id = pn->getPageFaultId();
    (void) FetchAndOr64(&dispatcher->pgfltCompleted[id/64],
			uval64(1) << (id%64));
    sendInterrupt(SoftIntr::PGFLT_COMPLETION);
    fnMgr.free(pn);
}

void
ProcessAnnex::releaseDispatcherMemory()
{
    if (exceptionLocal.realModeMemMgr != NULL) {
	uval addr;
	(void) DREF(dispatcherFCMRef)->removeEstablishedPage(dispatcherOffset,
							     PAGE_SIZE, addr);
	exceptionLocal.realModeMemMgr->dealloc(addr, PAGE_SIZE);
    } else {
	(void) DREF(dispatcherFCMRef)->disEstablishPage(dispatcherOffset,
							PAGE_SIZE);
    }
    (void) DREF(dispatcherFCMRef)->removeReference();
}

void
ProcessAnnex::makeReady()
{
    tassert(!hardwareInterruptsEnabled(),err_printf("Enabled!\n"));

    if (cpuDomainNext == NULL) {
	cpuDomainAnnex->addProcessAnnex(this);
    }
}

void
ProcessAnnex::makeNotReady()
{
    tassert(!hardwareInterruptsEnabled(),err_printf("Enabled!\n"));

    if (cpuDomainNext != NULL) {
	cpuDomainAnnex->removeProcessAnnex(this);
    }
}
