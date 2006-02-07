/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMComputation.C,v 1.92 2005/08/25 18:37:40 rosnbrg Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include "mem/FCMComputation.H"
#include <trace/traceMem.h>
#include "defines/paging.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/FR.H"
#include "mem/PageFaultNotification.H"
#include "mem/PM.H"
#include "cobj/CObjRootSingleRep.H"
#include "mem/FRComputation.H"
#include <mem/CacheSync.H>
#include "mem/PageCopy.H"
#include "mem/PerfStats.H"
#include "mem/FRCRW.H"

// if number unused last time less than copy/factor copy indicated
uval FCMComputation::CopyOnForkFactor=5;
// if less than Always pages, copy
uval FCMComputation::CopyOnForkAlways=5;
//MAA temp
uval numForks=0, numAdopts=0, numNew=0, numEmpty=0, numCopies=0,
    numChanges=0, numPagesCopied=0, numUnused=0;
void
MarcPrintStats()
{
    err_printf(
	"numForks numAdopts numNew numEmpty numCopies numChanges "
	"numPagesCopied numUnused factor\n"
	"%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld\n",
	numForks, numAdopts, numNew, numEmpty, numCopies,
	numChanges, numPagesCopied, numUnused,
	FCMComputation::CopyOnForkFactor,
	FCMComputation::CopyOnForkAlways);
    numForks=0, numAdopts=0, numCopies=0, numNew=0, numEmpty=0,
	numChanges=0, numPagesCopied=0, numUnused=0;
}

SysStatus
FCMComputation::Create(FCMRef &ref, FRRef cr, uval pgSize, uval pageable)
{
    SysStatus rc;
    FCMComputation *fcm;

    fcm = new FCMComputation;
    if (fcm == NULL) return -1;
    rc = fcm->init(ref, cr, pgSize, pageable, 1, 0, 0);
    TraceOSMemFCMComputationCreate((uval)ref);

    return rc;
}

SysStatus
FCMComputation::Create(
    FCMRef &ref, FRRef cr, FRRef baseFR, uval pgSize, uval pageable)
{
    SysStatus rc;
    FCMComputation *fcm;
    FCMRef copyParent;

    fcm = new FCMComputation;
    if (fcm == NULL) return -1;
    rc = fcm->init(ref, cr, pgSize, pageable, 1, 0, 0);
    DREF(baseFR)->attachFCM(copyParent, (FCMRef)fcm);
    // we lie - forkParent may not be an FCMComputation
    // code is very careful to only call
    // fcm methods unless it is "sure" the parent is an FCMComputation
    fcm->forkParent = (FCMComputationRef)copyParent;
    fcm->isCRW = 1;
    fcm->mapBase = 1;

    TraceOSMemFCMComputationCreate((uval)ref);

    return rc;
}

SysStatus
FCMComputation::CreateFixedNumaNode(
    FCMRef &ref, FRRef cr, uval pgSize, uval pageable, uval nnode)
{
    FCMComputation *fcm;
    SysStatus       rc;

    fcm = new FCMComputation;
    if (fcm == NULL) return -1;

    rc = fcm->init(ref, cr, pgSize, pageable, 1, 0, 0);
    fcm->numanode = nnode;		// override default in fcm::init
    TraceOSMemFCMComputationCreate((uval)ref);
    return rc;
}

/*
 * getPage is the pin/unpin interface.
 * If mapBase is true, we have a problem.  For now, we just
 * go to a private copy.  BUT - raw writing a constant in text to a
 * socket/pty will cause this.  Is it worth fixing doing better.
 * Doing better is very hard.  It would be wrong to provide the mapBase
 * page in response to a getPage if a write is ever possible, even
 * if this getPage is for read.  Because a latter write to the pinned
 * page could not be served while the page was pinned.
 * You might think it would be OK if the region via which the pin occurs
 * is read only, as TEXT is.  BUT - what if the debugger then tries to
 * convert the region to write.  At that point, even is we satisfied writes
 * with a copy, we'd have a problem tying the release back to the underlying
 * mapBase page.  In the worst case, the copy could also be pinned!
 */
SysStatusUval
FCMComputation::getPage(
    uval fileOffset, void *&dataPtr, PageFaultNotification *fn)
{
    if (mapBase) {
	lock.acquire();
	locked_unmapBase();
	lock.release();
    }
    return FCMDefault::getPage(fileOffset, dataPtr, fn);
}

/*
 * If the fork parent has a frame which has already been copied by the
 * other immediate child, it should give the frame to this
 * immediate child.
 *
 * childRef passes in the leaf fcm, and we return our parent if its
 * a TRYPARENT return
 *
 * If the fork parent has a frame which needs to be copied, we
 * use the getPage/Lock protocol above.
 *
 * If the page is on disk and this is the first of two possible
 * requests, we need to allocate a frame and do IO.
 *
 * Finally, if the frame is on disk and this is the last request,
 * we need to give the disk block to the caller and let the caller
 * do the IO.
 *
 *
 * getForkPage differs from getPage in that it sometimes actually
 * gives a frame or disk block to the caller.
 *
 *
 * How do we remember on disk?  By assuming that anything on disk
 * has not been asked for yet!  We will try not to page out a frame that's
 * already been asked for once, but rather give it to the other child.
 *
 * Lookup has three basic choices - in memory, on disk, not found
 *
 * Copy on write is implemented by detecting cases where it is best
 * and returning MAPPAGE.  The pageDesc is marked mapped here, which
 * prevents it from disappearing.  Curiously, it is safe to give a page
 * to a child even if it is mapped, since in that case only that child
 * can have it mapped!  This is because we only give pages to immediate
 * children which are also leaves!
 *
 * Called from another object.
 */

