/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefault.C,v 1.150 2005/08/24 15:00:43 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared FCM services for mapping, unmapping,
 * getting/releasing for copy for FCM's attached to FR's (files).
 * **************************************************************************/

#include "kernIncs.H"
#include <trace/traceMem.h>
#include <trace/traceClustObj.h>
#include "defines/paging.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/FR.H"
#include "mem/FCMDefaultRoot.H"
#include "mem/FCMDefault.H"
#include "mem/PageFaultNotification.H"
#include "mem/PM.H"
#include "cobj/CObjRootSingleRep.H"
#include "defines/experimental.H"
#include "mem/PageCopy.H"
#include "proc/Process.H"
#include <sys/KernelInfo.H>
#include "mem/PerfStats.H"
#include "mem/IORestartRequests.H"

SysStatus
FCMDefault::init(FCMRef &ref, FRRef frRefArg, uval pgSize, uval ISpageable,
                 uval ISbackedBySwap, uval preFetchPages, uval maxPages)
{
    frRef = frRefArg;
    numanode = PageAllocator::LOCAL_NUMANODE; // default, allocate locally
    pinnedCount = 0;
    pageSize = pgSize;
#ifdef LARGE_PAGES_NONPAGEABLE
    if (pgSize != PAGE_SIZE) {
	pageable = 0;
    } else {
	pageable = ISpageable ? 1 : 0;	// convert to 1/0 for bit field
    }
#else
    pageable = ISpageable ? 1 : 0;	// convert to 1/0 for bit field
#endif
    backedBySwap = ISbackedBySwap ? 1 : 0; // convert to 1/0 for bit field


    // shared if pageable but not backed by swap (hence file), private opp.
    priv = !(ISpageable && !ISbackedBySwap);
    preFetch = preFetchPages;
    maxNumPages = maxPages?maxPages:FCM_MAX_NUMPAGES;
    giveBackNumPages = MAX(maxPages/4, GIVE_BACK_NUMPAGES);

#ifdef ENABLE_FCM_SWITCHING
    performedSwitch = 0;
#endif

    //err_printf("FCMDefault: %p, priv %lu, pageble %lu, backedbySwap %lu\n",
    //ref, priv, pageable, backedBySwap);

    //ref = (FCMRef)CObjRootSingleRep::Create(this);
    ref = rootCreate();
    if (ref == NULL) {
	delete this;
	return -1;
    }

    rq = new IORestartRequests(ref);
    pendingWrite = 0;

    if (pageable) {
	pmRef = GOBJK(ThePMRootRef);// eventually file cache pm
	DREF(pmRef)->attachFCM(ref);
    }

    return 0;
}


/*
 * N.B. Although this routine is called locked, it releases
 * and re-acquires the lock.  Take care.
 *
 */
SysStatus
FCMDefault::getFrame(uval& paddr, uval flags)
{
    SysStatus rc;

    _ASSERT_HELD(lock);

    uval virtAddr;
    // release lock while allocating
    lock.release();

    rc = DREF(pmRef)->allocPages(getRef(), virtAddr, pageSize, pageable, 
				 flags, numanode);

    lock.acquire();

    if (_FAILURE(rc)) return rc;

    paddr = PageAllocatorKernPinned::virtToReal(virtAddr);

#if 0
    // some debugging stuff
    if (numanode != PageAllocator::LOCAL_NUMANODE
	&& !DREFGOBJK(ThePinnedPageAllocatorRef)->isLocalAddr(virtAddr)
	&& (DREFGOBJK(ThePinnedPageAllocatorRef)->addrToNumaNode(virtAddr)
	    == numanode)) {
	err_printf("Got Remote allocation for %ld: p %lx, n %d\n",
		   numanode, virtAddr, DREFGOBJK(ThePinnedPageAllocatorRef)->
		   addrToNumaNode(virtAddr));
    }
#endif /* #if 0 */

    return 0;
}


/*
 * This routine may need to drop locks to handle cases where the page is
 * not in the cache.  To reinforce this, locks should not be held when
 * calling this routine.  Lock is held on return so that operation can
 * complete automatically.  In the future, we may return with only the
 * page individually locked.
 * N.B. lock is NOT held if no page found and can't allocate one
 */
