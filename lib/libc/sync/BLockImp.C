/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BLockImp.C,v 1.29 2004/07/08 17:15:31 gktse Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sync/BLock.H>
#include <scheduler/Scheduler.H>
#include <trace/traceLock.h>
#include <misc/linkage.H>
#include <sys/KernelInfo.H>

void
BaseUnFairBLock::_acquire(void)
{
    Lock_AssertProperCallingContext();

    waiting w;

    while(1) {
	uval curhead = *(volatile uval *)&bits;
	if (curhead==0) {
	    if (CompareAndStoreSynced(&bits, 0, 1)) {
		return;
	    }
	} else {
	    // sigh, lets add ourselves to the queue
	    w.next = (waiting *)(curhead & ~(uval)1);
	    w.waiter = Scheduler::GetCurThread();
	    uval newval = (uval)(&w)|1;
	    if (CompareAndStoreSynced(&bits, curhead, newval)) {
		// okay, now block
		Scheduler::Block();
	    }
	}
    }
}

void
BaseUnFairBLock::_release(void)
{
    Lock_AssertProperCallingContext();

    uval curr = SwapSynced(&bits, 0);
    waiting *w = (waiting *)(curr&~(uval)1);

    while (w) {
	waiting *next = w->next;
	Scheduler::Unblock(w->waiter);
	w = next;
    }
}

#include <sync/BlockedThreadQueues.H>

#define BITBLOCK_SPIN

#ifdef BITBLOCK_SPIN
// These are variables for experimentation purposes.
uval BitBLockSpinCount = 100;	// how many times to spin
uval BitBLockYieldCount = 100;	// how many times to yield to other threads
uval BitBLockYieldVPCount = 1;	// how many times to yield the processor
#endif

extern "C" void
BitBLock_SlowAcquireAndFetch(volatile uval64 *lptr,
			     uval64 lockBitMask, uval64 waitBitMask,
			     uval64 *datap)
{
    uval spinCount = 0;	// initialized in case BITBLOCK_SPIN is not defined
    SysTime start = 0;	// initializer needed to avoid compilation warning
    uval callChain[3];

    if (traceLockEnabled()) {
	start = Scheduler::SysTimeNow();
	GetCallChainSelf(0, callChain, 3);
    }

#ifdef BITBLOCK_SPIN

    uval const spinLimit = BitBLockSpinCount;
    uval const yieldLimit = spinLimit + BitBLockYieldCount;
    uval const yieldVPLimit = yieldLimit + BitBLockYieldVPCount;

    // try spinning and yielding for a while
    for (spinCount = 0; spinCount < yieldVPLimit; spinCount++) {
	uval64 l = *lptr;
	if ((l & lockBitMask) == 0) {
	    // lock currently free, try to grab before going slow
	    if ((FetchAndOr64Synced(lptr, lockBitMask) & lockBitMask) == 0) {
		// got lock
		if (datap != NULL) {
		    *datap = (*lptr) & ~(lockBitMask|waitBitMask);
		}
		TraceOSLockContendSpin(uval(lptr), spinCount,
			   (Scheduler::SysTimeNow() - start),
			   callChain[0], callChain[1], callChain[2]);
		return;
	    }
	    /* didn't get lock, but keep trying anyway, since it's likely
	     * not held for very long
	     */
	} else if ((l & waitBitMask) != 0) {
	    // already waiters, no point spinning
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

#endif /* #ifdef BITBLOCK_SPIN */

    BlockedThreadQueues::Element qe;
    DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(&qe,
							     (void *)lptr);

    // try to acquire lock, if failed then block
    while(1) {
	if ((FetchAndOr64Synced(lptr, lockBitMask) & lockBitMask) == 0)
	    break; // got lock

	// didn't acquire the lock
	uval64 cur = *lptr;
	if (cur & lockBitMask) {
	    if (cur & waitBitMask) {
		Scheduler::Block();
	    } else {
		// set wait bit and block only if lock bit still set
		uval64 nval = cur|waitBitMask;
		if (CompareAndStore64Synced(lptr, cur, nval)) {
		    Scheduler::Block();
		}
	    }
	}
    }
    DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
	&qe, (void *)lptr);

    if (datap != NULL) {
	*datap = (*lptr) & ~(lockBitMask|waitBitMask);
    }
    TraceOSLockContendBlock(uval(lptr), spinCount,
	       (Scheduler::SysTimeNow() - start),
	       callChain[0], callChain[1], callChain[2], 0, 0);
}

extern "C" void
BitBLock_SlowStoreAndRelease(volatile uval64 *lptr,
			     uval64 lockBitMask, uval64 waitBitMask,
			     uval64 data)
{
    // in this implementation, release lock, then wake up everyone
#if defined(TARGET_powerpc)
    // powerpc simos currently does not support cancelled reservations
    // through regular stores
    SwapSynced((volatile uval *)lptr, uval(data));
#elif defined(TARGET_mips64)
    *lptr = data;
#elif defined(TARGET_amd64)
    *lptr = data;	// XXX check pdb
#elif defined(TARGET_generic64)
    *lptr = data;
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */
    DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll((void *)lptr);
}
