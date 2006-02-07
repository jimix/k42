#ifndef __PAGE_FAULT_NOTIFICATION_H_
#define __PAGE_FAULT_NOTIFICATION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageFaultNotification.H,v 1.20 2004/07/21 20:06:02 mergen Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Class maintains information for notifying
 * page-fault completion
 * Locking strategy:
 *   - no locking, this class is accessed under the protection of some other
 *     lock in the process, FCM or it is not connected to any list, ie:
 *     - accesses to object when connected to a page frame are done under
 *       protection of the page frame lock
 *     - the object is atomically removed from the page frame list and
 *       then a request is made to the process with it, during this time
 *       it is only visible to thread that has it
 *     - it is removed and added to the process free list under the protection
 *       of the associated lock
 * **************************************************************************/

#include "mem/Access.H"

struct ProcessAnnex;	// forward declaration to avoid circularity

class PageFaultNotification {
    // following two fields initialized on creation, never changed
    ProcessAnnex *processAnnex;		// ProcessAnnex to be notified
    uval16	 pageFaultId;		// identifier
    uval         threadBlocked:1;	// thread blocked or a notification
    uval         _wasWoken:1;		// woke synchronous
    uval         success:1;		// indicates whether error occurred
    uval         prefetch:1;		// prefetch, don't notify, delete

    ThreadID    thread;		// blocked thread;

public:
    // believe it or not, this is allocated dynamically
    // Kernel page faults are always synchonous, so this is
    // never attempted in the kernel process.
    DEFINE_GLOBALPADDED_NEW(PageFaultNotification);

    uval        vaddr;	// for debugging only

    PageFaultNotification *next;	// pointer when linked on list

    /* --------- methods for upcall case ------------------------ */
    // initializing upcall notification for first time
    void initAsynchronous(ProcessAnnex *pa, uval id);
    /* --------- end methods for upcall case -------------------- */

    /* --------- methods for blocking case ---------------------- */
    void initSynchronous();
    /* --------- end methods for blocking case ------------------ */

    void initPrefetch();

    void doNotification();

    uval16       getPageFaultId() {return pageFaultId;}
    uval         wasWoken()       {return _wasWoken;}
    uval         isPrefetch()     {return prefetch;}

    void   setRC(SysStatus rc) { success = _SUCCESS(rc); }
};


#endif /* #ifndef __PAGE_FAULT_NOTIFICATION_H_ */
