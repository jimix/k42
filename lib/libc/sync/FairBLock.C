/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FairBLock.C,v 1.36 2004/07/08 17:15:31 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: mcs type fair lock using an algorithm
 *                     that does not require storage allocation.
 * The lock can be in the following states:
 *    head = tail = 0 lock is free
 *    head = 0, tail->lock lock is held, no waiters
 *    head -> head of waiter list, tail->tail of waiter list, waiters
 *
 *    the head pointer behaves like the waiter element next pointer, in
 *    that it is updated AFTER the tail pointer is swung atomically
 *    so state decisions are all made looking at tail, with waits for
 *    head to become non-zero as needed
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sync/FairBLock.H>
#include <scheduler/Scheduler.H>
#include <stdarg.h>
#include <trace/traceLock.h>
#include <misc/linkage.H>
#include <sys/KernelInfo.H>
#include <defines/lock_options.H>

#define FAIRBLOCK_SPIN

#ifdef FAIRBLOCK_SPIN
// These are variables for experimentation purposes.
uval FairBLockSpinCount = 100;	// how many times to spin
uval FairBLockYieldCount = 100;	// how many times to yield to other threads
uval FairBLockYieldVPCount = 1;	// how many times to yield the processor
#endif

FairBLock *TargetLock = NULL;

void
FairBLock::_acquire()
{
    Element myel;
    Element* el;
    uval spinCount = 0;	// initialized in case FAIRBLOCK_SPIN is not defined
    SysTime start = 0;	// initializer needed to avoid compilation warning
    SysTime delayStart = 0; // initializer needed to avoid compilation warning
    uval callChain[3];

    if (traceLockEnabled()) {
	start = Scheduler::SysTimeNow();
	GetCallChainSelf(0, callChain, 3);
    }

    if (this == TargetLock) BREAKPOINT;

#ifdef FAIRBLOCK_SPIN

    uval const spinLimit = FairBLockSpinCount;
    uval const yieldLimit = spinLimit + FairBLockYieldCount;
    uval const yieldVPLimit = yieldLimit + FairBLockYieldVPCount;

    // try spinning and yielding for a while
    for (spinCount = 0; spinCount < yieldVPLimit; spinCount++) {
	el = lock.tail;
	if (el==0) {
	    //Lock not held; try to grab lock
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      0, (uval)(&lock))) {
		// got the lock, return
		TraceOSLockContendSpin(uval(this), spinCount,
			   (Scheduler::SysTimeNow() - start),
			   callChain[0], callChain[1], callChain[2]);
		return;
	    }
	    /* didn't get lock, but keep trying anyway, since it's likely
	     * not held for very long
	     */
	} else if (el != &lock) {
	    // already waiters, so block
	    break;
	}
	if (spinCount > yieldLimit) {
	    // enough time yielding, try giving up the processor
	    Scheduler::YieldProcessor();
	} else if (spinCount > spinLimit) {
	    // enough time just spinning, now yield
	    Scheduler::Yield();
	}
    }

    // need to block

#endif

    while(1) {
	el=lock.tail;
	if (el==0) {
	    //Lock not held
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      0, (uval)(&lock))) {
		// got the lock, return
		return;
	    }
	    //Try again, something changed
	} else {
	    // lock is held
	    // queue on lock by first making myself tail
	    // and then pointing previous tail at me
	    myel.nextThread = 0;
	    myel.waiter = Scheduler::GetCurThread();
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      (uval)el, (uval)(&myel))) {
		// queued on the lock - now complete chain
		el->nextThread = &myel;
		while(myel.waiter != Scheduler::NullThreadID) {
		    Scheduler::Block();
		}
		// at this point, I have the lock.  lock.tail
		// points to me if there are no other waiters
		// lock.nextThread is not in use
		lock.nextThread = 0;
		// CompareAndStoreSynced "bets" that there are no
		// waiters.  If it succeeds, the lock is put into
		// the held, nowaiters state.
		if (!CompareAndStoreSynced((uval *)(&lock.tail),
					   (uval)(&myel),(uval)(&lock))) {
		    // failed to convert lock back to held/no waiters
		    // because there is another waiter
		    // spin on my nextThread - it may not be updated yet
		    if (myel.nextThread == 0) {
			if (traceLockEnabled()) {
			    delayStart = Scheduler::SysTimeNow();
			}
			Scheduler::Yield(); // do one yield before delaying
			while (myel.nextThread == 0) {
			    // no pre-emption so we delay
			    Scheduler::DelayMicrosecs(1000);
			}
			TraceOSLockDelayAcq(uval(this),
				   (Scheduler::SysTimeNow() - delayStart),
				   callChain[0], callChain[1], callChain[2]);
		    }
		    // record head of waiter list in lock, thus
		    // eliminating further need for myel
		    lock.nextThread = myel.nextThread;
		}
		// lock is held by me


		    TraceOSLockContendBlock(uval(this), spinCount,
			       (Scheduler::SysTimeNow() - start),
			       callChain[0], callChain[1], callChain[2],
#ifdef FAIRBLOCK_HANDOFF_TIME
			       (Scheduler::SysTimeNow() - handoffTime),
			       handoffProc);
#else
		               0, 0);
