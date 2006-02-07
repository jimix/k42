#ifndef __SEM_H_
#define __SEM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Sem.H,v 1.15 2004/07/08 17:15:31 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple semaphore
 * **************************************************************************/

#include <sync/BLock.H>
#include <sync/SLock.H>
#include <sync/atomic.h>
#include <trace/traceLock.h>

/* BaseSemaphore is a semaphore implementation where the lock is
 * maintained externally.  The lock is assumed locked on entry and
 * exit of all functions */
template <class Locktype> class BaseSemaphore {
protected:
    struct Element {
	Element *next;
	ThreadID id;
        sval count;
        sval error;
    };

    sval counter;

    // for threads blocked on P() requests
    Element *negHead;
    Element **negTail;

    // for threads blocked on Z() requests
    Element *zeroHead;
    Element **zeroTail;


public:

    // initialize the semaphore
    void init(sval c)
    {
        counter = c;
        negHead = NULL;
        zeroHead = NULL;
	negTail = NULL;
	zeroTail = NULL;
    }

    // check if zero
    uval tryZ()
    {
        if (counter == 0) {
            return 0;
        }
        return 1;
    }

    // block if not zero
    sval Z(Locktype *lock)
    {
    reset:
        if (counter != 0) {
	    Element el;

            el.error = 0;
            el.count = 0;
	    el.id = Scheduler::GetCurThread();
	    el.next = NULL;

	    if (!zeroTail) zeroTail = &zeroHead;

	    *zeroTail = &el;
	    zeroTail = &el.next;

	    lock->release();
	    while (el.id != Scheduler::NullThreadID)
		Scheduler::Block();

            if (el.error) {
                return el.error;
            }

            lock->acquire();
	    goto reset;
        }

        return 0;
    }

    // attempt to decrement by 'c'
    uval tryP(sval c = 1)
    {
	if (counter < c) {
	    return 0;
	} else {
	    counter -= c;
	}
	return 1;
    }

    // decrement by 'c'
    sval P(Locktype *lock, sval c = 1)
    {
    reset:
	if (counter < c) {
	    Element el;

            el.error = 0;
            el.count = c;
	    el.id = Scheduler::GetCurThread();
	    el.next = NULL;

	    if (!negTail) negTail = &negHead;

	    *negTail = &el;
	    negTail = &el.next;

	    lock->release();
	    while (el.id != Scheduler::NullThreadID)
		Scheduler::Block();

            if (el.error) {
                return el.error;
            }

            lock->acquire();
	    goto reset;
	} else {
	    counter -= c;
	}

        // check if zero to unblock any waiting threads
        while ((counter == 0) && (zeroHead != NULL)) {
            ThreadID thr;
            Element *el = zeroHead;

            // unblock the element
            zeroHead = zeroHead->next;
            if (zeroHead == NULL) {
                zeroTail = &zeroHead;
            }

            thr = el->id;
            el->id = Scheduler::NullThreadID;
            Scheduler::Unblock(thr);
        }

        return 0;
    }

    // increment by 'c'
    void V(sval c = 1)
    {
        sval tmpCounter;

        // increment the counter
        counter += c;
        tmpCounter = counter;

        // unblock any waiting threads that can go
        while (tmpCounter && negHead != NULL) {
            ThreadID thr;
            Element *el = negHead;

            // check if this element can be unblocked
            if (tmpCounter < el->count) {
                break;
            }

            // update the temporary counter
            tmpCounter -= el->count;

            // unblock the element
            negHead = negHead->next;
            if (negHead == NULL) {
                negTail = &negHead;
            }

            thr = el->id;
            el->id = Scheduler::NullThreadID;
            Scheduler::Unblock(thr);
	}
    }

    // wakeup all waiting threads with the given error code
    void wakeup(sval error)
    {
        while (zeroHead != NULL) {
            ThreadID thr;
            Element *el = zeroHead;

            // unblock the element
            zeroHead = zeroHead->next;
            if (zeroHead == NULL) {
                zeroTail = &zeroHead;
            }

            thr = el->id;
            el->error = error;
            el->id = Scheduler::NullThreadID;
            Scheduler::Unblock(thr);
        }

        while (negHead != NULL) {
            ThreadID thr;
            Element *el = negHead;

            // unblock the element
            negHead = negHead->next;
            if (negHead == NULL) {
                negTail = &negHead;
            }

            thr = el->id;
            el->error = error;
            el->id = Scheduler::NullThreadID;
            Scheduler::Unblock(thr);
        }
    }

    // count the different types of waiting threads
    void waitCount(sval &zCnt, sval &nCnt)
    {
        Element *ptr;

        zCnt = nCnt = 0;

        ptr = zeroHead;
        while (ptr) {
            ptr = ptr->next;
            zCnt++;
        }

        ptr = negHead;
        while (ptr) {
            ptr = ptr->next;
            nCnt++;
        }
    }

    // set the value directly
    void set(sval x)
    {
	counter = x;
    }

    // get the value of the semaphore
    sval get(void)
    {
	return counter;
    }
};


template <class Locktype> class TSemaphore {
public:
    BaseSemaphore<Locktype> sem;
    Locktype lock;

public:
    void init(sval c) {
        lock.init();
        sem.init(c);
    }

    uval tryP() {
	lock.acquire();
        uval ret = sem.tryP();
        lock.release();

        return ret;
    }

    void P() {
        lock.acquire();
        sem.P(&lock);
        lock.release();
    }

    void V(sval c=1) {
	lock.acquire();
        sem.V(c);
	lock.release();
    }

    void set(sval x) {
	lock.acquire();
        sem.set(x);
	lock.release();
    }
};

typedef TSemaphore<BLock> Semaphore;

#endif /* #ifndef __SEM_H_ */
