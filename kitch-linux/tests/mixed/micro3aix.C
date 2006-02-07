/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: micro3aix.C,v 1.5 2000/05/11 11:47:59 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/processor.h>
#include <sched.h>

#define COUNT_NUM 5000
#define ITERATIONS 50

/////////////////////////////////////////////////////////////////////////////
// Test 6: measure yield time between processes

pid_t pids[ITERATIONS];

void test6_runchild(int n)
{
    if((pids[n] = fork()) == 0) {
	if(execlp("./micro3_child", "micro3_child", NULL) != 0) {
	    printf("Exec failed, errno = %d.\n", errno);
	    exit(8);
	}
    }
    sleep(2);
}

void test6()
{
    printf("########################################\n"
	   "# Test 6: Inter-process yield time\n"
	   "#    -> Measures time taken to yield a VP when there are busy\n"
	   "#       VPs in other processes (checks how kernel\n"
	   "#       scheduler overhead scales with the number of processes.)\n"
	   "# [data in plotorX format]\n");
    int numchildren;
    int i;
    struct timeval before, after;
    int ret;

    printf("plot yield_vs_nprocs_aix %d green\n", ITERATIONS);

    for(numchildren = 0; numchildren < ITERATIONS; numchildren++) {
	// Do it once to warm up the cache and page it in:
	yield();

	gettimeofday(&before, NULL);
	for(i = 0; i < COUNT_NUM; i++) {
	    yield();
	}
	gettimeofday(&after, NULL);

	// Start another child process:
	test6_runchild(numchildren);

	long time = after.tv_usec - before.tv_usec +
	    (after.tv_sec - before.tv_sec) * 1000000l;

	printf("%d %ld\n", numchildren+1, (time) / (COUNT_NUM));

    }

    for(numchildren = 0; numchildren < ITERATIONS; numchildren++) {
	kill(pids[numchildren], SIGINT);
    }

    printf("xlabel Number of processes\n"
	   "ylabel Yield time (us)\n"
	   "title Yield time vs. number of processes\n"
	   "# All times averaged over %ld iterations\n", COUNT_NUM);
}

int
main (void)
{
    printf("##############################################################\n"
	   "# K42 Threading Microbenchmarks Part III\n"
	   "# AIX Run\n"
	   "##############################################################\n");

    if(bindprocessor(BINDPROCESS, getpid(), 0) != 0) {
	printf("Bindprocessor failed.\n");
	return 8;
    }

    test6();
    return 0;
}
