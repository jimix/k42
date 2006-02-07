/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SharedBufferProducer.C,v 1.4 2005/08/30 19:07:36 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SharedBufferProducer.H"
#include <scheduler/Scheduler.H>

/* virtual */ SysStatus
SharedBufferProducer::init(ProcessID pid, ObjectHandle &sfroh)
{
    SysStatus rc;

    // validate that constant size and numEntries make sense
    tassertMsg(numEntries*entryUvals+2 <= size/sizeof(uval),
	       "something wrong\n");

    lock.init();

    blockedThreadHighPriorityHead = NULL;
    blockedThreadOthersHead = NULL;

    uval smAddr;
   // create FR for shared memory to be used by this paging transport

    rc = initFR(pid, sfroh, smAddr);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    /* the initial part of the allocated buffer has the space for the
     * shared buffer; after all buffer entries we have one word to keep
     * the producer index (it records the index on the array of requests
     * where the produces will be storing the new request) 
     * and one work for the consumer index (it records the last index
     * where the consumer retrieved a request from).
     */
    baseAddr = (uval*) smAddr;
    pidx_ptr = baseAddr + numEntries*entryUvals;
    cidx_ptr = pidx_ptr + 1;
    *pidx_ptr = 0; 
    *cidx_ptr = numEntries - 1;

    return 0;
}

/* virtual */ SysStatus
SharedBufferProducer::locked_put(uval *request, uval highPriority,
				 uval shouldBlock)
{
    _ASSERT_HELD(lock);

    tassertMsg(pidx_ptr != NULL && pidx_ptr > baseAddr, "?");
    tassertMsg(cidx_ptr != NULL && cidx_ptr == pidx_ptr + 1, "?");
    tassertMsg(*pidx_ptr < numEntries, "?");
    tassertMsg(*cidx_ptr < numEntries, "?");

    SysStatus rc;

  retry:
    rc = 0;
    while (locked_isTransportFull()) {
	/* we already have too many ongoing requests (maximum our
	 * data structure holds is numEntries-1; it may be that some of those
	 * outstanding have already been consumed out of the buffer, but
	 * still we don't want to have too many outstanding requests
	 * because our consumer may not be able to handle them. */
	if (shouldBlock) {
	    rc = locked_block(highPriority);
	    tassertMsg(_SUCCESS(rc), "?");
	} else {
	    return _SERROR(2859, 0, EBUSY);
	}
    } 
    
    outstanding++;
    uval *ptr = (uval*) ((*pidx_ptr)*entryUvals + baseAddr) ;
    memcpy(ptr, request, entryUvals * sizeof(uval));
    uval old_pidx = *pidx_ptr;
    *pidx_ptr = (*pidx_ptr + 1) % numEntries;
    
    /* maximum available is numEntries - 1; we're detecting going from
     * empty to non empty */
    if (Avail(*pidx_ptr, *cidx_ptr, numEntries) == numEntries - 2) {
	//err_printf("KICK");
	rc = kickConsumer();
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) == EBUSY) {
		/* FIXME: for now we assume that if we can't send the
		 * async call, we have ongoing operations */
		tassertWrn(0, "kickConsumer returned EBUSY\n");
		outstanding--; // do not count current one
		passertMsg(outstanding >= 0, "outstanding %ld\n", outstanding);
		*pidx_ptr = old_pidx; // take the entry
		if (shouldBlock) {
		    // let's sleep and try again when progress has been made
		    rc = locked_block(highPriority);
		    goto retry;
		} else {
		    return _SERROR(2860, 0, EBUSY);
		}
	    } else if (_SGENCD(rc) == EINVAL) {
		// consumer process has gone away
		outstanding--; // do not count current one
		passertMsg(outstanding != 0, "basic assumption broken\n");
		*pidx_ptr = old_pidx; // take the entry
		return _SERROR(2932, 0, EINVAL);
	    }
	    passertMsg(_SUCCESS(rc), "look at this error rc 0x%lx\n", rc);
	}		
    } else {
	//err_printf("#%ld(%ld,%ld)#\n", outstanding, *pidx_ptr, *cidx_ptr);
    }

    return rc;
}

// highPriority == 0 means that the blocking request should not go to
// the high priority queue
SysStatus 
SharedBufferProducer::locked_block(uval highPriority)
{
    _ASSERT_HELD(lock);

    BlockedPageableThreads me;
    me.thread = Scheduler::GetCurThread();
    me.notified = 0;

    if (highPriority) {
	me.next = blockedThreadHighPriorityHead;
	blockedThreadHighPriorityHead = &me;
    } else {
	me.next = blockedThreadOthersHead;
	blockedThreadOthersHead = &me;
    }

    do {
	lock.release();
	Scheduler::Block();
	lock.acquire();
    } while (me.notified == 0);
    return 0;
}

/* virtual */ SysStatus
SharedBufferProducer::locked_requestCompleted()
{
    _ASSERT_HELD(lock);

    outstanding--;

    /* First we wake up threads blocked for reading. A read request
     * has already its page frame ready, so it makes sense to do
     * them first */
    while (blockedThreadHighPriorityHead != NULL) {
	//err_printf("unblocking read thread\n");
	blockedThreadHighPriorityHead->notified = 1;
	Scheduler::Unblock(blockedThreadHighPriorityHead->thread);
	blockedThreadHighPriorityHead = blockedThreadHighPriorityHead->next;
    }
    
    // now threads block for write
    while (blockedThreadOthersHead != NULL) {
	//err_printf("unblocking write thread\n");
	blockedThreadOthersHead->notified = 1;
	Scheduler::Unblock(blockedThreadOthersHead->thread);
	blockedThreadOthersHead = blockedThreadOthersHead->next;
    }

    return 0;
}

