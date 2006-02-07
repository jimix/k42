/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMDefaultMultiRep.C,v 1.23 2004/10/03 02:28:08 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared FCM services for mapping, unmapping,
 * getting/releasing for copy for FCM's attached to FR's (files).
 * **************************************************************************/

#include "kernIncs.H"
#include "trace/traceMem.h"
#include "defines/paging.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/FR.H"
#include "mem/FCMDefaultMultiRep.H"
#include "mem/FCMDefaultMultiRepRoot.H"
#include "mem/PageFaultNotification.H"
#include "defines/experimental.H"
#include "mem/PageCopy.H"
#include <sys/KernelInfo.H>
#include "mem/PerfStats.H"


/* static */ SysStatus
FCMDefaultMultiRep::Create(FCMRef &ref, FRRef cr, uval pageable)
{
//    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
//	err_printf("*** FCMDefaultMultiRep::Create(...) called.\n");
//    }
    return FCMDefaultMultiRepRoot::Create(ref, cr, pageable);
}


/*
 * N.B. Although this routine is called locked, it releases
 * and re-acquires the lock.  Take care.
 *
 */
SysStatus
FCMDefaultMultiRep::getFrame(LocalPageDescData *ld)
{
    SysStatus rc;
    uval paddr;

    tassert(ld->isLocked(), ;);

    uval virtAddr;
    // release lock while allocating
    ld->unlock();  // FIXME:  I don't think it is actuall necessary to drop
                   //         this lock.  As a dead lock can only be caused
                   //         by a recusive fault which is problematic
                   //         regardless. But for the moment we do anyway

    rc =  DREF(COGLOBAL(pmRef))->allocPages(getRef(), virtAddr, 
					    COGLOBAL(pageSize), 
					    0 /* non pageable??? */,
					    0,
					    COGLOBAL(numanode));

    if (_FAILURE(rc)) {
	ld->lock();  // This is safe as doingIO is an existence lock on the
	return rc;
    }

    paddr = PageAllocatorKernPinned::virtToReal(virtAddr);

    COGLOBAL(doSetPAddr(ld->getKey(),paddr,ld)); // gets local lock
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
#endif
    return 0;
}



/*
 * for now, this is called without any locks held
 * but doingIO servers as a lock of sorts
 * eventually, should be called holding the pg lock
 *
 * returns with rc = 1 if io is in progress
 * returns with rc = 0 and lock held if io is done.
 *                         pg is trustworthy in this case
 */
SysStatusUval
FCMDefaultMultiRep::startFillPage(uval offset, uval paddr,
				  PageFaultNotification* fn,
				  LocalPageDescData *ld)
{
    SysStatusUval rc;
    // now initiate I/O operation
    tassert(ld->isDoingIO(),
	    err_printf("startFillPage on unlocked page\n"));

    rc = DREF(COGLOBAL(frRef))->startFillPage(ld->getPAddr(), offset);

    tassert(_SUCCESS(rc), err_printf("error on startfillpage\n"));
    if (_SUCCESS(rc) && (rc == FR::PAGE_NOT_FOUND)) {
	uval vpaddr;
	vpaddr = PageAllocatorKernPinned::realToVirt(ld->getPAddr());
	PageCopy::Memset0((void *)vpaddr, COGLOBAL(pageSize));
	//err_printf("doing early callback: %lx %lx\n", objOffset, physAddr);
	//call skips doing complete on fn and keeps lock it aquires
        // This will set the dirty and cacheSynced bits
	rc = COGLOBAL(doFillAllAndLockLocal(offset, fn, ld));

	passert(rc == 0, err_printf("oops the page has been freed\n"));
	// above acquires local lock
	tassert(ld->isLocked(), err_printf("oops he not locked\n"));
	return 0;
    }
    // return indicates a wait on fn is needed
    return 1;
}


SysStatus
FCMDefaultMultiRep::ioComplete(uval fileOffset, SysStatus rc)
{
    (void)COGLOBAL(doIOCompleteAll(fileOffset, rc));
    return 0;
}

