/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessVPList.C,v 1.83 2004/04/06 21:00:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include "kernIncs.H"
#include <sys/KernelInfo.H>
#include "ProcessVPList.H"
#include <scheduler/Scheduler.H>
#include "mem/FCMFixed.H"
#include "init/memoryMapKern.H"
#include <exception/KernelTimer.H>
#include <mem/Region.H>
#include <mem/HATKernel.H>
#include "Process.H"

inline SysStatus
ProcessVPList::findProcessAnnex(RDNum rd, VPNum vp,
				VPInfo* &vpInfo, ProcessAnnex* &pa)
{
    if (vp < vpLimit) {
	vpInfo = dspTable->vpInfo[vp];
	if (vpInfo != NULL) {
	    if (rd < Scheduler::RDLimit) {
		pa = vpInfo->dspInfo[rd].pa;
		if (pa != NULL) {
		    return 0;
		}
	    }
	}
    }
    return _SERROR(1523, 0, ESRCH);
}

SysStatus
ProcessVPList::vpnumToPpnum(VPNum vp, VPNum &pp)
{
    VPInfo *vpInfo;

    if (requests.enter() < 0) {
	return _SERROR(2639, 0, ESRCH);	// process being destroyed
    }

    pp = ProcessAnnex::NO_PHYS_PROC;
    if (vp < vpLimit) {
	vpInfo = dspTable->vpInfo[vp];
	if (vpInfo != NULL) {
	    pp = vpInfo->pp;
	}
    }

    requests.leave();

    return (pp != ProcessAnnex::NO_PHYS_PROC) ? 0 : _SERROR(1107, 0, EINVAL);
}

void
ProcessVPList::init(uval uMode, uval isK, const char *nameStr, ProcessID pid)
{
    userMode = uMode;
    isKern = isK;
    strncpy(name, nameStr, MAX_NAME_LEN-1);
    name[MAX_NAME_LEN-1] = '\0';	// just in case
    processID = pid;

    // requests is initialized via constructor
    dspTable = &dspTableInitial;
    dspTable->vpInfo[0] = NULL;
    vpLimit = 1;
    vpCounter = 0;
}

void
ProcessVPList::initKern(uval ppCount)
{
    /*
     * Pre-allocate and initialize a DspTable and VPInfo structures sufficient
     * for the number physical processors we have.  We need pinned storage
     * for the kernel Process object, so we preempt the dynamic allocation of
     * these structures that would otherwise occur in createDispatcher().
     */
    vpLimit = ppCount;

    if (vpLimit > 1) {
	uval size = sizeof(DspTable) + ((vpLimit - 1) * sizeof(VPInfo *));
	dspTable = (DspTable *) AllocPinnedGlobalPadded::alloc(size);
	tassertMsg(dspTable != NULL, "DspTable allocation failed.\n");
    }

    dspTable->vpInfo[0] = &vpInfo0;
    dspTable->vpInfo[0]->init(0);

    for (VPNum vp = 1; vp < vpLimit; vp++) {
	dspTable->vpInfo[vp] =
	    (VPInfo *) AllocPinnedGlobalPadded::alloc(sizeof(VPInfo));
	tassertMsg(dspTable->vpInfo[vp] !=NULL, "VPInfo allocation failed.\n");
	dspTable->vpInfo[vp]->init(vp);
    }

    vpCounter = vpLimit;
}

