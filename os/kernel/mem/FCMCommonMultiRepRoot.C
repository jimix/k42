 /******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMCommonMultiRepRoot.C,v 1.36 2004/10/08 21:40:08 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared services for keeping track of regions
 * and pages.  Most FCM's are built on this
 * **************************************************************************/

#include <kernIncs.H>
#include <proc/Process.H>
#include "FCMCommonMultiRepRoot.H"
#include <mem/HAT.H>
#include <mem/Region.H>
#include <mem/CacheSync.H>
#include <mem/PM.H>
#include <mem/SegmentHATPrivate.H>
#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>
#include <trace/traceMem.h>
#include <mem/PageSet.H>
#include <mem/PageSetDense.H>
#include <mem/PageFaultNotification.H>
#include <mem/PageCopy.H>
#include <mem/PerfStats.H>

template<class BASE,class GALLOC,class LALLOC>
void
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::commonInit()
{
    pmRef	     = uninitPM();
    referenceCount   = 0;
    pageable	     = 0;
    backedBySwap     = 0;
    priv	     = 1;
    beingDestroyed   = 0;
    noSharedSegments = KernelInfo::ControlFlagIsSet(
	KernelInfo::NO_SHARED_SEGMENTS)?1:0;
    //FIXME when locked_cacheSyncAll is implememted make this zero
    mappedExecutable = 1;
    frRef            = NULL;
    pageSize         = PAGE_SIZE;
}

template<class BASE, class GALLOC,class LALLOC>
FCMCommonMultiRepRoot<BASE,GALLOC,
                      LALLOC>::FCMCommonMultiRepRoot(uval numPages) :
                          masterDHashTable(numPages)
{
//    err_printf("CommonRoot numPages=%ld\n", numPages);
    commonInit();
}

template<class BASE, class GALLOC,class LALLOC>
FCMCommonMultiRepRoot<BASE,GALLOC,
                      LALLOC>::FCMCommonMultiRepRoot(uval numPages,
                                                     uval minSize) :
                          masterDHashTable(numPages,minSize)
{
//    err_printf("CommonRoot numPages=%ld minSize=%ld\n", numPages,
//               minSize);
    commonInit();
}

template<class BASE,class GALLOC,class LALLOC>
void
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::doSetCacheSynced(
    LocalPageDescData *pg)
{
    setPFBit(DistribRoot);
    masterDHashTable.doOp(pg->getKey(), &MasterPageDescData::doCacheSync,
			  &LocalPageDescData::doSetCacheSync, 0);
}

template<class BASE,class GALLOC,class LALLOC>
void
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::doSetDirty(LocalPageDescData *pg)
{
    setPFBit(DistribRoot);
    masterDHashTable.doOp(pg->getKey(), &MasterPageDescData::doDirty,
			  &LocalPageDescData::doSetDirty, 0);
}

/*
 * part of MapPageInHAT that deals with missing segment.
 * acckey is original access mode - access has already been updated
 * by detectModified, but we must use the original mode to find the
 * shared segment if one exists.
 */
template<class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::noSegMapPageInHAT(
    uval paddr, uval vaddr, uval len, AccessMode::pageFaultInfo pfinfo,
    AccessMode::mode access, VPNum vp, HATRef hat, RegionRef reg,
    uval offset, uval acckey)
{
    SysStatus rc;
    setPFBit(DistribRoot);
    uval found;
    SegmentHATByAccessModeList *byAccessList;
    SegmentHATRef seghat;

    uval offkey = offset - vaddr + vaddr&~SEGMENT_MASK;

    // find the correct list-by-access-mode based on offset
    found = segmentHATList.find(offkey, byAccessList);
    if (!found) {
	byAccessList = new SegmentHATByAccessModeList;
	tassert(byAccessList != NULL, err_printf("oops\n"));
	// add byaccess list to list by offset
	segmentHATList.add(offkey, byAccessList);
    }

    // now look for seghat for correct access mode
    found = byAccessList->find(acckey, seghat);
    if (!found) {
	// create segmentHAT for offset and access mode
	setPFBit(CreatSeg);
	rc = SegmentHATShared::Create(seghat);
	tassert(_SUCCESS(rc), err_printf("oops\n"));

	// add seghat to byaccess list
	byAccessList->add(acckey, seghat);
    }

    // now attach segment to HAT
    rc = DREF(hat)->attachRegion(vaddr&~SEGMENT_MASK,SEGMENT_SIZE,seghat);
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    // have segment attached, retry map request
    // FIXME wasMapped logic needed
    rc = DREF(hat)->mapPage(paddr, vaddr, len, pfinfo, access, vp, 1, 0);
    tassert(rc != HAT::NOSEG, err_printf("oops\n"));

    return rc;
}