LocalPageDescData *
FCMDefaultMultiRep::findOrAllocatePageAndLock(uval fileOffset, SysStatus &rc,
					      uval *needsIO) {
    LocalPageDescData *ld;

    *needsIO = 0;
    rc = 0;
    // find or allocate in the hash table.
    // Note entry is locked on return.  If newly allocated
    // the entry will be set to empty.
    LHashTable::AllocateStatus astat =
	localDHashTable.findOrAllocateAndLock(fileOffset, &ld);

    // **** NOTE a newly allocated PD has doingIO set by default *****
    if (astat == LHashTable::FOUND) {
	// Hit
	TraceOSMemFCMDefFoundPage(fileOffset,
		  ld->getPAddr());
	return ld;
    }

    // END OF HOT-PATH
    // FIXME: consider working with only the master copy outside of the hot
    // path

    // Only the allocating thread of a pd ever executes this code
    // Note allocation is serialized so there is only one allocator.
    // The doingIO bit still serves as a existence lock.  The code
    // below relies on this.  May want to revist now that we have
    // a per descriptor lock.
    tassert(ld->isDoingIO(), err_printf("Not doingIO????\n"));
    *needsIO = 1;

    // Miss
    rc=getFrame(ld); // drops and reaquires locks but safe due to
                     // doingIO

    tassert(ld->isLocked(), err_printf("huh he not locked!\n"));
//    tassert(ld->match(fileOffset), err_printf("huh he does not match"
//					      " fileOffset\n"));

    if (_FAILURE(rc)) {
	// must clean up pagedesc we added and wakeup anyone waiting
	// notify but retain lock
	// TODO
	// FIXME: Please verify but it seems wrong not to drop the local lock
	ld->unlock();                        // Fixed with Marc
	COGLOBAL(doNotifyAllAndRemove(ld));  // all locks are released on exit
	return NULL;	                     // no paging space
    }
    TraceOSMemFCMDefGetPage(fileOffset, ld->getPAddr());

    // indicate that we have taken ownership of page
//    PageAllocatorKernPinned::initFrameDesc(ld->getPAddr(), getRef(),
//                                           fileOffset, 0);
    return ld;
}

SysStatusUval
FCMDefaultMultiRep::getPageInternal(
    uval fileOffset, PageFaultNotification *fn,
    LocalPageDescData **ld)
{
    ScopeTime timer(GetPageTimer);
    SysStatus rc;
    uval needsIO;

    TraceOSMemFCMDefaultMultiRepGetPageInternalStart(
	      fileOffset, (uval)myRoot);

    while (1) {
	*ld = findOrAllocatePageAndLock(fileOffset, rc, &needsIO);

	if (!*ld) {
	    tassert( _FAILURE(rc), err_printf("huh\n"));
	    TraceOSMemFCMDefaultMultiRepGetPageInternalEnd(
		      fileOffset, (uval)myRoot);
	    return rc;
	}

	if ((*ld)->isFree()) {
	    (*ld)->clearFree();  // Locally Free is just a flag value
	    // however, in the master it is a list
	}

	if (!((*ld)->isDoingIO())) {
	    tassert(!needsIO, err_printf("oops\n"));
	    // on success don't release lock
	    TraceOSMemFCMDefaultMultiRepGetPageInternalEnd(
		      fileOffset, (uval)myRoot);
	    return 0;
	}

	// END OF HOT-PATH
	uval paddr = (*ld)->getPAddr();  // set for us by getFrame
	if (fn) {
	    fn->next = (*ld)->getFN();
	    (*ld)->setFN(fn);
	    (*ld)->unlock();
	    if (needsIO) {
		// Look at this
		rc = startFillPage(fileOffset, paddr, fn, *ld);
		TraceOSMemFCMDefaultMultiRepGetPageInternalEnd(
			  fileOffset, (uval)myRoot);
		return rc;
	    }
	    TraceOSMemFCMDefaultMultiRepGetPageInternalEnd(
		      fileOffset, (uval)myRoot);
	    return 1;
	}

	// block here synchronously
	PageFaultNotification notification;
	notification.initSynchronous();
	notification.next = (*ld)->getFN();
	(*ld)->setFN(&notification);

	(*ld)->unlock();

	if (needsIO) {
	    // now initiate I/O operation
	    rc = startFillPage(fileOffset, paddr, &notification, *ld);
	    tassert(_SUCCESS(rc), err_printf("error on startfillpage\n"));
	    if (rc==0) {
		TraceOSMemFCMDefaultMultiRepGetPageInternalEnd(
			  fileOffset, (uval)myRoot);
		return 0;
	    }
	}

	while (!notification.wasWoken()) {
	    Scheduler::Block();
	}
    }
}

SysStatus
FCMDefaultMultiRep::releasePage(uval fileOffset, uval dirty)
{
    tassertMsg(dirty==0, "NYI");
    return COGLOBAL(doReleasePage(fileOffset));
}

SysStatusUval
FCMDefaultMultiRep::getPage(uval fileOffset, void *&dataPtr,
			    PageFaultNotification *fn)
{
    SysStatus rc;
    LocalPageDescData *ld;

 retry:
    rc = getPageInternal(fileOffset, fn, &ld);
    /*
     * rc == 0 if page was gotten and locked
     * rc >0 if io is in progress and fn will be posted
     * rc <0 failure
     */
    if (_SUCCESS(rc) && (rc == 0)) {
	uval paddr;
	// page has a master descriptor, now try to pin it
	// because of lock hierarchy, we must unlock the local
	// first, and may have to retry if we're really unlucky
	ld->unlock();
	if (COGLOBAL(doGetPage(fileOffset,&paddr))==-1) {
	    // the master descriptor disappeared, try again
	    goto retry;
	}
	dataPtr = (void *)PageAllocatorKernPinned::realToVirt(paddr);
    }

    return rc;
}