SysStatusUval
FCMComputation::getForkPage(
    PageDesc* callerPg, uval& returnUval,
    FCMComputationRef& childRef,
    PageFaultNotification *fn,
    uval copyOnWrite)
{
    SysStatusUval rc;
    PageDesc *pg;
    uval fileOffset;

    // start with prechecks to see if we have this page
    lock.acquire();

    if (beingDestroyed||hasCollapsed) {
	lock.release();
	return _SDELETED(1697);
    }

    if ((uval(forkChild1)|uval(forkChild2)) == 0) {
	// not a fork parent but a copy on write parent
	lock.release();
	return FCMDefault::getForkPage(
	    callerPg, returnUval, childRef, fn, copyOnWrite);
    }
    
    fileOffset = callerPg->fileOffset;

    pg = findPage(fileOffset);

    while (1) {
	_ASSERT_HELD(lock);

	if (pg) {
	    tassertMsg(!((childRef == forkChild1 && pg->forkCopied1) ||
		      (childRef == forkChild2 && pg->forkCopied2)),
		       "child wants page it already has\n");

	    if (pg->free) {
		// page is on the reclaim list
		pageList.dequeueFreeList(pg);
		pg->free = PageDesc::CLEAR;
		// pages on the free list can be doing IO, so continue
	    }

	    // not doingIO - its available
	    if (pg->doingIO==PageDesc::CLEAR) {
		//available
		callerPg->paddr = pg->paddr;

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
		    pg->ppset |= uval(1) << Scheduler::GetVP();

		    if (pg->cacheSynced == PageDesc::CLEAR) {
			// Machine dependent operation.
			setPFBit(CacheSynced);
			CacheSync(pg);
		    }

		    if (pg->mapped != PageDesc::SET) {
			// mark page mapped
			pg->mapped = PageDesc::SET;
		    }
		    lock.release();
		    return MAPPAGE;
		}

		/*
		 * this conditional checks for
		 * one child complete or missing
		 * and the request from the immediate other child
		 * in those cases, we can give the page to the caller.
		 */
		if ((((pg->forkCopied1 == PageDesc::SET) ||
		     (forkChild1 == 0))
		    &&
		    (childRef == forkChild2))
		   ||
		   (((pg->forkCopied2 == PageDesc::SET) ||
		     (forkChild2 == 0))
		    &&
		    (childRef == forkChild1))) {
		    /*
		     * we can give the page to this requestor
		     * There is a choice here on dealing with disk blocks
		     * that back dirty frames - we can free them or
		     * transfer them - we choose to transfer them
		     *
		     * getBlockID removes disk block from fr if
		     * exists, else sets blockID to -1
		     *
		     * FIXME MAA propogate ppset to child - this frame
		     * may be copyonwrite mapped in some child processors.
		     */

		    DREF(frRef)->getBlockID(fileOffset, returnUval);
		    // copy our ppset to caller to it can unmap if needed
		    // (only happens if copyonwrite logic is enabled)
		    callerPg->ppset = pg->ppset;
		    callerPg->mapped = pg->mapped;
		    // return our frame address to caller for its use
		    callerPg->paddr = pg->paddr;
		    // return cache synced state - its still valid
		    callerPg->cacheSynced = pg->cacheSynced;
		    if(pg->forkCopied1) copies1--;
		    if(pg->forkCopied2) copies2--;
		    pageList.remove(fileOffset);
		    lock.release();
		    return FRAMETOKEEP;
		}
		// page lock held until unLockPage call
		pg->doingIO = PageDesc::SET;
		if (childRef == forkChild1) {
		    pg->forkCopied1 = PageDesc::SET;
		    copies1++;
		} else if (childRef == forkChild2) {
		    pg->forkCopied2 = PageDesc::SET;
		    copies2++;
		}
		tassertMsg(!(pg->forkCopied1 && pg->forkCopied2),
			   "gave copies to both children\n");
		// copy our ppset to caller to it can unmap if needed
		// (only happens if copyonwrite logic is enabled)
		callerPg->ppset = pg->ppset;
		callerPg->mapped = pg->mapped;
		// return our frame address to caller for copy source
		callerPg->paddr = pg->paddr;
		returnUval = uval(pg);	// caller needs this to unlock
		lock.release();
		return FRAMETOCOPY;
	    }

	    // doingIO - queue or wait
	    // call release lock as side effect
	    rc = queueFnOrBlock(fn,  pg);
	    if (rc == DOINGIO) return DOINGIO;
	    lock.acquire();
	    pg = findPage(fileOffset);
	    // restart the getForkPage
	    continue;
	} else if (0 == DREF(frRef)->checkOffset(fileOffset)) {
	    /* no page but have disk block
	     * since we've lost track of any previous
	     * immediate child copies, we can only
	     * give away the block if we have only one child
	     * and he is the caller
	     */
	    if (((forkChild1 == 0) && (childRef == forkChild2))
		||
		((forkChild2==0) && (childRef == forkChild1))) {
		// only one copy needed - remove block from my FR
		DREF(frRef)->getBlockID(fileOffset, returnUval);
		lock.release();
		return BLOCKIDTOKEEP;
	    }
	    pg = addPage(fileOffset, uval(-1), pageSize);
	    pg->doingIO = PageDesc::SET;
	    rc = getFrame(pg->paddr);
	    lock.release();
	    passertMsg(_SUCCESS(rc), "oops\n");
	    // indicate that we have taken ownership of page
	    // PageAllocatorKernPinned::initFrameDesc
	    //       (pg->paddr, FCMRef(getRef()), pg->fileOffset, 0);
	    // just need to start the io.  ioComplete will
	    // notify the fork children, restarting the io.
	    rc = FCMDefault::startFillPage(fileOffset, 0, pg);
	    if (_FAILURE(rc)) return rc;
	    tassertMsg(rc==1, "where has the disk block gone?\n");
	    lock.acquire();
	    pg = findPage(fileOffset);
	    // restart the getForkPage
	    continue;
	} else {
	    childRef = forkParent;
	    lock.release();
	    return childRef?TRYPARENT:NOPAGE;
	}
    }
}

/*
 * get or create a page descriptor and frame for the file offset
 * if this can't be done immediately and an fn is provided, return
 * the notification key having queued the fn on the page descriptor
 * and started the IO.
 *
 * Called unlocked.  Returns with the page locked if the page is available
 * Returns 1 if an fn was provided and async io is in progress.
 *
 * This version must deal with the fork parent.
 *
 * The algorithm is:
 *
 * Look for the page - if found done.
 * Allocate a PageDesc and set it doingIO so lock can be released
 * See if there is a disk copy, if so allocate a frame and start IO
 * Call the parent.
 *
 */

