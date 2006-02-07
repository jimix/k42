/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocal.C,v 1.91 2004/11/18 19:40:14 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic C implementations of exception-level functions.
 * **************************************************************************/

#include "kernIncs.H"
#include "ExceptionLocal.H"
#include "proc/Process.H"
#include <sys/ProcessSet.H>
#include "ProcessAnnex.H"
#include "DispatcherDefaultKern.H"
#include "trace/traceException.h"
#include "mem/FCM.H"

#include <defines/pgflt_stats.H>
#include <mem/PerfStats.H>

RemoteIPCBuffer **ExceptionLocal::OldRemoteIPCBuffers = NULL;

void
ExceptionLocal::init(VPNum vpnum, MemoryMgrPrimitiveKern *memory)
{
    uval space;

    vp = vpnum;

    machineInit(vp, memory);

    enum {EXCEPTION_STACK_SIZE = (1 * PAGE_SIZE)};
    memory->alloc(space, EXCEPTION_STACK_SIZE, PAGE_SIZE);
    exceptionStack = space + EXCEPTION_STACK_SIZE;

    enum {DEBUG_STACK_SIZE = (2 * PAGE_SIZE)};
    memory->alloc(space, DEBUG_STACK_SIZE, PAGE_SIZE);
    mainDebugStack = space + DEBUG_STACK_SIZE;
    memory->alloc(space, DEBUG_STACK_SIZE, PAGE_SIZE);
    auxDebugStack = space + DEBUG_STACK_SIZE;
    currentDebugStack = mainDebugStack;

    exceptionMsgMgr = NULL;
    hwPerfMonRep = NULL;
    serverCDA = NULL;
    kernelTimer.init();
    dispatchQueue.init(vp, memory);
    ipcTargetTable.init(memory);
    ipcRetryManager.init();

    if (vp == 0) {
	memory->alloc(space, Scheduler::VPLimit*sizeof(RemoteIPCBuffer *), 8);
	OldRemoteIPCBuffers = (RemoteIPCBuffer **) space;
	for (VPNum i = 0; i < Scheduler::VPLimit; i++) {
	    OldRemoteIPCBuffers[i] = NULL;
	}
    }
}

void
ExceptionLocal::copyTraceInfo()
{
    TraceInfo *const trcInfo = &(kernelInfoPtr->traceInfo);

    trcInfoMask = trcInfo->mask;
    trcInfoIndexMask = trcInfo->indexMask;
    trcControl = trcInfo->traceControl;
    trcArray = trcInfo->traceArray;
}

/*static*/ void
ExceptionLocal::SoftIntrKernel(SoftIntr::IntrType intVal)
{
    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT);
    exceptionLocal.kernelProcessAnnex->deliverInterrupt(intVal);
}

/*static*/ void
ExceptionLocal::MakeReady(CommID commID)
{
    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT);

    ProcessAnnex *const pa = exceptionLocal.ipcTargetTable.lookupExact(commID);

    if (pa != NULL) {
	pa->makeReady();
    }
}

/*static*/ void
ExceptionLocal::HandleInterprocessorInterrupt()
{
    tassertSilent(!hardwareInterruptsEnabled(), BREAKPOINT);
    TraceOSExceptionIPInterrupt();

    /*
     * Hardware interprocessor interrupts are used for two purposes: alerting
     * the kernel to the arrival of new messages in the exception-level
     * MPMsg queue and making the kernel runnable when soft-interrupt bits
     * have been set.  We always check the MPMsg queues, but to cut down on
     * scheduling overhead, we only make the kernel runnable if it's really
     * necessary, that is, if it's currently running (perhaps in a borrowed
     * domain) or if it has work pending.
     */

    MPMsgMgrException::ProcessMsgs();

    ProcessAnnex *const pa = exceptionLocal.kernelProcessAnnex;
    if ((exceptionLocal.currentProcessAnnex == pa) || pa->stillRunnable()) {
	pa->makeReady();
    }
}

extern "C" void
ExceptionLocal_MakeCurrentProcessAnnexReady()
{
    exceptionLocal.currentProcessAnnex->makeReady();
}

/*static*/ void
ExceptionLocal::FreeRemoteIPCBuffer(RemoteIPCBuffer *ipcBuf)
{
    RemoteIPCBuffer **listp, *next;

    listp = &OldRemoteIPCBuffers[ipcBuf->sourceVP];
    do {
	next = *listp;
	ipcBuf->next = next;
    } while (!CompareAndStore((uval *) listp, (uval) next, (uval) ipcBuf));
}

