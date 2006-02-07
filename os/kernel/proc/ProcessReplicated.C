/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessReplicated.C,v 1.109 2004/12/16 23:09:04 awaterl Exp $
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
#include "proc/ProcessReplicated.H"
#include "proc/ProcessSetKern.H"
#include "mem/FCMPrimitive.H"
#include "mem/FCMFixed.H"
#include "mem/FR.H"
#include "mem/Region.H"
#include "mem/RegionDefault.H"
#include "mem/RegionPerProcessor.H"
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
#include "meta/MetaRegionDefault.H"
#include "meta/MetaFR.H"
#include "mem/PageSet.H"
#include "defines/paging.H"
#include "defines/MLSStatistics.H"
#include "mem/PerfStats.H"
#include "ProcessDataTransferObject.H"

ProcessReplicated::Root::Root()
{
  /* empty body */
}

ProcessReplicated::Root::Root(RepRef ref) : CObjRootMultiRep(ref)
{
    /* empty body */
}

ProcessReplicated::Root::Root(RepRef ref, sval c,
                              CObjRoot::InstallDirective idir)
                             : CObjRootMultiRep(ref, c, idir)
{
    /* empty body */
}

CObjRep *
ProcessReplicated::Root::createRep(VPNum vp)
{
    ProcessReplicated *rep = new ProcessReplicated;
    // The root checks regions bounds.  The rep should allow anything.
    rep->rlst.setRegionsBounds(0, 0,uval(-1));

    return rep;
}

/* virtual */ SysStatus
ProcessReplicated::Root::getDataTransferExportSet(DTTypeSet *set)
{
    set->addType(DTT_PROCESS_DEFAULT);
    return 0;
}

/* virtual */ SysStatus
ProcessReplicated::Root::getDataTransferImportSet(DTTypeSet *set)
{
    set->addType(DTT_PROCESS_DEFAULT);
    return 0;
}

/* virtual */ DataTransferObject *
ProcessReplicated::Root::dataTransferExport(DTType dtt, VPSet transferVPSet)
{
    tassert(dtt == DTT_PROCESS_DEFAULT, err_printf("wrong transfer type\n"));

    if (transferVPSet.firstVP() != Scheduler::GetVP()) {
        /* only do transfer on first VP */
        return 0;
    }

    tassertMsg(!updatedFlagDoNotDestroy, "process being updated twice\n");
    updatedFlagDoNotDestroy = true;
    return new ProcessDataTransferObject<AllocGlobal>(&rlst, &lazyState,
            &vpList, &matched, &memTransMgr, hatRefProcess, pmRef, OSData,
            &exported);
}

/* virtual */ SysStatus
ProcessReplicated::Root::dataTransferImport(DataTransferObject *dtobj,
                                            DTType dtt, VPSet transferVPSet)
{
    if (transferVPSet.firstVP() != Scheduler::GetVP()) {
        /* only do transfer on first VP */
        return 0;
    }

    /* FIXME FIXME FIXME: assumes same allocator! */
    ProcessDataTransferObject<AllocGlobal> *obj
        = (ProcessDataTransferObject<AllocGlobal> *)dtobj;
    tassert(dtt == DTT_PROCESS_DEFAULT, err_printf("wrong transfer type\n"));

    /* just byte-copy structures
     * FIXME: assumes original object is not destroyed */
    rlst = *obj->getRlst();
    lazyState = *obj->getLazyState();
    hatRefProcess = obj->getHatRef();
    vpList = *obj->getVpList();
    matched = *obj->getMatched();
    pmRef = obj->getPmRef();
    OSData = obj->getOsData();
    memTransMgr = *obj->getMemTransMgr();
    exported = *obj->getExported();

    delete obj;

    return 0;
}

DECLARE_FACTORY_STATICS(ProcessReplicated::Factory);