SysStatusUval
FCMComputation::getPageInternal(
    uval fileOffset, PageFaultNotification *fn, PageDesc*& pgarg,
    uval copyOnWrite)
{
    SysStatusUval rc;
    ScopeTime timer(GetPageTimer);
    PageDesc* pg;
#if 0 // removing assert to allow shadow to have mapBase set
    tassertMsg(!mapBase,
	       "Should not be serving requests while mapBase is true\n");
#endif

    /*
     * testing forkParent without holding the lock is safe because
     * the forkCopy logic is protected by the region "lock" which
     * guarantees the no requests are in flight when a fork copy
     * is done.
     */
    if (forkParent == 0) {
	return FCMDefault::getPageInternal(fileOffset, fn, pgarg, copyOnWrite);
    }

    lock.acquire();
    pgarg = pg = findPage(fileOffset);

    uval ridiculousLoopLimit = 1000000;
    while (1) {
	passertMsg(ridiculousLoopLimit-- > 0,
		   "Infinite loop in getPageInternal.\n");
	_ASSERT_HELD(lock);
	/*
	 * If we ever find a pagedesc or a disk block, we
	 * never again need to go to the fork Parent
	 * so releasing the lock and running the normal code
	 * must get the right answer.
	 * (unless its forkIO/forkIOComplete)
	 */

	if (((pg != 0) && (pg->forkIO == PageDesc::CLEAR)) ||
	    ((pg == 0) && (DREF(frRef)->checkOffset(fileOffset) == 0))) {
	    /*
	     * get the count of aggressively copied pages we've never
	     * used approximately right, where approvimately means
	     * i'm not sure there isn't an edge case.
	     * Notice that if a copied page is paged out before use we
	     * will never account for it, which is correct - we shouldn't
	     * have copied it if we couldn't use it in time.
	     */
	    if(pg && pg->copiedOnFork) {
		pg->copiedOnFork = PageDesc::CLEAR;
		lastChildUnmappedCount--;
	    }
	    lock.release();
	    return FCMDefault::getPageInternal(
		fileOffset, fn, pgarg, copyOnWrite);
	}

	if (pg == 0) {
	    /* allocate a new page - this serves as a page lock for this
	     * offset, once we release the lock.
	     * if subsequent calls come in, they will just queue notifications
	     */

	    pgarg = pg = addPage(fileOffset, uval(-1), pageSize);
	} else if (pg->forkIOLock == PageDesc::SET) {
	    rc = queueFnOrBlock(fn,  pg);
	    if (rc == DOINGIO) return 1;
	    lock.acquire();
	    pgarg = pg = findPage(fileOffset);
	    continue;
	}

	/*
	 * doingIO servers as an overall lock while this routine is working.
	 * forkIO directs request to try this path rather than
	 * just queueing - needed so we can restart a request.
	 * forkIOLock indictates that some thread is working this
	 * request, others should in fact queue
	 */

	pg->doingIO = PageDesc::SET;
	pg->forkIO = PageDesc::SET;
	pg->forkIOLock = PageDesc::SET;

	// no page available, no disk block, we have a fork parent
	rc = initiateGetParentPage(fileOffset, pg, fn, copyOnWrite);
	_ASSERT_HELD(lock);

	/* ISDELETED is a failure code, check first
	 * in this case, it just means a parent disappeared
	 * after its child told us to call it.
	 * So we just start all over again
	 */
	if (_ISDELETED(rc)) {
	    /*
	     * we have the fcm lock right now.
	     * clear the forkIOLock so retry above will be
	     * willing to reprocess this pg
	     */
	    tassertMsg(pg->doingIO && pg->forkIO && pg->forkIOLock,
		       "don't hold correct locks\n");
	    pg->forkIOLock = PageDesc::CLEAR;
	    continue;
	}

	if (_FAILURE(rc)) {
	    lock.release();
	    return rc;
	};

	switch (_SGETUVAL(rc)) {
	case DOINGIO:
	    /*fn was queued on the fork parent who has the page
	     *release the forkLock.  the first thread to be restarted
	     *for this operation will try again leave doingIO and
	     **forkIO set to distinguish from case of real io on the
	     **page we must notify anyone blocked, since it is
	     *possible that the request blocked on the fork parent
	     *page will ignore the notify and NOT retry.
	     *You might think it would be better to immediately
	     *retry other requests instead of queueing them when
	     *the forkIOLock is seen and then going through the cost
	     *of restart.  But this is a very rare case, and the rejected
	     *strategy does bad things if the original requester blocks
	     *on the fork parent IO instead of queuing a notification.
	     */
	    pg->forkIOLock = PageDesc::CLEAR;
	    // notify waiters and release lock
	    notify(pg);
	    return 1;
	case BLOCKIDTOKEEP:
	    pg = 0;			// its been freed
	    break;
	case RETRY:
	    // pg points to correct desc, lock still held
	    // once we have a valid frame, we can/must clear forkCopy
	    pg->doingIO = PageDesc::CLEAR;
	    pg->forkIO = PageDesc::CLEAR;
	    pg->forkIOLock = PageDesc::CLEAR;
	    _ASSERT_HELD(lock);
	    return 0;
	    break;
	case MAPPAGE:
	    // pg points to correct desc, lock still held
	    // once we have a valid frame, we can/must clear forkCopy
	    // page is ready to map - followed by release this pagedesc
	    pg->doingIO = PageDesc::CLEAR;
	    pg->forkIO = PageDesc::CLEAR;
	    pg->forkIOLock = PageDesc::CLEAR;
	    _ASSERT_HELD(lock);
	    return 0;
	    break;
	default:
	    // this includes NOPAGE, which should not happen
	    passertMsg(0, "Unexpected return code\n");
	}
    }
}


/*
 * queues fn and returns DOINGIO or blocks until complete
 * and returns COMPLETE (if fn is 0)
 * called wiht lock held, returns with lock free
 */
uval
FCMComputation::queueFnOrBlock(PageFaultNotification *fn, PageDesc* pg)
{
    _ASSERT_HELD(lock);
    if (fn) {
	// need to queue on the IO
	// for get page, notification structure already
	// initialized to original request
	fn->next = pg->fn;
	pg->fn = fn;
	lock.release();
	return DOINGIO;
    } else {
	// block here synchronously
	PageFaultNotification notification;
	notification.initSynchronous();
	notification.next = pg->fn;
	pg->fn = &notification;
	lock.release();
	while (!notification.wasWoken()) {
	    Scheduler::Block();
	}
	return COMPLETE;
    }
}

/*
 * called with lock held
 *
 * in cases when need to potentially get a forkParent page
 * tries to allocate (if necessary) and fill in a pg
 *
 * if no page is available
 *   if zeroFill then create a zero filled page
 *   otherwise free pg and return NOPAGE
 *
 * returns with lock held
 * returns:
 *    errors - pg has been freed
 *    NOPAGE - pg has been freed, there is no forkParent page
 *    BLOCKIDTOKEEP - pg has been freed, blockID has been added to FR
 *    DOINGIO - io is in progress and lock held
 *    RETRY - pg exists - reprocess it
 * This code is only executed by a leaf child.
 */

SysStatusUval
FCMComputation::initiateGetParentPage(
    uval fileOffset, PageDesc* pg, PageFaultNotification *fn,
    uval copyOnWrite)
{
    _ASSERT_HELD(lock);
    SysStatusUval rc;
    uval returnUval;
    PageFaultNotification *notf;
    uval vaddr, vaddr1;
    FCMComputationRef childRef, parentRef;
    childRef = forkParent; // becomes parent below
    tassertMsg(childRef, "oops\n");

    // move down the fork chain, passing child ref to each parent
    // call parent without holding lock but holding forkIOLock
    lock.release();

    do {
	parentRef = childRef;	// set by getForkPage call below
	childRef = getRef();

	rc = DREF(parentRef)->getForkPage(
	    pg, returnUval, childRef, fn, copyOnWrite);

	/*
	 * if the failure is deleted object our caller deals with
	 * it by starting all over
	 */
	if (_FAILURE(rc)) {
	    lock.acquire();
	    return rc;
	};
    } while (_SGETUVAL(rc) == TRYPARENT);

    lock.acquire();

    switch (_SGETUVAL(rc)) {
    case NOPAGE:
	// nopage and we've been asked to make one
	rc = getFrame(pg->paddr);
	passertMsg(_SUCCESS(rc), "oops\n");
	// indicate that we have taken ownership of page
	// PageAllocatorKernPinned::initFrameDesc(pg->paddr, FCMRef(getRef()),
	//				       pg->fileOffset, 0);
	vaddr = PageAllocatorKernPinned::realToVirt(pg->paddr);
	PageCopy::Memset0((void *)vaddr, pageSize);
	setPFBit(ZeroFill);
	DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	pg->dirty = PageDesc::SET;
	pg->cacheSynced = PageDesc::SET; // zero pages can't be useful code
	// notify waiters but retain lock
	notify(pg, 0, 0, 1);
	return RETRY;

    case DOINGIO:
	return DOINGIO;

    case FRAMETOCOPY:
	//KLUDGE alert - the parents frame address was returned in
	//our pg->paddr
	vaddr1 = PageAllocatorKernPinned::realToVirt(pg->paddr);
	// getFrame release and reaquires the lock.
	rc = getFrame(pg->paddr);
	passertMsg(_SUCCESS(rc), "oops\n");
	// indicate that we have taken ownership of page
	// PageAllocatorKernPinned::initFrameDesc(pg->paddr, FCMRef(getRef()),
	//				       pg->fileOffset, 0);
	vaddr = PageAllocatorKernPinned::realToVirt(pg->paddr);
	PageCopy::Memcpy((void*)vaddr, (void*)vaddr1, pageSize);
	setPFBit(CopyFrame);
	DREF(parentRef)->unLockPage(returnUval);
	if (pg->mapped) {
	    // this offset may have previously been mapped copyOnWrite
	    // we're not sure, so we must unmap it.
	    unmapPage(pg);
	}
	DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	pg->dirty = PageDesc::SET;
	// notify waiters but retain lock
	notify(pg, 0, 0, 1);
	return RETRY;

    case FRAMETOKEEP:
	// indicate that we have taken ownership of page
	// PageAllocatorKernPinned::initFrameDesc(pg->paddr, FCM::getRef(),
	//				       pg->fileOffset, 0);
	// returnUval is the blockID
	setPFBit(KeepFrame);
	if (returnUval != uval(-1)) {
	    // only give us the disk block if its useful
	    DREF(frRef)->setOffset(fileOffset, returnUval);
	} else {
	    DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	    pg->dirty = PageDesc::SET;
	}
	// notify waiters but retain lock
	notify(pg, 0, 0, 1);
	return RETRY;

    case MAPPAGE:
	// use the parent page
	tassertMsg(copyOnWrite,
		   "asked to use parent page for a write fault\n");
	pg->forkMapParent = PageDesc::SET;
	// notify waiters but retain lock
	notify(pg, 0, 0, 1);
	return MAPPAGE;

    case BLOCKIDTOKEEP:
	// got a disk block - give it to the FR - blockID in returnUval
	DREF(frRef)->setOffset(fileOffset, returnUval);
	/* now get rid of our pg descriptor even though if you
	 * follow the path we wind up remaking one.  it's
	 * technically easier to do this than to reproduce all the
	 * code which gets a page from disk and can't matter to
	 * performance since we're doing a diskio anyhow
	 */
	notf = pg->fn;
	tassertMsg(!pg->forkCopied1&&!pg->forkCopied2,
		"why are copy bits set here\n");
	pageList.remove(fileOffset);
	while (notf) {
	    PageFaultNotification *nxt = notf->next;
	    notf->setRC(0);
	    notf->doNotification();
	    notf = nxt;
	}
	return BLOCKIDTOKEEP;

    default:
	passertMsg(0, "oops\n");
	return 0;			// never reached
    }
}


