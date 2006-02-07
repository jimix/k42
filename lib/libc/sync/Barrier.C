/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Barrier.C,v 1.12 2005/04/15 17:39:35 dilma Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Barrier implementation, stolen from Hurricane
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>

#include "Barrier.H"

static void
SpinWait(uval length)
{
    uval i, j;

    for (i=0; i<length*100; i++) {
        j = j + length;
    }
}

SpinBarrier :: SpinBarrier(uval numWaiters)
{
    total     = numWaiters;
    upcount   = 0;
    downcount = 0;
}

// it is assume the barrier is not currently in use
void
SpinBarrier :: reinit(uval numWaiters)
{
    lock.init();
    total     = numWaiters;
    upcount   = 0;
    downcount = 0;
}

void
SpinBarrier :: enter()
{
    uval spintime;
    uval ltotal = total;

    if (upcount < ltotal) {
	/* Each process increments upcount on entry */
	lock.acquire();
	if (upcount == ltotal - 1) {
	    downcount = ltotal;
	}
	upcount++;
	lock.release();

	/* now wait until everyone has reached barrier */
	for (spintime = 0; upcount < ltotal; spintime++) {
	    SpinWait((spintime>>6) & 0x1f);
	}
    } else {
	/* Each process decrement downcount on entry */
	lock.acquire();
	if (downcount == 1) {
	    upcount = 0;
	}
	downcount--;
	lock.release();

	/* now wait until everyone has reached barrier */
	for (int sptime = 0; downcount > 0; sptime++) {
	    SpinWait((sptime>>6) & 0x1f);
	}
    }
}

SpinBarrier :: ~SpinBarrier()
{
    /* safely destroy by letting those left in the barrier to exit first
     * we assume that everyone has already entered the barrier, and we
     * are just waiting for them to leave the barrier
     */
    if( upcount != 0 ) {
	while (upcount < total) {
	    //err_printf("~Barrier: upcount %ld\n", upcount);
	    Scheduler::Yield();
	}
    } else {
	while (downcount > 0) {
	    err_printf("~Barrier: downcount %ld\n", downcount);
	    Scheduler::Yield();
	}
    }
}


SpinOnlyBarrier :: SpinOnlyBarrier(uval numWaiters)
{
    total     = numWaiters;
    upcount   = 0;
    downcount = 0;
}

// it is assume the barrier is not currently in use
void
SpinOnlyBarrier :: reinit(uval numWaiters)
{
    total     = numWaiters;
    upcount   = 0;
    downcount = 0;
}

void
SpinOnlyBarrier :: enter()
{
    uval spintime;
    uval ltotal = total;

    if (upcount < ltotal) {
	/* Each process increments upcount on entry */
	lock.acquire();
	if (upcount == ltotal - 1) {
	    downcount = ltotal;
	}
	upcount++;
	lock.release();

	/* now wait until everyone has reached barrier */
	for (spintime = 0; upcount < ltotal; spintime++) {
	    SpinWait((spintime>>6) & 0x1f);
	}
    } else {
	/* Each process decrement downcount on entry */
	lock.acquire();
	if (downcount == 1) {
	    upcount = 0;
	}
	downcount--;
	lock.release();

	/* now wait until everyone has reached barrier */
	for (spintime = 0; downcount > 0; spintime++) {
	    SpinWait((spintime>>6) & 0x1f);
	}
    }
}

SpinOnlyBarrier :: ~SpinOnlyBarrier()
{
    /* safely destroy by letting those left in the barrier to exit first
     * we assume that everyone has already entered the barrier, and we
     * are just waiting for them to leave the barrier
     */
    if (upcount != 0) {
	while (upcount < total) {
	    err_printf("~Barrier: upcount %ld\n", upcount);
	    Scheduler::Yield();
	}
    } else {
	while (downcount > 0) {
	    err_printf("~Barrier: downcount %ld\n", downcount);
	    Scheduler::Yield();
	}
    }
}


BlockBarrier :: BlockBarrier(uval numWaiters)
{
    total   = numWaiters;
    upcount = downcount = 0;
    list    = 0;
}

// it is assume the barrier is not currently in use
void
BlockBarrier :: reinit (uval numWaiters)
{
    total   = numWaiters;
    upcount = downcount = 0;
    list    = 0;
}

class BlockBarrier::BarrierList {
public:
    ThreadID     tid;
    BarrierList *next;

    DEFINE_GLOBAL_NEW(BarrierList);

    BarrierList(ThreadID t, BarrierList *l) { tid=t; next=l; }
};

void
BlockBarrier :: enter()
{
    BarrierList *tmp, *tmp2;
    uval         ltotal;
    ThreadID     tid;

    ltotal = total;

    lock.acquire();

    /* make sure processes are not still waking up from previous barrier */
    while (downcount) {
	lock.release();
	//err_printf("Barrier: downcount %d\n", downcount);
	//usleep(5000);			// sleep for 5 ms
	Scheduler::DelayMicrosecs(5000);
	lock.acquire();
    }

    upcount++;

    if (upcount >= ltotal) {
        /* wakeup everyone */

        downcount = ltotal - 1;
        upcount = 0;

	for (tmp = list; tmp != 0; tmp = tmp2) {
	    tmp2 = tmp->next;
	    Scheduler::Unblock(tmp->tid);
	}
	list = 0;
	lock.release();
	return;
    }

    /*  else, we add ourselves to the queue. */

    tid = Scheduler::GetCurThread();
    BarrierList bl(tid, list);
    list = &bl;

    lock.release();

    do {
	//err_printf("Barrier: upcount %d\n", upcount);
	Scheduler::Block();
	// verify woken up because barrier completed
    } while (upcount != 0);

    lock.acquire();
    downcount--;
    lock.release();
}

BlockBarrier :: ~BlockBarrier()
{
    /* safely destroy by letting those left in the barrier to exit first
     * we assume that everyone has already been woken up, but just not
     * executed the code to leave the barrier
     */

    lock.acquire();
    while (downcount) {
	lock.release();
	//err_printf("~Barrier: downcount %d\n", downcount);
	//usleep(5000);			// sleep for 5 ms
	Scheduler::DelayMicrosecs(5000);
	lock.acquire();
    }
    lock.release();
}
