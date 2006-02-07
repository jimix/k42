/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FaultNotificationMgr.C,v 1.19 2003/06/04 14:17:30 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Manages page-fault notifications
 * **************************************************************************/

#include "kernIncs.H"
#include "FaultNotificationMgr.H"
#include <sys/Dispatcher.H>
#include <scheduler/Scheduler.H>

PageFaultNotification *
FaultNotificationMgr::locked_slowAlloc(ProcessAnnex *pa)
{
    tassertMsg(freeList.isLocked(), "Lock not held.\n");

    PageFaultNotification *pn;

    if (nextFaultID < Dispatcher::NUM_PGFLT_IDS) {
	// can allocate another one
	pn = new PageFaultNotification;
	passertMsg(pn != NULL, "PageFaultNotification allocation failed.\n");
	pn->initAsynchronous(pa, nextFaultID);
	nextFaultID++;
    } else {
	// at the limit; can't allocate any more
	pn = NULL;
    }
    return pn;
}

void
FaultNotificationMgr::awaitAndFreeAllNotifications()
{
    PageFaultNotification *pn;
    // safe because dispatcher has already terminated so no new faults
    // are possible, and no faults can be in progress
    waitingThread = Scheduler::GetCurThread();
    uval numberAllocated = nextFaultID - 1;	// faultID 0 isn't used
    while (numberAllocated > 0) {
	freeList.acquire(pn);
	if (pn) {
	    freeList.release(pn->next);
	    delete pn;
	    numberAllocated--;
	} else {
	    freeList.release();
	    Scheduler::Block();
	}
    }
    nextFaultID = 1;				// faultID 0 isn't used
    waitingThread = Scheduler::NullThreadID;
}

void
FaultNotificationMgr::init()
{
    /*
     * We don't use 0 as a faultID because the page-fault return
     * path uses 0 to indicate a successfully-handled in-core fault.
     */
    nextFaultID = 1;
    waitingThread = Scheduler::NullThreadID;
    freeList.init(0);
    lastForDebug = NULL;
}