PageDesc *
FCMDefault::findOrAllocatePageAndLock(uval fileOffset, SysStatus &rc,
				      uval &needsIO, uval flags)
{
    PageDesc *pg;
    uval paddr;

    needsIO = 0;
    rc = 0;

#ifndef ENABLE_FCM_SWITCHING
    lock.acquire();
#else /* #ifndef ENABLE_FCM_SWITCHING */
    if (!lock.tryAcquire()) {
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    //err_printf("Lock contended... initiating switch.\n");
	}
	//breakpoint();

	((FCMDefaultRoot *)myRoot)->switchImplToMultiRep();
	// use above for normal switching below for testing null hot swap times
	//if (performedSwitch == 0) {
	//    TraceOSClustObjSwapStart((uval64)myRoot);
	//    ((FCMDefaultRoot *)myRoot)->
	//	switchImplToSameRep((FCMDefaultRoot *)myRoot);
	//    TraceOSClustObjSwapDone((uval64)myRoot);
	//    performedSwitch = 1;
	//}

	lock.acquire();
    }
#endif /* #ifndef ENABLE_FCM_SWITCHING */

    pg = findPage(fileOffset);

    if (pg != NULL) {

	//err_printf("H");
	TraceOSMemFCMDefFoundPage(fileOffset, pg->paddr);

	if (pg->doingIO) {
	    pg->freeAfterIO = PageDesc::CLEAR;
	}

    } else {
	//err_printf("[TID#%lx:]", Scheduler::GetCurThread());
	//err_printf("X");
	// allocate a new page
	pg = addPage(fileOffset, uval(-1), pageSize);
	pg->doingIO = PageDesc::SET;
	needsIO = 1;

	rc = getFrame(paddr,flags);

	if (_FAILURE(rc)) {
	    // must clean up pagedesc we added and wakeup anyone waiting
	    // notify but retain lock
	    notify(pg, 0, 0, 1);
	    pageList.remove(fileOffset);
	    lock.release();
	    return NULL;	// no paging space
	}

	// traceStep8: add the code in a .C/.H file to log the event
	// an example may be found in mem/FCMDeafult.C traceStep8
	TraceOSMemFCMDefGetPage(fileOffset, paddr);

	// set pagedesc paddr
	pg->paddr = paddr;

	// indicate that we have taken ownership of page
	//PageAllocatorKernPinned::initFrameDesc(paddr, getRef(),
	//				       fileOffset, 0);
    }

    return pg;
}

/*
 * for now, this is called without any locks held
 * but doingIO servers as a lock of sorts
 * N.B. don't try to modify the page descriptor without holding the big lock.
 * eventually, should be called holding the pg lock
 *
 * returns with rc = 2 and lock not held if page has been freed
 *                         only possible for zero fill page
 * returns with rc = 1 if io is in progress
 * returns with rc = 0 and lock held if io is done.
 *                         pg is trustworthy in this case
 */
SysStatusUval
FCMDefault::startFillPage(uval offset, PageFaultNotification* fn, PageDesc* pg)
{
    SysStatusUval rc;
    // now initiate I/O operation
    tassertMsg (pg->doingIO == PageDesc::SET,
		"startFillPage on unlocked page\n");

    rc = DREF(frRef)->startFillPage(pg->paddr, offset);
    tassert(_SUCCESS(rc), err_printf("error on startfillpage %016lx\n",rc));
    if (_FAILURE(rc)) return rc;
    if (_SUCCESS(rc) && (rc == FR::PAGE_NOT_FOUND)) {
	uval vpaddr;
	vpaddr = PageAllocatorKernPinned::realToVirt(pg->paddr);
	PageCopy::Memset0((void *)vpaddr, pageSize);
	setPFBit(ZeroFill);
	// safe to acquire lock and not re-find pg since doingIO is still set
	lock.acquire();
	//err_printf("doing early callback: %lx %lx\n", objOffset, physAddr);
	//call skips doing complete on fn and keeps lock it aquires
	pg->doingIO = PageDesc::CLEAR;
	//a zero fill page is initially clean since it matches the state
	//of the backing store (eg non existent)
	pg->cacheSynced = PageDesc::SET; // zero pages can't be useful code
	rc = ioCompleteInternal(pg, 0, fn, 1);
	if (rc==2) return 2;
	//return indicates no wait is needed with lock held
	_ASSERT_HELD(lock);
	return 0;
    }
    // return indicates a wait on fn is needed
    return 1;
}