#if 0
#define marcstresspaging
extern uval marcScan;
#endif

/*
 * Called on a child who may have a parent with only one child.  Because
 * no locks are held, this is only a hint.
 * Asks the only child to start a collapse - (usually) called by its parent.
 * lock is retained till the collapse finishes or fails.
 * If the collapse succeeds, this FCM is removed from the fork tree.
 * Its children become children of its parent.
 * Because other threads may be walking the fork tree, we must leave
 * the parent pointer pointing to the old parent.
 * Leafs never collapse with parents.  They may disappear if not in
 * use.  If a leaf is forkcopied, for copy will attempt to use the leafs
 * parent as a new fork parent when the parent has only this one child.
 */
/*virtual*/ SysStatus
FCMComputation::forkCollapse()
{
    SysStatus rc;

    lock.acquire();

    if (beingDestroyed||hasCollapsed) {
	lock.release();
	return _SDELETED(1700);
    }

    // don't even try if we are a leaf
    if ((!forkChild1) && (!forkChild2)) {
	lock.release();
	return 0;
    }

    // stop the IO first, since it releases and reaquires locks

//FIXME
//following is for testing only
#if defined(marcstresspaging)
    PM::Summary sum;
    uval numPages=100000;
    marcScan = 1;
    locked_pageScan();
    marcScan = 0;
    lock.release();
    giveBack(numPages ,sum);
    Scheduler::Yield();
    lock.acquire();
#endif /* #if defined(marcstresspaging) && ... */

    /*
     * wait for any in progress IO to complete
     * must make sure we don't miss any new IO started
     * by paging
     * N.B. locks may be dropped and reaquired here
     */
    finishAllIO();

    if (beingDestroyed||hasCollapsed) {
	lock.release();
	return _SDELETED(1698);
    }

    // don't even try if we are a leaf - may have changed while unlocked
    if ((!forkChild1) && (!forkChild2)) {
	lock.release();
	return 0;
    }

    uval ridiculousLoopLimit = 1000000;
    do {
	passertMsg(ridiculousLoopLimit-- > 0,
		   "Infinite loop in forkCollapse.\n");
	rc = DREF(forkParent)->adoptChildrenAndLock(forkChild1, forkChild2);
    } while (_FAILURE(rc) && _ISDELETED(rc));

    if (_FAILURE(rc) || (_SGETUVAL(rc) == 0)) {
	lock.release();
	return rc;
    }

    /*
     * all pages must be moved to the new parent
     * the FR contents must also be moved
     */

    PageDesc *pg, *nextpg;
    uval blockID;
    nextpg = pageList.getFirst();
    while ((pg=nextpg)) {
	nextpg = pageList.getNext(pg);
	tassertMsg(pg->pinCount == 0,
		   "non leaf can't have pinned page\n");
	if (pg->free) {
	    // page is on the reclaim list
	    pageList.dequeueFreeList(pg);
	    pg->free = PageDesc::CLEAR;
	    // pages on the free list can be doing IO, so continue
	}
	/*
	 * There is a choice here on dealing with disk blocks
	 * that back dirty frames - we can free them or
	 * transfer them - we choose to transfer them
	 */

	DREF((FRComputationRef)frRef)->getBlockID(pg->fileOffset, blockID);

	DREF(forkParent)->locked_givePage(pg, blockID);

	//dont bother with copies1 and copies2 - they dont matter except
	//for the immediate parent of leaf nodes
	pageList.remove(pg->fileOffset);
    }

    /*
     * move any blocks which were not associated with frames
     */
    DREF((FRComputationRef)frRef)->forkCopy(forkParent);

    DREF(forkParent)->locked_completeAdoption();

    tassertMsg(((forkChild1?1:0) + (forkChild2?1:0)) == referenceCount,
	       "Reference count fumble in collapse\n");

    referenceCount = 0;			// destroy checks

    forkChild1 = forkChild2 = 0;	// avoid race between collapse and adopt

    hasCollapsed = 1;			// prevent us from trying to detach

    lock.release();

    return notInUse();
}

