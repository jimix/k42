/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: multiProcForkJoin.C,v 1.16 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Multiprocessor microbenchmarks
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include <sync/MPMsgMgr.H>
#include <sys/systemAccess.H>
#include "bench.H"

extern "C" int pthread_setconcurrency(int level);

static const uval COUNT_NUM=50000;

uval Done;
uval TotalChildren;
uval FinishedChildren;
double AvgTime[12];
double MinTime[12];
double MaxTime[12];

/////////////////////////////////////////////////////////////////////////////
// Test 8: Show scalability of the scheduler by having fork-join
//         spread across processors.

void *
test_thread8(void *v_param)
{
    return 0;
}

SysStatus test8_child(uval child)
{
    pthread_t thread;
    TIMER_DECL(COUNT_NUM);

    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    if (Scheduler::GetVP() != child) {
	printf("woops.\n");
    }

    (void) FetchAndAddSynced(&TotalChildren, 1);

    while (!Done) {
	while (FinishedChildren > 0) {
	    if (Done) {
		return 0;
	    }
	    DO_YIELD;
	}

	// Do it once to warm up the cache:
	if (pthread_create (&thread, NULL, test_thread8, NULL) != 0) {
	    printf("Create failed!\n");
	}
	if (pthread_join (thread, NULL) != 0) {
	    printf("Join failed!\n");
	}

	// Do a whole bunch of creates and joins and see how long they take
	START_TIMER;
	if (pthread_create (&thread, NULL, test_thread8, NULL) != 0) {
	    printf("Create failed!\n");
	}
	if (pthread_join (thread, NULL) != 0) {
	    printf("Join failed!\n");
	}
	END_TIMER;

	AvgTime[child] = AVG_TIME;
	MinTime[child] = MIN_TIME;
	MaxTime[child] = MAX_TIME;

	(void) FetchAndAddSynced(&FinishedChildren, 1);
    }
    return 0;
}

void test8()
{
    printf("########################################\n"
	   "# Test 8: Multi-processor yielding\n"
	   "#    -> Show that the schedulers on multiple processors are \n"
	   "#       independant by measuring yield times accross a bunch\n"
	   "#       of processors\n");

    uval numprocessors;
    uval numchildren;
    uval child, i;
    double sum, min, max;
    SysStatus rc;

    // Print out warning about nanosleep() not being finished once
    // before starting our timings:
    sleep(1);

    numprocessors = DREFGOBJ(TheProcessRef)->ppCount();

    pthread_setconcurrency(numprocessors);

    printf("plot forkjoin_multiproc %ld green\n", numprocessors);

    Done = 0;
    TotalChildren = 0;
    FinishedChildren = numprocessors;

    for (numchildren = 1; numchildren <= numprocessors; numchildren++) {
	child = numchildren - 1;

	// Start up a thread on a remote processor:
	rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
				     SysTypes::DSPID(0, VPNum(child)),
				     test8_child, child);
	if (rc != 0) {
	    printf("Error, SendAsyncUval() returned %lx\n", rc);
	    return;
	}

	// Wait for the child thread to start:
	while (TotalChildren < numchildren) {
	    sched_yield();
	}

	// Init our aray:
	for (i = 0; i < numchildren; i++) {
	    AvgTime[i] = 0;
	}

	// This is a barrier release: when we set this to 0 all the
	// children start working:
	FinishedChildren = 0;

	// Wait for the children to finish:
	while (FinishedChildren < TotalChildren) {
	    sleep(1);
	}

	sum = 0;
	min = MinTime[0];
	max = MaxTime[0];
	for (i = 0; i < numchildren; i++) {
	    sum += AvgTime[i];
	    if (min > MinTime[i]) {
		min = MinTime[i];
	    }
	    if (max > MaxTime[i]) {
		max = MaxTime[i];
	    }
	    printf("Child %ld took min %f us, max %f us, avg %f us, per fork/join.\n", i,
		   MinTime[i], MaxTime[i], AvgTime[i]);
	}

	printf("%ld %f %f %f\n", numchildren, sum / double(numchildren),
	       min, max);
    }

    Done = 1;

    printf("xlabel Number of threads\n"
	   "ylabel Fork-join time (us)\n"
	   "title Fork-join time vs. number of threads\n"
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


    test8();
    return 0;
}