/*
 * get or create a page descriptor and frame for the file offset
 * if this can't be done immediately and an fn is provided, return
 * the notification key having queued the fn on the page descriptor
 * and started the IO.
 *
 * Called unlocked.  Returns with the page locked if the page is available
 * Returns 1 if an fn was provided and async io is in progress.
 */

SysStatusUval
FCMDefault::getPageInternal(
    uval fileOffset, PageFaultNotification *fn, PageDesc*& pgarg,
    uval copyOnWrite)
{
    ScopeTime timer(GetPageTimer);
    (void) copyOnWrite;			// see FCMComputation for example
    SysStatus rc;
    PageDesc* pg;
    uval needsIO;

    while (1) {
	pgarg = pg = findOrAllocatePageAndLock(fileOffset, rc, needsIO);
	if (!pg) {
	    tassertWrn( _FAILURE(rc), "huh\n");
	    return rc;
	}
	tassert((!pg->forkCopied1)&&(!pg->forkCopied2)&&
		(!pg->forkIO)&&(!pg->forkIOLock),
		err_printf("oops\n"));

	if (pg->free) {
	    setPFBit(FromFreeList);
	    // page is on the reclaim list
	    pageList.dequeueFreeList(pg);
	    pg->free = PageDesc::CLEAR;
	    // pages on the free list can be doing IO, so continue
	}

	if (!(pg->doingIO)) {
	    tassertMsg(!needsIO, "should be doing IO\n");
	    // on success don't release lock
	    return 0;
	}

	// page is doing I/O
	if (fn) { // retry from application
	    // for get page, notification structure already
	    // initialized to original request
	    fn->next = pg->fn;
	    pg->fn = fn;
	    lock.release();

	    /*
	     *N.B. doingIO serves as a page lock on this path
	     *     for now, we are not "holding" page locks when
	     *     we make calls but we sort of are anyhow
	     */
	    if (needsIO) {
		rc = startFillPage(fileOffset, fn, pg);
		// did page disappear (freeAfterIO got set)
		if (rc == 2) continue;
		return rc;
	    }

	    // return io in progress
	    return 1;
	}

	// block here synchronously
	PageFaultNotification notification;
	notification.initSynchronous();
	notification.next = pg->fn;
	pg->fn = &notification;

	lock.release();


	if (needsIO) {
	    // now initiate I/O operation
//	    if (preFetch>0) err_printf("page in : %016lx %p\n",fileOffset,this);
	    rc = startFillPage(fileOffset, &notification, pg);
	    tassert(_SUCCESS(rc), err_printf("error on startfillpage\n"));
	    if (rc==2) continue;
	    if (rc==0) return 0;
	}

	doPreFetch(fileOffset);

	while (!notification.wasWoken()) {
	    Scheduler::Block();
	}
    }
}



void
FCMDefault::doPreFetch(uval fileOffset)
{
    PageDesc *pg;
    SysStatus rc;
    uval needsIO;

    for (uval i=1; i < preFetch; i++) {
	pg = findOrAllocatePageAndLock(fileOffset + pageSize*i, rc, needsIO,
				       PageAllocator::PAGEALLOC_NOBLOCK);
	if (!pg) {
	    err_printf("Aborted prefetch: would block\n");
	    return;
	}
	tassert((!pg->forkCopied1)&&(!pg->forkCopied2)&&
		(!pg->forkIO)&&(!pg->forkIOLock),
		err_printf("oops\n"));

	if (pg->free) {
	    // page is on the reclaim list
	    pageList.dequeueFreeList(pg);
	    pg->free = PageDesc::CLEAR;
	    // pages on the free list can be doing IO, so continue
	}

	if (!(pg->doingIO)) {
	    tassert(!needsIO, err_printf("oops\n"));
	    // success for this page
	    lock.release();
	    continue;
	}

	lock.release();

	if (needsIO) {
	    // now initiate I/O operation
	    rc = startFillPage(fileOffset + i * pageSize, NULL, pg);
	    tassert(_SUCCESS(rc), err_printf("error on startfillpage\n"));
	}
    }
}