/*static*/ void
ExceptionLocal::AcceptRemoteIPC(ProcessAnnex *curProc)
{
    uval len, checkedLen;
    RemoteIPCBuffer *const ipcBuf = curProc->pendingRemoteIPC;
    uval ipcType;
    CommID callerID;
    IPCRegsArch ipcRegs;

    if ((ipcBuf->ipcType == SYSCALL_IPC_CALL) && !curProc->ipcCallsEnabled) {
	/*
	 * The IPC can't be delivered because the IPC_CALL entry point is
	 * disabled.  We have to do an explicit check here, rather than rely
	 * NullIPCCallEntryException, because null-entry-point handler can't
	 * deal with an IPC that arrived from a remote source.  It's okay to
	 * proceed with an IPC_RTN whether or not the entry point is disabled,
	 * because NullIPCReturnEntryException simply discards the IPC.
	 */
	return;
    }

    /*
     * Nothing further can keep us from delivering the IPC, so we can take
     * responsibility for IPC buffer.  We don't need an atomic operation
     * because no one will be changing the field while it's non-NULL.
     */
    curProc->pendingRemoteIPC = NULL;

    /*
     * Re-post the preempt request that got us here, in case some other
     * mechanism (IPC retry notification, for example) is also depending on it.
     */
    (void) curProc->dispatcher->interrupts.fetchAndSet(SoftIntr::PREEMPT);

    /*
     * Load up the PPC page from the buffer.
     */
    len = ipcBuf->ipcPageLength;
    checkedLen = (len <= PPCPAGE_LENGTH_MAX) ? len : PPCPAGE_LENGTH_MAX;
    SET_PPC_LENGTH(len);
    memcpy(PPCPAGE_DATA, (char *) (ipcBuf + 1), checkedLen);

    ipcType = ipcBuf->ipcType;
    callerID = ipcBuf->callerID;
    ipcRegs = ipcBuf->ipcRegs;

    FreeRemoteIPCBuffer(ipcBuf);

    TraceOSExceptionAcceptRemoteIPC(ipcType, callerID);

    /*
     * Drop to assembler to load the registers, return ipcBuf to its
     * originating exceptionLocal, and launch curProc at an IPC entry point.
     */
    ExceptionLocal_AcceptRemoteIPC(&ipcRegs, callerID, ipcType, curProc);
    // does not return
}

extern "C" SysStatus
ExceptionLocal_SetEntryPoint(EntryPointNumber entryPoint,
			     EntryPointDesc entry)
{
    if ((entryPoint < 0) || (entryPoint >= NUM_ENTRY_POINTS)) {
	return _SERROR(1108, 0, EINVAL);
    }
    exceptionLocal.currentProcessAnnex->setEntryPoint(entryPoint, entry);
    return 0;
}

extern "C" ProcessAnnex *
ExceptionLocal_GetNextProcess()
{
    ProcessAnnex *curProc;

    // curProc may have yielded with the ppc page in use (erroneously).
    RESET_PPC_LENGTH();

    curProc = exceptionLocal.currentProcessAnnex;

    if (curProc->pendingRemoteIPC != NULL) {
	ExceptionLocal::AcceptRemoteIPC(curProc);
	// Returns only if the IPC couldn't be accepted for some reason.
    }

    if (curProc->dispatcher->hasWork && !curProc->isReady()) {
	curProc->makeReady();
    }

    if (curProc->ipcRetrySources != 0) {
	exceptionLocal.ipcRetryManager.notify(curProc);
    }
    exceptionLocal.ipcRetryManager.checkRemoteNotifications();

    curProc = exceptionLocal.dispatchQueue.getNextProcessAnnex();

    TraceOSExceptionProcessYield(curProc->commID);
    curProc->switchContext();

    return curProc;
}