// If something other than a region needs to prevent FCM destruction
// is must add a reference (count).  see ProcessVPList for example.
template<class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::addReference()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    setPFBit(DistribRoot);
    referenceCount++;
    return 0;
}

template<class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::locked_removeReference()
{
    _ASSERT_HELD(lock);
    referenceCount--;
    if (!referenceCount && regionList.isEmpty()) {
	lock.release();
	return notInUse();
    } else {
	lock.release();
	return 0;
    }
}

/*
 *N.B. this logic is unfortunately shadowed in detachForkChild
 *if you make changes, look there as well
 */
template<class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::removeReference()
{
    setPFBit(DistribRoot);
    lock.acquire();
    return locked_removeReference();
}

template<class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::attachRegion(
    RegionRef regRef, PMRef newPM, AccessMode::mode accessMode)
{
    SysStatus rc = 0;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    if (beingDestroyed) return -1;

    //err_printf("FCM %lx attach from reg %lx / pm %lx\n",
    //       getRef(), regRef, newPM);

    regionList.add(regRef, new RegionInfo(newPM));

    // only change PM for private FCMs and only if we don't have one yet
    if (priv && uninitPM(pmRef)) {
	//err_printf("attachRegion: switching from pm %lx to %lx\n",
	//   uninitPM(pmRef) ? pmRef : 0, newPM);
	if (!uninitPM(pmRef)) DREF(pmRef)->detachFCM((FCMRef)this->getRef());
	pmRef = newPM;
	DREF(pmRef)->attachFCM((FCMRef)this->getRef());
    }

    if(AccessMode::isExecute(accessMode) && !mappedExecutable) {
	passertMsg(0, "implement me\n");
	// locked_cacheSyncAll();
	mappedExecutable = 1;
    }
    
    TraceOSMemFCMCOMAtchReg((uval)regRef, (uval64)this->getRef());

    return rc;
}

template<class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::destroy()
{
    SysStatus rc;

    lock.acquire();

    if (beingDestroyed) {
	lock.release();
	return 0;
    }

    beingDestroyed = 1;

    rc = doDestroy();


    lock.release();

    return rc;
}

