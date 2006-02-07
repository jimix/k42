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
 * $Id: Lock.H,v 1.1 2004/02/11 23:04:01 lbsoares Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: Lock machinery used in various locks
 * **************************************************************************/

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
    AutoLock(Lock *l) : lock(l)         { lock->acquire(); }
    AutoLock(Lock &l) : lock(&l)        { lock->acquire(); }

    DEFINE_NOOP_NEW(AutoLock);
    ~AutoLock()                         { lock->release(); }
};

// only locks that should be widely used are blocking locks
#include <sync/BLock.H>

#define _ASSERT_HELD(a) \
       passert(a.isLocked(), err_printf("lock not held\n"));

#define _ASSERT_HELD_PTR(a) \
       passert(a->isLocked(), err_printf("lock not held\n"));

#endif /* #ifndef __SYNC_LOCK_H_ */
