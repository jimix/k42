/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMSharedTrivial.C,v 1.14 2004/10/08 21:40:08 jk Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/PageDesc.H"
#include "mem/PageList.H"
#include "mem/FCMSharedTrivial.H"
#include "mem/Region.H"
#include "mem/PM.H"
#include <trace/traceMem.h>
#include <cobj/CObjRootSingleRep.H>
#include "mem/PageCopy.H"
#include "mem/PerfStats.H"

template<class PL, class ALLOC>
/* static */ SysStatus
FCMSharedTrivial<PL,ALLOC>::Create(FCMRef &ref)
{
   FCMSharedTrivial<PL,ALLOC> *fcm;

   fcm = new FCMSharedTrivial<PL,ALLOC>;
   if (!fcm) return -1;
   fcm->reg = 0;
   fcm->pm = 0;

   ref = (FCMRef)CObjRootSingleRep::Create(fcm);
   if (ref == NULL) {
	delete fcm;
	return -1;
   }
   TraceOSMemFCMSharedTrivialCreate((uval)ref);
   return 0;
}

template<class PL, class ALLOC>
/* virtual */SysStatusUval
FCMSharedTrivial<PL,ALLOC>::mapPage(uval offset,
		       uval regionVaddr,
		       uval regionSize,
		       AccessMode::pageFaultInfo pfinfo,
		       uval vaddr, AccessMode::mode access,
		       HATRef hat, VPNum vp,
		       RegionRef /*reg*/, uval /*firstAccessOnPP*/,
		       PageFaultNotification */*fn*/)
{
    SysStatus rc;
    PageDesc *pg;
    ScopeTime timer(MapPageTimer);

    tassert(pm != 0, err_printf("FCMSharedTrivial: no pm\n"));

    offset += vaddr - regionVaddr;

    lock.acquire();

    // check cache
    pg = pageList.find(offset);

    // cache miss
    if (pg == NULL)  {
	uval virtAddr;
	// FIXME:  This is DANGEROUS as we are not releasing lock
	//         before calling pm.  Must fix this for general
	//         operation.  For the moment it is a quick hack
	//         as this fcm does not support page reclaimation
	// MARK non pageable to avoid blocking in PM
	rc = DREF(pm)->allocPages(getRef(), virtAddr, PAGE_SIZE, 
				  0 /* non pageable*/);

	if (_FAILURE(rc)) return rc;

	PageCopy::Memset0((void *)virtAddr, PAGE_SIZE);

	pg=pageList.enqueue(offset,
			    PageAllocatorKernPinned::virtToReal(virtAddr),
			    PAGE_SIZE);
    }

    tassert(_SUCCESS(pg),
	    err_printf("OOPS unable to get page!!!!\n"));

    rc = DREF(hat)->mapPage(pg->paddr, vaddr, pg->len, pfinfo, access, vp, 1);

    tassert(_SUCCESS(rc), err_printf("OOPS HAT::mapPage failed\n"));

    lock.release();

    return rc;
}

template<class PL, class ALLOC>
/* virtual */SysStatus
FCMSharedTrivial<PL,ALLOC>::attachRegion(RegionRef regRef, PMRef pmRef,
					 AccessMode::mode accessMode)
{
    AutoLock<LockType> al(lock); // locks now, unlocks on return

    //err_printf("FCMSharedTrivial::attachRegion(): reg=%p pm=%p regRef=%p "
	       //"pmRef=%p\n", reg, pm, regRef, pmRef);
    tassert(reg == 0,
	    err_printf("FCMSharedTrivial second attach\n"));
    reg = regRef;
    pm  = pmRef;
    return 0;

}

template<class PL, class ALLOC>
SysStatus
FCMSharedTrivial<PL,ALLOC>::printStatus(uval kind)
{
    switch (kind) {
    case PendingFaults:

	break;
    default:
	err_printf("kind %ld not known in printStatus\n", kind);
    }
    return 0;
}

template class FCMSharedTrivial<PageList<AllocGlobal>,AllocGlobal>;
template class FCMSharedTrivial<PageList<AllocPinnedGlobal>,AllocPinnedGlobal>;
template class FCMSharedTrivial<PageList<AllocPinnedGlobalPadded>,
                          AllocPinnedGlobalPadded>;