// Lock here only protects global data.  It does not sycn calls
// for mapping and unmapping
template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::doDestroy()
{
    SegmentHATByAccessModeList *byAccessList;
    SegmentHATRef seghat;
    uval offkey, acckey;
    SysStatus rc;

    _ASSERT_HELD(lock);

    TraceOSMemFCMCommonMultiRepDestroy((uval)this->getRef());
    tassert(referenceCount == 0,
	    err_printf("destroy while referenceCount non zero\n"));

    {   // remove all ObjRefs to this object
	SysStatus rc=this->exported.close();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    //first detach from all regions.
    // we assume only called from FR so FR doesn't need callback on last
    // region detached

    if (!regionList.isEmpty()) {
	RegionRef   reg;
	RegionInfo *rinfo;
	while (regionList.removeHead(reg, rinfo)) {
	    delete rinfo;
	    //err_printf("FCMCommon::destroy() destroying reg %p : %p\n",
	    //       reg, getRef());
	    // we release locks in case region is currently trying to call us
	    lock.release();
	    DREF(reg)->destroy();
	    lock.acquire();
	}
    }

    // no regions are attached, so all frames are unmapped.  free them
    // note that this call may drop locks if pages are currently in use
    locked_deallocPageList();

    // FIXME:****** ADD destructors to DHashTables  ***********
    // if pageList has any other storage free it
    // pageList.destroy();

    if (!uninitPM(pmRef)) {
	PMRef tmpref = pmRef;
	pmRef = uninitPM();		// mark in case other activity ongoing
	DREF(tmpref)->detachFCM((FCMRef)this->getRef());
    }

    // now destroy lists and seghats
    while (segmentHATList.removeHead(offkey, byAccessList)) {
	while (byAccessList->removeHead(acckey, seghat)) {
	    //err_printf("Destroying %p for acc %lx off %lx\n",
	    //       seghat, acckey, offkey);
	    rc = DREF((SegmentHATShared **)seghat)->sharedSegDestroy();
	    tassert(_SUCCESS(rc), err_printf("oops\n"));
	}
	delete byAccessList;
    }

    // now schedule FCM for freeing
    DREFGOBJ(TheCOSMgrRef)->destroyCO((CORef)this->getRef(),
		    (COSMissHandler *)this);

    return 0;
}

template <class BASE,class GALLOC,class LALLOC>
void
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::doSetFreeAfterIO(MasterPageDescData * md)
{

    tassert(md->isLocked(),
            err_printf("trying to set freeAfterIO which the md locked\n"));

    tassert(md->isDoingIO(),
            err_printf("trying to set freeAfterIO but the md is NOT"
                       " doingIO\n"));

    md->setFreeAfterIO();
    // freeAfterIO must be maintained globally consistent
    // doingIO set so no need to lock locals
    masterDHashTable.doOp(md, &LocalPageDescData::doSetFreeAfterIO,
                          (DHashTableBase::OpArg)0,
                          DHashTableBase::LOCKNONE);
}

template <class BASE,class GALLOC,class LALLOC>
void
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::locked_deallocPageList(uval block)
{
    void *curr;
    uval skip;
    MasterPageDescData* md;
    FreeFrameList ffl;
//    uval cnt=0, IONBcnt=0, IOBcnt=0, Ecnt=0;

    _ASSERT_HELD(lock);

    skip = 0;
    masterDHashTable.addReference();
    curr = masterDHashTable.getNextAndLock(0, &md);
    while (curr) {
//        cnt++;
//        if (Ecnt > 40000) breakpoint();
//        if (cnt > 400) {
//            err_printf("curr=%p md=%p offset=%p paddr=%p :"
//                       "Ecnt=%ld IONBcnt=%ld IOBcnt=%ld\n",
//                       curr, md, (void *)(md->getFileOffset()),
//                       (void *)(md->getPAddr()), Ecnt, IONBcnt, IOBcnt);
//            cnt=0;
//        }
        if (md->isDoingIO()) {
            if (block==DONT_BLOCK) {
//                IONBcnt++;
                if (!md->isFreeAfterIO()) {
                    doSetFreeAfterIO(md);
                    skip = 1;
                }
                md->unlock();
            } else {
                // block here synchronously
//                IOBcnt++;
                PageFaultNotification notification;
                notification.initSynchronous();
                notification.next = md->getFN();
                md->setFN(&notification);
                md->unlock();

                lock.release();
                while (!notification.wasWoken()) {
                    Scheduler::Block();
                }
                lock.acquire();
            }
        } else {
//            Ecnt++;
            uval vaddr;
            // Remember in general local map requests can be occuring
            // until the page descriptor has been completely removed.
            // Hence we first record page info so we can give back
            // after its descriptor has been removed. (this is different
            // from the code paths in which doingIO is set)
            tassertMsg(md->getLen() == pageSize, "list just for fixed size\n");
            vaddr = PageAllocatorKernPinned::realToVirt(md->getPAddr());
            // Record frame for deallocation
	    //FIXME maa do we need sizes in free frame list
	    if (pageSize == PAGE_SIZE) {
		ffl.freeFrame(vaddr);
	    } else {
		DREF(pmRef)->deallocPages(FCMRef(this->getRef()), vaddr,
			pageSize);
	    }
            // Remove Page descriptor
            LocalPageDescData::EmptyArg emptyArg = { 0, // doNotify
                                                     0, // skipFn=0
                                                     0 };
            masterDHashTable.doEmpty(md,
                                     (DHashTableBase::OpArg)
                                     &emptyArg);
            // no need to unlock anything after empty even though we locked
            // md to begin with.  All these protocols with DHashTable need to
            // be clarified and improved
        }
#if 0
        if (skip) {
            curr = masterDHashTable.getNextAndLock(curr, &md);
        } else {
            curr = masterDHashTable.getNextAndLock(0, &md);
        }
#else
        curr = masterDHashTable.getNextAndLock(curr, &md);
#endif
    }
    masterDHashTable.removeReference();

    // FIXME I think it is safe to do this with no locks or
    // reference to the hash table.  But should probably check this.
    if (ffl.isNotEmpty()) {
        SysStatus rc;
	rc = DREF(pmRef)->deallocListOfPages((FCMRef)this->getRef(), &ffl);
	tassert(_SUCCESS(rc), err_printf("dealloc list failed\n"));
    }

}



template <class BASE,class GALLOC,class LALLOC>
void
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::updateRegionPPSet(RegionRef reg)
{
    // update region info
    RegionInfo *rinfo;
    regionList.acquireLock();
    if (!regionList.locked_find(reg, rinfo)) {
	// don't think this can ever happen, even with races
	passert(0, err_printf("No region found while updating ppset\n"));
    }
    rinfo->ppset.addVP(Scheduler::GetVP());
    regionList.releaseLock();
}

template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::updatePM(PMRef pmDetachingFrom)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    // check if the one we are detaching from is the current one
    // we only change our attachment if private, otherwise always attached
    // to file cache pm
    if (priv && pmRef == pmDetachingFrom) {
	// detach from current pm and attach to whoever is left, unless same
	// get head while holding region lock to protect data
	void *iter=NULL;
	PMRef newpm=GOBJK(ThePMRootRef);	// default
	RegionRef regRef;
	RegionInfo *rinfo;
	regionList.acquireLock();
	iter = regionList.next(iter,regRef,rinfo);
	if (iter != NULL) newpm = rinfo->pm;
	regionList.releaseLock();

	// any PMs left and is it different from last
	if (newpm != pmRef) {
	    //err_printf("FCM::updatePM %lx: switching from pm %lx to %lx\n",
	    //       getRef(), !uninitPM(pmRef) ? pmRef : 0, newpm);
	    //printRegionList();
	    if (!uninitPM(pmRef)) {
		DREF(pmRef)->detachFCM((FCMRef)this->getRef());
	    }
	    pmRef = newpm;
	    DREF(pmRef)->attachFCM((FCMRef)this->getRef());
	}
    }

    return 0;
}