extern "C" ProcessAnnex *
ExceptionLocal_GetHandoffProcess(CommID targetID)
{
    ProcessAnnex *curProc, *target;

    // curProc may have yielded with the ppc page in use (erroneously).
    RESET_PPC_LENGTH();

    curProc = exceptionLocal.currentProcessAnnex;
    if (curProc->dispatcher->hasWork && !curProc->isReady()) {
	curProc->makeReady();
    }

    target = exceptionLocal.ipcTargetTable.lookupWild(targetID);

    /*
     * If the target exists on this processor and is not disabled, switch
     * to it.  Otherwise simply re-launch the current process.
     */
    if ((target != NULL) && (target->reservedThread == NULL)) {
	/*
	 * We stay in the current resource domain (as a borrowed domain), but
	 * we clear the cache of borrowers because there will be no "reply"
	 * to this handoff.
	 */
	exceptionLocal.dispatchQueue.clearCDABorrowers();
	TraceOSExceptionProcessHandoff(target->commID);
	target->switchContext();
	curProc = target;
    }

    return curProc;
}

uval checkAddr = 0xffffeeeeddddccc1ULL;

extern "C" SysStatus
ExceptionLocal_PgfltHandler(ProcessAnnex *srcProc,
			    uval faultInfo, uval faultAddr, uval noReflection)
{
    SysStatus rc;
    uval wasActive;
    PageFaultNotification *pn;

#ifdef COLLECT_FAULT_STATS
    //So that setPFBit works right
    uval oldTSU = Scheduler::GetThreadSpecificUvalSelf();
    Scheduler::SetThreadSpecificUvalSelf(0);
#endif // COLLECT_FAULT_STATS


    TraceOSExceptionPgflt((uval64) CurrentThread, faultAddr,
			  (uval64) srcProc->commID,
			  (uval64) srcProc->excStatePtr()->codeAddr(),
			  (uval64) faultInfo);

#ifndef NDEBUG
    /*
     * Check for segment fault in borrowed kernel address space.
     */
    if (exceptionLocal.currentSegmentTable->
			checkKernelSegmentFault(faultAddr)) {
	//FIXME - its hard to test this code
	//This printf because Marc wants to know if this
	//code ever gets executed
	err_printf("Tell Marc: segment fault %lx k %x\n",
		    faultAddr, srcProc->isKernel);
	return 0;	// we may re-fault for the page itself
    }
#endif
    StatTimer t1(ExceptionCode);
    PRESERVE_PPC_PAGE();

    enableHardwareInterrupts();

    tassertMsg(faultAddr!=checkAddr,"got it\n");
#ifdef COLLECT_FAULT_STATS
    if (faultInfo & DSIStatusReg::writeFault) {
	setPFBit(NeedWrite);
    }
#endif // COLLECT_FAULT_STATS

    wasActive = CurrentThread->isActive();
    if (!wasActive) {
	CurrentThread->activate();
    }

    if (noReflection) {
	pn = NULL;
    } else {
	pn = srcProc->fnMgr.alloc(srcProc);
#ifndef NDEBUG
	if (pn != NULL) pn->vaddr = faultAddr;
#endif
    }

    rc = DREF(srcProc->processRef)->
		handleFault(faultInfo, faultAddr, pn,
			    SysTypes::VP_FROM_COMMID(srcProc->commID));

    // Free the notification object for bad-address or in-core page faults.
    if ((pn != NULL) && (_FAILURE(rc) || (_SGETUVAL(rc) == 0))) {
	srcProc->fnMgr.free(pn);
    }

    if (!wasActive) {
	CurrentThread->deactivate();
    }

    tassertWrn(srcProc->isKernel || _SUCCESS(rc),
	       "User-mode bad-address fault: "
	       "commID 0x%lx, pc %p, addr %lx, rc %lx.\n",
	       srcProc->commID,
	       srcProc->excStatePtr()->codeAddr(), faultAddr, rc);

    disableHardwareInterrupts();

    RESTORE_PPC_PAGE();

    t1.record();

    TraceOSExceptionPgfltDone((uval) srcProc->commID,
		    (uval64) CurrentThread, faultAddr, rc,
		    Scheduler::GetThreadSpecificUvalSelf());
#ifdef COLLECT_FAULT_STATS
    Scheduler::SetThreadSpecificUvalSelf(oldTSU);
#endif // COLLECT_FAULT_STATS

    return rc;
}

extern "C" void
ExceptionLocal_AwaitDispatch(ProcessAnnex *srcProc)
{
    TraceOSExceptionAwaitDispatch((uval64) CurrentThread);
    exceptionLocal.dispatchQueue.awaitDispatch(srcProc);
    TraceOSExceptionAwaitDispatchDone(srcProc->commID);
}

