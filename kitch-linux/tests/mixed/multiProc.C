/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: multiProc.C,v 1.15 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Multiprocessor microbenchmarks
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>        // for exit();
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include <sys/systemAccess.H>
#include "bench.H"

extern "C" int pthread_setconcurrency(int level);

#define COUNT_NUM 5000l

int done = 0;

/////////////////////////////////////////////////////////////////////////////
// Test 7: Show scalability of the scheduler by having yielding threads
//         spread accross processors.

void *test7_thread(void *v_param)
{
    SystemSavedState saveArea;
    SystemEnter(&saveArea);
    VPNum origVP, curVP;
    origVP = Scheduler::GetVP();
    SystemExit(&saveArea);

    while (!done) {
	SystemEnter(&saveArea);
	curVP = Scheduler::GetVP();
	SystemExit(&saveArea);
	if (curVP != origVP) {
	    printf("Thread %ld Migrated to VP %d\n", (long)v_param, (int)curVP);
	    origVP = curVP;
	}

	DO_YIELD;
    }

    return NULL;
}

void test7_runchild()
{
    pthread_t thread;
    static int childnum = 1;
    if (pthread_create (&thread, NULL, test7_thread, (void *)(long)childnum++) != 0) {
	printf("Create failed!\n");
	exit(8);
    }

    printf("Waiting for migration...\n");
    // Give the migration manager a chance to move the child (or us)
    // to another processor
    for (int i = 0; i < 100000; i++) {
	sched_yield();
    }
    printf("...finished waiting.\n");
}

void test7()
{
    printf("########################################\n"
	   "# Test 7: Multi-processor yielding\n"
	   "#    -> Show that the schedulers on multiple processors are \n"
	   "#       independant by measuring yield times accross a bunch\n"
	   "#       of processors\n");

    int numchildren;
    TIMER_DECL(COUNT_NUM);

    printf("plot yield_multiproc 12 green\n");

    pthread_setconcurrency(12);

    for (numchildren = 0; numchildren < 12; numchildren++) {
	// Do it once to warm up the cache and page it in:
	DO_YIELD;

	START_TIMER;
	DO_YIELD;
	END_TIMER;

	printf("%d %f %f %f\n", numchildren + 1,
	       AVG_TIME, MIN_TIME, MAX_TIME);

	// Start another child process:
	test7_runchild();
    }

    done = 1;

    printf("xlabel Number of threads\n"
	   "ylabel Yield time (us)\n"
	   "title Yield time vs. number of threads\n"
	   "# All times averaged over %ld iterations\n", COUNT_NUM);
}

int
main (void)
{
    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    printf("###############################################################\n"
	   "# K42 Threading Microbenchmarks Part III\n"
	   "# \n"
	   "# Number of CPUs: %ld\n"
	   "# Ticks per second: %ld\n"
	   "###############################################################\n",
	   DREFGOBJ(TheProcessRef)->ppCount(),
	   (unsigned long)Scheduler::TicksPerSecond());


    test7();
    return 0;
}
