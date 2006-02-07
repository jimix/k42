#ifndef __SYNC_LOCK_H_
#define __SYNC_LOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Lock.H,v 1.14 2001/10/05 21:48:20 peterson Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Lock machinery used in various locks
 * **************************************************************************/

#include <sync/atomic.h>

#define __LOCK_BIT(name,bitnum)			\
    enum {					\
	_LOCK_BIT = __##name##_START + bitnum	\
    }

#define __WAIT_BIT(name,bitnum)			\
    enum {					\
	_WAIT_BIT = __##name##_START + bitnum	\
    }

#define __LOCK_BIT_MASK(BS) (uval64(1) << BS::_LOCK_BIT)
#define __WAIT_BIT_MASK(BS) (uval64(1) << BS::_WAIT_BIT)

/*
 * Structure used for a pointer lock, where we steal the bottom two
 * bits for locking
 */
struct PtrBits : BitStructure {
    __BIT_FIELD(8*sizeof(void*), ptr, BIT_FIELD_START);
    __LOCK_BIT(ptr, 0);
    __WAIT_BIT(ptr, 1);
};


/*
 * Struct used for basic spin and blocking locks, where we are not using
 * bits stolen from some other data
 */
struct LockBits : BitStructure {
    __BIT_FIELD(64, bits, BIT_FIELD_START);
    __LOCK_BIT(bits, 0);
    __WAIT_BIT(bits, 1);
};

/*
 * A class used to ease implementation of simple locking case, acquires
 * lock when a variable of this class is declared, releases lock
 * when variable is destroyed.  Is a bit inefficient, but eases use.
 */
template <class Lock>
class AutoLock {
    Lock *lock;
public:
    // Beware: the constructor using a reference **does not get inlined**
    AutoLock(Lock *l) : lock(l)		{ lock->acquire(); }
    AutoLock(Lock &l) : lock(&l)	{ lock->acquire(); }

    DEFINE_NOOP_NEW(AutoLock);
    ~AutoLock()				{ lock->release(); }
};

// only locks that should be widely used are blocking locks
#include <sync/BLock.H>
#include <sync/PtrBLock.H>

#define _ASSERT_HELD(a) \
	tassert(a.isLocked(), err_printf("lock not held\n"));

#define _ASSERT_HELD_PTR(a) \
	tassert(a->isLocked(), err_printf("lock not held\n"));

#endif /* #ifndef __SYNC_LOCK_H_ */
