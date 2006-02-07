/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessShared.C,v 1.85 2004/10/08 21:40:09 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include <misc/macros.H>
#include "proc/ProcessShared.H"
#include "proc/ProcessSetKern.H"
#include "mem/FCMPrimitive.H"
#include "mem/FCMFixed.H"
#include "mem/FR.H"
#include "mem/Region.H"
#include "mem/RegionDefault.H"
#include "mem/HATDefault.H"
#include "mem/HATKernel.H"
#include "mem/PageAllocatorKern.H"
#include "mem/PageFaultNotification.H"
#include "mem/PM.H"
#include <sys/Dispatcher.H>
#include <sys/memoryMap.H>
#include <trace/trace.H>
#include <trace/traceUser.h>
#include <cobj/TypeMgr.H>
#include <cobj/XHandleTrans.H>
#include <scheduler/Scheduler.H>
#include <sys/ProcessSet.H>
#include <cobj/CObjRootSingleRep.H>
#include "meta/MetaRegionDefault.H"
#include "meta/MetaFR.H"
#include "mem/PageSetDense.H"
#include "mem/PerfStats.H"
#include "ProcessDataTransferObject.H"

template<class ALLOC>
/* virtual */ SysStatus
ProcessShared<ALLOC>::Root::getDataTransferExportSet(DTTypeSet *set)
{
    set->addType(DTT_PROCESS_DEFAULT);
    return 0;
}

template<class ALLOC>
/* virtual */ SysStatus
ProcessShared<ALLOC>::Root::getDataTransferImportSet(DTTypeSet *set)
{
    set->addType(DTT_PROCESS_DEFAULT);
    return 0;
}

template<class ALLOC>
/* virtual */ DataTransferObject *
ProcessShared<ALLOC>::Root::dataTransferExport(DTType dtt, VPSet transferVPSet)
{
    ProcessShared<ALLOC> *rep = (ProcessShared<ALLOC> *)therep;
    tassert(dtt == DTT_PROCESS_DEFAULT, err_printf("wrong transfer type\n"));

    if (transferVPSet.firstVP() != Scheduler::GetVP()) {
        /* only do transfer on first VP */
        return 0;
    }

    tassertMsg(!rep->updatedFlagDoNotDestroy, "process being updated twice\n");
    rep->updatedFlagDoNotDestroy = true;

    return new ProcessDataTransferObject<ALLOC>(
            &rep->rlst, &rep->lazyState, &rep->vpList, &rep->matched,
            &rep->memTransMgr, rep->hatRefProcess, rep->pmRef, rep->OSData,
            &exported);
}

template<class ALLOC>
/* virtual */ SysStatus
ProcessShared<ALLOC>::Root::dataTransferImport(DataTransferObject *dtobj,
                                               DTType dtt, VPSet transferVPSet)
{
    if (transferVPSet.firstVP() != Scheduler::GetVP()) {
        /* only do transfer on first VP */
        return 0;
    }

    /* FIXME FIXME FIXME: assumes same allocator! */
    ProcessDataTransferObject<ALLOC> *obj
        = (ProcessDataTransferObject<ALLOC> *)dtobj;
    ProcessShared<ALLOC> *rep = (ProcessShared<ALLOC> *)therep;
    tassert(dtt == DTT_PROCESS_DEFAULT, err_printf("wrong transfer type\n"));

    /* just byte-copy structures
     * FIXME: assumes original object is not destroyed */
    rep->rlst = *obj->getRlst();
    rep->lazyState = *obj->getLazyState();
    rep->hatRefProcess = obj->getHatRef();
    rep->vpList = *obj->getVpList();
    rep->matched = *obj->getMatched();
    rep->pmRef = obj->getPmRef();
    rep->OSData = obj->getOsData();
    rep->memTransMgr = *obj->getMemTransMgr();
    exported = *obj->getExported();

    delete obj;

    return 0;
}

template<class ALLOC>
ProcessShared<ALLOC>::ProcessShared(HATRef h, PMRef pm, uval um, uval ik,
				    const char *name)
{
    hatRefProcess	= h;
    pmRef               = pm;

    ProcessID pid;
    if (ik) {
	pid = _KERNEL_PID;
    } else {
	// need kernel version
	((ProcessSetKern *)DREFGOBJ(TheProcessSetRef))->getNextProcessID(pid);
    }

    vpList.init(um, ik, name, pid);
}

DECLARE_TEMPLATED_FACTORY_STATICS(<class ALLOC>, ProcessShared<ALLOC>::Factory);