SysStatus
ProcessVPList::createDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
				EntryPointDesc entry, uval dispatcherAddr,
				uval initMsgLength, char *initMsg,
				ProcessRef procRef, HATRef hatRef)
{
    SysStatus rc;
    VPInfo *vpInfo;
    uval newLimit, size;
    DspTable *newTable;
    ProcessAnnex *pa;
    SegmentTable *segTable;
    Dispatcher *dsp, *dspUser;
    RegionRef dspRegRef;
    FCMRef dspFCMRef;
    uval dspOffset, dspAddrKern;

    tassertMsg(cda->getPP() == Scheduler::GetVP(), "CDA not on this pp.\n");

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    if (vp >= Scheduler::VPLimit) {
	return _SERROR(1752, 0, EINVAL);
    }

    if (rd >= Scheduler::RDLimit) {
	return _SERROR(1751, 0, EINVAL);
    }

    if (PAGE_ROUND_DOWN(dispatcherAddr) != dispatcherAddr) {
	return _SERROR(1327, 0, EINVAL);
    }

    if (requests.enter() < 0) {
	return _SERROR(1328, 0, ESRCH);	// process being destroyed
    }

    if ((vp < vpLimit) && (dspTable->vpInfo[vp] != NULL)) {
	vpInfo = dspTable->vpInfo[vp];
    } else {
	// We don't have a VPInfo structure for this vp.  Create one, guarded
	// by stop()'ing requests.  RequestCountWithStop doesn't support an
	// upgrade operation, so we have to "leave" before we can "stop".
	requests.leave();
	if (requests.stop() < 0) {
	    return _SERROR(2640, 0, ESRCH);	// process being destroyed
	}

	if (vp >= vpLimit) {
	    // We have to increase the size of the table.  We make the first
	    // increment larger than subsequent ones to lessen ramp-up costs.
	    newLimit = (vpLimit == 1) ? 16 : (vpLimit * 2);
	    // Make sure the newLimit is large enough to include vp.  We won't
	    // blow up because we know that vp < Scheduler::VPLimit.
	    while (vp >= newLimit) {
		newLimit *= 2;
	    }

	    // Allocate a new table.  DspTable includes space for one VPInfo
	    // pointer, hence the "newLimit - 1" in the following calculation.
	    size = sizeof(DspTable) + ((newLimit - 1) * sizeof(VPInfo *));
	    newTable = (DspTable *) AllocGlobalPadded::alloc(size);
	    tassertMsg(newTable != NULL, "DspTable allocation failed.\n");

	    // Copy content of the old table to the new, and initialize the
	    // rest of the new table.
	    for (uval i = 0; i < vpLimit; i++) {
		newTable->vpInfo[i] = dspTable->vpInfo[i];
	    }
	    for (uval i = vpLimit; i < newLimit; i++) {
		newTable->vpInfo[i] = NULL;
	    }

	    // Free the old table, unless it is the initial (pre-allocated)
	    // table.
	    if (vpLimit > 1) {
		size = sizeof(DspTable) + ((vpLimit - 1) * sizeof(VPInfo *));
		AllocGlobalPadded::free(dspTable, size);
	    }

	    // Install the new table.
	    dspTable = newTable;
	    vpLimit = newLimit;
	}

	// We have to check vpInfo[vp] again now that requests are stop'd.
	vpInfo = dspTable->vpInfo[vp];
	if (vpInfo == NULL) {
	    if (vp == 0) {
		// Space for the first VPInfo structure is pre-allocated.
		vpInfo = &vpInfo0;
	    } else {
		vpInfo = new VPInfo;
		tassertMsg(vpInfo != NULL, "VPInfo allocation failed.\n");
	    }
	    vpInfo->init(cda->getPP());
	    dspTable->vpInfo[vp] = vpInfo;
	    vpCounter++;
	    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
		err_printf("Mapping program %s, pid 0x%lx, vp %ld to pp %ld.\n",
			   name, processID, vp, vpInfo->pp);
	    }
	}

	// Restart and then re-enter the request counter.
	requests.restart();
	if (requests.enter() < 0) {
	    return _SERROR(2641, 0, ESRCH);	// process being destroyed
	}
    }

    /*
     * At this point the requests counter has been enter'd and vpInfo points
     * to a valid VPInfo structure for this vp.  All further processing is
     * done under the vp lock.
     */

    vpInfo->lock.acquire();

    if (vpInfo->pp != cda->getPP()) {
	// VP is not on this physical processor.
	rc = _SERROR(1750, 0, EINVAL);
	goto CleanupAndReturn;
    }

    if (vpInfo->dspInfo[rd].pa != NULL) {
	// Dispatcher already exists.
	rc = _SERROR(1329, 0, EEXIST);
	goto CleanupAndReturn;
    }

    dspUser = (Dispatcher *) dispatcherAddr;
    if (isKern) {
	dspFCMRef = NULL;
	dspOffset = 0;
	dsp = dspUser;
	// Set a bogus interrupt bit to make the dispatcher runnable.
	(void) dsp->interrupts.fetchAndSet(SoftIntr::PREEMPT);
    } else {
	rc = DREF(procRef)->vaddrToRegion(dispatcherAddr, dspRegRef);
	if (_FAILURE(rc)) goto CleanupAndReturn;
	rc = DREF(dspRegRef)->vaddrToFCM(vp, dispatcherAddr, 0,
					 dspFCMRef, dspOffset);
	if (_FAILURE(rc)) goto CleanupAndReturn;
	rc = DREF(dspFCMRef)->addReference();
	if (_FAILURE(rc)) goto CleanupAndReturn;
	rc = archAllocDispatcherPage(dispatcherAddr, dspAddrKern);
	tassertMsg(_SUCCESS(rc), "archAllocDispatcherPage failed.\n");
	rc = DREF(dspFCMRef)->establishPage(dspOffset, dspAddrKern, PAGE_SIZE);
	tassertMsg(_SUCCESS(rc), "establishPage failed.\n");
	dsp = (Dispatcher *) dspAddrKern;
	dsp->init(dspid);
	rc = dsp->asyncBufferLocal.storeMsg(_KERNEL_PID, 0,
					    0, initMsgLength, initMsg);
	if (_FAILURE(rc)) {
	    (void) DREF(dspFCMRef)->disEstablishPage(dspOffset, PAGE_SIZE);
	    (void) DREF(dspFCMRef)->removeReference();
	    goto CleanupAndReturn;
	}
	(void) dsp->interrupts.fetchAndSet(SoftIntr::ASYNC_MSG);
    }

    rc = DREF(hatRef)->getSegmentTable(vp, segTable);
    tassertMsg(_SUCCESS(rc), "getSegmentTable failed.\n");

    pa = new ProcessAnnex();
    tassertMsg(pa != NULL, "ProcessAnnex allocation failed.\n");
    pa->init(procRef, processID, userMode, isKern,
	     dspUser, dsp, dspFCMRef, dspOffset,
	     segTable, dspid);

    pa->setEntryPoint(RUN_ENTRY, entry);

    vpInfo->dspInfo[rd].pa = pa;
    vpInfo->dspCounter++;

    InterruptState is;
    disableHardwareInterrupts(is);
    exceptionLocal.ipcTargetTable.enter(pa);
    pa->attach(cda);
    enableHardwareInterrupts(is);

    rc = 0;

