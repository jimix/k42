/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: microaix.C,v 1.5 2000/05/11 11:47:59 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/processor.h>

#define COUNT_NUM 5000

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
    int i;
    struct timeval before, after;

    // Do it once to warm up the cache:
    (void)pthread_create (&thread, NULL, test_thread1, NULL);
    (void)pthread_join (thread, NULL);

    gettimeofday(&before, NULL);
    for(i = 0; i < COUNT_NUM; i++) {
	// Ignore return codes so that we don't measure the overhead
	// of checking them:
	(void)pthread_create (&thread, NULL, test_thread1, NULL);
	(void)pthread_join (thread, NULL);
    }
    gettimeofday(&after, NULL);

    long time = after.tv_usec - before.tv_usec +
	(after.tv_sec - before.tv_sec) * 1000000l;

    printf("Total time: %ld\n"
	   "Iterations: %ld\n"
	   "Time per iteration: %ld <===\n",
	   time,
	   COUNT_NUM,
	   (time) / (COUNT_NUM));
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

    struct timeval before, after;
    int i;

    // Warm up the cache:
    if(system("./nullProg") != 0) {
        printf("System failed\n");
        return;
    }

    gettimeofday(&before, NULL);
    for(i = 0; i < 50; i++) {
        system("./nullProg");
    }
    gettimeofday(&after, NULL);

    long time = after.tv_usec - before.tv_usec +
        (after.tv_sec - before.tv_sec) * 1000000l;

    printf("Total time: %ld\n"
	   "Iterations: %ld\n"
	   "Time per iteration: %ld <===\n",
	   time,
	   TEST2_ITERATIONS,
	   (time) / (TEST2_ITERATIONS));
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
    int i, length;
    struct timeval before, after;
    int rc;

    printf("Test3: sched_yield\n"
	   "plot yield_vs_queue %d green\n", MAX_LEN);
    done = 0;
    for(length = 0; length < MAX_LEN; length++) {
	// Do it once to warm up the cache:
	sched_yield();

	gettimeofday(&before, NULL);
	for(i = 0; i < COUNT_NUM; i++) {
	    sched_yield();
	}
	gettimeofday(&after, NULL);

	long time = after.tv_usec - before.tv_usec +
	    (after.tv_sec - before.tv_sec) * 1000000l;

	printf("%d %ld\n", length + 1, (time) / (COUNT_NUM));

	// Spawn a thread to cause contention:
	if((rc = pthread_create(&threads[length], NULL, test_thread3, NULL)) != 0) {
	    printf("Error creating thread, rc = %d\n", rc);
	}
    }
    done = 1;

    printf("xlabel Number of threads\n"
	   "ylabel Yield time (us)\n"
	   "title sched_yield() time vs. queue length\n"
	   "# All times averaged over %ld iterations\n", COUNT_NUM);

    for(length = 0; length < MAX_LEN; length++) {
	if((rc = pthread_join(threads[length], NULL)) != 0) {
	    printf("Error joining with thread %d, rc = %d\n", length, rc);
	}
    }
}

int
main (void)
{
    printf("##############################################################\n"
	   "# K42 Threading Microbenchmarks\n"
	   "# AIX Run\n"
	   "##############################################################\n");

    if(bindprocessor(BINDPROCESS, getpid(), 0) != 0) {
	printf("Bindprocessor failed.\n");
	return 8;
    }

    test1();
    test2();
    test3();
    return 0;
}