/*virtual*/ SysStatus
FCMComputation::adoptChildrenAndLock(
    FCMComputationRef child1, FCMComputationRef child2)
{
    lock.acquire();
    /*
     * see end of collapse - there is an unlocked window before
     * destroy starts - but in that window both forkChild1 and
     * forkChild2 will be null
     */
    if (beingDestroyed || hasCollapsed ||
       (uval(forkChild1) | uval(forkChild2)) == 0) {
	lock.release();
	return _SDELETED(1698);
    }

//FIXME
//following is for testing only
#if defined(marcstresspaging)
    PM::Summary sum;
    uval numPages=100000;
    marcScan = 1;
    locked_pageScan();
    marcScan = 0;
    lock.release();
    giveBack(numPages ,sum);
    Scheduler::Yield();
    lock.acquire();
#endif /* #if defined(marcstresspaging) && ... */

    /*
     * wait for any in progress IO to complete
     * must make sure we don't miss any new IO started
     * by paging
     * clear copy bits since we are adopting new children
     * who may not have a copy of the frame
     */
    finishAllIO(1);

    // locks were released and reaquired so check again
    if (beingDestroyed || hasCollapsed ||
       (uval(forkChild1) | uval(forkChild2)) == 0) {
	lock.release();
	return _SDELETED(1698);
    }

    /*
     * we started out with only one child - even though we've dropped
     * and aquired locks we should still have only one child.
     */
    tassertMsg(!forkChild1 || !forkChild2,
	       "where did the other child come from?\n");


    /*
     * we overwrite the child which is calling us - he must deal
     * with his reference count, we deal with ours
     * N.B. setParent does not acquire the parent lock!
     * the parent may be blocked in an adopt call on our old
     * parent - this works out in the end since by the time
     * it calls our old parent, our old parent returns deleted
     * and our new parent tries again.
     */
    forkChild1 = child1;
    forkChild2 = child2;
    if (forkChild1) {
	DREF(forkChild1)->setParent(getRef());
    }
    if (forkChild2) {
	DREF(forkChild2)->setParent(getRef());
    }
    if (forkChild1 && forkChild2) {
	// we had one child, now we have two
	referenceCount++;
    }
    tassertMsg(((forkChild1?1:0) + (forkChild2?1:0)) == referenceCount,
	       "Reference count fumble in adoptChildrenAndLock\n");

    return 1;
}

/*virtual*/ SysStatus
FCMComputation::locked_completeAdoption()
{
    _ASSERT_HELD(lock);
    // nothing to do now, not maintainaing statistics/summary info
    lock.release();
    return 0;
}


extern uval marcAllowCopyOnWrite;

/* fork copy request from region we create a new parent, move contents
 * of this FCM to parent and create a new child. We depend on the
 * region having stopped the world - in that there are no requests in
 * flight. There may be unfinished IO requests, however
 */
/*virtual*/ SysStatus
FCMComputation::forkCopy(FRRef& newChildFRRef)
{
    //MAA temp
    numForks++;
    SysStatus rc;
    FRRef FRParent, FRChild;
    FCMRef fcmRef;
    FCMComputationRef fcmParent, fcmChild, oldForkParent;
    FreeFrameList ffl;

    // for testing
    uval copyOnFork;
    uval copyCount, pinCopyCount;
    uval parentCopies, childUnmapped, parentPages;
    copyCount = 0;
    copyOnFork = 0;

    lock.acquire();
    // verify that only one region is attached.
    // assume it is the caller
    void *curr;
    RegionRef regionRef;
    RegionInfo* regionInfo;
    curr = regionList.next(0, regionRef, regionInfo);
    passertMsg(curr != 0, "FCM not attached to calling region\n");

    //MAA temp
    uval regionVaddr, regionSize;
    DREF(regionRef)->getVaddr(regionVaddr);
    DREF(regionRef)->getSize(regionSize);
    
    curr = regionList.next(curr, regionRef, regionInfo);
    if (curr) {
	// attempting to fork FCM with multiple mappings - this is
	// a user error
	return _SERROR(1352, 0, EINVAL);
    }
    if (mapBase) {
	FRRef frRef;
	DREF(forkParent)->getFRRef(frRef);
	rc = FRCRW::Create(newChildFRRef, frRef);
	lock.release();
	return rc;
    }

//FIXME
//following is for testing only
#if defined(marcstresspaging)
    PM::Summary sum;
    uval numPages=100000;
    marcScan = 1;
    locked_pageScan();
    marcScan = 0;
    lock.release();
    giveBack(numPages ,sum);
    Scheduler::Yield();
    lock.acquire();
#endif /* #if defined(marcstresspaging) && ... */

    /*
     * wait for any in progress IO to complete
     * must make sure we don't miss any new IO started
     * by paging
     */
    finishAllIO();

    // now create new FR and FCM for the new child

    rc = FRComputation::Create(FRChild, pageSize);
    newChildFRRef = FRChild;

    passertMsg(_SUCCESS(rc), "cant create FR\n");

    getFCM(FRChild, fcmRef);
    fcmChild = (FCMComputationRef)fcmRef;

    /*
     * until we get our parent locked, it can disappear
     * by collapsing with its parent - so keep trying
     * N.B. parent is always locked, even if adoption
     * is rejected.  This is so parent doesn't itself
     * collapse while we are doing the forkCopy.
     */
    while ((oldForkParent = forkParent)) {
    	// see if my parent only has one child
	rc = DREF(oldForkParent)->adoptChildAndLock(
	    fcmChild, FRParent, parentCopies, childUnmapped, parentPages);
	if (_FAILURE(rc) && _ISDELETED(rc)) {
	    continue;
	} else if (_SUCCESS(rc) && (_SGETUVAL(rc) == 1)) {
	    /*
	     * our parent will adopt the new child.  there is no
	     * relevent grandparent in the story
	     */
	    //MAA temp
	    numAdopts++;
	    oldForkParent = 0;
	    copyCount = pageList.getNumPages();
#if 0
	    err_printf("pages %ld parentCopies %ld childUnmapped "
		       "%ld parent %ld %lx %lx %lx\n",
		       copyCount, parentCopies, childUnmapped,
		       parentPages, uval(this), regionVaddr, regionSize);
#endif
	    //MAA temp
	    !copyCount && numEmpty++;
	    if((copyCount < CopyOnForkAlways) ||
	       (copyCount > CopyOnForkFactor*(parentCopies+childUnmapped))) {
		if(pageSize == PAGE_SIZE) {
		    copyOnFork = 1;
		    //MAA temp
		    numCopies++;
		    if(!lastForkUsedCopy) {
			numChanges++;
			lastForkUsedCopy = 1;
		    }
		}
	    }
	    //MAA temp
	    if(!copyOnFork && lastForkUsedCopy) {
		numChanges++;
		lastForkUsedCopy = 0;
	    }
	    goto adopted;
	} else {
	    /*
	     * our parent cannot adopt, so will become the grandparent.
	     * we leave this oldForkParent locked until the work is done
	     * do prevent interference by a simultateous forkCollapse
	     */
	    copyOnFork = lastForkUsedCopy;
	    numCopies++;
	    break;
	}
    }

    oldForkParent || numNew++;
    tassertMsg(oldForkParent || !copyOnFork, "how did copyOnFork get set?\n");
    
    // if we have a parent, he is locked
    rc = FRComputation::Create(FRParent, pageSize);
    passertMsg(_SUCCESS(rc), "cant create FR\n");

    getFCM(FRParent, fcmRef);
    fcmParent = (FCMComputationRef)fcmRef;

    DREF(fcmParent)->setChildrenAndLock((FCMComputationRef)getRef(), fcmChild);
    // on return, forkParent is locked.

    DREF(fcmParent)->setParent(forkParent);

    //new parent (fcmParent) becomes child of old parent (forkParent)
    //replacing me.
    if (forkParent) {
	DREF(forkParent)->locked_replaceChild(getRef(),fcmParent);
    }

    setParent(fcmParent);
    /*
     * at this point forkParent is locked, and oldForkParent is now
     * our grandparent and is locked.
     */

adopted:
    /*
     * we unmap all the pages.
     * eventually, we will deal with copy on write (not ref)
     * and with unmapping strategies.
     * there are difficult tradeoffs in unmapping for copy on write
     * page by page unmapping is expensive.  but if most pages are
     * read only it may pay to unmap only the read/write ones.
     * The copyOnFork heuristic does the unmaps later, on the
     * assumption that most frames are not unmapped at all.
     */

    if(!copyOnFork) {
	locked_unmapAll();
    }
    
    /* now that we can hold the lock for good, allocate frames
     * for pinned pages.  We have to loop since we release lock
     * in getFrame.
     * This code is no good for multiple page sizes since
     * we allocate frames of one size
     *
     * In copyOnFork mode, we allocate space for all the frames.
     * Since we only copy the ones that are mapped, this may be
     * overkill, but for now we don't have the facts to do better
     *
     * Also, we must guard against running out.  Because we release
     * locks while allocated, it is possible that we don't really
     * have enought frames.  This is ok, just stop copying.
     * But - we must make sure we have enough for the pinned
     * frames which MUST be copied.  We do some counting to
     * make sure.  We count down both copyCount and pinCopyCount
     * and don't let copyCount go below pinCopyCount.
     */
    pinCopyCount = pinnedCount;
    copyCount = copyOnFork?copyCount:pinnedCount;
    if (copyCount) {
	lock.release();
	rc = DREF(pmRef)->allocListOfPages((FCMRef)getRef(), copyCount, &ffl);
	lock.acquire();
	passertMsg(_SUCCESS(rc), "out of frames\n");
    }
    DREF(fcmChild)->setParent(forkParent);

    /*
     * all pages except established pages must be moved to the new parent.
     * the FR contents must also be moved
     */

    PageDesc *pg, *nextpg;
    uval blockID;
    nextpg = pageList.getFirst();
    while ((pg=nextpg)) {
	nextpg = pageList.getNext(pg);
	if (pg->established == PageDesc::CLEAR) {
	    if (pg->free) {
		// page is on the reclaim list
		pageList.dequeueFreeList(pg);
		pg->free = PageDesc::CLEAR;
	    }

	    tassertMsg(!(pg->forkCopied1 || pg->forkCopied2),
		    "we are not a fork parent\n");
	    /*
	     * if pg is pinned, or if we are doing agressive copy,
	     * its mapped, and we have a frame, copy it now.
	     */
	    if (pg->pinCount != 0 ||
		(copyOnFork && pg->mapped && (copyCount > pinCopyCount))) {
		uval newvaddr;
		// pinned page
		newvaddr = ffl.getFrame();
		tassertMsg(newvaddr, "failed to reserve enough frames\n");
		PageCopy::Memcpy((void *)newvaddr,
		       (void *)PageAllocatorKernPinned::realToVirt(pg->paddr),
		       pg->len);
		DREF(fcmChild)->givePageCopy(
		    pg->fileOffset, pg->len, newvaddr);
		// parent may have a stale copy - get rid of it
		// parentCopies is number of potentially stale pages in parent
		if(parentCopies) {
		    rc = DREF(forkParent)->locked_dropPage(pg->fileOffset);
		    if (_SGETUVAL(rc) == 1) parentCopies--;
		}
		copyCount--;
		if(pg->pinCount != 0) pinCopyCount--;
	    } else {
	    /*
	     * There is a choice here on dealing with disk blocks
	     * that back dirty frames - we can free them or
	     * transfer them - we choose to transfer them
	     */
		if(copyOnFork) {
		    /* in copyOnFork mode, we haven't yet unmapped
		     */
		    if (pg->mapped) {
			unmapPage(pg);
		    } 
		}
		DREF((FRComputationRef)frRef)->
		    getBlockID(pg->fileOffset, blockID);

		DREF(forkParent)->locked_givePage(pg, blockID);

		pageList.remove(pg->fileOffset);
	    }
	}
    }


    // we may have extra frames - free them.
    if (ffl.isNotEmpty()) {
	DREF(pmRef)->deallocListOfPages((FCMRef)getRef(), &ffl);
    }

    /*
     * move any blocks which were not associated with frames
     */
    DREF((FRComputationRef)frRef)->forkCopy(forkParent);

    // we need to set the pm of the parent.
    // do it after transfereing the pages
    // we don't set the new child, it will get its pm from the
    // region that attaches to it
    DREF(forkParent)->locked_setPM(pmRef);
    // unlock our parent

    DREF(forkParent)->locked_completeAdoption();
    // if we really pushed a new parent and had a grandparent,
    // unlock it as well
    if (oldForkParent) {
	DREF(oldForkParent)->locked_completeAdoption();
    }
    lock.release();


    return 0;
}