SysStatusUval
FCMDefault::getPage(uval fileOffset, void *&dataPtr, PageFaultNotification *fn)
{
    SysStatus rc;
    uval paddr;
    PageDesc* pg;
    tassertMsg(!mapBase, "Should not be pinning while mapBase is true\n");

    // some interfaces optimize for reads.  Here, we want a real copy
    // of the page, so claim no copyOnWrite
    // in practice, the parameter is ignored in FCMDefault anyhow, but
    // we have to say something!
    rc = getPageInternal(fileOffset, fn, pg, 0);
    /*
     * rc == 0 if page was gotten and FCM was locked
     * rc >0 if io is in progress and fn will be posted
     * rc <0 failure
     */
    if (_SUCCESS(rc) && (rc == 0)) {
	paddr = pg->paddr;
	dataPtr = (void *)PageAllocatorKernPinned::realToVirt(paddr);
	// lock page
	if (pg->pinCount == 0) pinnedCount++;
	pg->pinCount++;
	tassertMsg(pg->pinCount != 0, "pinCount may have wrapped\n");
	referenceCount++;
	lock.release();
    }

    return rc;
}

SysStatus
FCMDefault::releasePage(uval fileOffset, uval dirty)
{
    PageDesc* pg;
    lock.acquire();
    pg = findPage(fileOffset);
    tassertMsg(pg && pg->pinCount,"release non locked page\n");
    tassertMsg(!mapBase, "Should not be unpinning while mapBase is true\n");
    if (dirty) {
	DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	pg->dirty=PageDesc::SET;
    }
    pg->pinCount--;
    if (pg->pinCount == 0) {
	pinnedCount--;
#if 0
    //FIXME - do we need notification when pincount goes to zero
    // notify anyone who might have blocked on this, and free lock
	notify(pg, 0, 0, 1);
#endif
    }
    //N.B. returns with lock released
    return locked_removeReference();
}


//set to zero to turn off copy on write
uval marcAllowCopyOnWrite = 0;
SysStatusUval
FCMDefault::mapPage(uval offset, uval regionVaddr, uval regionSize,
		    AccessMode::pageFaultInfo pfinfo, uval vaddr,
		    AccessMode::mode access, HATRef hat, VPNum vp,
		    RegionRef reg, uval firstAccessOnPP,
		    PageFaultNotification *fn)
{
    SysStatus rc;
    PageDesc* pg;
    uval copyOnWrite;
    setPFBit(fcmDefault);
    ScopeTime timer(MapPageTimer);

    /*
     * we round vaddr down to a pageSize boundary.
     * thus, pageSize is the smallest pageSize this FCM supports.
     * for now, its the only size - but we may consider using multiple
     * page sizes in a single FCM in the future.
     * Note that caller can't round down since caller may not know
     * the FCM pageSize.
     */
    vaddr &= -pageSize;
    
    // set copyOnWrite to indicate if a copyOnWrite optimization
    // is feasable - namely if the fault is for read and the
    // access mode can be converted to read only
    copyOnWrite =
	marcAllowCopyOnWrite &&
	!AccessMode::isWriteFault(pfinfo) &&
	AccessMode::makeReadOnly((access));

    if (firstAccessOnPP) updateRegionPPSet(reg);

    offset += vaddr - regionVaddr;

    //FIXME - this kludge bounds the size of a pageable
    //        FCM until we have proper algorithms
    if (pageable && (pageList.getNumPages() > maxNumPages)) {
         err_printf("\tGiving back 0x%lx pages (0x%lx > 0x%lx)\n",
                    giveBackNumPages, pageList.getNumPages(), maxNumPages);

	lock.acquire();
	locked_giveBack(giveBackNumPages);
	locked_pageScan(PM::HIGH);
	lock.release();
    }

 retry:

    //err_printf("mapPage for %lx, vp %ld\n", offset, vp);
    rc = getPageInternal(offset, fn, pg, copyOnWrite);
    /*
     * rc == 0 if page was gotten and locked
     * rc >0 if io is in progress and fn will be posted
     * rc <0 failure
     */
    if (_SUCCESS(rc) && (rc == 0)) {
	//err_printf("mapping %lx\n", offset);
	rc = mapPageInHAT(vaddr, pfinfo, access, vp, hat, reg, pg, offset);

	//err_printf("done mapping %lx\n", offset);
	if (pg->forkMapParent) {
	    // this was a dummy descriptor to let us map a parent page
	    tassertMsg(0==pg->fn, "notify should have been done\n");
	    pageList.remove(pg->fileOffset);
	}
	lock.release();
	if (_SGENCD(rc) == ENOMEM) {
	    err_printf("No mem on mapPage: sleeping and retrying\n");
	    Scheduler::DelayMicrosecs(100000);
	    goto retry;
	}
    } else if (_SUCCESS(rc)) {
	// Defered page fault
	// I've been sloppy about returning the FaultID all the way up
	// so just get it right.  Debugging a mistake here is hopeless
	tassertMsg(fn, "deferring page fault without fn\n");
	rc = fn->getPageFaultId();
    }

    return rc;
}

