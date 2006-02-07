/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPrimitive.C,v 1.70 2005/08/24 15:00:43 dilma Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: zero fill memory FCM - not attached to an FR
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/FCMPrimitive.H"
#include "mem/PageList.H"
#include "mem/PageSet.H"
#include "mem/PageSetDense.H"
#include "mem/PM.H"
#include "mem/SegmentHATPrivate.H"
#include <trace/traceMem.h>
#include <cobj/CObjRootSingleRep.H>
#include "mem/PerfStats.H"

template<class PL, class ALLOC>
/* static */ SysStatus
FCMPrimitive<PL,ALLOC>::Create(FCMRef &ref)
{
    FCMPrimitive<PL,ALLOC> *fcm;

    fcm = new FCMPrimitive<PL,ALLOC>;
    if (fcm == NULL) return -1;

    ref = (FCMRef)CObjRootSingleRep::Create(fcm);

    //err_printf("FCMPrimitiv: %lx, priv %ld, pageble %ld, backedbySwap %ld\n",
    //       ref, fcm->priv, fcm->pageable, fcm->backedBySwap);

    TraceOSMemFCMPrimitiveCreate((uval)ref);

    return 0;
}

template<class PL, class ALLOC>
/* virtual */ SysStatusUval
FCMPrimitive<PL,ALLOC>::getPage(uval fileOffset, void *&dataPtr,
		      PageFaultNotification */*fn*/)
{
    SysStatus rc;
    uval paddr;

    this->lock.acquire();

    PageDesc *pg = this->findPage(fileOffset);
    if (!pg) {
	// allocate a new page
	uval virtAddr;
	this->lock.release();
	rc = DREF(this->pmRef)->allocPages(this->getRef(), virtAddr,
		this->pageSize, this->pageable);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
	this->lock.acquire();
	if ((pg = this->findPage(fileOffset)) != 0) {
	    // added in our absence
	    DREF(this->pmRef)->deallocPages(this->getRef(), virtAddr,
		    this->pageSize);
	    paddr = pg->paddr;
	    TraceOSMemFCMPrimFoundPage(fileOffset, paddr);
	} else {
	    paddr = PageAllocatorKernPinned::virtToReal(virtAddr);
	    TraceOSMemFCMPrimGetPage(fileOffset, paddr);
	    pg = this->addPage(fileOffset, paddr, this->pageSize);
	}
    } else {
	paddr = pg->paddr;
	TraceOSMemFCMPrimFoundPage(fileOffset, paddr);
    }
    tassert(1, err_printf(" should use offset %ld\n", fileOffset));
    dataPtr = (void *)PageAllocatorKernPinned::realToVirt(pg->paddr);
    return 0;
}

template<class PL, class ALLOC>
/* virtual */ SysStatus
FCMPrimitive<PL,ALLOC>::releasePage(uval /*ptr*/, uval dirty)
{
    tassertMsg(dirty==0, "NYI");
    // currently there is only a lock on the whole stinker
    this->lock.release();
    return 0;
}

#include "defines/MLSStatistics.H"

template<class PL, class ALLOC>
/* virtual */ SysStatusUval
FCMPrimitive<PL,ALLOC>::mapPage(uval offset, uval regionVaddr, uval regionSize,
				AccessMode::pageFaultInfo pfinfo, uval vaddr,
				AccessMode::mode access, HATRef hat, VPNum vp,
				RegionRef reg, uval firstAccessOnPP,
				PageFaultNotification */*fn*/)
{
    SysStatus rc;
    uval paddr;
    uval unneededFrameAddr=0;
    setPFBit(fcmPrimitive);
    ScopeTime timer(MapPageTimer);

    /*
     * we round vaddr down to a pageSize boundary.
     * thus, pageSize is the smallest pageSize this FCM supports.
     * for now, its the only size - but we may consider using multiple
     * page sizes in a single FCM in the future.
     * Note that caller can't round down since caller may not know
     * the FCM pageSize.
     */
    vaddr &= -this->pageSize;

    if (firstAccessOnPP) this->updateRegionPPSet(reg);

    this->lock.acquire();

    offset += vaddr - regionVaddr;

    //err_printf("FCMPrimitive::mapPage(o %lx, h %lx)\n", offset, hat);

    MLSStatistics::StartTimer(4);
    PageDesc *pg = this->findPage(offset);
    MLSStatistics::DoneTimer(4);
    if (!pg) {
	// allocate a new page
	uval virtAddr;
	this->lock.release();
	rc = DREF(this->pmRef)->allocPages(this->getRef(), virtAddr,
		this->pageSize, this->pageable);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
	this->lock.acquire();
	if ((pg = this->findPage(offset)) != 0) {
	    // added in our absence
	    unneededFrameAddr = virtAddr;
	    paddr = pg->paddr;
	    TraceOSMemFCMPrimFoundPage(offset, paddr);
	} else {
	    paddr = PageAllocatorKernPinned::virtToReal(virtAddr);
	    TraceOSMemFCMPrimMapPage(vaddr, offset, paddr,
		      (uval64)this);
	    pg = this->addPage(offset, paddr, this->pageSize);
            pg->cacheSynced = PageDesc::SET;
	    DILMA_TRACE_PAGE_DIRTY(this,pg,1);
	    pg->dirty = PageDesc::SET;
	}
    } else {
	paddr = pg->paddr;
	TraceOSMemFCMPrimFound1Page(offset, paddr);
    }
    tassert(1, err_printf(" should use offset %ld\n", offset));

    MLSStatistics::StartTimer(5);
    rc = this->mapPageInHAT(vaddr, pfinfo, access, vp, hat, reg, pg, offset);
    MLSStatistics::DoneTimer(5);

    this->lock.release();

    /*
     * do the free not holding a lock for safety sake
     */
    if (unneededFrameAddr != 0) {
	DREF(this->pmRef)->deallocPages(this->getRef(), unneededFrameAddr,
		this->pageSize);
    }

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
template<class PL, class ALLOC>
/* virtual */ SysStatus
FCMPrimitive<PL,ALLOC>::detachRegion(RegionRef regRef)
{
    uval found;
    RegionInfo *rinfo;

    // check before locking to avoid callback deadlock on destruction
    if (this->beingDestroyed) return 0;

    this->lock.acquire();

    found = this->regionList.remove(regRef, rinfo);
    if (!found) {
	// assume race on destruction call
	tassert(this->beingDestroyed,
		err_printf("no region; no destruction race\n"));
	this->lock.release();
	return 0;
    }
    if (this->regionList.isEmpty()) {
	this->lock.release();
	this->notInUse();
    } else {
	this->lock.release();
	updatePM(rinfo->pm);
    }
    delete rinfo;
    return 0;
}

//instantiate templates

template class FCMPrimitive<PageList<AllocGlobal>,AllocGlobal>;
template class FCMPrimitive<PageList<AllocPinnedGlobal>,AllocPinnedGlobal>;
template class FCMPrimitive<PageSet<AllocGlobal>,AllocGlobal>;
template class FCMPrimitive<PageSet<AllocPinnedGlobal>,AllocPinnedGlobal>;
template class FCMPrimitive<PageSetDense,AllocGlobal>;