/*
 * provide a contigous set of frames mapping to the FCM
 * this code is complicated by the fact that the frame may already be backed,
 * either in memory on on disk.
 * for completeness, if the frame is already mapped we copy the values
 * to the new frame
 */
template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::establishPage(uval offset,
				     uval frameVmapsRaddr,
				     uval length)
{
    SysStatus rc;
    uval paddr;
    uval endAddr = frameVmapsRaddr+length;
    MasterPageDescData *md;
    setPFBit(DistribRoot);

//    breakpoint();

    for (;frameVmapsRaddr<endAddr;frameVmapsRaddr+=pageSize,
	    offset+=pageSize) {
	paddr = PageAllocatorKernPinned::virtToReal(frameVmapsRaddr);
    retry:
	if (masterDHashTable.findOrAllocateAndLock(offset, &md) ==
	    DHashTableBase::ALLOCATED) {
	    tassert(md->isDoingIO(), ;);  // initial state should be doingIO
	    tassert(md->firstReplica() == Scheduler::VPLimit, ;); // and no
	                                                          // replicas
	    md->setPAddr(paddr);
	    md->setEstablished();
	    md->clearDoingIO();
	} else {
	    if (md->isDoingIO()) {  // Page is busy
		blockOnIO(md);       // Drop Locks and wait until done
		goto retry;          // Had to drop locks here so must retry
		                     // page may have been unmapped etc.
	    } else {
		masterDHashTable.lockAllReplicas(md);
		// copy contents
		uval oldframeVmapsRaddr =
		    PageAllocatorKernPinned::realToVirt(md->getPAddr());
		setPFBit(CopyFrame);
		PageCopy::Memcpy((void*)frameVmapsRaddr,
		       (void*)oldframeVmapsRaddr,
		       pageSize);
		rc = DREF(pmRef)->deallocPages(
		    (FCMRef)this->getRef(), oldframeVmapsRaddr, pageSize);
		md->setPAddr(paddr);
		masterDHashTable.doOp(md, &LocalPageDescData::doSetPAddr,
				      (DHashTableBase::OpArg)paddr,
				      DHashTableBase::LOCKNONE);
		md->setEstablished(); // only needs to be do on Master copy
		masterDHashTable.unlockAllReplicas(md);
	    }
	}
	md->unlock();
    }
    return 0;
}