/* virtual */ SysStatus
ProcessReplicated::Factory::create(ProcessRef& outPref, HATRef h, PMRef pm,
                                   ProcessRef caller, const char *name)
{
    // LOCKING: see comment top of ProcessReplicated.H
    SysStatus rc;

    // Create Root
    ProcessReplicated::Root *newProcessRoot = new ProcessReplicated::Root;

    newProcessRoot->hatRefProcess = h;
    newProcessRoot->pmRef = pm;
    newProcessRoot->factoryRef = (FactoryRef)getRef();
    newProcessRoot->updatedFlagDoNotDestroy = false;

    // works for non-kernel process only
    ProcessID pid;
    rc = ((ProcessSetKern *)DREFGOBJ(TheProcessSetRef))->getNextProcessID(pid);

    newProcessRoot->vpList.init(/*userMode*/ 1, /*isKern*/ 0, name, pid);

    outPref =(ProcessRef)newProcessRoot->getRef();

    DREF(h)->attachProcess(outPref);

    DREF(pm)->attachRef();
#ifdef PAGING_TO_SERVER
    if (pid <= 5) {
	DREF(pm)->markNonPageable();
    }
#endif

    DREFGOBJ(TheProcessSetRef)->
	addEntry(_SGETUVAL(newProcessRoot->vpList.getPID()),
		 (BaseProcessRef&)outPref);

    if (_FAILURE(rc)) {
	// FIXME: Look at this in context of the use of newProcessRoot
	DREF(outPref)->destroy();
	return rc;
    }
    ObjectHandle oh;
    rc = DREF(outPref)->giveAccessByServer(oh, _KERNEL_PID,
			    MetaObj::controlAccess | MetaProcessClient::destroy
			    |MetaProcessClient::search |MetaObj::globalHandle
			    |MetaProcessServer::destroy,
			    MetaProcessClient::search);

//    cprintf("Called ProcessReplicated::Create\n");

    registerInstance((CORef)outPref); /* FIXME: check return value */

    return 0;
}

/* virtual */ SysStatus
ProcessReplicated::Factory::createReplacement(CORef ref, CObjRoot *&root)
{
    ProcessReplicated::Root *newRoot
        = new ProcessReplicated::Root((RepRef)ref, 1, CObjRoot::skipInstall);

    newRoot->factoryRef = (FactoryRef)getRef();
    registerInstance(ref); /* FIXME: check return value */

    root = newRoot;
    return 0;
}

/* virtual */ SysStatus
ProcessReplicated::Factory::destroy(ProcessRef ref)
{
    SysStatus rc;

    rc = deregisterInstance((CORef)ref);
    if (_FAILURE(rc)) {
        return rc;
    }

    /* FIXME: what happens if it doesn't get destroyed?
     * is there a race between deregistration and destruction? */
    return DREF(ref)->reclaimSelf();
}

#ifndef USE_PROCESS_FACTORIES
/* static */ SysStatus
ProcessReplicated::Create(ProcessRef& outPref, HATRef h, PMRef pm,
			  ProcessRef caller, const char *name)
{
    // LOCKING: see comment top of ProcessReplicated.H
    SysStatus rc;

    // Create Root
    ProcessReplicated::Root *newProcessRoot = new ProcessReplicated::Root;

    newProcessRoot->hatRefProcess = h;
    newProcessRoot->pmRef = pm;
    newProcessRoot->updatedFlagDoNotDestroy = false;

    // works for non-kernel process only
    ProcessID pid;
    rc = ((ProcessSetKern *)DREFGOBJ(TheProcessSetRef))->getNextProcessID(pid);

    newProcessRoot->vpList.init(/*userMode*/ 1, /*isKern*/ 0, name, pid);

    outPref =(ProcessRef)newProcessRoot->getRef();

    DREF(h)->attachProcess(outPref);

    DREF(pm)->attachRef();
#ifdef PAGING_TO_SERVER
    if (pid <= 5) {
	DREF(pm)->markNonPageable();
    }
#endif

    DREFGOBJ(TheProcessSetRef)->
	addEntry(_SGETUVAL(newProcessRoot->vpList.getPID()),
		 (BaseProcessRef&)outPref);

    if (_FAILURE(rc)) {
	// FIXME: Look at this in context of the use of newProcessRoot
	DREF(outPref)->destroy();
	return rc;
    }
    ObjectHandle oh;
    rc = DREF(outPref)->giveAccessByServer(oh, _KERNEL_PID,
			    MetaObj::controlAccess | MetaProcessClient::destroy
			    |MetaProcessClient::search |MetaObj::globalHandle
			    |MetaProcessServer::destroy,
			    MetaProcessClient::search);

//    cprintf("Called ProcessReplicated::Create\n");
    return 0;
}
#endif

