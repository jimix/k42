/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSRamSwap.C,v 1.21 2004/10/29 16:30:21 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides trivially ram-backed swap file system
 * **************************************************************************/

#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include "mem/PageAllocatorKernPinned.H"
#include "bilge/FSRamSwap.H"
#include "mem/FR.H"
#include "mem/PageCopy.H"

/* static */ SysStatus
FSRamSwap::ClassInit(VPNum vp, uval swap)
{
    SysStatus rc=0;

    if (vp != 0) return 0;

    FSRamSwap *theMe = new FSRamSwap();

    theMe->init(swap);

    CObjRootSingleRep::Create(theMe, (RepRef)GOBJK(TheFSSwapRef));

    return rc;
}

void
FSRamSwap::init(uval pages)
{
    SysStatus rc;
    uval i;
    uval pageAddr;
    chain *c, *freeList;

    swapRunning = 0;

    if (pages != 0) {
	uval avail;
	DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(avail);
	if (avail/(PAGE_SIZE*2) < pages) {
	    err_printf("Not enough memory to provide %ld pages\n",
		       pages);
	    numFree = 0;
	} else {
	    numFree = avail/PAGE_SIZE - pages;
	    err_printf("FSRamSwap: mem %ld swap %ld of %ld pages\n",
		       pages, numFree, avail/PAGE_SIZE);
	    swapRunning = pages;
	}
    } else {
	numFree = 0;
    }

    freeList = 0;
    // pre-allocate all resources needed
    for (i = 0; i < numFree; i++) {
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(pageAddr,
							      PAGE_SIZE);
	tassert(_SUCCESS(rc), err_printf("Not enough mem\n"));
	c = (chain*)pageAddr;
	c->next = freeList;
	freeList = c;
    }

    freeListLock.init(freeList);

    rc = Scheduler::ScheduleFunction(DoAsyncOps, (uval)this, asyncID);
    tassert(_SUCCESS(rc), err_printf("woops\n"));
    return;
}

SysStatus
FSRamSwap::doPutPage(uval physAddr, uval blockID)
{
    uval vpaddr;

    vpaddr = PageAllocatorKernPinned::realToVirt(physAddr);
    PageCopy::Memcpy((void *)blockID, (void *)vpaddr, PAGE_SIZE);

    return 0;
}

SysStatusUval
FSRamSwap::doFillPage(uval physAddr, uval blockID)
{
    uval vpaddr;

    vpaddr = PageAllocatorKernPinned::realToVirt(physAddr);

    PageCopy::Memcpy((void *)vpaddr, (void *)blockID, PAGE_SIZE);

    return 0;
}


/* static */ void
FSRamSwap::DoAsyncOps(uval p)
{
    FSRamSwap *ptr = (FSRamSwap *)p;
    while (1) {
	// safe to block first since all requests will
	// issue an unblock to this thread, including the
	// first one.
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
	ptr->doAsyncOps();
    }
}

void
FSRamSwap::doAsyncOps()
{
    Request *req;
    SysStatus rc;
    while (requests.removeHead(req)) {
	// perform read
	if (req->reqType == Request::READ) {
	    err_printf("R");
	    //err_printf("u_fillpage: %lx %lx %lx\n",
	    //       req->ref, req->paddr, req->offset);
	    rc = doFillPage(req->paddr, req->blockID);
	    DREF(req->ref)->ioComplete(req->offset, rc);
	} else {
	    err_printf("W");
	    //err_printf("u_putpage: %lx %lx %lx\n",
	    //       req->ref, req->paddr, req->offset);
	    rc = doPutPage(req->paddr, req->blockID);
	    DREF(req->ref)->ioComplete(req->offset, rc);
	}
	delete req;
    }
}


SysStatus
FSRamSwap::startFillPage(uval physAddr, FRComputationRef ref, uval offset,
			 uval blockID, PagerContext context)
{
    //err_printf("startfill: %lx, %lx, %lx\n", frRef, physAddr, objOffset);
    Request *req = new Request(physAddr, ref, offset, blockID, Request::READ);
    requests.addToEndOfList(req);
    Scheduler::Unblock(asyncID);	// notify async daemon of new work
    return 0;
}

SysStatus
FSRamSwap::putPage(uval physAddr, uval& blockID, PagerContext context)
{
    chain *freeList;
    if (blockID==uval(-1)) {
	freeListLock.acquire(freeList);
	passert(freeList != 0, err_printf("FSRamSwap is full\n"));
	blockID = (uval)freeList;
	freeList = freeList->next;
	numFree--;
	freeListLock.release(freeList);
    }
    return doPutPage(physAddr, blockID);
}

SysStatus
FSRamSwap::startPutPage(uval physAddr, FRComputationRef ref, uval offset,
			uval& blockID, PagerContext context, 
			IORestartRequests *rr)
{
    //err_printf("startput: %lx, %lx, %lx\n", frRef, physAddr, objOffset);
    chain *freeList;

    // NOTE rr arg ignored, do not handle running out of ram disk space
    if (blockID==uval(-1)) {
	freeListLock.acquire(freeList);
	passert(freeList != 0, err_printf("FSRamSwap is full\n"));
	blockID = (uval)freeList;
	freeList = freeList->next;
	numFree--;
	freeListLock.release(freeList);
    }

    Request *req = new Request(physAddr, ref, offset, blockID, Request::WRITE);
    requests.addToEndOfList(req);
    Scheduler::Unblock(asyncID);	// notify async daemon of new work
    return 0;
}

SysStatus
FSRamSwap::freePage(uval blockID, PagerContext context)
{
    chain *c, *freeList;

    freeListLock.acquire(freeList);
    c = (chain*)blockID;
    c->next = freeList;
    freeList = c;
    numFree++;
    freeListLock.release(freeList);
    return 0;
}

SysStatus
FSRamSwap::swapActive()
{
    return swapRunning;
}