template <class BASE,class GALLOC,class LALLOC>
void
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::blockOnIO(MasterPageDescData*& md)
{
    tassert(md->isLocked(), ;);
    // block here synchronously
    PageFaultNotification notification;

    while (md && md->isDoingIO()) {
	notification.initSynchronous();
	notification.next = md->getFN();
	md->setFN(&notification);
	md->unlock();

	while (!notification.wasWoken()) {
	    Scheduler::Block();
	}
    }
    return;
}

// Remove an established page.  Unlike disEstablish, in this case
// the page is forgotten by the FCM.  The caller is responsible for
// the allocation of the page.  The vMapsRAddr of the page is returned.
template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::removeEstablishedPage(
    uval offset, uval length, uval &vMapsRAddr)
{
    MasterPageDescData* md;
    typename MasterHashTable::EmptyContinuation ec;
    SysStatus rc;

    uval endoffset = offset+length;
    vMapsRAddr = uval(-1);
    for (;offset<endoffset;offset+=pageSize) {
	rc = masterDHashTable.startEmpty(offset,&ec);
	if ( rc == -1) {
	    tassert(0,err_printf("page %lx not established\n", offset));
	    return _SERROR(1331, 0, EINVAL);
	}
	md=ec.getData();
	unmapPage(md);
	LocalPageDescData::EmptyArg emptyArg = { 0, // doNotify = 0;
						 0,
						 0 };
	rc = masterDHashTable.finishEmpty(&ec,
					  (DHashTableBase::OpArg)
					  &emptyArg);
	tassert(rc==0, ;);
    }
    return 0;
}



// this protected service assume appropriate locks are held
// FIXME - must deal with shared segment as well
template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::unmapPageLocal(uval fileOffset)
{
    SysStatus rc = 0;
    void *iter = 0;
    VPNum mypp = Scheduler::GetVP();
    // First unmap the page in any shared segments this FCM owns
    //err_printf("unmapPageLocal: off %lx on %lx\n", fileOffset,
    //       uval(Scheduler::GetVP()));
    regionList.acquireLock();
    SegmentHATByAccessModeList *byAccessList;
    uval baseOffset;
    while ((iter = segmentHATList.next(iter, baseOffset, byAccessList))) {
	void *iter1 = NULL;
	uval acckey;			// need uval for list stuff
	SegmentHATRef seghat;
	if ((fileOffset - baseOffset) < SEGMENT_SIZE) {
	    while ((iter1 = byAccessList->next(iter1, acckey, seghat))) {
		rc = DREF(seghat)->unmapPage(HATRef(0), fileOffset-baseOffset);
		tassert(_SUCCESS(rc), err_printf("oops\n"));
	    }
	}
    }
    // Next unmap the page in any regions using private segments for this
    // page.
    RegionRef regRef;
    RegionInfo *rinfo;
    iter = 0;
    while ((iter=regionList.next(iter,regRef,rinfo))) {
	if (rinfo->ppset.isSet(mypp) &&
	    (SegmentHATPrivate::UnMapShared || noSharedSegments ||
	     !DREF(regRef)->isSharedOffset(fileOffset))) {
	    DREF(regRef)->unmapPage(fileOffset);
	}
    }
    regionList.releaseLock();
    return rc;
}

