#ifndef __FAIR_BLOCK_H_
#define __FAIR_BLOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FairBLock.H,v 1.19 2003/01/16 19:43:29 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A fair blocking locks.
 * **************************************************************************/
#include <sync/BLock.H>
#include <defines/lock_options.H>

class FairBLock {
    struct Element {
	Element * nextThread;		// guy blocked on me
	union {
	    ThreadID waiter;		// ThreadID of blocked thread
	    Element* tail;		// tail of blocked queue
	};
    };

    Element lock;
#ifdef FAIRBLOCK_TRACK_OWNER
    uval owner;				// for debugging use
#endif /* #ifdef FAIRBLOCK_TRACK_OWNER */
#ifdef FAIRBLOCK_HANDOFF_TIME
    SysTime handoffTime;  // for measuring how long it takes to give lock
    uval handoffProc;  // records what processor the thread was on
#endif /* #ifdef FAIRBLOCK_HANDOFF_TIME */

    void _acquire();

    void _release();

public:
    void acquire();

    void release();

    uval tryAcquire(void);		// returns 1 if lock gotten, 0 if not

    uval isLocked(void) { return (uval)(lock.tail); }

    void init(void) {
	lock.nextThread = lock.tail = 0;
    }

    FairBLock(void) { init(); }
};

class FairBLockTraced {
    struct Element {
	Element * nextThread;		// guy blocked on me
	union {
	    ThreadID waiter;		// ThreadID of blocked thread
	    Element* tail;		// tail of blocked queue
	};
    };
    char * name;
    Element lock;
#ifdef FAIRBLOCK_TRACK_OWNER
    uval owner;				// for debugging use
#endif /* #ifdef FAIRBLOCK_TRACK_OWNER */

    void _acquire();

    void _release();

public:
    void acquire();

    void release();

    uval tryAcquire(void);		// returns 1 if lock gotten, 0 if not

    uval isLocked(void) { return (uval)(lock.tail); }

    void init(char* n) {
	name = n;
	lock.nextThread = lock.tail = 0;
    }

    FairBLockTraced(void) { init("null"); }
};

#ifdef USE_LOCK_FAST_PATHS
#include __MINC(FairBLock.H)
#endif /* #ifdef USE_LOCK_FAST_PATHS */

#endif /* #ifndef __FAIR_BLOCK_H_ */
