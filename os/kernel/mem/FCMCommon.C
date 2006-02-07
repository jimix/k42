/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMCommon.C,v 1.125 2005/08/24 15:00:42 dilma Exp $
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
#include <mem/FCMCommon.H>
#include <mem/HAT.H>
#include <mem/Region.H>
#include <mem/CacheSync.H>
#include <mem/PM.H>
#include <mem/SegmentHATPrivate.H>
#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>
#include <trace/traceMem.h>
#include <trace/traceMisc.h>
#include <mem/PageList.H>
#include <mem/PageSet.H>
#include <mem/PageSetDense.H>
#include <mem/PerfStats.H>
#include <mem/PageFaultNotification.H>
#include <mem/PageCopy.H>
#include <defines/paging.H>

template<class PL, class ALLOC>
FCMCommon<PL,ALLOC>::FCMCommon()
{
    pmRef	     = uninitPM();
    referenceCount   = 0;
    pageable	     = 0;
    backedBySwap     = 0;
    priv	     = 1;
    beingDestroyed   = 0;
    mappedExecutable = 0;
    noSharedSegments = KernelInfo::ControlFlagIsSet(
	KernelInfo::NO_SHARED_SEGMENTS)?1:0;
    isCRW            = 0;
    mapBase          = 0;
    frRef            = NULL;
    pageSize         = PAGE_SIZE;
#ifdef DILMA_TRACE_PAGE_DIRTY // this should go away soon
    dirtyCounter     = 0;
    lastCounterTrace = 0;
#endif // #ifdef DILMA_TRACE_PAGE_DIRTY_SET // this should go away soon

}

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::updatePM(PMRef pmDetachingFrom)
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
		DREF(pmRef)->detachFCM(getRef());
	    }
	    pmRef = newpm;
	    DREF(pmRef)->attachFCM(getRef());
	}
    }

    return 0;
}

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::locked_setPM(PMRef newpm)
{
    _ASSERT_HELD(lock);
    if (newpm != pmRef) {
	//err_printf("FCM::updatePM %lx: switching from pm %lx to %lx\n",
	//       getRef(), !uninitPM(pmRef) ? pmRef : 0, newpm);
	//printRegionList();
	if (!uninitPM(pmRef)) DREF(pmRef)->detachFCM(getRef());
	pmRef = newpm;
	DREF(pmRef)->attachFCM(getRef());
    }
    return 0;
}

template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::updateRegionPPSet(RegionRef reg)
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

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::attachRegion(RegionRef regRef, PMRef newPM,
				  AccessMode::mode accessMode)

{
    SysStatus rc = 0;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    if (beingDestroyed) return -1;

    //err_printf("FCM %lx attach from reg %lx / pm %lx\n",
    //       getRef(), regRef, newPM);
    //err_printf("adding regoin %lx to %lx\n", (uval)regRef, (uval)this);
    regionList.add(regRef, new RegionInfo(newPM));

    // only change PM for private FCMs and only if we don't have one yet
    // In the case of pageable private, PMRoot is equivalent to not being set
    if (priv && (uninitPM(pmRef) || pmRef == GOBJK(ThePMRootRef)) ) {
      //err_printf("attachRegion: switching from pm %lx to %lx\n",
      //   uninitPM(pmRef) ? pmRef : 0, newPM);
	if (!uninitPM(pmRef)) DREF(pmRef)->detachFCM(getRef());
	pmRef = newPM;
	DREF(pmRef)->attachFCM(getRef());
    }

#ifdef PAGING_TO_SERVER
    // have to pin FCM if accessed by a server to gaurantee forward progress
    if (pageable) {
	ProcessID pid = DREF(regRef)->getPID();
	if (pid <= 5) {
	    pageable = 0;
	}
    }
#endif // PAGING_TO_SERVER

    if (AccessMode::isExecute(accessMode) && !mappedExecutable) {
	mappedExecutable = 1;
	locked_cacheSyncAll();
    }

    TraceOSMemFCMCOMAtchReg((uval)regRef, (uval64)getRef());

    return rc;
}