extern "C" void
ExceptionLocal_RequestRetryNotification(ProcessAnnex *target)
{
    exceptionLocal.ipcRetryManager.
	requestNotification(exceptionLocal.currentProcessAnnex, target);
}

extern "C" void
ExceptionLocal_PPCPrimitiveAwaitRetry(ProcessAnnex *srcProc, CommID targetID)
{
    TraceOSExceptionAwaitPPCRetry(
		    (uval64) CurrentThread, targetID);

    PRESERVE_PPC_PAGE();

    enableHardwareInterrupts();

    Scheduler::DelayMicrosecs(10000);

    disableHardwareInterrupts();

    RESTORE_PPC_PAGE();

    exceptionLocal.dispatchQueue.awaitDispatch(srcProc);
    TraceOSExceptionAwaitPPCRetryDone(srcProc->commID);
}

extern "C" SysStatus
ExceptionLocal_IPCRemote(IPCRegsArch *ipcRegsP,
			 CommID targetID,
			 uval ipcType,
			 ProcessAnnex *srcProc)
{
    uval len, checkedLen;
    uval ipcBufSize;
    RemoteIPCBuffer **listp, *list, *ipcBuf;
    SysStatus rc;
    BaseProcessRef pref;

    TraceOSExceptionIPCRemote(
		    (uval64) CurrentThread, ipcType, targetID);

    enableHardwareInterrupts();
    CurrentThread->activate();

    // Deallocate any remote IPC buffers that have been returned here.
    listp = &ExceptionLocal::OldRemoteIPCBuffers[exceptionLocal.vp];
    list = (RemoteIPCBuffer *) FetchAndClear((uval *) listp);
    while (list != NULL) {
	ipcBuf = list;
	list = list->next;
	AllocPinnedGlobalPadded::free(ipcBuf, ipcBuf->size);
    }

    len = GET_PPC_LENGTH();
    checkedLen = (len <= PPCPAGE_LENGTH_MAX) ? len : PPCPAGE_LENGTH_MAX;

    ipcBufSize = sizeof(RemoteIPCBuffer) + checkedLen;
    ipcBuf = (RemoteIPCBuffer *) AllocPinnedGlobalPadded::alloc(ipcBufSize);
    if (ipcBuf != NULL) {
	// remember source and size for deallocation
	ipcBuf->sourceVP = exceptionLocal.vp;
	ipcBuf->size = ipcBufSize;

	ipcBuf->ipcType = ipcType;
	ipcBuf->callerID = srcProc->commID;
	ipcBuf->ipcRegs = *ipcRegsP;

	ipcBuf->ipcPageLength = len;
	memcpy((char *) (ipcBuf + 1), PPCPAGE_DATA, checkedLen);
	RESET_PPC();

	// Find the process that matches targetID, and call it.
	rc = DREFGOBJ(TheProcessSetRef)->
		getRefFromPID(SysTypes::PID_FROM_COMMID(targetID), pref);

	if (_SUCCESS(rc)) {
	    rc = DREF((ProcessRef) pref)->sendRemoteIPC(targetID, ipcBuf);
	}

	if (_FAILURE(rc)) {
	    /*
	     * Restore the PPC page and deallocate the ipcBuffer if the
	     * remote call failed.  If it succeeded, the target end is
	     * responsible for the buffer and its content.
	     */
	    SET_PPC_LENGTH(len);
	    memcpy(PPCPAGE_DATA, (char *) (ipcBuf + 1), checkedLen);
	    AllocPinnedGlobalPadded::free(ipcBuf, ipcBufSize);
	    /*
	     * Add ipcType into the return code, so that the sender's IPC
	     * fault handler will know how to process the fault.
	     */
	    rc = _SERROR(_SERRCD(rc), ipcType, _SGENCD(rc));
	}
    } else {
	rc = _SERROR(2310, ipcType, EAGAIN);
    }

    CurrentThread->deactivate();
    disableHardwareInterrupts();

    if (_FAILURE(rc) && (_SGENCD(rc) == EAGAIN)) {
	exceptionLocal.ipcRetryManager.
	    requestNotificationRemote(srcProc, targetID);
    }

    TraceOSExceptionIPCRemoteDone(srcProc->commID);

    return rc;
}