/*virtual*/ SysStatus
ProcessReplicated::attachDynamicRegion(
    uval &vaddr, uval size, RegionRef reg, RegionType::Type regionType, 
    uval alignment)
{
    AutoLock<BLock> al(&COGLOBAL(lock));

    // Attempt to attach to global list
    SysStatus rc=COGLOBAL(rlst).attachDynamicRegion(
	vaddr, size, reg, regionType, alignment);
    // If failure to attach globally just return
    if (rc != 0) {
	return rc;
    }

    // If success attach locally this should not fail.
    rc = rlst.attachFixedRegion(vaddr, size, reg, regionType);
    tassert(!rc, err_printf("Can't add region locally"));
    return rc;
}

/*virtual*/ SysStatus
ProcessReplicated::attachFixedRegion(
    uval vaddr, uval size, RegionRef reg, RegionType::Type regionType)
{
    AutoLock<BLock> al(&COGLOBAL(lock));// obtain global lock now release on exit

    // Attempt to attach to global list
    SysStatus rc=COGLOBAL(rlst).attachFixedRegion(vaddr, size, reg, regionType);

    // If failure to attach globally just return
    if (rc != 0) {
	return rc;
    }

    // If success attach locally this should not fail.
    rc = rlst.attachFixedRegion(vaddr, size, reg, regionType);
    tassert(!rc, err_printf("Can't add region locally"));
    return rc;
}

/*virtual*/ SysStatus
ProcessReplicated::attachWithinRangeRegion(
    uval &vaddr, uval vaddr2, uval size, RegionRef reg,
    RegionType::Type regionType, uval alignment)
{
    AutoLock<BLock> al(&COGLOBAL(lock));// obtain global lock now release on exit

    // Attempt to attach to global list
    SysStatus rc=COGLOBAL(rlst).attachWithinRangeRegion(
	vaddr, vaddr2, size, reg, regionType, alignment);
    // If failure to attach globally just return
    if (rc != 0) {
	return rc;
    }

    // If success attach locally this should not fail.
    rc = rlst.attachFixedRegion(vaddr, size, reg, regionType);
    tassert(!rc, err_printf("Can't add region locally"));
    return rc;
}

