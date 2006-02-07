/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: yieldtest.C,v 1.15 2005/06/28 19:48:47 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/syscalls.H>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>

uval threadCount;

void
thread(uval id)
{
    for (uval i = 3*(id + 1); i > 0; i--) {
 	cprintf("thread %ld:  count %ld.\n", id, i);
 	Scheduler::Yield();
    }
    cprintf("thread %ld:  terminating.\n", id);
    threadCount--;	// FIXME - should be atomic.
}

int
main()
{
    NativeProcess();

    threadCount = 3;

    for (uval i = 0; i < threadCount; i++) {
 	cprintf("main:      spawning thread %ld\n", i);
	(void) Scheduler::ScheduleFunction(thread, i);
    }

    while (threadCount > 0) {
	cprintf("main:      threadCount %ld.\n", threadCount);
	Scheduler::Yield();
    }

    cprintf("main:      terminating.\n");
}