/* only for use by the FR
 * called while in adoption, so lock is held
 */

/*virtual*/ SysStatus
FCMComputation::newBlockID(uval fileOffset, uval blockID)
{
    _ASSERT_HELD(lock);
    PageDesc *pg;
    uval vaddr;
    pg = findPage(fileOffset);
    if (pg) {
	tassertMsg(!pg->doingIO && !pg->mapped, "Page still in use\n");
	// indicate that we are giving up ownership of pages
	// PageAllocatorKernPinned::clearFrameDesc(pg->paddr);

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(pg->paddr);
	DREF(pmRef)->deallocPages((FCMRef)getRef(), vaddr, pg->len);

	if (pg->free) {
	    // page is on the reclaim list
	    pageList.dequeueFreeList(pg);
	    pg->free = PageDesc::CLEAR;
	    // pages on the free list can be doing IO, so continue
	}

	// remove from page list
	tassertMsg(!pg->forkCopied1&&!pg->forkCopied2,
		"why are copy bits set here\n");
	pageList.remove(pg->fileOffset);
    }

    // install block passed into our FR, replacing old block if
    // there was one
    DREF(frRef)->setOffset(fileOffset, blockID);

    return 0;
}


/*virtual*/ SysStatusUval
FCMComputation::adoptChildAndLock(
    FCMComputationRef child, FRRef& parentFR,
    uval& parentCopies, uval& childUnmapped, uval& parentPages)
{
    lock.acquire();
    if (beingDestroyed||hasCollapsed) {
	lock.release();
	return _SDELETED(1697);
    }

    if (forkChild1 && forkChild2) {
	// can't adopt, already have two children
	return 0;
    }

//FIXME
//following is for testing only
#if defined(marcstresspaging)
    PM::Summary sum;
    uval numPages=100000;
    marcScan = 1;
    locked_pageScan();
    marcScan = 0;
    lock.release();
    giveBack(numPages ,sum);
    Scheduler::Yield();
    lock.acquire();
#endif /* #if defined(marcstresspaging) && ... */


    /*
     *finishAllIO releases locks, so its possible to completely loose
     *control and have this fcm collapsed into its parent while
     *we're trying.
     *All thats certain is that when it returns, lock is held and
     *no IO is in progress.  So we check again for having disappeared.
     *clear the copy flags at the same time - its a new ball game.
     *you might wonder why we don't carefully reset only the copied
     *bit for the new child. Its because if the copied bit for the
     *old child is on, that page will be replaced by the copy from the
     *old child anyhow
     */
    finishAllIO(1);

    if (beingDestroyed||hasCollapsed) {
	lock.release();
	return _SDELETED(1699);
    }

    if (forkChild1==0) {
	forkChild1 = child;
	parentCopies = copies2;
    } else if (forkChild2==0) {
	forkChild2 = child;
	parentCopies = copies1;
    } else {
	passertMsg(0, "where did the extra child come from?\n");
    }
    copies1 = copies2 = 0;
    childUnmapped = lastChildUnmappedCount;
    parentPages = pageList.getNumPages();
    referenceCount++;
    tassertMsg(((forkChild1?1:0) + (forkChild2?1:0)) == referenceCount,
	       "Reference count fumble in adoptChildAndLock\n");

    parentFR = frRef;
    return 1;
}