/*static*/ SysStatus
ExceptionLocal::PPCAsyncRemote(ProcessAnnex *srcProc, CommID targetID,
			       XHandle xhandle, uval methnum,
			       uval length, uval *buf)
{
    SysStatus rc;
    uval wasActive;
    BaseProcessRef pref;

    TraceOSExceptionPPCAsyncRemote(
		    (uval64) CurrentThread, targetID);

    enableHardwareInterrupts();

    wasActive = CurrentThread->isActive();
    if (!wasActive) {
	CurrentThread->activate();
    }

    // find out the process that matches targetID, and call
    rc = DREFGOBJ(TheProcessSetRef)->
	    getRefFromPID(SysTypes::PID_FROM_COMMID(targetID), pref);

    if (_SUCCESS(rc)) {
	rc = DREF((ProcessRef)pref)->sendRemoteAsyncMsg(targetID,
							srcProc->commID,
							xhandle, methnum,
							length, buf);
    }

    if (!wasActive) {
	CurrentThread->deactivate();
    }

    disableHardwareInterrupts();

    TraceOSExceptionPPCAsyncRemoteDone(srcProc->commID);

    return rc;
}

extern "C" SysStatus
ExceptionLocal_PPCAsyncRemote(ProcessAnnex *srcProc, CommID targetID,
			      XHandle xhandle, uval methnum,
			      uval length, uval *buf)
{
    SysStatus rc;

    exceptionLocal.dispatchQueue.
	pushCDABorrower(exceptionLocal.kernelProcessAnnex);

    rc = ExceptionLocal::PPCAsyncRemote(srcProc, targetID,
					xhandle, methnum, length, buf);

    exceptionLocal.dispatchQueue.popCDABorrower();
    if (exceptionLocal.dispatchQueue.currentCDABorrower() != srcProc) {
	exceptionLocal.dispatchQueue.clearCDABorrowers();
	exceptionLocal.dispatchQueue.awaitDispatch(srcProc);
    }

    return rc;
}

extern "C" void
ExceptionLocal_TracePPCCall(CommID newCommID)
{
    TraceOSExceptionPPCCall(newCommID);
}

extern "C" void
ExceptionLocal_TracePPCReturn(CommID newCommID)
{
    TraceOSExceptionPPCReturn(newCommID);
}

/*
 * PPC_ASYNC call in kernel implemented by just procedure call
 */
void
PPC_ASYNC(SysStatus &rc, CommID targetID, XHandle xhandle, uval methnum)
{
    InterruptState tmp;
    disableHardwareInterrupts(tmp);

    ProcessAnnex *const srcProc = exceptionLocal.kernelProcessAnnex;
    tassertMsg(exceptionLocal.currentProcessAnnex == srcProc,
	       "Not in kernel!\n");

    ProcessAnnex *target = exceptionLocal.ipcTargetTable.lookupWild(targetID);

    if (target != NULL) {
	// local case
	rc = target->dispatcher->asyncBufferLocal.storeMsg(srcProc->commID,
							   xhandle, methnum,
							   GET_PPC_LENGTH(),
							   PPCPAGE_DATA);
	TraceOSExceptionPPCAsyncLocal(target->commID, rc);
	RESET_PPC();			// done with PPC_PAGE
	if (_SUCCESS(rc)) {
	    target->deliverInterrupt(SoftIntr::ASYNC_MSG);
	}
    } else {
	// remote case
	uval const length = GET_PPC_LENGTH();
	uval buf[AsyncBuffer::MAX_LENGTH];
	memcpy(buf, PPCPAGE_DATA, length);
	RESET_PPC();			// done with PPC_PAGE

	rc = ExceptionLocal::PPCAsyncRemote(srcProc, targetID,
					    xhandle, methnum, length, buf);
    }

    enableHardwareInterrupts(tmp);
}

/*static*/ void
ExceptionLocal::EnterDebugger()
{
    exceptionLocal.currentDebugStack = exceptionLocal.auxDebugStack;
    exceptionLocal.enterDebuggerArch();
}

/*static*/ void
ExceptionLocal::ExitDebugger()
{
    exceptionLocal.exitDebuggerArch();
    exceptionLocal.currentDebugStack = exceptionLocal.mainDebugStack;
}

/*static*/ uval
ExceptionLocal::InDebugger()
{
    return exceptionLocal.currentDebugStack == exceptionLocal.auxDebugStack;
}

#include <scheduler/SchedulerService.H>
#include <stub/StubSchedulerService.H>