// this protected service assume appropriate locks are held
// FIXME - must deal with shared segment as well
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::unmapPageLocal(uval fileOffset)
{
    SysStatus rc = 0;
    void *iter = 0;
    VPNum mypp = Scheduler::GetVP();
    //err_printf("unmapPageLocal: off %lx on %lx\n", fileOffset,
    //       uval(Scheduler::GetVP()));
    regionList.acquireLock();
    // First unmap the page in any shared segments this FCM owns
    // we are counting on the common lock for the FCM protecting seg list
    // need region lock held here too (maybe would prefer reader-lock)
    _ASSERT_HELD(lock);
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

template<class PL, class ALLOC>
struct FCMCommon<PL,ALLOC>::UnmapPageMsg {
    FCMRef   myRef;
    sval     barrier;
    ThreadID waiter;
    uval     fileOffset;
};

template<class PL, class ALLOC>
/* static */ SysStatus
FCMCommon<PL,ALLOC>::UnmapPageMsgHandler(uval msgUval)
{
    UnmapPageMsg *const msg = (UnmapPageMsg *) msgUval;
    ((FCMCommon*)DREF(msg->myRef))->unmapPageLocal(msg->fileOffset);
    uval w;
    w = FetchAndAddSignedVolatile(&msg->barrier,-1);
    // w is value before subtract
    if (w==1) Scheduler::Unblock(msg->waiter);
    return 0;
}

// this protected service assume appropriate locks are held
// FIXME - multi-rep locking protocol needs some work
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::internal_unmapPage(uval offset, uval ppset)
{
    VPNum myvp = Scheduler::GetVP();	// process we are running on
    UnmapPageMsg msg;
    uval set;
    VPNum numprocs, vp;
    SysStatus rc = 0;
    //err_printf("unmapPage: off %lx, ppset %lx\n", pg->fileOffset, pg->ppset);

    msg.myRef	   = (FCMRef)getRef();
    msg.barrier	   = 1;
    msg.waiter	   = Scheduler::GetCurThread();
    msg.fileOffset = offset;
    
    numprocs = DREFGOBJK(TheProcessRef)->vpCount();
    for (set=ppset, vp=0; (set!=0) && (vp<numprocs); vp++, set>>=1 ) {
	if ((set & 0x1) && (vp != myvp)) {
	    FetchAndAddSignedVolatile(&msg.barrier,1);
	    rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
					 SysTypes::DSPID(0, vp),
					 UnmapPageMsgHandler, uval(&msg));
	    tassert(_SUCCESS(rc),err_printf("unmapPage remote failed\n"));
	}
    }

    if (ppset & (uval(1)<<myvp)) {
	rc = unmapPageLocal(offset);
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
    return rc;
}


/*
 * unmap all pages on all processors.
 * done by asking each region to unmap all
 * should we distribute this the other way?
 * that is, on each processor ask each region to
 * unmap itself locally.  less messages that way
 */
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::locked_unmapAll()
{
    SysStatus rc;
    PageDesc *pg;
    _ASSERT_HELD(lock);
    regionList.acquireLock();
    void *iter=NULL;
    RegionRef regRef;
    RegionInfo *rinfo;
    while ((iter=regionList.next(iter,regRef,rinfo))) {
	//err_printf("removing regoin %lx from %lx\n",
	//	   (uval)regRef, (uval)this);
	// unmap the whole region
	if (!rinfo->ppset.isEmpty()) {
	    DREF(regRef)->unmapRange(0, uval(-1));
	}
    }
    regionList.releaseLock();

    // now destroy lists and seghats - this is easier than
    // trying to unmap them although it does imply that
    // unmapping happens by abandoning segments which
    // is not always best.  this is a low traffic path?
    SegmentHATByAccessModeList *byAccessList;
    SegmentHATRef seghat;
    uval offkey, acckey;
    while (segmentHATList.removeHead(offkey, byAccessList)) {
	while (byAccessList->removeHead(acckey, seghat)) {
	    //err_printf("Destroying %p for acc %lx off %lx\n",
	    //       seghat, acckey, offkey);
	    rc = DREF((SegmentHATShared **)seghat)->sharedSegDestroy();
	    tassert(_SUCCESS(rc), err_printf("oops\n"));
	}
	delete byAccessList;
    }


    // no nothing is mapped, so clean up page descriptors
    pg = pageList.getFirst();
    while (pg) {
	pg->ppset = 0;
	pg->mapped = PageDesc::CLEAR;
	// also clear FrameArray accessed bit
	// PageAllocatorKernPinned::clearAccessed(pg->paddr);
	pg = pageList.getNext(pg);
    }
    return 0;
}


template<class PL, class ALLOC>
PageDesc *
FCMCommon<PL,ALLOC>::findPage(uval fileOffset)
{
    PageDesc *p;

    tassert(lock.isLocked(), err_printf(
	"FCM lock should be held before entering this function\n"));

    if ((p = pageList.find(fileOffset))) {
	return p;
    } else {
	;
    }
    return 0;
}

template<class PL, class ALLOC>
PageDesc *
FCMCommon<PL,ALLOC>::addPage(uval fileOffset, uval paddr, uval len)
{
    tassert(lock.isLocked(), err_printf(
	"FCM lock should be held before entering this function\n"));
    tassert(paddr != 0, err_printf("using 0 as frame address\n"));

    return pageList.enqueue(fileOffset, paddr, len);
}

/* Implement logic for tracking modified pages. Currently, we do not
 * use hardware help.  Rather, if the requested access is write, the
 * page fault is for read and the frame is not modified, we map the
 * frame read only if possible.  If the requested access is write and
 * the page fault is for write, we mark the page modified and map the
 * frame write.  On machines which can't map read only, such as PwrPC
 * for supervisor only, we immediatly mark the page modified
 */

template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::detectModified(PageDesc* pg, AccessMode::mode &access,
				    AccessMode::pageFaultInfo pfinfo)
{
    ScopeTime timer(DMTimer);
#if 0
    cprintf("%lx mapped, access %lx pfinfo %lx dirty %lx\n",
	    pg->paddr, access, pfinfo, pg->dirty);
#endif /* #if 0 */

    tassert((pg->free==PageDesc::CLEAR) && (pg->doingIO==PageDesc::CLEAR),
	    err_printf("mapping unavailable page\n"));

    pg->ppset |= uval(1) << Scheduler::GetVP();

    if (mappedExecutable && pg->cacheSynced == PageDesc::CLEAR) {
	// Machine dependent operation.
	setPFBit(CacheSynced);
	CacheSync(pg);
    }

    if (pg->mapped != PageDesc::SET) {
	setPFBit(UnMapped);
	// mark page mapped
	pg->mapped = PageDesc::SET;
#if 0
This was commented out by Orran (Marc requested), but this should
be properly removed by Marc
	// also mark framearray to indicate page is now mapped
	PageAllocatorKernPinned::setAccessed(pg->paddr);
#endif
	if (pg->free == PageDesc::SET) {
	    pageList.dequeueFreeList(pg);
	    pg->free = PageDesc::CLEAR;
	}
    }

    if (pg->forkMapParent) {
	uval rc;
	rc = AccessMode::makeReadOnly(access);
	tassert(rc, err_printf("can't make copyOnWrite page read only\n"));
	return;
    }

    if ((!AccessMode::isWrite(access)) || pg->dirty) return;

    if (AccessMode::isWriteFault(pfinfo)) {
	setPFBit(WriteFault);
	DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	pg->dirty = PageDesc::SET;
	return;
    }

    if (AccessMode::makeReadOnly(access)) {
	setPFBit(MakeRO);
#if 0
	cprintf("              new ro access %lx\n", access);
#endif /* #if 0 */
	return;
    }

    // Can't reduce this access mode to read only - so assume
    // frame will be modified and map it

    DILMA_TRACE_PAGE_DIRTY(this,pg,1);
    pg->dirty = PageDesc::SET;

    return;
}
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::mapPageInHAT(uval vaddr,
				  AccessMode::pageFaultInfo pfinfo,
				  AccessMode::mode access, VPNum vp,
				  HATRef hat, RegionRef reg,
				  PageDesc *pg, uval offset)
{
    SysStatus rc;
    ScopeTime timer(HATTimer);

    // need original access mode
    // need uval for list and original access value
    uval acckey = uval(access);
    // key identifies a segment which maps the offset offkey to the first
    // page of the segment containing vaddr, and
    // thus maps offset to vaddr in that segment.
    uval found;
    SegmentHATByAccessModeList *byAccessList;
    SegmentHATRef seghat;
    uval wasMapped;

    if (!mappedExecutable && AccessMode::isExecute(access)) {
	// transition to executable  - normally there will be
	// almost nothing mapped unless the file has just been written
	locked_cacheSyncAll();
	mappedExecutable = 1;
    }

    wasMapped = pg->mapped;

    // note, access may be modified by call
    detectModified(pg, access, pfinfo);


    // we optimize the common case by not even testing for shared segment
    // before trying to map - is this a good trade?
    // notice that if noSharedSegments is true, we always map here,
    // creating a SegmentHATPrivate if necessary
    rc = DREF(hat)->mapPage(pg->paddr,  vaddr, pg->len, pfinfo, access, vp,
			    wasMapped, noSharedSegments);

    if (rc != HAT::NOSEG) {
	return rc;
    }

    if (!DREF(reg)->isSharedVaddr(vaddr)) {
	setPFBit(CreatSeg);
	return DREF(hat)->mapPage(pg->paddr, vaddr, pg->len, pfinfo, access,
				  vp, wasMapped, 1 /*create private segment*/);
    }

    setPFBit(MapShrSeg);

    // can use a shared segment - note that we know there is a space
    // in the hat since we got HAT::NOSEG above
    // compute the offset of the beginning of the segment congtaining
    // vaddr

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
    // N.B. use original (region) access - current page access
    // may have been changed by detectModified
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
    rc = DREF(hat)->mapPage(pg->paddr, vaddr, pg->len, pfinfo, access, vp,
			    wasMapped, 0);
    tassert(rc != HAT::NOSEG, err_printf("oops\n"));

    return rc;
}

template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::blockOnIO(PageDesc*& pg)
{
    _ASSERT_HELD(lock);
    // block here synchronously
    PageFaultNotification notification;
    const uval offset = pg->fileOffset;
    while (pg && pg->doingIO) {
	notification.initSynchronous();
	notification.next = pg->fn;
	pg->fn = &notification;

	lock.release();

	while (!notification.wasWoken()) {
	    Scheduler::Block();
	}

	lock.acquire();
	pg = findPage(offset);
    }
    return;
}

template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::finishAllIO(uval clearCopyFlags)
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

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::destroy()
{
    SysStatus rc;

    //err_printf("FCMCommon::destroy() : %p\n", getRef());

    lock.acquire();

    if (beingDestroyed) {
	lock.release();
	return 0;
    }

    tassertMsg(referenceCount == 0,
	       "destroy called while reference count is non-zero\n");
    
    beingDestroyed = 1;

    rc = doDestroy();

    lock.release();

    return rc;
}

/*
 * provide a contigous set of frames mapping to the FCM
 * this code is complicated by the fact that the frame may already be backed,
 * either in memory on on disk.
 * for completeness, if the frame is already mapped we copy the values
 * to the new frame
 */
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::establishPage(uval offset, uval frameVmapsRaddr,
				   uval length)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    SysStatus rc;
    locked_unmapBase();
    uval paddr;
    uval endAddr = frameVmapsRaddr+length;
    PageDesc* pg;
    for (;frameVmapsRaddr<endAddr;frameVmapsRaddr+=pageSize,
	    offset+=pageSize) {
	paddr = PageAllocatorKernPinned::virtToReal(frameVmapsRaddr);
	pg = findPage(offset);
	if (pg) {
	    // make sure its not doing IO
	    blockOnIO(pg);

	    // unmap old page if its mapped
	    unmapPage(pg);

	    // copy contents
	    uval oldframeVmapsRaddr =
		PageAllocatorKernPinned::realToVirt(pg->paddr);
	    setPFBit(CopyFrame);
	    PageCopy::Memcpy((void*)frameVmapsRaddr,
		   (void*)oldframeVmapsRaddr,
		   pageSize);
	    // indicate that we are giving up ownership of pages
	    // PageAllocatorKernPinned::clearFrameDesc(pg->paddr);
	    rc = DREF(pmRef)->deallocPages(
		getRef(), oldframeVmapsRaddr, pageSize);
	    pg->paddr = PageAllocatorKernPinned::virtToReal(frameVmapsRaddr);
	} else {
	    pg=addPage(offset, paddr, pageSize);
	}
	pg->established = PageDesc::SET;
	// assume page is in correct cache state
	pg->cacheSynced = PageDesc::SET;
	// is not backed by a good disk block, and in any case
	// is pinned.
	DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	pg->dirty = PageDesc::SET;
    }
    return 0;
}

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::establishPagePhysical(uval offset, uval paddr,
					    uval length)
{
    SysStatus retvalue;
    retvalue = establishPage(offset, PageAllocatorKernPinned::realToVirt(paddr),
			 length);
    return (retvalue);
}

