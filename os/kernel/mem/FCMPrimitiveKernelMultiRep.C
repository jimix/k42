/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPrimitiveKernelMultiRep.C,v 1.15 2004/10/03 02:28:08 okrieg Exp $
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
#include "mem/FCMPrimitiveKernelMultiRep.H"
#include "mem/FCMPrimitiveKernelMultiRepRoot.H"
#include "mem/PageFaultNotification.H"
#include "mem/PageCopy.H"
#include <sys/KernelInfo.H>
#include "mem/PerfStats.H"
#include "defines/experimental.H"

/* static */ SysStatus
FCMPrimitiveKernelMultiRep::Create(FCMRef &ref)
{
    SysStatus rc;

//    err_printf("FCMPrimitiveKernelMultiRep::Create\n");

    rc=FCMPrimitiveKernelMultiRepRoot::Create(ref);

    TraceOSMemFCMPrimitiveCreate((uval)ref);

    return rc;
}

void
FCMPrimitiveKernelMultiRep::getPageInternal(uval fileOffset,
                                            LocalPageDescData **ld)
{
    SysStatus rc;
    uval paddr;
    ScopeTime timer(GetPageTimer);

  retry:
    // find or allocate in the hash table.
    // Note entry is locked on return.  If newly allocated
    // the entry will be set to empty.
    LHashTable::AllocateStatus astat =
	localDHashTable.findOrAllocateAndLock(fileOffset, ld);

    tassert((*ld), err_printf("what no ld!!!\n"));
    // if it was not found in the table a new descriptor has been
    // allocated.
    if (astat == LHashTable::ALLOCATED) {
        uval virtAddr;
        (*ld)->unlock();  // FIXME:  I don't think it is actuall necessary to
                       // drop this lock.  As a dead lock can only be caused
                       // by a recusive fault which is problematic
                       // regardless. But for the moment we do anyway
        rc =  DREF(COGLOBAL(pmRef))->allocPages(getRef(),
                                                virtAddr, COGLOBAL(pageSize), 
						0 /* non-pageable */);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
        paddr = PageAllocatorKernPinned::virtToReal(virtAddr);

        COGLOBAL(doSetPAddrAndIOComplete((*ld)->getKey(),paddr,*ld));
    } else if ((*ld)->isDoingIO()) {
	// block here synchronously
	PageFaultNotification notification;
	notification.initSynchronous();
	notification.next = (*ld)->getFN();
	(*ld)->setFN(&notification);

	(*ld)->unlock();

	while (!notification.wasWoken()) {
	    Scheduler::Block();
	}
        goto retry;
    }
}

/*
 * since this is a pinned FCM, no need to track pinned pages - just
 * give out the address.
 */
/* virtual */ SysStatusUval
FCMPrimitiveKernelMultiRep::getPage(uval fileOffset, void *&dataPtr,
                                     PageFaultNotification */*fn*/)
{
    LocalPageDescData *ld;

    getPageInternal(fileOffset, &ld);

    dataPtr = (void *)PageAllocatorKernPinned::realToVirt(ld->getPAddr());

    ld->unlock();
    
    return 0;
}

/* virtual */ SysStatus
FCMPrimitiveKernelMultiRep::releasePage(uval /*ptr*/, uval dirty)
{
    tassertMsg(dirty==0, "NYI");
    return 0;
}

/* virtual */ SysStatusUval
FCMPrimitiveKernelMultiRep::mapPage(uval offset, uval regionVaddr,
                                    uval regionSize,
                                    AccessMode::pageFaultInfo pfinfo,
                                    uval vaddr,
                                    AccessMode::mode access,
                                    HATRef hat, VPNum vp,
                                    RegionRef reg, uval firstAccessOnPP,
                                    PageFaultNotification *fn)
{
    SysStatus rc;
    LocalPageDescData *ld;
    ScopeTime timer(MapPageTimer);
    uval retry;

    /*
     * we round vaddr down to a pageSize boundary.
     * thus, pageSize is the smallest pageSize this FCM supports.
     * for now, its the only size - but we may consider using multiple
     * page sizes in a single FCM in the future.
     * Note that caller can't round down since caller may not know
     * the FCM pageSize.
     */
    vaddr &= -COGLOBAL(pageSize);

    //    err_printf(".");

    // FIXME: for the moment we keep the region list globally but later
    //        we can try and distributed it.  For the moment optimizing
    //        only the case of incore faults after at least one access of
    //        the region.
    if (firstAccessOnPP) COGLOBAL(updateRegionPPSet(reg));

    offset += vaddr - regionVaddr;
    getPageInternal(offset, &ld);
    
    rc = mapPageInHAT(vaddr, pfinfo, access, vp, hat, reg,
		      ld, offset, &retry);
    tassertMsg(retry==0, "retry != 0\n");
    ld->unlock();
    return rc;
}

/*
 * For simple paging FCM, one the last region detaches the FCM is
 * destroyed.  Should this be enforced?  Its a little tricky -
 * the FCM starts out with nothing attached to is - one or more
 * regions attach.  So enforcement needs a state variable set at the
 * first one->zero attach count transition.  We DON'T do that - just
 * trust the user.
 */
/* virtual */ SysStatus
FCMPrimitiveKernelMultiRep::detachRegion(RegionRef regRef)
{
#if 1
    passertMsg(1, "NYI\n");
#else
    uval found;
    RegionInfo *rinfo;

    // check before locking to avoid callback deadlock on destruction
    if (beingDestroyed) return 0;

    lock.acquire();

    found = regionList.remove(regRef, rinfo);
    if (!found) {
	// assume race on destruction call
	tassert(beingDestroyed,err_printf("no region; no destruction race\n"));
	lock.release();
	return 0;
    }
    if (regionList.isEmpty()) {
	lock.release();
	notInUse();
    } else {
	lock.release();
	updatePM(rinfo->pm);
    }
    delete rinfo;
#endif
    return 0;
}

