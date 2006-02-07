#ifndef __FAULT_NOTIFICATION_MGR_H_
#define __FAULT_NOTIFICATION_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FaultNotificationMgr.H,v 1.17 2003/06/04 14:17:30 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Manages page-fault notifications
 * **************************************************************************/

#include <mem/PageFaultNotification.H>
#include <scheduler/Scheduler.H>

struct ProcessAnnex;	// forward declaration to avoid circularity

class FaultNotificationMgr {
    // lock protects entire field
    PtrBLock<PageFaultNotification> freeList;
    uval nextFaultID;
    ThreadID waitingThread;
    PageFaultNotification *lastForDebug;	// for debugging only

    PageFaultNotification *locked_slowAlloc(ProcessAnnex *pa);

public:
    PageFaultNotification *alloc(ProcessAnnex *pa) {
	PageFaultNotification *pn;
	freeList.acquire(pn);
	if (pn != NULL) {
	    freeList.release(pn->next);
	} else {
	    pn = locked_slowAlloc(pa);
	    freeList.release();
	}
#ifndef NDEBUG
	lastForDebug = pn;
#endif
	return pn;
    }

    void free(PageFaultNotification *pn) {
	PageFaultNotification *oldHead;
	freeList.acquire(oldHead);
	pn->next = oldHead;
	freeList.release(pn);
	if (waitingThread != Scheduler::NullThreadID) {
	    Scheduler::Unblock(waitingThread);
	}
    }

    void awaitAndFreeAllNotifications();
    void init();
};

#endif /* #ifndef __FAULT_NOTIFICATION_MGR_H_ */
