/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RequestCount.C,v 1.22 2002/05/03 14:59:31 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Object for controlling users of an object.
 *                     Counts number of requests in flight
 *                     Shuts object down and waits for existing requests
 *                           to finish
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "RequestCount.H"

sval RequestCount::shutdown()
{
    sval r;

    // Mark myself as thread doing shutdown
    do {
	// if requests is less than zero, someone already shutting down
	if ((r = FetchAndNopSigned(&requests)) < 0) {
	    return r;
	}

	// if shutdownThread set, someone already shutting down
	if ((ThreadID)FetchAndNop((uval *)&shutdownThread) !=
					    Scheduler::NullThreadID) {
	    return r;
	}
    } while(!CompareAndStoreSynced(&shutdownThread,
				   (uval)Scheduler::NullThreadID,
				   (uval)Scheduler::GetCurThread()));

    // atomically decrement requests by huge number
    r = FetchAndAddSignedSynced(&requests, -((sval)LONG_MAX/2));

    if (r==0) { // done
	// not really necessary to clear, but what the heck
	(ThreadID)SwapSynced((uval *)&shutdownThread,
			     (uval)Scheduler::NullThreadID);
	return 0;
    }

    // waiting for requests to complete
    while (FetchAndNop((uval *)&shutdownThread) != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    err_printf("completed request in flight %p\n",this);
    return 0;
}

sval RequestCountWithStop::shutdown()
{
    sval r;

    // get the lock for a while.
    lock.acquire();

    // Mark myself as thread doing shutdown
    do {
	// if requests is less than zero, someone already shutting down
	if ((r = FetchAndNopSigned(&requests)) < 0) {
	    lock.release();
	    return r;
	}

	// if shutdownThread set, someone already shutting down
	if ((ThreadID) FetchAndNop((uval *)&shutdownThread) !=
						Scheduler::NullThreadID) {
	    lock.release();
	    return r;
	}
    } while(!CompareAndStoreSynced(&shutdownThread,
				   (uval)Scheduler::NullThreadID,
				   (uval)Scheduler::GetCurThread()));

    // atomically decrement requests by huge number
    r = FetchAndAddSignedSynced(&requests, -((sval)LONG_MAX/2));

    // Once we set the shutdown thread, we have the real shutdown/stop
    // lock.  We must release the stop lock to allow enters to fail
    // quickly.
    lock.release();

    if (r==0) {
	// not really necessary to clear, but what the heck
	(ThreadID)SwapSynced((uval *)&shutdownThread,
			     (uval)Scheduler::NullThreadID);
	return 0;
    }

    // waiting for requests to complete
    while (FetchAndNop((uval *)&shutdownThread) != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
    err_printf("completed request in flight %p\n",this);
    return 0;
}

sval RequestCountWithStop::stop()
{
    sval r;

    // get the lock.
    lock.acquire();

    // Mark myself as thread doing stop
    do {
	// if requests is less than zero, someone already shutting down
	if ((r = FetchAndNopSigned(&requests)) < 0) {
	    lock.release();
	    return -1;
	}

	// if shutdownThread set, someone already shutting down
	if ((ThreadID)FetchAndNop((uval *)&shutdownThread) !=
					    Scheduler::NullThreadID) {
	    lock.release();
	    return -1;
	}
    } while(!CompareAndStoreSynced(&shutdownThread,
				   (uval)Scheduler::NullThreadID,
				   (uval)Scheduler::GetCurThread()));

    // atomically decrement requests by huge number
    r = FetchAndAddSignedSynced(&requests, -((sval)LONG_MAX/2));

    // Once we set the shutdown thread, we have the real shutdown/stop
    // lock.

    if (r==0) {
	// clear the lock for the next time
	(ThreadID)SwapSynced((uval *)&shutdownThread,
			     (uval)Scheduler::NullThreadID);
	return 0;
    }

    // waiting for requests to complete
    while (FetchAndNop((uval *)&shutdownThread) != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
//    err_printf("completed request in flight %p\n",this);
    return 0;
}

void
RequestCountWithStop::restart()
{
    _ASSERT_HELD(lock);
    tassertMsg(shutdownThread == Scheduler::NullThreadID,
	       "restart found a waiting shutdownThread\n");
    /* restore count to normal operating range
     * note that enters and leaves may be happing while we do this
     * and it all works correctly since enter in effect polls for a
     * non-negative requests value.
     */
    FetchAndAddSignedSynced(&requests, ((sval)LONG_MAX/2));
    lock.release();
}
