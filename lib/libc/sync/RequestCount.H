#ifndef __REQUEST_COUNT_H_
#define __REQUEST_COUNT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RequestCount.H,v 1.21 2005/01/23 23:59:19 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Object for controlling users of an object.
 *                     Counts number of requests in flight
 *                     Shuts object down and waits for existing requests
 *                           to finish
 *
 * Why this works:
 *            request can be in 4 states: +, 0, -N, (other) -
 *            the negative states imply a shutdown in progress.
 *            in a 64 bit implementation, N = (LONG_MAX)/2 is larger than
 *            any possible number of outstanding requests, so once in shutdown
 *            state requests can never become non-negative.
 *            The state transitions are:
 *                  enter  0 -> + , + -> +   (note that enter never changes
 *                                              a negative state)
 *                  leave  + -> + , + -> 0 , - -> - , - -> -N
 *                  shutdown 0 -> -N , + -> -
 *
 *             shutdown blocks iff it makes the second transition. leave
 *                      unblocks on the - -> -N transitions, which can
 *                      only happen if shutdown has previously blocked
 *                      (or is in the process of blocking)
 **************************************************************************/

#include <sync/atomic.h>
#include "limits.h"
#include <scheduler/Scheduler.H>
#include <cobj/sys/COSMgr.H>

/* To turn on compiling the class RequestCountWithRCU; off by default; use
 * at your own risk
 */
#include <defines/experimental.H>

class RequestCount {
protected:
    sval  requests;
    ThreadID shutdownThread;		// thread doing shutdown
    /* N.B. init is NOT synchonized with other operations.  Some external fact
     * must be used to guarantee that init occurs before the first use.
     * Normally, a RequestCount is in an object whose ref is only published
     * after initialization.
     */
    inline void init() {
	shutdownThread = Scheduler::NullThreadID;
	FetchAndClearVolatile((uval*)(&requests));
    }
public:
    RequestCount() { init(); };

    /* Enter records a use if the user is NOT shut down.
     * If the user is shut down,
     * returns a negative value. (use r since r is already in a register)
     */

    inline sval enter() {
	sval r = FetchAndAddSignedVolatile(&requests, 1);
	if (r < 0) { leave(); }
	return r;
    }

    /* Leave records the end of a use */
    inline void leave() {
	sval r;
	r = FetchAndAddSignedVolatile(&requests,-1);
	// note, r is value previous to decrement
	// check for zero because its cheaper to do than
	// comparing to a very large negative number,
	// and return is the common path.
	if (r > 0) return;

	// note, r is value previous to decrement
	if (r == (-(LONG_MAX/2)+1)) {
	    /*
	     * Atomically zero thread id for synchronization
	     * on destruction, and unblock thread.
	     */
	    ThreadID id =
		(ThreadID)SwapSynced((uval *)&shutdownThread,
				     (uval)Scheduler::NullThreadID);
	    if (id != Scheduler::NullThreadID) {
		Scheduler::Unblock(id);
	    }
	}
	return;
    }

    /* shutdown marks the RequestCount (and presumably the enclosing
     * object) as in shutdown.  No new enter requests will work.
     * shutdown blocks until all existing requests are completed.  if
     * the RequestCount is already shutdown or pending shutdown,
     * shutdown returns a non-zero value.  If this caller "won",
     * it returns 0 when shutdown is complete.
     */
    sval shutdown();
};

/*
 * RequestCountWithBlock adds the possibility of stopping for a while.
 * A stop call causes all new requests to block, all old request
 * to finish before it completes.  New requests continue to block.
 * The restart call releases all blocked requests.
 * Only one stop or shutdown may be in progress.  If a shutdown request
 * is make while a stop is in progress, it blocks until the restart
 * succeeds.
 * A stop request made when a shutdown or stop is in progress fails.
 *
 * This is implemented by adding a lock to the object.  Normal enter and
 * leave request are exactly as in RequestCount - no path length is added.
 * But, when an enter finds the count negative, it aquires the lock and checks
 * the count again while holding the lock.  If the count is still negative, it
 * fails.  Otherwise, it tries again after releasing the lock.
 *
 * Leave is exactly as before.
 *
 * stop aquires the lock, then does the shutdown logic.  Once the
 * shutdown is complete it returns with the lock still held.
 *
 * unblock resets the count to zero and releases the lock.
 *
 * shutdown aquires the lock, then fails or sets the count negative,
 * releases the lock and continus the shutdown process.
 */

class RequestCountWithStop: public RequestCount {
protected:
    BLock lock;
    /* N.B. init is NOT synchonized with other operations.  Some external fact
     * must be used to guarantee that init occurs before the first use.
     * Normally, a RequestCount is in an object whose ref is only published
     * after initialization.
     */
    inline void init() {
	lock.init();
    }

    inline sval enterInternal(uval *retry) {
        *retry = 0;
        sval r = FetchAndAddSignedVolatile(&requests, 1);
        if (r >= 0) return r;
        leave();
        lock.acquire();
        // if requests is neg while lock is held,
        // shutdown has won.  Otherwise, try again.
        if((r=requests)<0) {
            lock.release();
            return r;
        }
        // try again
        lock.release();
        *retry = 1;
        return r;
    }

public:
    RequestCountWithStop(): RequestCount() { init(); };