CleanupAndReturn:
    vpInfo->lock.release();
    requests.leave();
    return rc;
}

SysStatus
ProcessVPList::detachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
				HATRef hatRef)
{
    SysStatus rc;
    VPInfo *vpInfo;
    ProcessAnnex *pa;
    uval64 ipcRetryIDs;

    tassertMsg(cda->getPP() == Scheduler::GetVP(), "CDA not on this pp.\n");

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    if (requests.enter() < 0) {
	return _SERROR(2642, 0, ESRCH);	// process being destroyed
    }

    rc = findProcessAnnex(rd, vp, vpInfo, pa);
    if (_FAILURE(rc)) {
	requests.leave();
	return rc;
    }

    if (!pa->isAttached(cda)) {
	requests.leave();
	return _SERROR(2643, 0, EINVAL);
    }

    vpInfo->lock.acquire();

    disableHardwareInterrupts();

    if (pa->reservedThread != NULL) {
	/*
	 * FIXME:  For now, don't try to detach a dispatcher that is currently
	 *         disabled.  We have to do better in the long run.
	 */
	enableHardwareInterrupts();
	rc = _SERROR(2312, 0, EAGAIN);
	goto CleanupAndReturn;
    }

    pa->detach();
    exceptionLocal.ipcTargetTable.remove(pa);

    if (KernelTimer::TimerRequestTime(pa) != SysTime(-1)) {
	/*
	 * PA has a timeout request registered.  Rather than try to reproduce
	 * it on the new processor, we simply generate a TIMER_EVENT soft
	 * interrupt so that the dispatcher can sort things out for itself.
	 */
	(void) pa->dispatcher->interrupts.fetchAndSet(SoftIntr::TIMER_EVENT);
    }
    exceptionLocal.kernelTimer.remove(pa);

    ipcRetryIDs = IPCRetryManager::GetIPCRetryIDs(pa);
    if (ipcRetryIDs != 0) {
	/*
	 * PA has IPCs waiting to be retried.  Simply generate notifications
	 * for all of them, to be delivered when the dispatcher runs.
	 */
	pa->dispatcher->ipcRetry |= ipcRetryIDs;
	(void) pa->dispatcher->interrupts.
				fetchAndSet(SoftIntr::IPC_RETRY_NOTIFY);
    }
    exceptionLocal.ipcRetryManager.remove(pa);

    enableHardwareInterrupts();

    vpInfo->dspCounter--;
    if (vpInfo->dspCounter > 0) {
	rc = 0;
	goto CleanupAndReturn;
    }

    /*
     * This VP's last dispatcher has now been detached, so detach the VP.
     * Switch to the canonical kernel address space, in case we're currently
     * "borrowing" the address space we're about to unmap.
     */
    ((HATKernel*)(DREFGOBJK(TheKernelHATRef)))->switchToKernelAddressSpace();

    rc = DREF(hatRef)->detachVP(vp);
    tassertMsg(_SUCCESS(rc), "hat->detachVP() failed.\n");

    vpInfo->pp = ProcessAnnex::NO_PHYS_PROC; // VP now ready for re-attachment
    rc = 0;

CleanupAndReturn:
    vpInfo->lock.release();
    requests.leave();
    return rc;
}