SysStatus
ProcessReplicated::splitRegion(uval newVaddr, uval newSize, RegionRef newReg,
                               RegionType::Type newType)
{
    SysStatus rc=0;
    RegionRef curReg;
    RegionType::Type curType;
    uval curSize, curVaddr;
    ProcessReplicated *rep;
    
    // find current region 
    rc=COGLOBAL(rlst).findRegion(newVaddr, curReg, curVaddr, curSize, curType);

    if (_FAILURE(rc)) return rc;
    
    if ((newVaddr == curVaddr) && (newSize > curSize)) {
        tassertMsg(0,"new region larger than current we do not support this\n");
//        return _SERROR(a,b,EINVAL);
        return _SERROR(0,0,EINVAL);
    }

    // set and verify bounds creating extra region if needed
    uval truncStart, truncSize;
    RegionRef extraReg=0;
    // FIXME: hand to initialized when compiling on kelf to suppress 
    //        warnings :-(
    uval extraVaddr=0, extraSize=0, extraOffset=0;
    RegionType::Type extraType=(RegionType::Type)0;

    if ((newVaddr == curVaddr) && (newSize == curSize)) {
        // new region is really a replacement note:
        //    truncates will delete region or region list element
        //    if the current sizes are passed as arguments
        truncStart=curVaddr; truncSize=curSize;
    } else if (newVaddr == curVaddr) {
        // new region goes at the front
        truncStart=curVaddr; truncSize=newSize;
    } else  if ((newVaddr + newSize) == (curVaddr + curSize)) {
        // new region goes at the end
        truncStart=newVaddr; truncSize=newSize;
    } else {
        // new region goes in the middle and requires and extra region
        // be inserted at the end
        truncStart = newVaddr;
        truncSize  = curSize - (newVaddr - curVaddr);
        extraVaddr = newVaddr + newSize ;
        extraSize  = truncSize - newSize;
        rc=DREF(curReg)->getOffset(extraOffset);
        tassertMsg(_SUCCESS(rc),"%ld: failed to get current region offset\n",rc);
        extraOffset += (extraVaddr - curVaddr);
        extraType  = curType;
        rc=DREF(curReg)->cloneUnattachedFixedAddrLen(extraReg,
                                                     extraVaddr, extraSize,
                                                     extraOffset, extraType);
        if (_FAILURE(rc)) return rc;
    }
    // stall new regions so that access don't proceed till 
    // we are done with truncating the current region
    rc=DREF(newReg)->stall();
    passertMsg(_SUCCESS(rc), 
               "%ld: new region failed to stall!!! Is there a race?\n",rc);
    if (extraReg) {
        rc=DREF(extraReg)->stall();
        passertMsg(_SUCCESS(rc), 
               "%ld: extra region failed to stall!!! Is there a race?\n",rc);
    }

    // Now mainpulate region list data structures to reflect new layout
    COGLOBAL(lock).acquire();
    
    rc=COGLOBAL(rlst).truncateAndInsertInPlace(truncStart, truncSize, curReg,
                                               newVaddr, newSize, newReg, 
                                               newType,
                                               extraVaddr, extraSize, extraReg,
                                               extraType);
        
    if (_FAILURE(rc)) {
        // cleanup here need to put things back the way they where including
        // destroying extraRegion if it was created.
        passertMsg(0, "%ld: hmmm we need to cleanup if the truncate failed\n",
                   rc);
    }
        
    // If success truncate local copies
    COGLOBAL(lockReps());    // lock reps (stop creation of new reps)
    /* Loop through all reps and detach region from local list.
       Without a more complex protocol we must do detach to all
       reps even though they may not have the region in there
       local list.  Currently we data ship this operation.
       Functionshiping maybe more appropriate depending on
       how local memory on deallocated on a remote processor is
       reclaimed */
    for (void *curr=COGLOBAL(nextRep(0,(CObjRep *&)rep));
         curr; curr=COGLOBAL(nextRep(curr,(CObjRep *&)rep))) {
        (void)rep->rlst.detachRegion(curReg);  
    }
    COGLOBAL(unlockReps());    // Done looping through reps unlock.
    
    COGLOBAL(lock).release();
        
    /**********************************************************************
     *  Region list and Regions are now inconsistent and no locks are held
     **********************************************************************/
    
    // At this point region lists are updated but the current region 
    // has not been resized and the new and extra regions are still stalled
    // Operations that might be sensitive such as faults must use a retry.
    
    // All locks have been dropped we can now resize current region.
    // Note this may cause the current region to be destroyed
    // We rely on truncate to do all the mapping manipulations.
    // I assume that in the case in which the region is not using shared
    // segments and has only been accessed on one processor then the
    // unmap happens efficiently (eg is not broadcast).
    DREF(curReg)->truncate(truncStart, truncSize);

    /**********************************************************************
     *  Region list and Regions are now consistent aagain
     **********************************************************************/

    // ok now unstall the new and extra regions
    DREF(newReg)->unStall();
    if (extraReg) DREF(extraReg)->unStall();
    
    return rc;
}

#ifdef PROTOTYPE_SUBLIST
/* virtual */ SysStatus 
ProcessReplicated::attachFixedSubRegion(uval newVaddr, uval newSize,
                                        RegionRef newReg,
                                        RegionType::Type newType)
{
    ProcessReplicated *rep;
    RegionRef curReg;
    AutoLock<BLock> al(&COGLOBAL(lock));//obtain global lock now release on exit

    // Attempt to attach to global list
    SysStatus rc=COGLOBAL(rlst).attachFixedSubRegion(newVaddr, newSize, 
                                                     newReg, newType, curReg);

    // If failure to attach globally just return
    if (rc != 0) {
	return rc;
    }

    COGLOBAL(lockReps());    // lock reps (stop creation of new reps)
    /* Loop through all reps and detach region from local list.
       Without a more complex protocol we must do detach to all
       reps even though they may not have the region in there
       local list.  Currently we data ship this operation.
       Functionshiping maybe more appropriate depending on
       how local memory on deallocated on a remote processor is
       reclaimed */
    for (void *curr=COGLOBAL(nextRep(0,(CObjRep *&)rep));
	 curr; curr=COGLOBAL(nextRep(curr,(CObjRep *&)rep))) {
        (void)rep->rlst.detachRegion(curReg);  
    }
    COGLOBAL(unlockReps());    // Done looping through reps unlock.


    return rc;
}
#endif

