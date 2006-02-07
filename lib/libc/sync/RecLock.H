#ifndef __REC_LOCK_H_
#define __REC_LOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RecLock.H,v 1.5 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Recursive locks.
 * **************************************************************************/

#include <sync/BLock.H>
#include <sync/SLock.H>
#include <sync/atomic.h>
#include <scheduler/Scheduler.H>
#include <scheduler/Thread.H>


template <class Lock>
class TmpRecLock {
protected:
    Thread* _owner;
    Lock lock;
    uval count;
public:
    void init(){ lock.init(); _owner = NULL;};

    Thread* owner(){ return _owner; };
    void acquire(){
	if (Scheduler::GetCurThreadPtr() == _owner) {
	    ++count;
	    return;
	}
	lock.acquire();
	_owner = Scheduler::GetCurThreadPtr();
    }

    void release(){
	tassert(Scheduler::GetCurThreadPtr() == _owner,
	       err_printf("Unlocking lock that is not held\n"));
	if (--count > 0) {
	    return;
	}
	_owner = NULL;
	lock.release();
    }

    uval isLocked(){
	return lock.isLocked();
    }

    uval tryAcquire(){
	if (Scheduler::GetCurThreadPtr() == _owner) {
	    return 1;
	}
	return this->writerLock.tryAcquire();
    }
};

typedef TmpRecLock<BLock> RecBLock;
typedef TmpRecLock<SLock> RecSLock;


template <class Lock>
class TmpCountLock {
protected:
    Lock lock;
    Thread* _owner;
    uval count;
public:
    void init(){ lock.init(); _owner = NULL; count = 0;};

    Thread* owner(){ return _owner; };
    void acquire(){
	if (Scheduler::GetCurThreadPtr() == _owner) {
	    ++count;
	    return;
	}
	lock.acquire();
	_owner = Scheduler::GetCurThreadPtr();
	++count;
    }

    void release(){
	if (Scheduler::GetCurThreadPtr() != _owner) {
	    return;
	}
	--count;
	if (count==0) {
	    _owner = NULL;
	    lock.release();
	}
    }

    uval isLocked(){
	return lock.isLocked();
    }

    uval tryAcquire(){
	if (Scheduler::GetCurThreadPtr() == _owner) {
	    ++count;
	    return 1;
	}
	return this->writerLock.tryAcquire();
    }
};

typedef TmpCountLock<BLock> CountBLock;
typedef TmpCountLock<SLock> CountSLock;

#endif /* #ifndef __REC_LOCK_H_ */
