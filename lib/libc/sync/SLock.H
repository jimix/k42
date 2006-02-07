#ifndef __SLOCK_H_
#define __SLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SLock.H,v 1.19 2001/10/05 21:48:23 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Spin only lock, should be used only disabled
 * and/or at exception level in kernel (at least until we implement
 * pre-emption)
 * **************************************************************************/

template <class BS>
class BitSLock {
    BS bits;

public:
    void acquire(void) {
	const uval64 mask = __LOCK_BIT_MASK(BS);
	while ((FetchAndOr64Synced(&bits.data, mask) & mask) != 0) ;
    }
    void acquire(BS &bs) {
	const uval64 mask = __LOCK_BIT_MASK(BS);
	while ((FetchAndOr64Synced(&bits.data, mask) & mask) != 0) ;
	bs.data = bits.data & ~mask;
    }
    uval tryAcquire(void) {
	const uval64 mask = __LOCK_BIT_MASK(BS);
        return ((FetchAndOr64Synced(&bits.data, mask) & mask) == 0);
    }
    uval tryAcquire(BS &bs) {
	uval rc;

	const uval64 mask = __LOCK_BIT_MASK(BS);
        rc = ((FetchAndOr64Synced(&bits.data, mask) & mask) == 0);
	bs.data = bits.data & ~mask;
	return rc;
    }
    void release(void) {
	// safe since there is no wait bit and we have lock
	const uval64 mask = __LOCK_BIT_MASK(BS);
	bits.data &= ~mask;
    }
    void release(BS bs) {
	// safe since there is no wait bit and we have lock
	bits = bs;
    }
    uval isLocked(void) {
	const uval64 mask = __LOCK_BIT_MASK(BS);
	return ((fetchAndNop(&bits.data) & mask) != 0);
    }
    void init(void) {
	bits.data = uval64(0);
    }
    void init(BS bs) {
	const uval64 mask = __LOCK_BIT_MASK(BS);
	bits.data = (bs.data & ~mask);
    }
    void get(BS &bs) {
	// safe since there is no wait bit and we have lock
	const uval64 mask = __LOCK_BIT_MASK(BS);
	bs.data = bits.data & ~mask;
    }
    void set(BS bs) {
	// safe since there is no wait bit and we have lock
	const uval64 mask = __LOCK_BIT_MASK(BS);
	bits.data = (bits.data & mask) | bs.data;
    }
    BitSLock<BS>(void) { init(); }
    BitSLock<BS>(BS bs) { init(bs); }
};

class SLock {
    struct Element {
	Element * nextThread;		// guy blocked on me
	union {
	    uval waiter;		// 1 while waiting
	    Element* tail;		// tail of blocked queue
	};
    };

    Element lock;

    void _acquire();

    void _release();

//#define SLOCKMEGAKLUGE
#ifdef SLOCKMEGAKLUGE
//      for now, all the scheduler and exception stacks are at
//      the same virtual address, but in processor specific memory.
//      So creating a waiterel on the stack causes disaster when
//      more than one processor are involved in a lock.
//      We we make a fixed pool of waiter elements instead.
//      in the long run, we'll put the stacks in shared memory
//      with pointers in exception and scheduler local
    Element pool[100];
#endif /* #ifdef SLOCKMEGAKLUGE */
public:
    // FIXME: move implementation here when done debugging
    // FIXME: make this a machine specific file, with LL/SC on machines
    // that support
    void acquire();
    void release();

    uval tryAcquire(void) {
        return CompareAndStoreSynced((uval *)(&lock.tail), 0, (uval)(&lock));
    }

    uval isLocked(void) { return (uval)(lock.tail); }

    void init(void) {
	lock.nextThread = lock.tail = 0;
#ifdef SLOCKMEGAKLUGE
	uval i;
	for(i=0;i<(sizeof(pool)/sizeof(Element));i++) {
	    pool[i].waiter=uval(-1);
	}
#endif /* #ifdef SLOCKMEGAKLUGE */
    }

    SLock(void) { init(); }
};


#endif /* #ifndef __SLOCK_H_ */