/*
 * this is part of a very poor implementation of transfering pages
 * from one FCM to another as part of fork.
 * We should really pass the data structures themselves, not move each
 * page.  See FR for an example.
 * But since the page list is itself a silly implementation, I didn't bother
 * to optimize this part.
 * givePage is called with a page descriptor and makes a new one for the
 * page described.
 * We assume that NO IO is in progress while all this is happening.
 * Lock is still held because of adoption is ongoing
 * When we accept a page, we accept its forkCopied status, since we
 * are adopting the children of the giver, or the giver is a leaf and
 * nothing has been copied.
 * Note that givePage may replace a page which claims to be mapped.  But
 * since the giver is the Only current child, it can't really be mapped
 * any more, so just ignore the mapped state in the page being replaced!
 */
SysStatus
FCMComputation::locked_givePage(PageDesc* pg, uval blockID)
{
    PageDesc* newpg;

    _ASSERT_HELD(lock);
    uval vaddr;
    newpg = findPage(pg->fileOffset);
    if (newpg) {
	tassertMsg(!newpg->doingIO, "Page still in use\n");
	    // indicate that we are given up ownership of pages

	if (newpg->free) {
	    // page is on the reclaim list
	    pageList.dequeueFreeList(newpg);
	    newpg->free = PageDesc::CLEAR;
	}

	// PageAllocatorKernPinned::clearFrameDesc(newpg->paddr);

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(newpg->paddr);
	DREF(pmRef)->deallocPages((FCMRef)getRef(), vaddr, newpg->len);

	newpg->paddr = pg->paddr;
	newpg->len = pg->len;
    } else {
	newpg = addPage(pg->fileOffset, pg->paddr, pg->len);
    }

    newpg->setFlags(pg);
    if (newpg->forkCopied1) copies1++;
    if (newpg->forkCopied2) copies2++;

    // free old disk block if any, set new if any
    DREF((FRComputationRef)frRef)->setOffset(newpg->fileOffset, blockID);

    return 0;
}

SysStatus
FCMComputation::locked_dropPage(uval fileOffset)
{
    PageDesc* newpg;

    _ASSERT_HELD(lock);
    uval vaddr;
    newpg = findPage(fileOffset);
    if (newpg) {
	tassertMsg(!newpg->doingIO, "Page still in use\n");

	// give back physical page
	vaddr = PageAllocatorKernPinned::realToVirt(newpg->paddr);
	DREF(pmRef)->deallocPages((FCMRef)getRef(), vaddr, newpg->len);

	pageList.remove(fileOffset);
	return _SRETUVAL(1);
    } else {
	return _SRETUVAL(0);
    }
}

/*
 * used only by forkCopy to give a copy of a page to the other child
 */
SysStatus
FCMComputation::givePageCopy(uval fileOffset, uval len, uval vaddr)
{
    PageDesc* pg;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    pg = addPage(fileOffset,
		 PageAllocatorKernPinned::virtToReal(vaddr),
		 len);
    DILMA_TRACE_PAGE_DIRTY(this,pg,1);
    pg->dirty = PageDesc::SET;
    pg->copiedOnFork = PageDesc::SET;
    lastChildUnmappedCount++;
    //MAA temp
    numPagesCopied++;
    return 0;
}

// make a new CRW using same base as current
SysStatus
FCMComputation::newCRW(FRRef& newFRRef) {
    FRRef baseFRRef;
    SysStatus rc;
    if(!isCRW) return _SERROR(2710, 0, EINVAL);
    rc = DREF(forkParent)->getFRRef(baseFRRef);
    tassertMsg(_SUCCESS(rc), "why no FR rc=%lx", rc);
    return FRCRW::Create(newFRRef, baseFRRef);
}

SysStatus
FCMComputation::doDestroy()
{
    _ASSERT_HELD(lock);

    // kludge code to detect leaks - this technique won't work for
    // fr's that are allowed to destroy there fcm's without going away
    if(frRef) {
	tassertMsg(((FRComputation*)(DREF(frRef)))->maanofcm(), "oops\n");
    }
    
    SysStatusUval rc = 0;

    FCMComputationRef parentRef;

    tassertMsg((uval(forkChild1)|uval(forkChild2)) == 0,
	       "still have children\n");

    locked_unmapBase();

    /*
     * We need to detach from our forkParent before destroying
     * ourselves.  But this could cause our forkParent to want
     * to destroy itself, leading to an unbounded recursion on
     * a fixed stack.  We convert the recursion to an iteration.
     *
     * detachForkChild will detach us from our parent, remembering
     * if the parent has become a destroy candidate.
     * completeDetachForkChild will let each parent detach from
     * its parent and destroy.
     */
    if (!hasCollapsed && forkParent) {
	rc = locked_detachFromParent(parentRef);
	while(_SGETUVAL(rc) == 3 && !recursiveDestroy) {
	    // parentRef is the copy on write base of childRef
	    // tell it this user is done
	    // passertWrn(0, "marc wants an example of this code running\n");
	    rc = DREF(parentRef)->removeReferenceNoRecursion(parentRef);
	}

	while(_SUCCESS(rc) && _SGETUVAL(rc) == 1) {
	    // call updates parentRef to next parent needing a call back
	    rc = DREF(parentRef)->completeDetachForkChild(parentRef);
	}
    }
    FCMDefault::doDestroy();
    // sad kludge to avoid passing an extra parameter all over the place
    // return parentRef to call that caused the destroy
    if (recursiveDestroy && _SGETUVAL(rc) == 3) {
	recursiveDestroy = 3;
	forkParent = parentRef;
    }
    return rc;
}

SysStatusUval
FCMComputation::locked_detachFromParent(FCMComputationRef& parentRef)
{
    _ASSERT_HELD(lock);
    /*
     * detach myself from my parent, who will tell me
     * if he needs to be called later to complete his
     * death.
     */
    SysStatusUval rc;
    FCMComputationRef forAssertParent = 0;
    do {
	//FIXME should be tassert
	passertMsg(forAssertParent != forkParent, "impossible loop\n");
	forAssertParent = forkParent;
	tassertMsg(sval(lastChildUnmappedCount) >=0,
		   "lastChildUnmappedCount %ld\n", lastChildUnmappedCount)
	rc = DREF(forkParent)->
	    detachForkChild(getRef(), lastChildUnmappedCount);
	//MAA temp
	numUnused+=lastChildUnmappedCount;
	/*because of locking in the parent, if an adoption has changed
	 *our parent, the change will have completed before detachForkChild
	 *can return or fail.
	 */
    } while(_FAILURE(rc) || _SGETUVAL(rc) == 2);
    
    parentRef = forkParent;		// needed if rc == 1 or 3
    /*
     * we reset our forkParent - this prevents destroy from trying
     * to detach again.  Note that once detachForkChild succeeds above
     * we are no longer subject to forkCollapse adoption, and so
     * forkParent will never change again.
     */
    forkParent = 0;			
    return rc;
}

/*
 *A child calls this method of its parent to detach itself.
 *returns are
 *0 if this FCM is still in use (also see FCMCommon detachForkChild)
 *1 if this fcm needs to be called again to destroy itself
 *2 if this fcm has collapsed and is thus no longer callers parent
 *3 if this is a copyonwrite base.
 */