template <class BASE,class GALLOC,class LALLOC>
struct FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::UnmapPageMsg {
    FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>   *myRoot;
    sval     barrier;
    ThreadID waiter;
    uval     fileOffset;
};

template <class BASE,class GALLOC,class LALLOC>
/* static */ SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::UnmapPageMsgHandler(uval msgUval)
{
    UnmapPageMsg *const msg = (UnmapPageMsg *) msgUval;
    msg->myRoot->unmapPageLocal(msg->fileOffset);
    uval w;
    w = FetchAndAddSignedVolatile(&msg->barrier,-1);
    // w is value before subtract
    if (w==1) Scheduler::Unblock(msg->waiter);
    return 0;
}

// this protected service assume appropriate locks are held
// FIXME - multi-rep locking protocol needs some work
template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::unmapPage(MasterPageDescData *md)
{
    VPNum myvp = Scheduler::GetVP();	// process we are running on
    UnmapPageMsg msg;
    uval set;
    VPNum numprocs, vp;
    SysStatus rc = 0;
    uval ppset;

    tassert(md!=NULL, ;);
    tassert(md->isLocked(), ;);

    // Here we assume that the locals are either locked explicity or
    // their doingIO flag has been set (eg. in the case of fsync).
    masterDHashTable.doOp(md, &LocalPageDescData::doUnmap,
			  (DHashTableBase::OpArg)&ppset,
			  DHashTableBase::LOCKNONE);


    //err_printf("unmapPage: off %lx, ppset %lx\n", pg->fileOffset, pg->ppset);

    msg.myRoot	   = this;
    msg.barrier	   = 1;
    msg.waiter	   = Scheduler::GetCurThread();
    msg.fileOffset = md->getFileOffset();
    numprocs = DREFGOBJK(TheProcessRef)->vpCount();
    for (set=ppset, vp=0; (set!=0) && (vp<numprocs);
	 vp++, set>>=1 ) {
	if ((set & 0x1) && (vp != myvp)) {
	    FetchAndAddSignedVolatile(&msg.barrier,1);
	    rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
					 SysTypes::DSPID(0,vp),
					 UnmapPageMsgHandler, uval(&msg));
	    tassert(_SUCCESS(rc),err_printf("unmapPage remote failed\n"));
	}
    }

    if (ppset & (uval(1)<<myvp)) {
	rc = unmapPageLocal(md->getFileOffset());
	tassert(_SUCCESS(rc),err_printf("unmapPageLocal failed\n"));
    }

    //including ourselves in the barrier prevents multiple
    //unblocks - the barrier can't go to zero till we
    //decrement it - and may lead to no unblocks at all
    //if we're last
    FetchAndAddSignedVolatile(&msg.barrier,-1);

    while (msg.barrier != 0) {
	Scheduler::Block();
    }

    // also clear FrameArray accessed bit
    // PageAllocatorKernPinned::clearAccessed(md->getPAddr());

    return rc;
}



/*
 * unmap all pages on all processors.
 * done by asking each region to unmap all
 * should we distribute this the other way?
 * that is, on each processor ask each region to
 * unmap itself locally.  less messages that way
 */