/*
 * in special case of a zero fill page which gets freed because
 * freeAfterIO is set, return 2 and always unlock
 */

/* virtual */ SysStatus
FCMDefault::ioCompleteInternal(PageDesc* pg, SysStatus rc,
			       PageFaultNotification* skipFn,  uval keepLock)
{
    _ASSERT_HELD(lock);
    if (pg->freeAfterIO) {
	uval vaddr;
	pg->doingIO = PageDesc::CLEAR;

	// we are meant to get rid of this page
	tassert(!pg->mapped, err_printf("Freeing but mapped\n"));
	// indicate that we are given up ownership of pages
	// PageAllocatorKernPinned::clearFrameDesc(pg->paddr);

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(pg->paddr);
	DREF(pmRef)->deallocPages(getRef(), vaddr, pg->len);

	// notify waiters, but retain lock
	notify(pg, rc, skipFn, 1);
	tassertWrn(pg->freeAfterIO,"Should not have freed page: %p %p\n",
		   pg,pg->fn);
	// remove from page list
	pageList.remove(pg->fileOffset);
	lock.release();
	checkEmpty();
	return 2;			// page has disappeared
    } else {
	// when either read or write completes
	// page is "clean" since it matches the disk
	DILMA_TRACE_PAGE_DIRTY(this,pg,-1);
	pg->dirty = PageDesc::CLEAR;
	pg->doingIO = PageDesc::CLEAR;
	notify(pg, rc, skipFn, keepLock);
    }
    return 0;
}

/* virtual */ SysStatus
FCMDefault::ioComplete(uval fileOffset, SysStatus rc)
{
    tassert(_SUCCESS(rc),
	    err_printf("bogus return on I/O request <%ld %ld %ld>\n",
		       _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc)));
    //err_printf("fill Complete for %lx, trying lock....\n", fileOffset);
    lock.acquire();
    //err_printf("got lock for fill Complete for %lx\n", fileOffset);
    PageDesc *pg = findPage(fileOffset);
    tassert(pg && pg->doingIO,
	    err_printf("bad request from FR %lx\n", fileOffset));
    tassert(!(pg->forkIO),
	    err_printf("why are we here with forkIO set?\n"));
    ioCompleteInternal(pg, rc);
    // the FR doesn't care what we did with the frame - always return ok
    return 0;
}

// Don't destroy FCM when last use disappears.  Only destroy
// if FR (file representative) detaches (file is really gone)
// or page replacement removes last page (lazy close).
// FIXME: no locking going on here???
SysStatus
FCMDefault::detachRegion(RegionRef regRef)
{
    //err_printf("FCMDefault::detachRegion(%lx) : %lx\n", regRef, getRef());
    RegionInfo *rinfo;
    PMRef       pm;
    uval        found;

    // check before locking to avoid callback on destruction deadlock
    if (beingDestroyed) return 0;

    lock.acquire();

    found = regionList.remove(regRef, rinfo);
    if (!found) {
	// assume race on destruction call
	tassert(beingDestroyed,err_printf("no region; no destruction race\n"));
	lock.release();
	return 0;
    }
    pm = rinfo->pm;
    delete rinfo;
    if (!referenceCount && regionList.isEmpty()) {
	//err_printf("FCMDefault::detachRegion(%lx) regionlist empty: %lx\n",
	//   regRef, getRef());
	lock.release();
	notInUse();
    } else {
	lock.release();
    }
    //err_printf("FCMDefault::detachRegion maybe doing updatePM\n");
    // above code may have destroyed us; updatePM info if we still have a pm
    if (!beingDestroyed) updatePM(pm);

    //err_printf("FCMDefault::detachRegion all done\n");

    return 0;
}

