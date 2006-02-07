/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: multiProcaix.C,v 1.5 2000/05/11 11:48:00 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Multiprocessor microbenchmarks
 * **************************************************************************/
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/processor.h>

#define COUNT_NUM 5000l

#define TIMER_DECL struct timeval before, after
#define START_TIMER gettimeofday(&before, NULL)
#define END_TIMER gettimeofday(&after, NULL)
#define DO_YIELD sched_yield()
#define DELTA_TIME (after.tv_usec - before.tv_usec + (after.tv_sec - before.tv_sec) * 1000000l)

int done = 0;

/////////////////////////////////////////////////////////////////////////////
// Test 7: Show scalability of the scheduler by having yielding threads
//         spread accross processors.

void *test7_thread(void *v_param)
{
    if(bindprocessor(BINDTHREAD, thread_self(), (cpu_t)v_param) != 0) {
	printf("Error binding thread to processor %d, errno = %d.\n",
	       (int)v_param, errno);
    }

    while(!done) {
	DO_YIELD;
    }

    return NULL;
}

void test7_runchild()
{
    pthread_t thread;
    static int childnum = 1;
    if(pthread_create (&thread, NULL, test7_thread, (void *)childnum++) != 0) {
	printf("Create failed!\n");
	exit(8);
    }

    // Give the child a chance to start:
    sleep(2);
}

void test7()
{
    printf("########################################\n"
	   "# Test 7: Multi-processor yielding\n"
	   "#    -> Show that the schedulers on multiple processors are \n"
	   "#       independant by measuring yield times accross a bunch\n"
	   "#       of processors\n");

    int numchildren;
    int i;
    TIMER_DECL;

    printf("plot yield_multiproc 12 green\n");

    for(numchildren = 0; numchildren < 12; numchildren++) {
	// Do it once to warm up the cache and page it in:
	DO_YIELD;

	START_TIMER;
	for(i = 0; i < COUNT_NUM; i++) {
	    DO_YIELD;
	}
	END_TIMER;

	printf("%d %ld\n", numchildren+1, (DELTA_TIME / (COUNT_NUM)));

	// Start another child process:
	if(numchildren != 11) {
	    test7_runchild();
	}
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
    printf("##############################################################\n"
	   "# K42 Threading Microbenchmarks Part III\n"
	   "# AIX run\n"
	   "##############################################################\n");

    test7();
    return 0;
}
