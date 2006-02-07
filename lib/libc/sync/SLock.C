/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SLock.C,v 1.27 2004/07/08 17:15:31 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of spin locks
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/Dispatcher.H>
#include <misc/hardware.H>
#include <trace/traceLock.h>
#include <misc/linkage.H>
#include <sys/KernelInfo.H>
#include <scheduler/Scheduler.H>
#include "SLock.H"


void
SLock::_acquire()
{
    Element waiterel;
    Element* myel=&waiterel;
    Element* el;
    uval spinCount = 0;
    SysTime start = 0;	// initializer needed to avoid compilation warning
    uval callChain[3];

    if (traceLockEnabled()) {
	start = Scheduler::SysTimeNow();
	GetCallChainSelf(0, callChain, 3);
    }

    while (1) {
	el=lock.tail;
	if (el==0) {
	    //Lock not held
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      0, (uval)(&lock))) {
		// got the lock, return
		TraceOSLockContendSpin(uval(this), spinCount,
			   (Scheduler::SysTimeNow() - start),
			   callChain[0], callChain[1], callChain[2]);
		return;
	    }
	    //Try again, something changed
	} else {
	    // lock is held
	    // queue on lock by first making myself tail
	    // and then pointing previous tail at me
#ifdef SLOCKMEGAKLUGE
	    uval i = Scheduler::GetVP()*6;
	    myel = 0;
	    //find and grab a free element
	    while (!myel) {
		if (CompareAndStoreSynced(&(pool[i].waiter), val(-1), 1)) {
		    myel = &(pool[i]);
		} else {
		    i += 1;
		    if (i==(sizeof(pool)/sizeof(Element)))
			i = 0;
		}
	    }
#else
	    tassertSilent(el != myel,breakpoint());
#endif
	    myel->nextThread = 0;
	    myel->waiter = 1;
	    if (CompareAndStoreSynced((uval *)(&lock.tail),
				      (uval)el, (uval)(myel))) {
		// queued on the lock - now complete chain
		el->nextThread = myel;
		while (FetchAndNop(&(myel->waiter)) != 0) {
		    spinCount++;
		}
		// at this point, I have the lock.  lock.tail
		// points to me if there are no other waiters
		// lock.nextThread is not in use
		lock.nextThread = 0;
		// CompareAndStore "bets" that there are no
		// waiters.  If it succeeds, the lock is put into
		// the held, nowaiters state.
		if (!CompareAndStoreSynced((uval *)(&lock.tail),
					  (uval)(myel), (uval)(&lock))) {
		    // failed to convert lock back to held/no waiters
		    // because there is another waiter
		    // spin on my nextThread - it may not be updated yet
		    while (FetchAndNop((uval*)&(myel->nextThread)) == 0) {
			spinCount++;
		    }
		    // record head of waiter list in lock, thus
		    // eliminating further need for myel
		    lock.nextThread =
			(Element *) FetchAndNop((uval*)&(myel->nextThread));
		}
#ifdef SLOCKMEGAKLUGE
		myel->waiter = uval(-1);
#endif
		// lock is held by me
		TraceOSLockContendSpin(uval(this), spinCount,
			   (Scheduler::SysTimeNow() - start),
			   callChain[0], callChain[1], callChain[2]);
		return;
	    }
#ifdef SLOCKMEGAKLUGE
		myel->waiter = uval(-1);
#endif
	    //Try again, something changed
	}
    }
}

void
SLock::_release()
{
    Element* el;
    // CompareAndStore betting there are no waiters
    // if it succeeds, the lock is placed back in the free state
    if (!CompareAndStoreSynced((uval*)(&lock.tail), (uval)(&lock), 0)) {
	// there is a waiter - but nextThread may not be updated yet
	while ((el=(Element*)FetchAndNop((uval*)&lock.nextThread)) == 0) {
	}
	el->waiter = 0;
	// waiter is responsible for completeing the lock state transition
    }
}


void
SLock::acquire()
{
    if (!CompareAndStoreSynced((uval *)(&lock.tail), 0, (uval)(&lock))) {
	_acquire();
    }
}

void
SLock::release()
{
    tassert(isLocked(), err_printf("attempt to release not held lock\n"));
    if (!CompareAndStoreSynced((uval*)(&lock.tail), (uval)(&lock), 0)) {
	_release();
    }
}
