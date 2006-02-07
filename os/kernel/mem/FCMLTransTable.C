/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMLTransTable.C,v 1.10 2005/08/24 15:00:43 dilma Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: zero fill memory FCM - not attached to an FR
 * **************************************************************************/

#include "kernIncs.H"
#include "PageAllocatorKernPinned.H"
#include "FCMLTransTable.H"
#include "PageList.H"
#include "PageSet.H"
#include "PageSetDense.H"
#include "PM.H"
#include "SegmentHATPrivate.H"
#include <trace/traceMem.h>
#include <cobj/CObjRootSingleRep.H>
#include "mem/PerfStats.H"

template <class ALLOC>
/* static */ SysStatus
FCMLTransTable<ALLOC>::Create(FCMRef &ref, FRRef myFR, uval dObj)
{
    FCMLTransTable *fcm;

    fcm = new FCMLTransTable;

    if (fcm == NULL) return -1;

    fcm->init(myFR, dObj);
    
    ref = (FCMRef)CObjRootSingleRep::Create(fcm);

    return 0;
}

/* static */ SysStatus
FCMLTransTablePinned::Create(FCMRef &ref, FRRef myFR, uval dObj)
{
    FCMLTransTablePinned *fcm;

    fcm = new FCMLTransTablePinned;

    fcm->init(myFR, dObj);

    if (fcm == NULL) return -1;

    ref = (FCMRef)CObjRootSingleRepPinned::Create(fcm);

    return 0;
}

template <class ALLOC>
void
FCMLTransTable<ALLOC>::init(FRRef myFR, uval dObj)
{
    defaultObject = dObj;
    this->frRef = myFR;
}

template <class ALLOC>
void
FCMLTransTable<ALLOC>::initPage(uval virtAddr, uval vaddr)
{
    uval *start;
    uval *vaddrPtr = (uval *)PAGE_ROUND_DOWN(vaddr);
    for (start=(uval *)virtAddr;
         (uval)start<(virtAddr + this->pageSize);
         start+=2,vaddrPtr+=2) {
        *start=(uval)(vaddrPtr+1);
        *(start+1)=defaultObject;
    }
}

template <class ALLOC>
/* virtual */ SysStatusUval
FCMLTransTable<ALLOC>::mapPage(uval offset, uval regionVaddr, uval regionSize,
                        AccessMode::pageFaultInfo pfinfo, uval vaddr,
                        AccessMode::mode access, HATRef hat, VPNum vp,
                        RegionRef reg, uval firstAccessOnPP,
                        PageFaultNotification */*fn*/)
{
    SysStatus rc;
    uval paddr;
    uval unneededFrameAddr=0;
    setPFBit(fcmLTrans);
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

//    err_printf("FCMLTransTable::mapPage(o %lx, rV=%lx, rS=%lx, vaddr=%lx,"
//               "vp=%ld)\n", offset, regionVaddr, regionSize, vaddr, vp);

//    breakpoint();
    PageDesc *pg = this->findPage(offset);

    if (!pg) {
	// allocate a new page
	uval virtAddr;
	this->lock.release();
	rc = DREF(this->pmRef)->allocPages(this->getRef(), virtAddr,
		this->pageSize, 0 /* non pageable */);
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
            initPage(virtAddr,vaddr);
	}
    } else {
	paddr = pg->paddr;
	TraceOSMemFCMPrimFound1Page(offset, paddr);
    }
    tassert(1, err_printf(" should use offset %ld\n", offset));

    rc = this->mapPageInHAT(vaddr, pfinfo, access, vp, hat, reg, pg, offset);

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

template class FCMLTransTable<AllocGlobal>;
template class FCMLTransTable<AllocPinnedGlobal>;