/*virtual*/ SysStatus
ProcessReplicated::detachRegion(RegionRef reg) {
    /* FIXME:  If Memory deallocation needs to be done locally then
               use function shipping */
    AutoLock<BLock> al(&COGLOBAL(lock)); // obtain global lock now release exit
    ProcessReplicated *rep;

    COGLOBAL(lockReps());    // lock reps (stop creation of new reps)
    /* Loop through all reps and detach region from local list.
       Without a more complex protocol we must do detach to all
       reps even though they may not have the region in there
       local list.  Currently we data ship this operation.
       Functionshiping maybe more appropriate depending on
       how local memory on deallocated on a remote processor is
       reclaimed */
    for (void *curr=COGLOBAL(nextRep(0,(CObjRep *&)rep));
	 curr; curr=COGLOBAL(nextRep(curr,(CObjRep *&)rep))) {
	(void)rep->rlst.detachRegion(reg);  // Ignoring failures
    }
    COGLOBAL(unlockReps());    // Done looping through reps unlock.

    return COGLOBAL(rlst).detachRegion(reg); // Remove from global list
}

/*virtual*/ SysStatusUval
ProcessReplicated::handleFault(AccessMode::pageFaultInfo pfinfo, uval vaddr,
			       PageFaultNotification *pn, VPNum vp)
{
    RegionRef reg;
    SysStatus rc;
    StatTimer timer(RegionLookup);
    ScopeTime scope(ProcessTimer);
    MLSStatistics::StartTimer(0);

  retry:
    // look in local list global lock not required
    MLSStatistics::StartTimer(1);
    rc = rlst.vaddrToRegion(vaddr, reg);
    MLSStatistics::DoneTimer(1);

    if (rc != 0) {
	// Not found in local list must search global
	COGLOBAL(lock).acquire();                // obtain global lock
	rc=COGLOBAL(rlst).vaddrToRegion(vaddr, reg); // search global list

	if (rc == 0) {  // Found in global list then add to local list
	    uval rVaddr;
	    uval rSize;
	    DREF(reg)->getVaddr(rVaddr);
	    DREF(reg)->getSize(rSize);
	    // Defaulting alignment in local list
	    // bogus, get rid of when local list doesn't require
	    rlst.attachFixedRegion(rVaddr, rSize, reg, RegionType::K42Region);
	}
	COGLOBAL(lock).release(); // release global lock
    }

    if (rc != 0) {
	err_printf("%s,%d: Invalid memory access: "
		   "processID 0x%lx addr 0x%lx, type %lx\n",
		   __FILE__, __LINE__,
		   _SGETUVAL(getPID()), vaddr,pfinfo);
	MLSStatistics::DoneTimer(0);
	return rc;
    }

    MLSStatistics::StartTimer(2);
    timer.record();
    rc = DREF(reg)->handleFault(pfinfo, vaddr, pn, vp);

    // if the region says that it does not handle this
    // address then the region and region list are out of sync.
    // We must have caugth things in the middle of a truncate.
    // Simply retry until things are back in sync.
    //  FIXME:  do better with return types!!!
    if (_FAILURE(rc) && (_SCLSCD(rc) == Region::AddressOutOfRange))
	goto retry;

    /* Increment our page fault count, using the fact that Region
       objects return 0 from their handleFault method if the page was
       in-core and a positive value if it was out-of-core.  We cannot
       assume that C++ zero-initializes the fields of our resource
       usage structure, so we zero them in createRep.  We do not take
       any locks for this increment.  When userspace asks for this
       information via the getrusage system call our root object will
       sum over the representative list.  */
    if (rc == 0) {
	this->rusage.minflt++;
    } else if (rc > 0) {
	this->rusage.majflt++;
    }

    MLSStatistics::DoneTimer(2);

    MLSStatistics::DoneTimer(0);

    return rc;
}

#if 0 //MAA
/*virtual*/ SysStatus
ProcessReplicated::getMemory(__in uval size, __inout uval &vaddr)
{
    // LOCKING: none needed, only internal access is getRef,
    // comes back and allocates region via region list
    SysStatus rc;
    FCMRef fcmRef;
    RegionRef regionRef;

    size = PAGE_ROUND_UP(size);

//    cprintf("in process default alloc region %ld\n", size);

    // create region on the users behalf with the size and memory type desired
    rc = FCMPrimitive<PageSet<AllocGlobal>, AllocGlobal>::Create(fcmRef);
    if (rc) return rc;

    rc = RegionDefault::CreateFixedLen(regionRef, getRef(),
				       vaddr, size, 0, fcmRef, 0,
				       AccessMode::writeUserWriteSup);
    return rc;
}
#endif /* #if 0 //MAA */