template<class ALLOC>
/* virtual */ SysStatus
ProcessShared<ALLOC>::Factory::create(ProcessRef& outPref, HATRef h, PMRef pm,
                                      ProcessRef caller, const char *name)
{
    // LOCKING: see comment top of ProcessShared.H
    SysStatus rc = 0;
    ProcessShared* newProcess =
	new ProcessShared(h, pm, /*userMode*/ 1, /*isKern*/ 0, name);
    newProcess->factoryRef = (FactoryRef)getRef();
    newProcess->updatedFlagDoNotDestroy = false;

    // Enter Process into the Translation Table
    new typename ProcessShared<ALLOC>::Root(newProcess);
    outPref = (ProcessRef)newProcess->getRef();

    DREF(h)->attachProcess(outPref);

    DREF(pm)->attachRef();

    DREFGOBJ(TheProcessSetRef)->
	addEntry(_SGETUVAL(newProcess->getPID()), (BaseProcessRef&)outPref);

    if (_FAILURE(rc)) {
	newProcess->destroy();
	return rc;
    }

    registerInstance((CORef)outPref); /* FIXME: check return value */

    return 0;
}

template<class ALLOC>
/* virtual */ SysStatus
ProcessShared<ALLOC>::Factory::createReplacement(CORef ref, CObjRoot *&root)
{
    ProcessShared* newProcess = new ProcessShared();
    newProcess->factoryRef = (FactoryRef)getRef();
    root = new typename ProcessShared<ALLOC>::Root(newProcess, (RepRef)ref,
                                          CObjRoot::skipInstall);
    registerInstance(ref); /* FIXME: check return value */
    return 0;
}

template<class ALLOC>
/* virtual */ SysStatus
ProcessShared<ALLOC>::Factory::destroy(ProcessRef ref)
{
    SysStatus rc;

    rc = deregisterInstance((CORef)ref);
    if (!_SUCCESS(rc)) {
        return rc;
    }

    /* FIXME: what happens if it doesn't get destroyed?
     * is there a race between deregistration and destruction? */
    return DREF(ref)->reclaimSelf();
}

#ifndef USE_PROCESS_FACTORIES
template<class ALLOC>
/* static */ SysStatus
ProcessShared<ALLOC>::Create(ProcessRef& outPref, HATRef h, PMRef pm,
		      ProcessRef caller, const char *name)
{
    // LOCKING: see comment top of ProcessShared.H
    SysStatus rc = 0;
    ProcessShared* newProcess =
	new ProcessShared(h, pm, /*userMode*/ 1, /*isKern*/ 0, name);
    newProcess->updatedFlagDoNotDestroy = false;

    // Enter Process into the Translation Table
    new typename ProcessShared<ALLOC>::Root(newProcess);
    outPref = (ProcessRef)newProcess->getRef();

    DREF(h)->attachProcess(outPref);

    DREF(pm)->attachRef();

    DREFGOBJ(TheProcessSetRef)->
	addEntry(_SGETUVAL(newProcess->getPID()), (BaseProcessRef&)outPref);

    if (_FAILURE(rc)) {
	newProcess->destroy();
	return rc;
    }

    return 0;
}
#endif

template<class ALLOC>
/*virtual*/ SysStatusUval
ProcessShared<ALLOC>::handleFault(AccessMode::pageFaultInfo pfinfo, uval vaddr,
				  PageFaultNotification *pn, VPNum vp)
{
    // LOCKING: internal to region list
    RegionRef reg;
    SysStatus rc;
    ScopeTime scope(ProcessTimer);
    StatTimer timer(RegionLookup);

    rc = rlst.vaddrToRegion(vaddr, reg);

    if (rc != 0) {
	err_printf("%s,%d: Invalid memory access: "
		   "processID 0x%lx addr 0x%lx, type %ld\n",
		   __FILE__, __LINE__,
		   _SGETUVAL(getPID()), vaddr,pfinfo);
	return rc;
    }

    timer.record();
    rc = DREF(reg)->handleFault(pfinfo, vaddr, pn, vp);

    return rc;
}