    /* Enter records a use if the user is NOT shut down.
     * If the user is shut down,
     * returns a negative value. (use r since r is already in a register)
     */

    inline sval enter() {
        uval retry;
	while(1) {
            sval r = enterInternal(&retry);
            if (!retry) return r;
	}
    }

    /*
     * stop returns 0 if it "wins".  The lock is held and
     * all requests are either blocked in enter or finished.
     * stop returns negative if a shutdown is in progress
     */
    sval stop();

    /*
     * restart returns to normal operation after a stop
     */
    void restart();

    /* shutdown marks the RequestCount (and presumably the enclosing
     * object) as in shutdown.  No new enter requests will work.
     * shutdown blocks until all existing requests are completed.  if
     * the RequestCount is already shutdown or pending shutdown,
     * shutdown returns a non-zero value.  If this caller "won",
     * it returns 0 when shutdown is complete.
     */
    sval shutdown();
};

#ifdef RCU_REQUEST_COUNT // not properly tested, broken,...
class RequestCountWithStopRCU : protected RequestCountWithStop {
public:
    typedef uval RequestToken;
    typedef void (*Operation)(uval);

protected:
    BLock opLock;
    Operation op;
    uval opArg;
    uval opInProgress;
    COSMgr::ThreadMarker threadMarker;

    void init() {
        opLock.init();
        // Seed the threadMarker with a valid value
        DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(threadMarker);
    }
public:
    RequestCountWithStopRCU() : RequestCountWithStop(), op(0), opArg(0),
                                opInProgress(0) { init(); }
    inline sval enter(RequestToken *token) {
        uval retry;
        sval rc;
        while (1) {
            // We are testing op without lock ....
            // Transitions to non zero are confirmed using RCU (we
            // wait for a generation to pass before taking actions)
            // Transitions back to zero are not confirmed so rest of code
            // must deal with inconsistences eg do an enter even if op has
            // been set to 0... these cases are handled under the lock.
            if (!op) {
                *token = 0;
                return 1;
            }
            rc = RequestCountWithStop::enterInternal(&retry);
            if (!retry) {
                *token = 1;
                return rc;
            }
        }
    }

    inline void leave(RequestToken token) {
        if (token == 1) {
            COSMgr::MarkerState state;
            RequestCountWithStop::leave();

            // As an optimization we check the thread mark without holding
            // the lock to allow leaves which occur before the marker has
            // elaspsed to complete without serializing.  
            DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(threadMarker,
                                                            state);
            if (state != COSMgr::ACTIVE) {
                opLock.acquire();
                // It is possible for an enter() to have induced a leave with
                // a non-zero token when the op has already been set to 0.
                // (enter has no lock guarding its test of op)
                // but now that we have the lock we can detect this case 
                // and simply return.  Additionally, 
                // if the operation has already been started we don't need
                // to do anything just get out of the way.  
                if (!op || opInProgress) {
                    opLock.release();
                    return;
                }
                // Operation has not been started so
                // reconfirm that the threadMarker has really elapsed
                // now that we are sure that is must have been set, since
                // it was done under the lock in qdo()
                DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(threadMarker,
                                                                state);
                if (state != COSMgr::ACTIVE) {
                    // If it has elapsed then record the fact that the
                    // operation is now in progress
                    opInProgress = 1;
                }  else {
                    opLock.release();
                    return ;
                }
                opLock.release();

                // If we get here then we must do the operation.
                if (stop()==0) {  
                    (*op)(opArg);
                    opLock.acquire();
                      op = 0;
                      opArg = 0;
                      opInProgress = 0;
                    opLock.release();
                    restart();
                } else {
                    tassertMsg(0, "shutdown started???\n");
                    return;
                }
            }
        }
    }

    inline sval qdo(Operation o, uval a) {
        opLock.acquire();
          if (op) { 
              // someone beat us in intiating a qdo we return failure
              opLock.release(); 
              return -1; 
          }

          op    = o;
          opArg = a;

          DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(threadMarker);

          // What about threads created now?  What happens if they see
          // op == 0 due to the write being out of order?
          // Consider that at this point the following occurs:

          //  a thread is created and does an enter seeing op == 0 (thus
          //  skips doing a "real" enter) and is  stopped for one reason or
          //  another while accessing the data structure which the request
          //  count is  supposed to be guarding.  If the new thread is not
          //  considered by the thread marker we might incorrectly  declare a 
          //  quiescent state as the thread has not done a real enter.

          // However:

          // The thread we are on acts as an existence  lock for the 
          // global thread marker.  Therefore the thread marker cannot elapse
          // to declare quiescents until the thread we are on, current threads
          // and any threads created before our termination complete.
          // In other words new threads cannnot leak past us until we terminate
          // at which point those threads will see op != 0.

        opLock.release();
        return 1;
    }

    inline sval shutdown() {
        tassertMsg(0, "nyi\n");
        return -1;
    }
};
#endif // #ifdef RCU_REQUEST_COUNT // not properly tested, broken,...

#endif /* #ifndef __REQUEST_COUNT_H_ */