// Unwire established pages - FCM may now page them and
// return frames to PageManager
// Note that we don't take over PageManager ownership of an established
// page until this point.  This may be wrong, but problably doesn't matter
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::disEstablishPage(uval offset, uval length)
{
    PageDesc* pg;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    uval endoffset = offset+length;
    for (;offset<endoffset;offset+=pageSize) {
	pg = findPage(offset);
	if (pg) {
	    pg->established = PageDesc::CLEAR;
	    // indicate that we have taken ownership of page
	    // PageAllocatorKernPinned::initFrameDesc(
	    // pg->paddr, FCMRef(getRef()), pg->fileOffset, 0);
	}
    }
    return 0;
}

// Remove an established page.  Unlike disEstablish, in this case
// the page is forgotten by the FCM.  The caller is responsible for
// the allocation of the page.  The vMapsRAddr of the page is returned.
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::removeEstablishedPage(
    uval offset, uval length, uval &vMapsRAddr)
{
    PageDesc* pg;
    lock.acquire();
    uval endoffset = offset+length;
    vMapsRAddr = uval(-1);
    for (;offset<endoffset;offset+=pageSize) {
	pg = findPage(offset);
	if (pg) {
	    if (vMapsRAddr == uval(-1)) {
		vMapsRAddr = PageAllocatorKernPinned::realToVirt(pg->paddr);
	    }
	    pg->established = PageDesc::CLEAR;
	    unmapPage(pg);
	    //FIXME - must deal with releasing disk block as well
	    tassertMsg(0==pg->fn, "notify should have been done\n");
	    pageList.remove(offset);
	} else {
	    tassert(0,err_printf("page %lx not established\n", offset));
	    lock.release();
	    return _SERROR(2538, 0, EINVAL);
	}
    }
    lock.release();
    checkEmpty();
    return 0;
}