#endif
		return;
	    }
	    //Try again, something changed
	}
    }
}

void
FairBLock::_release()
{
    Element* el;
    ThreadID waiter;
    SysTime delayStart = 0; // initializer needed to avoid compilation warning
    uval callChain[3];

    // CompareAndStore betting there are no waiters
    // if it succeeds, the lock is placed back in the free state
    if (!CompareAndStoreSynced((uval*)(&lock.tail), (uval)(&lock), 0)) {
	// there is a waiter - but nextThread may not be updated yet
	if ((el=lock.nextThread) == 0) {
	    if (traceLockEnabled()) {
		delayStart = Scheduler::SysTimeNow();
		GetCallChainSelf(0, callChain, 3);
	    }
	    Scheduler::Yield();		// do one yield before delaying
	    while((el=lock.nextThread) == 0) {
		Scheduler::DelayMicrosecs(1000);
	    }
	    TraceOSLockDelayRel(uval(this),
		       (Scheduler::SysTimeNow() - delayStart),
		       callChain[0], callChain[1], callChain[2]);
	}
	waiter = (ThreadID)(el->waiter);
	tassert(waiter != Scheduler::NullThreadID,
		err_printf("lock waiter chain damaged\n"));
	el->waiter = (uval)(Scheduler::NullThreadID);
#ifdef FAIRBLOCK_HANDOFF_TIME
	if (traceLockEnabled()) {
	    handoffTime = Scheduler::SysTimeNow();
            handoffProc = KernelInfo::PhysProc();
	}
#endif
	Scheduler::Unblock(waiter);
	// waiter is responsible for completeing the lock state transition
    }
}

void
FairBLockTraced::_acquire()
{
    Element myel;
    Element* el;
    uval spinCount = 0;

    TraceOSLockAcqSpin((uval)this, 
    		     (uval)Scheduler::GetCurThreadPtr(), name);

#ifdef FAIRBLOCK_SPIN

    uval const spinLimit = FairBLockSpinCount;
    uval const yieldLimit = spinLimit + FairBLockYieldCount;
    uval const yieldVPLimit = yieldLimit + FairBLockYieldVPCount;

    // try spinning and yielding for a while
    for (spinCount = 0; spinCount < yieldVPLimit; spinCount++) {
	el = lock.tail;
	if (el==0) {
	    //Lock not held; try to grab lock
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      0, (uval)(&lock))) {
		// got the lock, return
		return;
	    }
	    /* didn't get lock, but keep trying anyway, since it's likely
	     * not held for very long
	     */
	} else if (el != &lock) {
	    // already waiters, so block
	    break;
	}
	if (spinCount > yieldLimit) {
	    // enough time yielding, try giving up the processor
	    Scheduler::YieldProcessor();
	} else if (spinCount > spinLimit) {
	    // enough time just spinning, now yield
	    Scheduler::Yield();
	}
    }