/*virtual*/ SysStatus
ProcessReplicated::userHandleFault(uval vaddr, VPNum vp)
{
    // LOCKING: none needed, see handleFault
    return handleFault(AccessMode::readFault, vaddr, NULL, vp);
}

/*virtual*/ SysStatus
ProcessReplicated::destroy()
{
    SysStatus rc;

    /* FIXME: we want to destroy ourselves, just not the nested data structures
     * we have passed on in a hot-swap */
    if (COGLOBAL(updatedFlagDoNotDestroy)) {
        return 0;
    }

    /*
     * delete all the fault notifications and annexes
     * N.B. ProcessVPList synchronizes this to guarantee that any
     * creates in progress complete before the deleteAll, and that
     * no new creates will succeed once the deleteAll starts
     */
    COGLOBAL(vpList).deleteAll();

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    // **************************************************************
    // From here on, only one destroy can be in progress.  see
    // closeExportedXObjectList for implementation of this syncronization

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
    // Note : region destroy occurs on one processor but detach is distributed
    //        to all processors ensuring local lists are cleaned up
    COGLOBAL(rlst).deleteRegionsAll();

    // now free the HAT
    DREF(COGLOBAL(hatRefProcess))->destroy();

    COGLOBAL(lazyState).detach();

    // tell pm we are releasing our reference
    DREF(COGLOBAL(pmRef))->detachRef();

    // more to do here ...
    // ports and procs - unless we just punt till they go away.

#ifdef USE_PROCESS_FACTORIES
    DREF(COGLOBAL(factoryRef))->destroy(getRef());
#else
    destroyUnchecked();
#endif

    return (0);
}

/*virtual*/ SysStatus
ProcessReplicated::reclaimSelf()
{
    // schedule the object for deletion
    return destroyUnchecked();
}

/*virtual*/ SysStatus
ProcessReplicated::kill()
{
    // 0 for process replicated 1 for process shared
    TraceOSUserProcKill(getPID(), 0);
    destroy();
    TraceOSUserProcKillDone(getPID(), 0);
    return 0;
}

/*virtual*/ SysStatus
ProcessReplicated::regress()
{
    extern void runRegression();

    runRegression();
    return 0;
}

/*virtual*/ void
ProcessReplicated::kosher()
{
    COGLOBAL(rlst).kosher();
}
    /**********************************************************************
      below are functions pulled into process to do user-level loading
      some of them maybe should be first class calls of their own while
      others will remain here
    ***********************************************************************/

/*virtual*/ SysStatus
ProcessReplicated::regionDestroy(uval regionAddr)
{
    RegionRef regRef;
    SysStatus rc;
    //FIXME:  Why is this only looking at local list????
    rc = vaddrToRegion(regionAddr, regRef);
    if (rc) return rc;
    return DREF(regRef)->destroy();
}

/*virtual*/ SysStatus
ProcessReplicated::regionTruncate(uval start, uval length)
{
    RegionRef regRef;
    ProcessReplicated *rep;

    COGLOBAL(lock).acquire();
    // Attempt to truncate region in  global list
    SysStatus rc=COGLOBAL(rlst).truncate(start, length, regRef);
    // If failure to truncate globally just return
    if (_FAILURE(rc)) {
        COGLOBAL(lock).release();
	return rc;
    }

    // If success truncate local copies
    COGLOBAL(lockReps());    // lock reps (stop creation of new reps)
    /* Loop through all reps and detach region from local list.
       Without a more complex protocol we must do detach to all
       reps even though they may not have the region in there
       local list.  Currently we data ship this operation.
       Functionshiping maybe more appropriate depending on
       how local memory on deallocated on a remote processor is
       reclaimed */
    for (void *curr=COGLOBAL(nextRep(0,(CObjRep *&)rep));
	 curr; curr=COGLOBAL(nextRep(curr,(CObjRep *&)rep))) {
        // FIXME: is this really right?  Should we not just remove reference
        // in local list to be safe eg. (void)rep->rlst.detachRegion(regRef);  
	(void)rep->rlst.truncate(start, length, regRef);  // Ignoring failures
    }
    COGLOBAL(unlockReps());    // Done looping through reps unlock.

    COGLOBAL(lock).release();
    return DREF(regRef)->truncate(start, length);
}

