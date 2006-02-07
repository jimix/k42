#ifndef __IORESTARTREQUSTS_H_
#define __IORESTARTREQUSTS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: IORestartRequests.H,v 1.1 2004/10/29 16:30:32 okrieg Exp $
 *****************************************************************************/

/**
 * There are two uses of this structure:
 * 1. We allocate a structure to be passed down from FCM, on 
 *    a condition where we would have blocked we enqueue and 
 *    get a call back to FCM to restart the operaiton.
 * 2. We allocate one of the objects sychnronously on stack when
 *    support old implementations or for forward progress gaurantees.
 * We eventually hope to get rid of the second use.
 * For the first use, we decided to always undo any state we have on the error
 * path, rather than saving it at the leaf where the data structure is enqueued.
 * that is, the FCM always restarts operations from scratch.  There are two 
 * reasons for this: 1) we do ths to allow requests to re-prioritize,
 * 2) its the only way we can gaurantee forward progress, i.e., to
 * avoid resing any resource unless we can gaurantee that all resouces
 * needed to complete and operation are available.
 */

/* for FCM case, return error and all we care about is fcm to be restarted 
 * and next pointer, the FCM will have some external way to say that this
 * objct is being used (likely pointer to a page descriptor being written for 
 * now. */

#ifndef NUM_DEFS
extern uval ok_enqueue;
extern uval ok_wokethread;
extern uval ok_crthread;
extern uval ok_sleepthread;
extern uval ok_wakethread;
extern uval ok_wakeFCM;
#endif

class IORestartRequests {
    union {
	FCMRef fcmRef;			// fcm waiting for response, if 0 is 
	uval isNotSync;			// i.e., if !== 0, is fcm
    };

    // next when queued on list
    struct IORestartRequests *next;
                                        // sync request
    // provide some space for generic synchronous blocks
    struct SyncInfo {
	ThreadID    thread;		// blocked thread;
	uval woken;			// synchronous request, thread woken
    } si;


public:
    void enqueue(IORestartRequests *&head) {
	// locking on outside
	ok_enqueue++;
	next = head;
	head = this;
    }

    // must allocate explicitly on FCM in case of paged FCM
    DEFINE_GLOBAL_NEW(IORestartRequests);

    // constructor for FCM allocated data structure
    IORestartRequests(FCMRef ref) {
	fcmRef = ref; 
    }

    // constructor on stack
    IORestartRequests() {
	ok_crthread++;
	isNotSync = 0;
	next = 0;
	si.thread = Scheduler::GetCurThread();
	si.woken = 0;
    }

    void wait() { 
	ok_sleepthread++;
	tassertMsg((isNotSync == 0), "woops");
	while (si.woken == 0) {
	    Scheduler::Block();
	}
	ok_wokethread++;
    }

    /* NOTE, you better not be holding a lock on queue when doing this, 
     * should move the list to a temporary variable, and release lock
     * since call will come right back and try again
     */
    static void NotifyAll(IORestartRequests *qcopy) {
	while (qcopy) {
	    IORestartRequests *curr = qcopy;
	    // dequeue before notify, since next may be modified when notify
	    qcopy = qcopy->next;
	    
	    // now can notify
	    if (curr->isNotSync) {
		ok_wakeFCM++;
		DREF(curr->fcmRef)->resumeIO();
	    } else {
		ok_wakethread++;
		curr->si.woken = 1;
		Scheduler::Unblock(curr->si.thread);
	    }
	}
    }
};
#endif /* #ifndef __IORESTARTREQUSTS_H_ */