SysStatus
ProcessVPList::attachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
				HATRef hatRef)
{
    SysStatus rc;
    VPInfo *vpInfo;
    ProcessAnnex *pa;

    tassertMsg(cda->getPP() == Scheduler::GetVP(), "CDA not on this pp.\n");

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    if (requests.enter() < 0) {
	return _SERROR(2644, 0, ESRCH);	// process being destroyed
    }

    rc = findProcessAnnex(rd, vp, vpInfo, pa);
    if (_FAILURE(rc)) {
	requests.leave();
	return rc;
    }

    vpInfo->lock.acquire();

    if (pa->pp != ProcessAnnex::NO_PHYS_PROC) {
	rc = _SERROR(2645, 0, EINVAL);
	goto CleanupAndReturn;
    }

    if (vpInfo->dspCounter == 0) {
	tassertMsg(vpInfo->pp == ProcessAnnex::NO_PHYS_PROC,
		   "VP not detached.\n");
	vpInfo->pp = cda->getPP();
	rc = DREF(hatRef)->attachVP(vp);
	tassertMsg(_SUCCESS(rc), "attachVP failed.\n");
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("Migrated pid 0x%lx, vp %ld to pp %ld.\n",
		       processID, vp, vpInfo->pp);
	}
    } else if (cda->getPP() != vpInfo->pp) {
	rc = _SERROR(2646, 0, EINVAL);
	goto CleanupAndReturn;
    }

    vpInfo->dspCounter++;

    InterruptState is;
    disableHardwareInterrupts(is);
    exceptionLocal.ipcTargetTable.enter(pa);
    pa->attach(cda);
    enableHardwareInterrupts(is);

    rc = 0;

CleanupAndReturn:
    vpInfo->lock.release();
    requests.leave();
    return rc;
}

void
ProcessVPList::VPInfo::deleteDispatchers()
{
    RDNum rd;
    ProcessAnnex *pa;

    tassertMsg((pp == Scheduler::GetVP()) ||
		    (pp == ProcessAnnex::NO_PHYS_PROC),
	       "Wrong processor.\n");

    for (rd = 0; rd < Scheduler::RDLimit; rd++) {
	pa = dspInfo[rd].pa;
	if (pa != NULL) {
	    tassertMsg((pa->pp == Scheduler::GetVP()) ||
			    (pa->pp == ProcessAnnex::NO_PHYS_PROC),
		       "Wrong processor.\n");
	    if (pa->pp != ProcessAnnex::NO_PHYS_PROC) {
		//N.B. don't use the disableHardwareInterrupts(is) form here
		//     since waitForTerminate() may enable/disable while
		//     blocking and restore value could become stale.
		disableHardwareInterrupts();
		exceptionLocal.ipcTargetTable.remove(pa);
		exceptionLocal.kernelTimer.remove(pa);
		exceptionLocal.ipcRetryManager.remove(pa);
		pa->waitForTerminate();
		pa->detach();
		enableHardwareInterrupts();
	    }
	    if (pa->pendingRemoteIPC != NULL) {
		ExceptionLocal::FreeRemoteIPCBuffer(pa->pendingRemoteIPC);
		pa->pendingRemoteIPC = NULL;
	    }
	    pa->awaitAndFreeAllNotifications();
	    pa->releaseDispatcherMemory();
	    delete pa;
	}
    }

    // Switch to the canonical kernel address space, just in case we are
    // currently "borrowing" the address space we're about to tear down.
    ((HATKernel*)(DREFGOBJK(TheKernelHATRef)))->switchToKernelAddressSpace();
}