/*static*/ void
ExceptionLocal::PrintStatus(uval dumpThreads)
{
    SysStatus rc;
    uval i, j, keyIter, numThreads;
    ProcessAnnex *pa, *prev;
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    // MAX_THREADS is defined to be large enough to fill the ppc page
    // we get stack overflows if we use that much, so use half.
    uval const MAX_NUM_THREADS = SchedulerService::MAX_THREADS/2;
    Thread::Status threadStatus[MAX_NUM_THREADS];
    SysTime const tps = SchedulerTimer::TicksPerSecond();

    disableHardwareInterrupts();

    for (i = 0; i < exceptionLocal.ipcTargetTable._tableSize; i++) {

	pa = exceptionLocal.ipcTargetTable._table[i];

	while (pa != NULL) {

	    err_printf("ProcessAnnex %p (%s), pid 0x%lx, rd %ld, vp %ld\n",
					pa, pa->dispatcher->progName,
					SysTypes::PID_FROM_COMMID(pa->commID),
					SysTypes::RD_FROM_COMMID(pa->commID),
					SysTypes::VP_FROM_COMMID(pa->commID));

	    err_printf("    dispatcher %p, interrupts 0x%x\n",
					pa->dispatcher,
					pa->dispatcher->interrupts.flags);
#ifdef DEBUG_SOFT_INTERRUPTS
	    uval any = 0;
	    for (uval intr = 0; intr < SoftIntr::MAX_INTERRUPTS; intr++) {
		uval count = pa->dispatcher->interrupts.outstanding[intr];
		if (count != 0) {
		    if (!any) err_printf("    interrupt counts:");
		    err_printf(" 0x%lx(%ld)", intr, count);
		    any = 1;
		}
	    }
	    if (any) err_printf("\n");
#endif
	    err_printf("    dispatchTime %lld.%09lld, "
			"state offsets(exc,trap,user): 0x%lx 0x%lx 0x%lx\n",
					pa->dispatchTicks / tps,
					((pa->dispatchTicks % tps) *
							1000000000ull) / tps,
					pa->excStateOffset,
					pa->trapStateOffset,
					pa->userStateOffset);
	    err_printf("    terminator 0x%lx, reservedThread %p\n",
					pa->terminator,
					pa->reservedThread);
	    err_printf("    ppcTargetID 0x%lx, ppcThreadID 0x%lx\n",
					pa->ppcTargetID,
					pa->ppcThreadID);

	    if ((SysTypes::PID_FROM_COMMID(pa->commID) == _KERNEL_PID) &&
				(SysTypes::RD_FROM_COMMID(pa->commID) != 0)) {
		err_printf("    idle loop\n");
	    } else if (dumpThreads) {
		if (pa->reservedThread != NULL) {
		    err_printf("    cannot obtain thread status of "
							    "disabled vp\n");
		    err_printf("        sending PrintStatus request instead\n");
		    pa->deliverInterrupt(SoftIntr::PRINT_STATUS);
		} else {
		    // assume the pa commid is right one
		    schedServ.initOHWithCommID(
			SysTypes::WILD_COMMID(pa->commID),
			XHANDLE_MAKE_NOSEQNO(CObjGlobals::SchedulerServiceIndex));

		    err_printf("    threads:\n");
		    keyIter = 0;
		    do {
			enableHardwareInterrupts();
			rc = schedServ._getStatus(keyIter, numThreads,
						  MAX_NUM_THREADS,
						  threadStatus);
			disableHardwareInterrupts();
			tassert(_SUCCESS(rc), err_printf("woops\n"));
			for (j = 0; j < numThreads; j++) {
			    threadStatus[j].print();
			}
		    } while (numThreads > 0);
		}
	    }

	    // Rescan the chain since we may have hardware-enabled for a while.
	    prev = pa;
	    pa = exceptionLocal.ipcTargetTable._table[i];
	    while ((pa != NULL) && (pa != prev)) pa = pa->ipcTargetNext;
	    if (pa == NULL) {
		// Prev no longer exists, so reprint the whole chain.
		pa = exceptionLocal.ipcTargetTable._table[i];
	    } else {
		pa = pa->ipcTargetNext;
	    }
	}
    }

    enableHardwareInterrupts();
}

/*
 * Put this routine here just to avoid creating a whole DispatcherDefaultKern.C
 * file just for this routine.
 */
extern "C" Thread *
DispatcherDefaultKern_ExceptionCreateThread(DispatcherDefaultKern *dispatcher)
{
    return dispatcher->exceptionCreateThread();
}