// If something other than a region needs to prevent FCM destruction
// is must add a reference (count).  see ProcessVPList for example.
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::addReference()
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    referenceCount++;
    return 0;
}

/*
 *N.B. this logic is unfortunately shadowed in detachForkChild
 *if you make changes, look there as well
 */
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::removeReference()
{
    lock.acquire();
    return locked_removeReference();
}

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::locked_removeReference()
{
    _ASSERT_HELD(lock);
    tassertMsg(referenceCount > 0, "reference count going negative\n");
    referenceCount--;
    if (!referenceCount && regionList.isEmpty()) {
	lock.release();
	return notInUse();
    } else {
	lock.release();
	return 0;
    }
}

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::doDestroy()
{
    //err_printf("FCMCommon::destroy() : %lx\n", getRef());

    SysStatus rc;

    _ASSERT_HELD(lock);


    TraceOSMemFCMCommonDestroy((uval)getRef());
    tassert(referenceCount == 0,
	    err_printf("destroy while referenceCount non zero\n"));

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
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

    //err_printf("FCMCommon::destroy() : dealloc pagelist %p\n", getRef());

    // no regions are attached, so all frames are unmapped.  free them
    // note that this call may drop locks if pages are currently in use
    locked_deallocPageList();

    // if pageList has any other storage free it
    pageList.destroy();

    if (!uninitPM(pmRef)) {
	PMRef tmpref = pmRef;
	pmRef = uninitPM();		// mark in case other activity ongoing
	DREF(tmpref)->detachFCM(getRef());
    }

    // now destroy lists and seghats
    SegmentHATByAccessModeList *byAccessList;
    SegmentHATRef seghat;
    uval offkey, acckey;
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
    destroyUnchecked();

    //err_printf("FCMCommon::destroy() all done : %p\n", getRef());

    return 0;
}

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::discardCachedPages()
{
    lock.acquire();
    locked_unmapAll();
    locked_deallocPageList(DONT_BLOCK);
    lock.release();
    checkEmpty();
    return 0;
}