struct ProcessVPList::DeleteVPMsg : MPMsgMgr::MsgAsync {
    VPInfo *vpInfo;
    ThreadID waiter;
    sval *barrierp;

    virtual void handle() {
	VPInfo *myVPInfo = vpInfo;
	ThreadID myWaiter = waiter;
	sval *myBarrierp = barrierp;
	free();

	myVPInfo->deleteDispatchers();

	if (FetchAndAddSignedVolatile(myBarrierp, -1) == 1) {
	    Scheduler::Unblock(myWaiter);
	}
    }
};

void
ProcessVPList::deleteAll()
{
    SysStatus rc;
    VPNum vp;
    VPNum const pp = Scheduler::GetVP();	// processor we are running on
    VPInfo *vpInfo;
    sval barrier;
    uval size;

    // block until any inflight creates finish, and prevent
    // any new creates
    if (requests.shutdown() < 0) {
	return;				// already destroyed
    }

    barrier = 1;

    for (vp = 0; vp < vpLimit; vp++) {
	vpInfo = dspTable->vpInfo[vp];
	if ((vpInfo != NULL) &&
		((vpInfo->pp != pp) &&
		    (vpInfo->pp != ProcessAnnex::NO_PHYS_PROC))) {
	    DeleteVPMsg *const msg =
		new(Scheduler::GetEnabledMsgMgr()) DeleteVPMsg;
	    msg->vpInfo = vpInfo;
	    msg->waiter = Scheduler::GetCurThread();
	    msg->barrierp = &barrier;
	    (void) FetchAndAddSignedVolatile(&barrier, 1);
	    rc = msg->send(SysTypes::DSPID(0, vpInfo->pp));
	    tassertMsg(_SUCCESS(rc), "DeleteVPMsg send failed.\n");
	}
    }

    for (vp = 0; vp < vpLimit; vp++) {
	vpInfo = dspTable->vpInfo[vp];
	if ((vpInfo != NULL) &&
		((vpInfo->pp == pp) ||
		    (vpInfo->pp == ProcessAnnex::NO_PHYS_PROC))) {
	    vpInfo->deleteDispatchers();
	}
    }

    (void) FetchAndAddSignedVolatile(&barrier, -1);	// count ourself
    while (barrier > 0) {
	Scheduler::Block();
    }

    if (vpLimit > 1) {
	for (vp = 1; vp < vpLimit; vp++) {
	    vpInfo = dspTable->vpInfo[vp];
	    if (vpInfo != NULL) {
		delete vpInfo;
	    }
	}
	size = sizeof(DspTable) + ((vpLimit - 1) * sizeof(VPInfo *));
	AllocGlobalPadded::free(dspTable, size);
    }
}

SysStatus
ProcessVPList::sendInterruptKernel(DispatcherID dspid, SoftIntr::IntrType i)
{
    SysStatus rc;
    VPInfo *vpInfo;
    ProcessAnnex *pa;
    SoftIntr priorInts;

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    tassert(rd == 0, err_printf("Sending interrupt to non-zero kernel RD.\n"));

    tassert(vp != Scheduler::GetVP(),
	    err_printf("Kernel vp %ld sending interrupt to self.\n", vp));

    if (i >= SoftIntr::MAX_INTERRUPTS) {
	/*
	 * There are no soft-interrupt bits associated with the exception-level
	 * MPMsg queues, so calls from MPMsgMgrException will have an
	 * out-of-range interrupt number.  For these we simply generate the
	 * IPI.  The IPI handler always checks the exception-level queues.
	 */
	HWInterrupt::SendIPI(vp);
    } else {
	/*
	 * For interrupts that really have soft-interrupt bits, we send the IPI
	 * only if there were no prior soft interrupts pending.  We don't
	 * bother with the requests counter.  The kernel process won't
	 * disappear.
	 */
	rc = findProcessAnnex(rd, vp, vpInfo, pa);
	tassertMsg(_SUCCESS(rc), "Bogus kernel vp number %ld.\n", vp);
	priorInts = pa->dispatcher->interrupts.fetchAndSet(i);
	if (!priorInts.pending()) {
	    HWInterrupt::SendIPI(vp);
	}
    }

    return 0;
}