#endif //FAIRBLOCK_SPIN
    // need to block

    TraceOSLockAcqBlock((uval)this, 
    		     (uval)Scheduler::GetCurThreadPtr(), name);

    while(1) {
	el=lock.tail;
	if (el==0) {
	    //Lock not held
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      0, (uval)(&lock))) {
		// got the lock, return
		return;
	    }
	    //Try again, something changed
	} else {
	    // lock is held
	    // queue on lock by first making myself tail
	    // and then pointing previous tail at me
	    myel.nextThread = 0;
	    myel.waiter = Scheduler::GetCurThread();
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      (uval)el, (uval)(&myel))) {
		// queued on the lock - now complete chain
		el->nextThread = &myel;
		while(myel.waiter != Scheduler::NullThreadID) {
		    Scheduler::Block();
		}
		// at this point, I have the lock.  lock.tail
		// points to me if there are no other waiters
		// lock.nextThread is not in use
		lock.nextThread = 0;
		// CompareAndStoreSynced "bets" that there are no
		// waiters.  If it succeeds, the lock is put into
		// the held, nowaiters state.
		if (!CompareAndStoreSynced((uval *)(&lock.tail),
					   (uval)(&myel),(uval)(&lock))) {
		    // failed to convert lock back to held/no waiters
		    // because there is another waiter
		    // spin on my nextThread - it may not be updated yet
		    if (myel.nextThread == 0) {
			Scheduler::Yield(); // do one yield before delaying
			while (myel.nextThread == 0) {
			    // no pre-emption so we delay
			    Scheduler::DelayMicrosecs(1000);
			}
		    }
		    // record head of waiter list in lock, thus
		    // eliminating further need for myel
		    lock.nextThread = myel.nextThread;
		}
		// lock is held by me
		return;
	    }
	    //Try again, something changed
	}
    }
}

void
FairBLockTraced::_release()
{
    Element* el;
    ThreadID waiter;

    // CompareAndStore betting there are no waiters
    // if it succeeds, the lock is placed back in the free state
    if (!CompareAndStoreSynced((uval*)(&lock.tail), (uval)(&lock), 0)) {
	// there is a waiter - but nextThread may not be updated yet
	if ((el=lock.nextThread) == 0) {
	    Scheduler::Yield();		// do one yield before delaying
	    while((el=lock.nextThread) == 0) {
		Scheduler::DelayMicrosecs(1000);
	    }
	}
	waiter = (ThreadID)(el->waiter);
	tassert(waiter != Scheduler::NullThreadID,
		err_printf("lock waiter chain damaged\n"));
	el->waiter = (uval)(Scheduler::NullThreadID);
	Scheduler::Unblock(waiter);
	// waiter is responsible for completeing the lock state transition
    }
}

void
FairBLockTraced::acquire()
{
    Lock_AssertProperCallingContext();
    _acquire();
    TraceOSLockAcqLock((uval)this, 
		     (uval)Scheduler::GetCurThreadPtr(), name);
}

void
FairBLockTraced::release()
{
    Lock_AssertProperCallingContext();
    tassert(isLocked(), err_printf("attempt to release not held lock\n"));
    TraceOSLockRel1((uval)this, 
		     (uval)Scheduler::GetCurThreadPtr(), name);
    _release();
    TraceOSLockRel2((uval)this, 
		    (uval) Scheduler::GetCurThreadPtr(), name);
}

uval
FairBLockTraced::tryAcquire(void)
{
    Lock_AssertProperCallingContext();
    if (CompareAndStoreSynced((uval *)(&lock.tail), 0, (uval)(&lock))) {
	TraceOSLockAcqLock((uval)this, 
			 (uval)Scheduler::GetCurThreadPtr(), name);
	return 1;
    }
    return 0;
}

#ifndef USE_LOCK_FAST_PATHS
void
FairBLock::acquire()
{
    Lock_AssertProperCallingContext();
    _acquire();
#ifdef FAIRBLOCK_TRACK_OWNER
    owner = (uval)Scheduler::GetCurThreadPtr();
#endif // FAIRBLOCK_TRACK_OWNER
}

void
FairBLock::release()
{
    Lock_AssertProperCallingContext();
#ifdef FAIRBLOCK_TRACK_OWNER
    tassert(owner == (uval)Scheduler::GetCurThreadPtr(),
	    err_printf("freeing lock not held by you\n"));
    owner = 0;
#endif // FAIRBLOCK_TRACK_OWNER
    tassert(isLocked(), err_printf("attempt to release not held lock\n"));
    _release();
}

uval
FairBLock::tryAcquire(void)
{
    Lock_AssertProperCallingContext();
    if (CompareAndStoreSynced((uval *)(&lock.tail), 0, (uval)(&lock))) {
#ifdef FAIRBLOCK_TRACK_OWNER
	owner = (uval)Scheduler::GetCurThreadPtr();
#endif // FAIRBLOCK_TRACK_OWNER
	return 1;
    }
    return 0;
}
#endif /* #ifndef USE_LOCK_FAST_PATHS */
