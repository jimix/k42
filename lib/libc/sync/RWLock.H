#ifndef __RWLOCK_H_
#define __RWLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RWLock.H,v 1.20 2004/07/08 17:15:31 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Reader-Writer locks.
 * **************************************************************************/

#include <sync/BLock.H>
#include <sync/SLock.H>
#include <sync/Sem.H>
#include <sync/atomic.h>
#include <misc/ListSimpleKey.H>
#include <trace/traceLock.h>
#include <misc/linkage.H>
#define DEBUG(args...)
//err_printf(args)
template <class Lock>
class TmpRWLock {

protected:
    enum LockerType {
	Reader,
	Writer
    };
    typedef uval ThreadIDPtr;
    ListSimpleKey<uval,uval ,AllocGlobal> waiters;
    Lock lock;
    sval32 readers;
public:
    void init() {
	lock.init();
	readers=0;
	waiters.reinit();
    };

    void acquireR() {
    reset:
	lock.acquire();
	ThreadID id = Scheduler::GetCurThread();


	uval dummy;
	if (readers<0  || waiters.find(Writer,dummy)) {

	    waiters.addToEndOfList(Reader,/*ThreadID**/(uval)&id);
	    lock.release();
	    while (id != Scheduler::NullThreadID)
		Scheduler::Block();
	    goto reset;
	}
	++readers;
	lock.release();
    }

    void releaseR() {
	lock.acquire();

	tassert(readers > 0, err_printf("not a reader\n"));
	--readers;
	if (readers == 0) {
	    uval threadIDp;
	    uval type;
	    if (waiters.removeHead(type, threadIDp)) {
		tassert(type==Writer,
			err_printf("RWLock Expected writer %li\n",type));
		ThreadID id = *(ThreadID*)threadIDp;
		*(ThreadID*)threadIDp = Scheduler::NullThreadID;
		Scheduler::Unblock(id);
		//We hand-off the lock to writers
		return;
	    }
	}

	lock.release();
    }

    void acquireW() {
	lock.acquire();

	ThreadID id = Scheduler::GetCurThread();

	if (readers!=0) {
	    waiters.addToEndOfList(Writer,/*ThreadID**/(uval)&id);
	    lock.release();
	    while (id != Scheduler::NullThreadID)
		Scheduler::Block();

	    //We've been given the lock by whoever unblocked us.
	}
	readers = -1;
	lock.release();
    }

    void upgrade() {
	ThreadID id = Scheduler::GetCurThread();
	lock.acquire();

	tassert(readers > 0, err_printf("not a reader\n"));
	--readers;
	if (readers != 0) {
	    waiters.addToEndOfList(Writer,/*ThreadID**/(uval)&id);
	    lock.release();
	    while (id != Scheduler::NullThreadID)
		Scheduler::Block();

	    //We've been given the lock by whoever unblocked us.
	}
	readers = -1;
	lock.release();
    }

    void downgrade() {
	lock.acquire();
	tassertMsg(readers == -1, "Downgrading a lock that is not held\n");
	readers = 1;
	uval threadIDp;
	while (waiters.remove(Reader,threadIDp)) {
	    ThreadID id = *(ThreadID*)threadIDp;
	    *(ThreadID*)threadIDp = Scheduler::NullThreadID;
	    Scheduler::Unblock(id);
	}
	lock.release();
    }

    void releaseW() {
	lock.acquire();
	tassertMsg(readers == -1, "Releasing a lock that is not held\n");
	readers = 0;
	uval threadIDp;
	uval dummy;
	if (waiters.remove(Writer, threadIDp)) {
	    ThreadID id = *(ThreadID*)threadIDp;
	    *(ThreadID*)threadIDp = Scheduler::NullThreadID;
	    Scheduler::Unblock(id);

	    //We hand-off the lock to the new guy
	} else {
	    while (waiters.removeHead(dummy, threadIDp)) {
		ThreadID id = *(ThreadID*)threadIDp;
		*(ThreadID*)threadIDp = Scheduler::NullThreadID;
		Scheduler::Unblock(id);
	    }
	    lock.release();
	}
    }

    uval tryAcquireW() {
	uval rc=0;
	lock.acquire();

	if (readers==0) {
	    rc = 1;
	    readers = -1;
	}
	lock.release();
	return rc;
    }

    uval tryUpgrade() {
	uval rc=0;
	ThreadID id = Scheduler::GetCurThread();
	lock.acquire();

	tassert(readers > 0, err_printf("not a reader\n"));
	if (readers!=1) {
	    rc = 0;
	} else {
	    readers = -1;
	    rc = 1;
	}
	lock.release();
	return rc;
    }

    // This makes it easy to use these locks with AutoLock
    inline void acquire() { acquireW(); };
    inline void release() { releaseW(); };

    // isLocked(), isLockedRead(), and isLockedWrite are used by assertions
    uval isLocked() { return (readers == 0 ? 0 : 1); }
    uval isLockedRead() { return (readers > 0 ? 1 : 0); }
    uval isLockedWrite() { return (readers == -1 ? 1 : 0); }
};

#define _ASSERT_HELD_READ(a) \
	tassert(a.isLockedRead(), err_printf("lock not held for read\n"));
#define _ASSERT_HELD_PTR_READ(a) \
	tassert(a->isLockedRead(), err_printf("lock not held for read\n"));
#define _ASSERT_HELD_WRITE(a) \
	tassert(a.isLockedWrite(), err_printf("lock not held for write\n"));

typedef TmpRWLock<BLock> RWBLock;
typedef TmpRWLock<SLock> RWSLock;


#endif /* #ifndef __RWLOCK_H_ */