SysStatus
ProcessVPList::sendInterrupt(DispatcherID dspid, SoftIntr::IntrType i)
{
    SysStatus rc;
    VPInfo *vpInfo;
    ProcessAnnex *pa;

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    if (requests.enter() < 0) {
	return _SERROR(2647, 0, ESRCH);	// process being destroyed
    }

    rc = findProcessAnnex(rd, vp, vpInfo, pa);
    if (_FAILURE(rc)) {
	requests.leave();
	return rc;
    }

    pa->sendInterrupt(i);

    requests.leave();
    return 0;
}

SysStatus
ProcessVPList::sendRemoteIPCKernel(CommID target, RemoteIPCBuffer *ipcBuf)
{
    // We can't get here except by error.
    return _SERROR(2330, 0, EINVAL);
}

SysStatus
ProcessVPList::sendRemoteIPC(CommID target, RemoteIPCBuffer *ipcBuf)
{
    SysStatus rc;
    VPInfo *vpInfo;
    ProcessAnnex *pa;

    DispatcherID dspid;
    dspid = SysTypes::DSPID_FROM_COMMID(target);
    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    if (vp == SysTypes::VP_WILD) {
	if (ipcBuf->ipcType != SYSCALL_IPC_CALL) {
	    return _SERROR(2328, 0, EINVAL);
	}
	// note eventually find closest vp, rather than just 0
	vp = 0;
    } else {
	if (ipcBuf->ipcType != SYSCALL_IPC_RTN) {
	    return _SERROR(2329, 0, EINVAL);
	}
    }

    if (requests.enter() < 0) {
	return _SERROR(2648, 0, ESRCH);	// process being destroyed
    }

    rc = findProcessAnnex(rd, vp, vpInfo, pa);
    if (_FAILURE(rc)) {
	requests.leave();
	return rc;
    }

    if (!CompareAndStore((uval *) &pa->pendingRemoteIPC, 0, (uval) ipcBuf)) {
	// target already has a remote IPC pending
	requests.leave();
	return _SERROR(2308, 0, EAGAIN);
    }

    pa->sendInterrupt(SoftIntr::PREEMPT);

    requests.leave();
    return 0;
}

SysStatus
ProcessVPList::sendRemoteAsyncMsgKernel(CommID /*target*/, CommID /*source*/,
					XHandle /*xhandle*/, uval /*methodnum*/,
					uval /*length*/, uval */*buf*/)
{
    // We can't get here except by error.
    return _SERROR(2331, 0, EINVAL);
}

SysStatus
ProcessVPList::sendRemoteAsyncMsg(CommID target, CommID source,
				  XHandle xhandle, uval methodnum,
				  uval length, uval *buf)
{
    SysStatus rc;
    VPInfo *vpInfo;
    ProcessAnnex *pa;

    DispatcherID dspid;
    dspid = SysTypes::DSPID_FROM_COMMID(target);
    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    // note eventually find closest vp, rather than just 0
    if (vp == SysTypes::VP_WILD) vp = 0;

    if (requests.enter() < 0) {
	return _SERROR(2649, 0, ESRCH);	// process being destroyed
    }

    rc = findProcessAnnex(rd, vp, vpInfo, pa);
    if (_FAILURE(rc)) {
	requests.leave();
	return rc;
    }

    /*
     * We use the VP lock to serialize access to the remote async IPC buffer.
     * A per-dispatcher lock could be used instead but probably isn't needed.
     */
    vpInfo->lock.acquire();
    rc = pa->dispatcher->asyncBufferRemote.storeMsg(source, xhandle, methodnum,
						    length, buf);
    vpInfo->lock.release();

    if (_FAILURE(rc)) {
	requests.leave();
	return rc;
    }

    pa->sendInterrupt(SoftIntr::ASYNC_MSG);

    requests.leave();
    return 0;
}
