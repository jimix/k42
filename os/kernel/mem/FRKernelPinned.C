/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRKernelPinned.C,v 1.3 2005/06/07 03:46:38 jk Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include "mem/FRKernelPinned.H"
#include "mem/FCMStartup.H"
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaFRKernelPinned.H>
#include <mem/RegionDefault.H>
#include <proc/ProcessSetKern.H>

/* virtual */ SysStatus
FRKernelPinned::init()
{
    FRCommon::init();
    CObjRootSingleRepPinned::Create(this);
    return 0;
}

void
FRKernelPinned::ClassInit(VPNum vp)
{
    err_printf("FRKernelPinned::_ClassInit() called\n");
    if (vp!=0) return;
    MetaFRKernelPinned::init();
}

SysStatus
FRKernelPinned::_Create(ObjectHandle &frOH, uval &kaddr, uval size,
				__CALLER_PID callerPID)
{
    SysStatus rc;
    uval paddr;
    
    size = PAGE_ROUND_UP(size);

    FRKernelPinned *fr = new FRKernelPinned;

    if (fr == NULL) {
	return -1;
    }

    fr->init();
    fr->pageSize = PAGE_SIZE;
    fr->size = size;
    
    rc = fr->giveAccessByServer(frOH, callerPID);
    if (_FAILURE(rc)) {
	goto destroy;
    }

    rc = DREFGOBJK(ThePinnedPageAllocatorRef)
	->allocPagesAligned(kaddr, size, PAGE_SIZE);
    if (_FAILURE(rc)) {
	goto destroy;
    }

    paddr = DREFGOBJK(ThePinnedPageAllocatorRef)->virtToReal(kaddr);

    rc = FCMStartup::Create(fr->fcmRef, paddr, size);
    if (_FAILURE(rc)) {
	err_printf("allocation of fcm failed\n");
	goto destroy;
    }

    /* while we're testing, mark the beginning of the region */
    *((unsigned int *)kaddr) = 0xdeadbeef;

    return rc;
destroy:
    fr->destroy();
    return rc;
}

SysStatus
FRKernelPinned::_InitModule(uval initfn)
{
    void (*f)(void);
    f = reinterpret_cast<void (*)(void)>(initfn);
    f();
    return 0;
}

SysStatus
FRKernelPinned::locked_getFCM(FCMRef &r)
{
    _ASSERT_HELD(lock);
    r = fcmRef;
    return 0;
}

SysStatus
FRKernelPinned::startPutPage(uval physAddr, uval offset, IORestartRequests *rr)
{
    err_printf("FRKernelPinned::startPutPage()\n");
    return 0;
}

SysStatus
FRKernelPinned::putPage(uval physAddr, uval offset)
{
    err_printf("FRKernelPinned::putPage()\n");
    return 0;
}

SysStatusUval
FRKernelPinned::startFillPage(uval physAddr, uval offset)
{
    err_printf("FRKernelPinned::startFillPage()\n");
    return 0;
}

SysStatus
FRKernelPinned::_fsync()
{
    err_printf("FRKernelPinned::_fsync()\n");
    return 0;
}