#if 0 //MAA
template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::getMemory(__in uval size, __inout uval &vaddr)
{
    // LOCKING: none needed, only internal access is getRef,
    // comes back and allocates region via region list
    SysStatus rc;
    FCMRef fcmRef;
    RegionRef regionRef;

    size = PAGE_ROUND_UP(size);

//    cprintf("in process default alloc region %ld\n", size);

    // create region on the users behalf with the size and memory type desired
    rc = FCMPrimitive<PageSet<AllocGlobal>,AllocGlobal>::Create(fcmRef);
    if (rc) return rc;

    rc = RegionDefault::CreateFixedLen(regionRef, getRef(),
				       vaddr, size, 0, fcmRef, 0,
				       AccessMode::writeUserWriteSup);
    return rc;
}
#endif /* #if 0 //MAA */

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::userHandleFault(uval vaddr, VPNum vp)
{
    // LOCKING: none needed, see handleFault
    return handleFault(AccessMode::readFault, vaddr, NULL, vp);
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::destroy()
{
    SysStatus rc;

    /* FIXME: we want to destroy ourselves, just not the nested data structures
     * we have passed on in a hot-swap */
    if (updatedFlagDoNotDestroy) {
        return 0;
    }

    // delete all the fault notifications and annexes
    vpList.deleteAll();

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    // **************************************************************
    // From here on, only one destroy can be in progress.  see
    // closeExportedXObjectList for implementation of this syncronization

    lazyState.detach();

    // remove all ObjRefs owned by the process
    rc=closeMatchedXObjectList();
    tassert(_SUCCESS(rc),
	    err_printf("Free of process owned ObjRefs failed %p\n", this));

    // remove reference to us from processset - must be done after
    // closeMatchedXObjectList since XBaseObjs record ProcID's and must
    // be able to convert them to ProcessRefs.
    rc = DREFGOBJ(TheProcessSetRef)->remove(_SGETUVAL(getPID()));
    tassertWrn(_SUCCESS(rc), "Removing pid failed.\n");

    // delete all the regions we are using
    rlst.deleteRegionsAll();

    // now free the HAT
    DREF(hatRefProcess)->destroy();

    // tell pm we are releasing our reference
    DREF(pmRef)->detachRef();

    // more to do here ...
    // ports and procs - unless we just punt till they go away.

#ifdef USE_PROCESS_FACTORIES
    DREF(factoryRef)->destroy(getRef());
#else
    destroyUnchecked();
#endif

    return (0);
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::reclaimSelf()
{
    // schedule the object for deletion
    return destroyUnchecked();
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::kill()
{
    // 0 for process replicated 1 for process shared
    TraceOSUserProcKill(getPID(), 1);
    destroy();
    TraceOSUserProcKillDone(getPID(), 1);
    return 0;
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::regress()
{
    extern void runRegression();

    runRegression();
    return 0;
}

template<class ALLOC>
/*virtual*/ void
ProcessShared<ALLOC>::kosher()
{
    rlst.kosher();
}
    /**********************************************************************
      below are functions pulled into process to do user-level loading
      some of them maybe should be first class calls of their own while
      others will remain here
    ***********************************************************************/

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::regionDestroy(uval regionAddr)
{
    SysStatus rc;
    RegionRef regRef;
    rc = vaddrToRegion(regionAddr, regRef);
    if (rc) return rc;
    return DREF(regRef)->destroy();
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::regionTruncate(uval start, uval length)
{
    SysStatus rc;
    RegionRef regRef;
    rc = rlst.truncate(start, length, regRef);
    if (_FAILURE(rc)) return rc;
    return DREF(regRef)->truncate(start, length);
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::unmapRange(uval start, uval size)
{
    RegionRef regRef;
    SysStatus rc;
    /* do this via the region since the region knows which
     * processors to visit
     */
    //FIXME must loop through all regions in range
    rc = vaddrToRegion(start, regRef);
    if (rc) return rc;
    return DREF(regRef)->unmapRange(start,size);
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::createDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
				       EntryPointDesc entry,
				       uval dispatcherAddr,
				       uval initMsgLength, char *initMsg)
{
    return vpList.createDispatcher(cda, dspid, entry, dispatcherAddr,
				   initMsgLength, initMsg,
				   getRef(), hatRefProcess);
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::detachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid)
{
    return vpList.detachDispatcher(cda, dspid, hatRefProcess);
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::attachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid)
{
    return vpList.attachDispatcher(cda, dspid, hatRefProcess);
}

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::waitForTermination()
{
    ProcessID tmppid = _SGETUVAL(getPID());
    BaseProcessRef tmpref;
    Scheduler::DeactivateSelf();
    while (_SUCCESS(DREFGOBJ(TheProcessSetRef)->
				getRefFromPID(tmppid, tmpref))) {
	// cprintf("waiting for \"%s\".\n", name.getString());
	Scheduler::DelayMicrosecs(100000);
    }
    Scheduler::ActivateSelf();

    return 0;
}

template<class ALLOC>
/*virtual*/ SysStatusUval
ProcessShared<ALLOC>::ppCount()
{
    return DREFGOBJK(TheProcessRef)->vpCount();
}

#include <stub/StubUsrTst.H>

template<class ALLOC>
/*virtual*/ SysStatus
ProcessShared<ALLOC>::testUserIPC(ObjectHandle oh)
{
    typename ProcessShared<ALLOC>::ObjectHandleHolder *ohHolder =
	new typename ProcessShared<ALLOC>::ObjectHandleHolder;
    ohHolder->oh = oh;
    cprintf("testUserIPC: creating thread to call user.\n");
    Scheduler::ScheduleFunction(
	ProcessShared<ALLOC>::DoUserIPC, (uval) ohHolder);
    for (uval i = 0; i < 5; i++) {
	cprintf("testUserIPC: sleeping (%ld).\n", i);
	Scheduler::DelayMicrosecs(5000);
    }
    cprintf("testUserIPC: returning.\n");
    return 0;
}

#include <stub/StubUsrTst.H>


template<class ALLOC>
/*static*/ void
ProcessShared<ALLOC>::DoUserIPC(uval ohHolderArg)
{
    ObjectHandleHolder *ohHolder = (ObjectHandleHolder *) ohHolderArg;

    StubUsrTst stub(StubObj::UNINITIALIZED);
    stub.setOH(ohHolder->oh);

    cprintf("DoUserIPC: calling user\n");
    stub.gotYa(15);
    cprintf("DoUserIPC: done calling user\n");

    delete ohHolder;
}

//template instantiate
template class ProcessShared<AllocPinnedGlobalPadded>;
template class ProcessShared<AllocGlobalPadded>;