/*virtual*/ SysStatus
ProcessReplicated::unmapRange(uval start, uval size)
{
    RegionRef regRef;
    SysStatus rc;
    /* do this via the region since the region knows which
     * processors to visit
     */
    //FIXME must loop through all regions in range
    // FIXME: why is this only looking at local list?????
    rc = vaddrToRegion(start, regRef);
    if (rc) return rc;
    return DREF(regRef)->unmapRange(start,size);
}

/*virtual*/ SysStatus
ProcessReplicated::createDispatcher(CPUDomainAnnex *cda, DispatcherID dspid,
				    EntryPointDesc entry, uval dispatcherAddr,
				    uval initMsgLength, char *initMsg)
{
    return COGLOBAL(vpList).createDispatcher(cda, dspid, entry, dispatcherAddr,
					     initMsgLength, initMsg,
					     getRef(), COGLOBAL(hatRefProcess));
}

/*virtual*/ SysStatus
ProcessReplicated::detachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid)
{
    return COGLOBAL(vpList).detachDispatcher(cda, dspid,
					     COGLOBAL(hatRefProcess));
}

/*virtual*/ SysStatus
ProcessReplicated::attachDispatcher(CPUDomainAnnex *cda, DispatcherID dspid)
{
    return COGLOBAL(vpList).attachDispatcher(cda, dspid,
					     COGLOBAL(hatRefProcess));
}

/*virtual*/ SysStatus
ProcessReplicated::waitForTermination()
{
    ProcessID tmppid = _SGETUVAL(getPID());
    BaseProcessRef tmpref;
    while (_SUCCESS(DREFGOBJ(TheProcessSetRef)->
				getRefFromPID(tmppid, tmpref))) {
	Scheduler::DeactivateSelf();
	Scheduler::DelayMicrosecs(100000);
	Scheduler::ActivateSelf();
    }

    return 0;
}

/*virtual*/ SysStatusUval
ProcessReplicated::ppCount()
{
    return DREFGOBJK(TheProcessRef)->vpCount();
}

#include <stub/StubUsrTst.H>


/*virtual*/ SysStatus
ProcessReplicated::testUserIPC(ObjectHandle oh)
{
    ProcessReplicated::ObjectHandleHolder *ohHolder =
	new ProcessReplicated::ObjectHandleHolder;
    ohHolder->oh = oh;
    cprintf("testUserIPC: creating thread to call user.\n");
    Scheduler::ScheduleFunction(ProcessReplicated::DoUserIPC, (uval) ohHolder);
    for (uval i = 0; i < 5; i++) {
	cprintf("testUserIPC: sleeping (%ld).\n", i);
	Scheduler::DelayMicrosecs(5000);
    }
    cprintf("testUserIPC: returning.\n");
    return 0;
}

#include <stub/StubUsrTst.H>


/*static*/ void
ProcessReplicated::DoUserIPC(uval ohHolderArg)
{
    ObjectHandleHolder *ohHolder = (ObjectHandleHolder *) ohHolderArg;

    StubUsrTst stub(StubObj::UNINITIALIZED);
    stub.setOH(ohHolder->oh);

    cprintf("DoUserIPC: calling user\n");
    stub.gotYa(15);
    cprintf("DoUserIPC: done calling user\n");

    delete ohHolder;
}

/* virtual */ SysStatus
ProcessReplicated::getRUsage(BaseProcess::ResourceUsage& usage) {
    void *curr;
    ProcessReplicated *rep;

    /* Zero the fields, because we are going to sum over our reps.  */
    usage.minflt = 0;
    usage.majflt = 0;

    /* Acquire lock on representative list.  */
    COGLOBAL(lockReps());

    /* For each representative, sum its statistics.  */
    for (curr = COGLOBAL(nextRep(0, (CObjRep *&)rep));
	 curr; curr = COGLOBAL(nextRep(curr, (CObjRep *&)rep))) {
	usage.minflt += rep->rusage.minflt;
	usage.majflt += rep->rusage.majflt;
    }

    /* Release our lock.  */
    COGLOBAL(unlockReps());

    return 0; 
}