//FIXME get rid of marcScan once testing is complete
// see FCMComputation::forkCopy - this is for testing
// its used to force all pages to disk so we can test
// disk related fork copy paths
uval marcScan=0;

void
FCMDefault::locked_pageScan(PM::MemLevelState memLevelState)
{
    uval numScan, numFree, numFreeTarget;
    uval i;
    PageDesc *pg, *nextpg;
    uval numToWrite;
    uval gaveBack, unmapped, doingIO, established;

    if (beingDestroyed) {
	return;
    }

    numScan =  pageList.getNumPages();
    numFree = pageList.getNumPagesFree();
    numFreeTarget = numScan>>5;
    if (PM::IsLow((memLevelState))) {
	if (PM::IsCrit(memLevelState)) {
	    numFreeTarget = numScan;
	} else {
	    numFreeTarget = numScan>>3;
	}
    }
    if (numFreeTarget == 0) {
	numFreeTarget = 1;
    }
    if (numFree>=numFreeTarget) {
	return;
    }

#ifdef PAGING_VERBOSE
    err_printf("FCM %p scanning %ld out of %ld pages\n", getRef(),
	       numScan, pageList.getNumPages());
#endif /* #ifdef PAGING_VERBOSE */
    numToWrite = 0;
    gaveBack = unmapped = doingIO = established = 0;
    nextpg = pageList.getNext(nextOffset);
    for (i = 0; i < numScan; i++) {
	pg = nextpg;
	if (pg == NULL) {
	    // either the end or empty
	    pg = pageList.getNext(uval(-1));
	    if (pg == NULL) break;
	}
	nextpg = pageList.getNext(pg);
	if (nextpg == NULL) {
	    // offset of -1 starts searching from the beginning
	    nextOffset = uval(-1);
	} else {
	    nextOffset = nextpg->fileOffset;
	}
#if 0
	err_printf("FCM %lx got page %lx/%lx, d %ld, io %ld, m %ld\n",
		   getRef(), pg->fileOffset, pg->paddr, pg->dirty,
		   pg->doingIO, pg->mapped);
#endif /* #if 0 */
	if (pg->doingIO) {
	    doingIO++;
	    continue;
	}
	if (pg->established) {		// pinned pages, just quietly skip
	    established++;
	    continue;
	}
	if (pg->mapped) {
	    tassert(!pg->free, err_printf("oops\n"));
	    unmapPage(pg);
	    tassert(pg->ppset == 0 && !pg->mapped, err_printf("oops\n"));
	    unmapped++;
	}
	tassert(pg->ppset == 0, err_printf("oops\n"));
	if (!pg->free) {
	    pg->free = PageDesc::SET;
	    pageList.enqueueFreeList(pg);
	    gaveBack++;
	    numFree++;
	    // stop when freelist is 1 percent of pages
	    if (numFree>=numFreeTarget) break;
	}
    }

#ifdef PAGING_VERBOSE
    err_printf("FCM %p: g %ld, um %ld, io %ld, est %ld, scan %ld, tot %ld\n",
	       getRef(), gaveBack, unmapped, doingIO, established, i, numFree);
#endif /* #ifdef PAGING_VERBOSE */

    return;
}



SysStatus
FCMDefault::getSummary(PM::Summary &sum) {
    sum.set(pageList.getNumPages(), pageList.getNumPagesFree());
    return 0;
}