template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::locked_deallocPageList(uval block)
{
    PageDesc *p, *restart;
    FreeFrameList ffl;
    SysStatus rc;
    uval tmp = 0;

    _ASSERT_HELD(lock);

    restart = 0;
    p = pageList.getFirst();
    while (p) {
	tassert(p->paddr != 0, err_printf("bad paddr\n"));
	tassert(p->established == PageDesc::CLEAR && p->pinCount == 0,
		err_printf("discarding established/pinned page\n"));


	// temporary hack; should be dealt with before calling dealloc
	if ((p->doingIO == PageDesc::SET) && block==DONT_BLOCK) {
	    p->freeAfterIO = PageDesc::SET;
	    restart = p;
	} else if (p->doingIO == PageDesc::SET) {
	    if ( p->forkIO == PageDesc::SET) {
		// forkcopy placeholder - a fork page access was pending
		// when we destroyed the region.  No actual page fault is in
		// flight - the region guarantees that.
		tassertMsg(p->forkIOLock == PageDesc::CLEAR,
			   "another thread is accessing this FCM\n");
		tassertMsg(0==p->fn,
			   "can't have notifies when forkIOLock is clear\n");
		pageList.remove(p->fileOffset);
	    } else {
		// block here synchronously
		PageFaultNotification notification;
		notification.initSynchronous();
		notification.next = p->fn;
		p->fn = &notification;
		lock.release();
		while (!notification.wasWoken()) {
		    Scheduler::Block();
		}
		lock.acquire();
	    }
	} else {
	    tassert(!p->fn, err_printf("Notifications on dealloc\n"));

	    TraceOSMemPageDealloc(p->paddr, (uval)pmRef);

	    tassertMsg((p->len == pageSize), "list just for fixed size\n");
	    tmp = PageAllocatorKernPinned::realToVirt(p->paddr);
	    //FIXME maa do we need sizes in free frame list
	    if (pageSize == PAGE_SIZE) {
		ffl.freeFrame(tmp);
	    } else {
		DREF(pmRef)->deallocPages(getRef(), tmp, pageSize);
	    }
	    pageList.remove(p->fileOffset);
	}

	// restart will be non null if we are skipping doingIO pages
	if (restart) {
	    p = pageList.getNext(restart);
	} else {
	    p = pageList.getFirst();
	}
    }

    if (ffl.isNotEmpty()) {
	rc = DREF(pmRef)->deallocListOfPages(getRef(), &ffl);
	tassert(_SUCCESS(rc), err_printf("dealloc list failed\n"));
    }
#ifdef marcdebug
    //maa debug check
    if (block==DO_BLOCK)
	DREFGOBJK(ThePinnedPageAllocatorRef)->fcmCheck(FCMRef(getRef()));
#endif /* #ifdef marcdebug */
}

