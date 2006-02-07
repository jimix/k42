#ifndef __BLOCK_H_
#define __BLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BLock.H,v 1.26 2003/01/16 19:43:29 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Blocking locks, this is the standard locking
 * type included everywhere.
 * **************************************************************************/

#include <defines/lock_options.H>

extern "C" void BitBLock_SlowAcquireAndFetch(volatile uval64 *lptr,
					     uval64 lockBitMask,
					     uval64 waitBitMask,
					     uval64 *datap);

extern "C" void BitBLock_SlowStoreAndRelease(volatile uval64 *lptr,
					     uval64 lockBitMask,
					     uval64 waitBitMask,
					     uval64 data);

#ifdef ENABLE_LOCK_CONTEXT_ASSERTIONS
void Lock_AssertProperCallingContext();
#else
#define Lock_AssertProperCallingContext()
#endif

#ifdef USE_LOCK_FAST_PATHS
#include __MINC(BLock.H)
#else /* #ifdef USE_LOCK_FAST_PATHS */
#include <sync/BLockGeneric.H>
#endif /* #ifdef USE_LOCK_FAST_PATHS */

template <class BS>
class BitBLock {
protected:
    BS bits;				// subclasses: access with caution
public:
    void acquire(void) {
	BitBLock_Acquire(&bits.data,
			 __LOCK_BIT_MASK(BS), __WAIT_BIT_MASK(BS));
    }

    void acquire(BS &bs) {
	BitBLock_AcquireAndFetch(&bits.data,
				 __LOCK_BIT_MASK(BS), __WAIT_BIT_MASK(BS),
				 &bs.data);
    }

    uval tryAcquire(void) {
        uval retvalue;
	retvalue = BitBLock_TryAcquire(&bits.data,
				   __LOCK_BIT_MASK(BS), __WAIT_BIT_MASK(BS));
	return(retvalue);
    }

    uval tryAcquire(BS &bs) {
        uval retvalue;
	retvalue = BitBLock_TryAcquireAndFetch(&bits.data,
					   __LOCK_BIT_MASK(BS),
					   __WAIT_BIT_MASK(BS),
					   &bs.data);
	return(retvalue);
    }

    uval isLocked(void) {
	const uval64 mask = __LOCK_BIT_MASK(BS);
	return ((FetchAndNop64(&bits.data) & mask) != 0);
    }

    void release(void) {
	tassert(isLocked(), err_printf("attempt to release not held lock\n"));
	BitBLock_Release(&bits.data,
			 __LOCK_BIT_MASK(BS), __WAIT_BIT_MASK(BS));
    }

    void release(BS bs) {
	tassert(isLocked(), err_printf("attempt to release not held lock\n"));
	BitBLock_StoreAndRelease(&bits.data,
				 __LOCK_BIT_MASK(BS), __WAIT_BIT_MASK(BS),
				 bs.data);
    }

    void init(void) {
	bits.data = uval64(0);
    }

    void init(BS bs) {
        const uval64 mask = (__LOCK_BIT_MASK(BS) | __WAIT_BIT_MASK(BS));
	bits.data = (bs.data & ~mask);
    }

    void get(BS &bs) {
	const uval64 mask = (__LOCK_BIT_MASK(BS) | __WAIT_BIT_MASK(BS));
	bs.data = bits.data & ~mask;
    }

    void set(BS bs) {
	const uval64 mask = (__LOCK_BIT_MASK(BS) | __WAIT_BIT_MASK(BS));
	uval64 oval, nval;
	do {
	    oval = bits.data;
	    nval = (oval & mask) | bs.data;
	} while (!CompareAndStore64(&bits.data, oval, nval));
    }

    BitBLock<BS>(void) { init(); }
    BitBLock<BS>(BS bs) { init(bs); }
};



/*
 * This lock releases all waiters to contend for the lock
 * this is sometimes the right thing to do to avoid uniprocessor
 * convoys, but can cause starvation in multiprocessors
 */
class BaseUnFairBLock {
    uval bits;
    void _acquire(void);

    void _release(void);

    struct waiting {
	waiting* next;
	ThreadID waiter;
    };

public:
    void acquire(void) {
	if (!CompareAndStoreSynced(&bits, 0, 1)) {
	    _acquire();
	}
    }
    uval tryAcquire(void) {
        return CompareAndStoreSynced(&bits, 0, 1);
    }

    uval isLocked(void) {
	return ((FetchAndNop(&bits) & 1) != 0);
    }

    void release(void) {
	tassert(isLocked(), err_printf("attempt to release not held lock\n"));
	if (!CompareAndStoreSynced(&bits, 1, 0)) {
	    _release();
	}
    }

    void init(void) {
	bits = 0;
    }
};

class UnFairBLock: public BaseUnFairBLock {
public:
    UnFairBLock(void) { init(); }
};


/*
 * Default Blocking lock is actually a fair blocking lock
 */
#include "FairBLock.H"
class BLock: public FairBLock {
};

#endif /* #ifndef __BLOCK_H_ */
