/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: micro.C,v 1.15 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <misc/hardware.H>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "bench.H"

#define COUNT_NUM 5000l

extern "C" int pthread_setconcurrency(int level);
extern "C" int __k42_linux_spawn(const char *,
				 char *const [], char *const [],
				 int);


/////////////////////////////////////////////////////////////////////////////
// Test 1:  measure fork-join time.

void *
test_thread1(void *v_param)
{
    return 0;
}

void test1()
{
    printf("########################################\n"
	   "# Test 1: thread fork-join time\n"
	   "#    -> Measures time taken to do a pthread_create() followed\n"
	   "#       by a pthread_join()\n");

    pthread_t thread;
    TIMER_DECL(COUNT_NUM);

    // Do it once to warm up the cache:
    (void)pthread_create (&thread, NULL, test_thread1, NULL);
    (void)pthread_join (thread, NULL);

    START_TIMER;
    // Ignore return codes so that we don't measure the overhead
    // of checking them:
    (void)pthread_create (&thread, NULL, test_thread1, NULL);
    (void)pthread_join (thread, NULL);
    END_TIMER;

    printf("Min: %f us\n"
	   "Max: %f us\n"
	   "Avg %f us\n"
	   "Num Iter %ld\n",
	   MIN_TIME, MAX_TIME, AVG_TIME, COUNT_NUM);
}

/////////////////////////////////////////////////////////////////////////////
// Test 2: measure program launch time

void test2()
{
    printf("########################################\n"
	   "# Test 2: program launch time\n"
	   "#    -> Measures time taken to exec a program that immediately\n"
	   "#       returns\n");

#define TEST2_ITERATIONS 50l

    TIMER_DECL(TEST2_ITERATIONS);
    char *fakeEnvp[] = {
	NULL};
    char *argv[] = { "/bench/nullProg", NULL };
    int ret;

    // Do it once to warm up the cache and page it in:
    ret = __k42_linux_spawn(argv[0], argv, fakeEnvp, 1);

    START_TIMER;
    ret = __k42_linux_spawn(argv[0], argv, fakeEnvp, 1);
    END_TIMER;

    printf("Min: %f us\n"
	   "Max: %f us\n"
	   "Avg %f us\n"
	   "Num Iter %ld\n",
	   MIN_TIME, MAX_TIME, AVG_TIME, COUNT_NUM);
}

/////////////////////////////////////////////////////////////////////////////
// Test 3: measure sched_yield time vs. queue length

volatile int done;

void *
test_thread3(void *v_param)
{
    while(done == 0) {
	sched_yield();
    }
    return 0;
}

void test3()
{
    printf("########################################\n"
	   "# Test 3: thread yield time\n"
	   "#    -> Measures time taken to perform a thread yield between\n"
	   "#       threads on a single VP (user level scheduler only)\n"
	   "# [data in plotorX format]\n");

#define MAX_LEN 50
    pthread_t threads[MAX_LEN];
    VPNum vpCnt;
    TIMER_DECL(COUNT_NUM);
    int rc;

    printf("plot yield_vs_queue %d green\n", MAX_LEN);
    done = 0;
    for(vpCnt = 0; vpCnt < MAX_LEN; vpCnt++) {
	// Do it once to warm up the cache:
	sched_yield();

	START_TIMER;
	sched_yield();
	END_TIMER;

	printf("%ld %f %f %f\n", vpCnt + 1, AVG_TIME, MIN_TIME, MAX_TIME);

	// Spawn a thread to cause contention:
	if((rc = pthread_create(&threads[vpCnt], NULL, test_thread3, NULL)) != 0) {
	    printf("Error creating thread, rc = %d\n", rc);
	}
    }
    done = 1;

    printf("xlabel Number of threads\n"
	   "ylabel Yield time (us)\n"
	   "title sched_yield() time vs. queue length\n"
	   "# All times averaged over %ld iterations\n", COUNT_NUM);

    for(vpCnt = 0; vpCnt < MAX_LEN; vpCnt++) {
	if((rc = pthread_join(threads[vpCnt], NULL)) != 0) {
	    printf("Error joining with thread %ld, rc = %d\n", vpCnt, rc);
	}
    }
}

/////////////////////////////////////////////////////////////////////////////
// Test 4: Measure yield times between idle VPs in the same process.
void test4()
{
    printf("########################################\n"
	   "# Test 4: Idle VP yield time\n"
	   "#    -> Measures time taken to yield a VP when there are idle\n"
	   "#       VPs in the same process (checks whether kernel\n"
	   "#       scheduler has any overhead for idle VPs.)\n");

    int rc;
    VPNum vpCnt;
    TIMER_DECL(COUNT_NUM);

    sleep(2);

    printf("plot yield_vs_numvp %ld yellow\n", Scheduler::VPLimit);

    if(DREFGOBJ(TheProcessRef)->ppCount() > 1) {
	printf("WARNING: test results not valid if "
	       "physical processors > 1.\n");
    }

    for(vpCnt = 0; vpCnt < Scheduler::VPLimit; vpCnt++) {
	// Set the number of VPs:
	rc = pthread_setconcurrency(vpCnt + 1);

	if (rc != 0) {
	    printf("Error, pthread_setconcurrency() returned %d\n", rc);
	    break;
	}

	// Do it once to warm up the cache:
	Scheduler::YieldProcessor();

	START_TIMER;
	Scheduler::YieldProcessor();
	END_TIMER;

	printf("%ld %f %f %f\n", vpCnt + 1, AVG_TIME, MIN_TIME, MAX_TIME);
    }

    // NOTE: at the end of this test case we have many more VPs active.

    printf("xlabel Number of VPs\n"
	   "ylabel Yield time (us)\n"
	   "title Scheduler::YieldProcessor() time vs. number of VPs\n"
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
	   "# K42 Threading Microbenchmarks\n"
	   "# \n"
	   "# Number of CPUs: %ld\n"
	   "# Ticks per second: %ld\n"
	   "###############################################################\n",
	   DREFGOBJ(TheProcessRef)->ppCount(),
	   (unsigned long)Scheduler::TicksPerSecond());

    test1();
    test2();
    test3();
    test4(); // Must be last test case, since it leaves a bunch of VPs active.
    return 0;
}