template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::locked_cacheSyncAll()
{
    PageDesc *pg;
    _ASSERT_HELD(lock);

    pg = pageList.getFirst();
    while (pg) {
	if (!pg->doingIO && !pg->cacheSynced) {
	    tassert(pg->paddr != 0, err_printf("bad paddr\n"));
	    CacheSync(pg);
	}
	pg = pageList.getNext(pg);
    }
}

template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::unLockPage(uval token)
{
    PageDesc *pg = (PageDesc*) token;
    lock.acquire();
    tassert(pg && pg->doingIO,err_printf("unLock non locked page\n"));
    pg->doingIO = PageDesc::CLEAR;
    // notify anyone who might have blocked on this, and free lock
    notify(pg);
    return 0;
}

template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::notify(PageDesc* pg, SysStatus rc,
			    PageFaultNotification* skipFn, uval keepLock)
{
    _ASSERT_HELD(lock);
    PageFaultNotification *notf = pg->fn;
    pg->fn = 0;
    if (!keepLock) lock.release();	// optimization, could unlock later
    while (notf) {
	PageFaultNotification *nxt = notf->next;
	if (notf != skipFn) {
	    notf->setRC(rc);
	    notf->doNotification();
	}
	notf = nxt;
    }
}



template<class PL, class ALLOC>
void
FCMCommon<PL,ALLOC>::printRegionList()
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