SysStatus
FCMDefault::giveBack(PM::MemLevelState memLevelState)
{
    SysStatus rc;
    uval numPagesStart;

    if (!pageable) {
	return _SRETUVAL(PageAllocatorKernPinned::PINNED);
    }

    // we sweep through the pagelist collecting up to listSize pages to
    // writeback, release locks do write, and start again if there is
    // more left
    lock.acquire();
    numPagesStart = pageList.getNumPagesFree();
    switch(memLevelState) {
    case PM::HIGH:
	rc = 0;				// no pages needed
	numPagesStart = 0;		// no need to checkEmpty
	break;
    case PM::MID:
	rc = locked_giveBack(numPagesStart/4 + 1);
	break;
    case PM::LOW:
	rc = locked_giveBack(numPagesStart/2 + 1);
	break;
    case PM::CRITICAL:
	locked_pageScan(memLevelState);
	rc = locked_giveBack(numPagesStart);
	break;
    default:
	passertMsg(0, "Bogus memLevelState %ld in giveBack\n",
		   uval(memLevelState));
	rc = -1;
    }
    locked_pageScan(memLevelState);
    lock.release();

    if (numPagesStart > 0) {
	/* Only checkEmpty if we had frames, since we
	 * don't want to repeatedly signal a transition to
	 * empty when nothing happened.
	 */
	checkEmpty();
    }
    return rc;
}

/* virtual */ SysStatus 
FCMDefault::resumeIO()
{
    SysStatus rc;

    // retry the pending request, note rq already protected by
    // pendingWrite flag
    tassertMsg((pendingWrite != NULL), "woops\n");
    rc=DREF(frRef)->startPutPage(pendingWrite->paddr, pendingWrite->fileOffset, 
				 rq);
    if (_FAILURE(rc)) {
	passertMsg((_SCLSCD(rc) == FR::WOULDBLOCK), "woops\n");
	// just return, still couldn't make forward progress
	return 0;
    } 

    lock.acquire();
    pendingWrite = 0;			// pending actually started
    lock.release();
    writeFreeFreeList(100);		// random number...
    return 0;
}


/* virtual */ SysStatusUval
FCMDefault::writeFreeFreeList(uval numPages)
{
    uval totalReturned = 0;
    PageDesc *pg;
    SysStatus rc;


    lock.acquire();

    while (1) {
	// could be pending from when released lock
	if (pendingWrite) { break; }

	pg = pageList.dequeueFreeList(PageDesc::DQ_DIRTY);
	if (pg == NULL) {
	    lock.release();
	    return totalReturned;
	}

	//N.B. can be doingIO, for example if fsync has happened
	tassert(!pg->mapped, err_printf("oops\n"));
	tassert(!pg->established, err_printf("oops\n"));
	pg->free = PageDesc::CLEAR;
	if (pg->doingIO) {
	    pg->freeAfterIO = PageDesc::SET;
	} else {
	    tassert ((pg->dirty == PageDesc::SET), err_printf("woops\n"));
	    pg->doingIO = PageDesc::SET;
	    DILMA_TRACE_PAGE_DIRTY(this,pg,-1);
	    pg->dirty = PageDesc::CLEAR;
	    pg->freeAfterIO = PageDesc::SET;
	    pendingWrite = pg;
	    lock.release();
	    rc=DREF(frRef)->startPutPage(pg->paddr, pg->fileOffset, rq);
	    lock.acquire();
	    if (_SUCCESS(rc)) {
		pendingWrite = 0;
	    } else if (_FAILURE(rc) && (_SCLSCD(rc) == FR::WOULDBLOCK)) {
		break;
	    } else {
		passertMsg(0, "woops");
		break;
	    }

	} 
	if (totalReturned >= numPages) break;
    }
    lock.release();
    return totalReturned;
}


#define USE_RESUME