enum {MULTIREP_MAP_PAGE_EVENT=0xA00, MAP_PAGE_RETRY=0xA01};

SysStatusUval
FCMDefaultMultiRep::mapPage(uval offset, uval regionVaddr, uval regionSize,
		    AccessMode::pageFaultInfo pfinfo, uval vaddr,
		    AccessMode::mode access, HATRef hat, VPNum vp,
		    RegionRef reg, uval firstAccessOnPP,
		    PageFaultNotification *fn)
{
    SysStatus rc;
    LocalPageDescData *ld;
    ScopeTime timer(MapPageTimer);

//    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
//	err_printf("p");
//    }


    /*
     * we round vaddr down to a pageSize boundary.
     * thus, pageSize is the smallest pageSize this FCM supports.
     * for now, its the only size - but we may consider using multiple
     * page sizes in a single FCM in the future.
     * Note that caller can't round down since caller may not know
     * the FCM pageSize.
     */
    vaddr &= -COGLOBAL(pageSize);

    // FIXME: for the moment we keep the region list globally but later
    //        we can try and distributed it.  For the moment optimizing
    //        only the case of incore faults after at least one access of
    //        the region.
    if (firstAccessOnPP) COGLOBAL(updateRegionPPSet(reg));

    offset += vaddr - regionVaddr;

 retry:
    rc = getPageInternal(offset, fn, &ld);
    /*
     * rc == 0 if page was gotten and locked
     * rc >0 if io is in progress and fn will be posted
     * rc <0 failure
     */
    if (_SUCCESS(rc) && (rc == 0)) {
	//err_printf("mapping %lx\n", offset);
	uval retry;

    	rc = mapPageInHAT(vaddr, pfinfo, access, vp, hat, reg,
			  ld, offset, &retry);
	if (retry) {
	    // if retry is set locks have already been dropped
	    goto retry;
	}
	//err_printf("done mapping %lx\n", offset);
	ld->unlock();
	if (_SGENCD(rc) == ENOMEM) {
	    err_printf("No mem on mapPage: sleeping and retrying\n");
	    Scheduler::DelayMicrosecs(100000);
	    goto retry;
	}
	return rc;
    }

    // I've been sloppy about returning the FaultID all the way up
    // so just get it right.  Debugging a mistake here is hopeless
    if (_SUCCESS(rc)) {
	/* if rc is 0, there may not be an fn, so we need to check
	 * when fn is null, getPageInternal blocks until it can
	 * return success (rc == 0) or a real error
	 */
	return fn->getPageFaultId();
    }

    return rc;
}

SysStatusUval
FCMDefaultMultiRep::getForkPage(
    PageDesc* callerPg, uval& returnUval, FCMComputationRef& childRef,
    PageFaultNotification *fn, uval copyOnWrite)
{
    SysStatusUval rc;
    LocalPageDescData *ld;
    uval fileOffset;

    fileOffset = callerPg->fileOffset;

    rc = getPageInternal(fileOffset, fn, &ld);

    /*
     * rc == 0 if page was gotten and FCM was locked
     * rc >0 if io is in progress and fn will be posted
     * rc <0 failure
     */

    if (_SUCCESS(rc) && (rc == 0)) {
	//available
        callerPg->paddr = ld->getPAddr();
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
	    callerPg->paddr = ld->getPAddr();
	    ld->setPP(Scheduler::GetVP());

	    if (!ld->osCacheSynced()) {
		// Machine dependent operation.
                tassertMsg(0, "oops not implemented completely\n");
                ld->unlock();
                COGLOBAL(doSetCacheSynced(pg));	// FIXME: implement some sort of
                                                //        try mechanism in doOp
                                                //        which would allow us
                                                //    to try the op holding our
                                                //        local lock.
                // FIXME : Must put in retry here as lock has been dropped
	    }

            if (!ld->isMapped()) {
                // mark page mapped
                ld->setMapped();
		// also mark framearray to indicate page is now mapped
		// PageAllocatorKernPinned::setAccessed(pg->paddr);
	    }
	    ld->unlock();
	    return MAPPAGE;
	}
#endif

        // page lock held until unLockPage call
	// copy our ppset to caller to it can unmap if needed
	// (only happens if copyonwrite logic is enabled)
	callerPg->ppset = ld->getPPSet();
	callerPg->mapped = ld->getMapped();
	returnUval = uval(ld);	// caller needs this to unlock
        // Nothing to unlock as we continue the hold page lock for the
        // duration of the copy.
	return FRAMETOCOPY;
    }

    // doingIO
    return (rc>0)?DOINGIO:rc;
}