template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::printStatus(uval kind)
{
    uval found = 0;
    switch (kind) {
    case PendingFaults:
	lock.acquire();
	PageDesc* pg;
	pg = pageList.getFirst();
	while (pg) {
	    if (pg->fn) {
		err_printf("Pending offset: %lx fn: %lx bits: %lx\n",
			   pg->fileOffset, uval(pg->fn),
			   *(((uval*)&(pg->fileOffset))+1));
		found = 1;
	    }
	    pg = pageList.getNext(pg);
	}
	lock.release();
	break;
    default:
	err_printf("kind %ld not known in printStatus\n", kind);
    }
    return found;
}

/*
 * This version can only report status
 */
template<class PL, class ALLOC>
SysStatus
FCMCommon<PL,ALLOC>::getSummary(PM::Summary &sum)
{
    // still tell the caller how many pages we have in case it's of use
    sum.set(pageList.getNumPages(), pageList.getNumPagesFree());
    return 0;
}

//template instantiation
template class FCMCommon<PageSet<AllocGlobal>,AllocGlobal>;
template class FCMCommon<PageSet<AllocPinnedGlobal>,AllocPinnedGlobal>;
template class FCMCommon<PageSet<AllocPinnedGlobalPadded>,
			 AllocPinnedGlobalPadded>;
template class FCMCommon<PageList<AllocGlobal>,AllocGlobal>;
template class FCMCommon<PageList<AllocPinnedGlobal>,AllocPinnedGlobal>;
template class FCMCommon<PageList<AllocPinnedGlobalPadded>,
			 AllocPinnedGlobalPadded>;
template class FCMCommon<PageSetDense,AllocGlobal>;