SysStatusUval
FCMDefault::locked_giveBack(uval numPages)
{
    _ASSERT_HELD(lock);
    //N.B. these arrays are on the stack, so they must be kept small
    FreeFrameList ffl;
    uval tmp=0;
    uval totalReturned = 0;
    PageDesc *pg;
    SysStatus rc;

    if (beingDestroyed) {
	    // we are being destroyed now, so info is soon to be useless
	    return -1;
    }

    while (totalReturned < numPages) {
	if (pendingWrite) {
	    pg = pageList.dequeueFreeList(PageDesc::DQ_CLEAN);
	} else {
	    pg = pageList.dequeueFreeList(PageDesc::DQ_HEAD);
	}
	if (pg == NULL) break;

	//N.B. can be doingIO, for example if fsync has happened
	tassert(!pg->mapped, err_printf("oops\n"));
	tassert(!pg->established, err_printf("oops\n"));
	pg->free = PageDesc::CLEAR;
	totalReturned+=pageSize/PAGE_SIZE;
	if (pg->doingIO) {
	    pg->freeAfterIO = PageDesc::SET;
	} else if (pg->dirty) {
	    pg->doingIO = PageDesc::SET;
	    DILMA_TRACE_PAGE_DIRTY(this,pg,-1);
	    pg->dirty = PageDesc::CLEAR;
	    pg->freeAfterIO = PageDesc::SET;
	    tassertMsg( (pendingWrite == NULL), "woops\n");
	    
#ifdef USE_RESUME
	    pendingWrite = pg;
#endif
	    lock.release();
#ifdef USE_RESUME
	    rc=DREF(frRef)->startPutPage(pg->paddr, pg->fileOffset, rq);
#else
	    rc=DREF(frRef)->startPutPage(pg->paddr, pg->fileOffset);
#endif
	    lock.acquire();
	    if (_SUCCESS(rc)) {
		pendingWrite = 0;
	    } else if (_FAILURE(rc) && (_SCLSCD(rc) != FR::WOULDBLOCK)) {
		passertMsg(0, "woops");
	    } 

	} else {
	    // clean page
	    tmp = PageAllocatorKernPinned::realToVirt(pg->paddr);
	    //FIXME maa do we need sizes in free frame list
	    // would be nice to release lock here, but then must do 
	    // pageList.remove before dealloc
	    tassertMsg(0==pg->fn, "notify should have been done\n");
	    pageList.remove(pg->fileOffset);
	    lock.release();
	    DREF(pmRef)->deallocPages(getRef(), tmp, pageSize);
	    lock.acquire();
	}
    }

    return totalReturned;
}

SysStatusUval
FCMDefault::getForkPage(
    PageDesc* callerPg, uval& returnUval, FCMComputationRef& childRef,
    PageFaultNotification *fn, uval copyOnWrite)
{
    SysStatusUval rc;
    PageDesc *pg;
    uval fileOffset;

    fileOffset = callerPg->fileOffset;

    rc = getPageInternal(fileOffset, fn, pg, copyOnWrite);

    /*
     * rc == 0 if page was gotten and FCM was locked
     * rc >0 if io is in progress and fn will be posted
     * rc <0 failure
     */

    if (_SUCCESS(rc) && (rc == 0)) {
	//available
	callerPg->paddr = pg->paddr;

// cant do copyonwrite to we can call back to unmap
#if 0
	/* check for copy on write first.  Thus, a frame that
	 * we could give to the child is instead mapped
	 * copyOnWrite.
	 *
	 * The normal case is a child/parent pair (the shell)
	 * continuously creating a new second child which then
	 * terminates.
	 *
	 * In that case, this order collects all the read only
	 * data pages in the fork parent, and they never are
	 * unmapped in the shell.  Only the written pages
	 * will be moved from the shell child to the parent
	 * at fork.
	 *
	 * The down side is that a read/write sequence on a
	 * page which could be moved up will be more expensive,
	 * particularly if the page is already dirty so we could
	 * have mapped it read/write in the child immediately.
	 *
	 * The alternative is to move this check below the
	 * check for giving the frame to the parent.
	 */

	if (copyOnWrite) {
	    //caller can accept copy on write mapping
	    callerPg->paddr = pg->paddr;
	    pg->ppset |= uval(1) << Scheduler::GetVP();

	    if (pg->cacheSynced == PageDesc::CLEAR) {
		// Machine dependent operation.
		setPFBit(CacheSynced);
		CacheSync(pg);
	    }

	    if (pg->mapped != PageDesc::SET) {
		// mark page mapped
		pg->mapped = PageDesc::SET;
		// also mark framearray to indicate page is now mapped
		// PageAllocatorKernPinned::setAccessed(pg->paddr);
	    }
	    lock.release();
	    return MAPPAGE;
	}
#endif
	// page lock held until unLockPage call
	pg->doingIO = PageDesc::SET;

	// copy our ppset to caller to it can unmap if needed
	// (only happens if copyonwrite logic is enabled)
	callerPg->ppset = pg->ppset;
	callerPg->mapped = pg->mapped;
	returnUval = uval(pg);	// caller needs this to unlock
	lock.release();
	return FRAMETOCOPY;
    }

    // doingIO
    return (rc>0)?DOINGIO:rc;
}
