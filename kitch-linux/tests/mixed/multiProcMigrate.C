/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: multiProcMigrate.C,v 1.16 2005/06/28 19:44:15 rosnbrg Exp $
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
#include <sys/systemAccess.H>
#include "bench.H"
#include <sys/errno.h>

extern "C" int pthread_setconcurrency(int level);

// Can't make this too big or we run out of threads: (ie, 2 is too big
// right now...)
#define COUNT_NUM 1

/////////////////////////////////////////////////////////////////////////////
// Test 9: Show that migration works by having a bunch of threads do a fixed
//         quantity of work, and see if increasing the number of threads increases
//	   execution time.

void *test9_child(void *childArg)
{
    uval i;
    volatile uval j = 0;
    // This # of iterations takes about 2.5s on kitch13
    for (i = 0; i < 50000000l; i++) {
	j++;
    }

    return childArg;
}

void test9()
{
    printf("########################################\n"
	   "# Test 9: Multi-processor yielding\n"
	   "#    -> Show that migration works by having a bunch of threads\n"
	   "#       do a fixed quantity of work, and see if increasing the\n"
	   "#       number of threads increases execution time.\n");

    uval numprocessors;
    uval numchildren;
    pthread_t thread[12];
    uval i;

    numprocessors = DREFGOBJ(TheProcessRef)->ppCount();
    pthread_setconcurrency(numprocessors);

    printf("plot migrate_multiproc %ld green\n", numprocessors);

    for (numchildren = 1; numchildren <= numprocessors; numchildren++) {

	TIMER_DECL(COUNT_NUM);

	START_TIMER;
	for (i = 0; i < numchildren; i++) {
	    if (pthread_create(&thread[i], NULL,
			       test9_child, (void *) i) != 0) {
		printf("Create failed!\n");
		return;
	    }
	}

	void * retval;
	for (i = 0; i < numchildren; i++) {
	    int rcode = pthread_join(thread[i], &retval);
	    if (rcode != 0) {
		printf("Join failed! (rcode %d, errno %d)\n", rcode, errno);
		return;
	    }
	    if ((uval)retval != i) {
		printf("Join failed! %ld returned %ld.\n", i, (uval)retval);
	    }
	}
	END_TIMER;

	printf("%ld %f %f %f\n", numchildren, AVG_TIME, MIN_TIME, MAX_TIME);
    }

    printf("xlabel Number of threads\n"
	   "ylabel Total run time (us)\n"
	   "title Purely parallel workload scaling\n"
	   "# All times averaged over %d iterations\n", COUNT_NUM);
}

int
main (void)
{
    printf("###############################################################\n"
	   "# K42 Threading Microbenchmarks Part IV\n"
	   "# \n"
	   "# Number of CPUs: %ld\n"
	   "# Ticks per second: %ld\n"
	   "###############################################################\n",
	   DREFGOBJ(TheProcessRef)->ppCount(),
	   (unsigned long)Scheduler::TicksPerSecond());

    test9();
    return 0;
}