template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::locked_unmapAll()
{
    _ASSERT_HELD(lock);
    regionList.acquireLock();
    void *iter=NULL;
    RegionRef regRef;
    RegionInfo *rinfo;
    while ((iter=regionList.next(iter,regRef,rinfo))) {
	//err_printf("removing regoin %lx from %lx\n",
	//	   (uval)regRef, (uval)this);
	// unmap the whole region
	DREF(regRef)->unmapRange(0, uval(-1));
    }
    regionList.releaseLock();

//FIXME: ******* KLUDGE
#if 0
    // no nothing is mapped, so clean up page descriptors
    pg = pageList.getFirst();
    while (pg) {
	pg->ppset = 0;
	pg->mapped = PageDesc::CLEAR;
	// also clear FrameArray accessed bit
	// PageAllocatorKernPinned::clearAccessed(pg->paddr);
	pg = pageList.getNext(pg);
    }
#endif
    return 0;
}

template <class BASE,class GALLOC,class LALLOC>
SysStatus
FCMCommonMultiRepRoot<BASE,GALLOC,LALLOC>::discardCachedPages()
{
    lock.acquire();
    locked_unmapAll();
    locked_deallocPageList(DONT_BLOCK);
    lock.release();
    checkEmpty();
    return 0;
}



#if 0

SysStatus
FCMCommonMultiRepRoot::locked_setPM(PMRef newpm)
{
    _ASSERT_HELD(lock);
    if (newpm != pmRef) {
	//err_printf("FCM::updatePM %lx: switching from pm %lx to %lx\n",
	//       getRef(), !uninitPM(pmRef) ? pmRef : 0, newpm);
	//printRegionList();
	if (!uninitPM(pmRef)) DREF(pmRef)->detachFCM((FCMRef)getRef());
	pmRef = newpm;
	PM::Summary sum(pageList.getNumPages(),pageList.getNumPagesFree());
	DREF(pmRef)->attachFCM((FCMRef)getRef(), &sum);
    }
    return 0;
}






//FIXME - should distribute the request
//        but I'm waiting for Bryan to drop his changes
//lock must be held on entry
SysStatus
FCMCommonMultiRepRoot::locked_unmapAll()
{
    PageDesc *pg;
    _ASSERT_HELD(lock);
    pg = pageList.getFirst();
    while (pg) {
	unmapPage(pg);
	pg = pageList.getNext(pg);
    }
    return 0;
}


void
FCMCommonMultiRepRoot::finishAllIO(uval clearCopyFlags=0)
{
    _ASSERT_HELD(lock);
    PageDesc *pg, *nextpg;
    nextpg = pageList.getFirst();
    while ((pg=nextpg)) {
	nextpg = pageList.getNext(pg);
	if (clearCopyFlags) {
	    pg->forkCopied1 = pg->forkCopied2 = PageDesc::CLEAR;
	}
	if (pg->doingIO) {
	    blockOnIO(pg);
	    // need to start again since lock was released
	    //FIXME - need a better way to do this iteration
	    nextpg = pageList.getFirst();
	}
    }
}


// Unwire established pages - FCM may now page them and
// return frames to PageManager
SysStatus
FCMCommonMultiRepRoot::disEstablishPage(uval offset, uval length)
{
    PageDesc* pg;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    uval endoffset = offset+length;
    for (;offset<endoffset;offset+=pageSize) {
	pg = findPage(offset);
	if (pg) {
	    pg->established = PageDesc::CLEAR;
	    // indicate that we have taken ownership of page
	    // PageAllocatorKernPinned::initFrameDesc(pg->paddr, FCMRef(getRef()),
						   pg->fileOffset, 0);
	}
    }
    return 0;
}


void
FCMCommonMultiRepRoot::printRegionList()
{
    err_printf("FCM %p RegionList:\n", getRef());
    regionList.acquireLock();
    void *iter=NULL;
    RegionRef regRef;
    RegionInfo *rinfo;
    while ( (iter = regionList.next(iter, regRef, rinfo)) != NULL) {
	err_printf("\treg %p, pm %p\n", regRef, rinfo->pm);
    }
    regionList.releaseLock();
}

#endif
template class FCMCommonMultiRepRoot<CObjRootMultiRep,AllocGlobalPadded,
                               AllocLocalStrict>;
template class FCMCommonMultiRepRoot<CObjRootMultiRepPinned,
                               AllocPinnedGlobalPadded,AllocPinnedLocalStrict>;