SysStatusUval
FCMComputation::detachForkChild(FCMComputationRef childRef, uval count)
{
    FCMComputationRef fcmTemp;
    lock.acquire();
    if (forkChild1 == childRef) {
	forkChild1 = 0;
    } else if (forkChild2 == childRef) {
	forkChild2 = 0;
    } else {
	SysStatusUval rc;
	// this fcm may be a copyonwrite parent, not a fork parent.
	// in that case, it has no children but is not collapsed
	rc = hasCollapsed?2:3;
	lock.release();
	return rc;
    }

    /*
     * used in aggressive fork copy heuristic.
     * only used if immediate parent of leaf FCM's
     * so don't worry about other cases.
     */
    lastChildUnmappedCount = count;
    
    /*
     * if referenceCount is 1, this is the last child
     * and we will soon be destroyed.
     */
    referenceCount--;
    tassertMsg(((forkChild1?1:0) + (forkChild2?1:0)) == referenceCount,
	       "Reference count fumble in detachForkChild\n");
    if (referenceCount==0 && regionList.isEmpty()) {
	lock.release();
	return 1;
    } else {
	fcmTemp=forkChild1?forkChild1:forkChild2;
	lock.release();
	//trigger a collapse
	if (fcmTemp) {
	    DREF(fcmTemp)->forkCollapse();
	}
	return 0;
    }
}

/*
 * a detach fork child call has left us with no users.
 * we are called again to destroy ourselves.
 * this work can't just be done at the end of detachForkChild above
 * because we must call detachForkChild on our parent, and
 * avoid a recursion.
 */
SysStatusUval
FCMComputation::completeDetachForkChild(FCMComputationRef& parentRef)
{
    SysStatusUval rc;
    lock.acquire();
    tassertMsg(!hasCollapsed, "can't collapse with no children\n");
    rc = 0;
    if(forkParent) {
	rc = locked_detachFromParent(parentRef);
    };
    lock.release();
    notInUse();
    return rc;
}
    
SysStatusUval
FCMComputation::removeReferenceNoRecursion(FCMComputationRef& parentRef)
{
    SysStatusUval rc;
    lock.acquire();
    recursiveDestroy = 1;
    rc = locked_removeReference();
    if (_SUCCESS(rc)) {
	if (recursiveDestroy == 3) {
	    parentRef = forkParent;
	    rc = 3;
	}
    }
    recursiveDestroy = 0;
    lock.release();
    return rc;
}

SysStatus
FCMComputation::attachRegion(RegionRef regRef, PMRef pmRef,
			     AccessMode::mode accessMode)
{
    lock.acquire();
    if (mapBase) {
	SysStatus rc;
	AccessMode::mode baseAccessMode = accessMode;
	uval execute = AccessMode::isExecute(accessMode);
	rc = AccessMode::makeReadOnly(baseAccessMode);
	if (_SUCCESS(rc)) {
	    if (execute) {
		baseAccessMode =
		    AccessMode::mode(baseAccessMode|AccessMode::execute);
	    }
	    DREF(forkParent)->attachRegion(regRef, pmRef, baseAccessMode);
	} else {
	    locked_unmapBase();
	}
    }
    lock.release();
    return FCMDefault::attachRegion(regRef, pmRef, accessMode);
}

SysStatus
FCMComputation::detachRegion(RegionRef regRef)
{
    lock.acquire();
    if (mapBase) {
	DREF(forkParent)->detachRegion(regRef);
    }
    lock.release();
    return FCMDefault::detachRegion(regRef);
}


SysStatusUval
FCMComputation::mapPage(uval fileOffset, uval regionVaddr,
			uval regionSize,
			AccessMode::pageFaultInfo pfinfo,
			uval vaddr, AccessMode::mode accessMode,
			HATRef hat, VPNum vp,
			RegionRef reg, uval firstAccessOnPP,
			PageFaultNotification *fn)
{
    SysStatusUval rc;
    setPFBit(fcmComp);
    StatTimer timer(MapPageTimer);

#if 0
    if(pageSize > PAGE_SIZE /*&& vaddr == 0x10051000000*/) {
	err_printf("large pf %lx %lx %lx %lx\n",
		   regionVaddr, regionSize, vaddr, pfinfo);
    }
#endif
    
    if(mapBase) {			// cheap check first
	lock.acquire();
	if (mapBase) {
	    SysStatus rc;
	    if (!AccessMode::isWriteFault(pfinfo)) {
		// we must keep our ppset correct so when we
		// do unmapBase we know which regions to unmap
		if (firstAccessOnPP) updateRegionPPSet(reg);
		AccessMode::mode baseAccessMode = accessMode;
		uval execute = AccessMode::isExecute(accessMode);
		rc = AccessMode::makeReadOnly(baseAccessMode);
		tassertMsg(_SUCCESS(rc),
			   "it worked at regionAttach - what changed\n");
		// makeReadOnly does not preserve execute permission
		// (should it?)
		if (execute) {
		    baseAccessMode =
			AccessMode::mode(baseAccessMode|AccessMode::execute);
		}
		rc = DREF(forkParent)->mapPage(
		    fileOffset, regionVaddr, regionSize, pfinfo, vaddr,
		    baseAccessMode, hat, vp, reg, firstAccessOnPP, fn);
		lock.release();
		return rc;
	    } else {
		locked_unmapBase();
	    }
	}
	lock.release();
    }

    rc = FCMDefault::mapPage(fileOffset, regionVaddr, regionSize,
			     pfinfo, vaddr, accessMode, hat, vp,
			     reg, firstAccessOnPP, fn);
    timer.record();
    return rc;
}


// methods to support copy on write of base
void
FCMComputation::locked_unmapBase()
{
    _ASSERT_HELD(lock);
    if (!mapBase) return;
    locked_unmapAll();
    regionList.acquireLock();
    void *iter=NULL;
    RegionRef regRef;
    RegionInfo *rinfo;
    while ((iter=regionList.next(iter,regRef,rinfo))) {
	DREF(forkParent)->detachRegion(regRef);
    }
    regionList.releaseLock();
    mapBase = 0;
    return;
}

// blockOnIO must deal with forkIO pages
/*virtual*/ void
FCMComputation::blockOnIO(PageDesc*& pg)
{
    _ASSERT_HELD(lock);
    while (pg && pg->doingIO && pg->forkIO) {
	/*
	 * this is a dummy page descriptor waiting for
	 * a parent page to complete IO
	 */
	if(pg->forkIOLock) {
	    /* this can't happen right now since this service is
	     * called when not other thread is in the FCM, but may as
	     * well put in the code.
	     */
	    PageFaultNotification notification;
	    const uval offset = pg->fileOffset;
	    notification.initSynchronous();
	    notification.next = pg->fn;
	    pg->fn = &notification;
	    lock.release();
	    while (!notification.wasWoken()) {
		Scheduler::Block();
	    }
	    lock.acquire();
	    pg = findPage(offset);
	} else {
	    tassertMsg(pg->fn == 0,
		       "unlocked forkIO dummy has notifications queued\n");
	    pageList.remove(pg->fileOffset);
	    pg = 0;
	    return;
	}
    }
    if(pg) FCMDefault::blockOnIO(pg);
    return;
}
