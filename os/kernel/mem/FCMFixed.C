/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMFixed.C,v 1.37 2004/10/08 21:40:08 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKernPinned.H"
#include "trace/traceMem.h"
#include <cobj/CObjRootSingleRep.H>
#include <mem/FCMFixed.H>
#include <mem/PM.H>
#include "mem/PerfStats.H"


template<class ALLOC>
SysStatus
FCMFixed<ALLOC>::Create(FCMRef &ref)
{
    FCMFixed<ALLOC> *fcm;

    fcm = new FCMFixed<ALLOC>;
    if (fcm == NULL) return -1;

    ref = (FCMRef)CObjRootSingleRepPinned::Create(fcm);
    TraceOSMemFCMDefaultCreate((uval)ref);

    if (ref == NULL) {
	delete fcm;
	return -1;
    }
    //err_printf("FCMFixed: %lx, priv %ld, pageble %ld, backedbySwap %ld\n",
    //       ref, fcm->priv, fcm->pageable, fcm->backedBySwap);

    return 0;
}

template<class ALLOC>
SysStatusUval
FCMFixed<ALLOC>::getPage(uval fileOffset, void *&dataPtr,
		  PageFaultNotification */*fn*/)
{
    this->lock.acquire();

    PageDesc *pg = this->findPage(fileOffset);
    if (!pg) {
	this->lock.release();
	return _SERROR(1114, 0, ENOMEM);
    }
    tassert(1, err_printf(" should use offset %ld\n", fileOffset));
    dataPtr = (void *)PageAllocatorKernPinned::realToVirt(pg->paddr);
    return 0;
}

template<class ALLOC>
SysStatus
FCMFixed<ALLOC>::releasePage(uval /*ptr*/, uval dirty)
{
    tassertMsg(dirty==0, "NYI");
    // currently there is only a lock on the whole stinker
    this->lock.release();
    return 0;
}

template<class ALLOC>
SysStatusUval
FCMFixed<ALLOC>::mapPage(uval offset, uval regionVaddr, uval regionSize,
			 AccessMode::pageFaultInfo pfinfo, uval vaddr,
			 AccessMode::mode access, HATRef hat, VPNum vp,
			 RegionRef reg, uval firstAccessOnPP,
			 PageFaultNotification */*fn*/)
{
    SysStatus rc;
    uval paddr;
    setPFBit(fcmFixed);
    ScopeTime timer(MapPageTimer);

    AutoLock<LockType> al(&this->lock); // locks now, unlocks on return

    if (firstAccessOnPP) this->updateRegionPPSet(reg);

    offset += vaddr - regionVaddr;

    PageDesc *pg = this->findPage(offset);
    if (!pg) {
	return _SERROR(1115, 0, ENOMEM);
    } else {
	paddr = pg->paddr;
	rc = 0;
    }
    tassert(1, err_printf(" should use offset %ld\n", offset));

    rc = this->mapPageInHAT(vaddr, pfinfo, access, vp, hat, reg, pg, offset);

    return rc;
}

//For simple paging FCM, one the last region detaches the FCM is
//destroyed.  Should this be enforced?  Its a little tricky -
//the FCM starts out with nothing attached to is - one or more
//regions attach.  So enforcement needs a state variable set at the
//first one->zero attach count transition.  We DON'T do that - just
//trust the user.
template<class ALLOC>
SysStatus
FCMFixed<ALLOC>::detachRegion(RegionRef regRef)
{
    RegionInfo *rinfo;
    uval        found;

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
	this->destroy();
    } else {
	this->lock.release();
	updatePM(rinfo->pm);
    }
    delete rinfo;
    return 0;
}

template<class ALLOC>
SysStatus
FCMFixed<ALLOC>::establishPage(uval offset, uval virtAddr, uval length)
{
    AutoLock<LockType> al(&this->lock); // locks now, unlocks on return
    uval paddr;
    uval endAddr = virtAddr+length;
    for (;virtAddr<endAddr;virtAddr+=this->pageSize) {
	paddr = PageAllocatorKernPinned::virtToReal(virtAddr);
	addPage(offset, paddr, this->pageSize);
    }
    return 0;
}

template<class ALLOC>
SysStatus
FCMFixed<ALLOC>::removeEstablishedPage(uval offset, uval paddr)
{
    AutoLock<LockType> al(&this->lock); // locks now, unlocks on return
    PageDesc *pg;

    pg = this->findPage(offset);
    if (paddr != pg->paddr) {
	tassertWrn( 0, "error page doesn't match\n");
	return _SERROR(1284, 0, EFAULT);
    }

    this->unmapPage(pg);

    // note we will need syncronization on page to ensure not re-mapped
    this->pageList.remove(offset);
    return 0;
}

template<class ALLOC>
SysStatus
FCMFixed<ALLOC>::establishPagePhysical(uval offset, uval paddr, uval length)
{
    AutoLock<LockType> al(&this->lock); // locks now, unlocks on return
    uval endAddr = paddr+length;
    for (;paddr<endAddr;paddr+=this->pageSize) {
	addPage(offset, paddr, this->pageSize);
    }
    return 0;
}

//instantiate templates

template class FCMFixed<AllocGlobal>;
template class FCMFixed<AllocPinnedGlobalPadded>;
