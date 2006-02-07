/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: micro3.C,v 1.20 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <io/FileLinux.H>
#include <sys/systemAccess.H>
#include "bench.H"

#define COUNT_NUM 5000l

/////////////////////////////////////////////////////////////////////////////
// Test 6: measure yield time between processes

void test6_runchild()
{
    char *envp[] = {NULL};
    char *argv[] = { "/bench/micro3_child", NULL };
    SysStatus rc;

    ProgExec::ArgDesc *args;
    rc = ProgExec::ArgDesc::Create(argv[0], argv, envp, args);
    if (_SUCCESS(rc)) rc = runExecutable(args, 0);
    args->destroy();

    if (_FAILURE(rc)) {
	printf("%s [%ld]: Command not found\n",
		   argv[0], _SGENCD(rc));
	return;
    }

    sleep(8);
}

void test6()
{
    printf("########################################\n"
	   "# Test 6: Inter-process yield time\n"
	   "#    -> Measures time taken to yield a VP when there are busy\n"
	   "#       VPs in other processes (checks how kernel\n"
	   "#       scheduler overhead scales with the number of processes.)\n");

    int numchildren;
    TIMER_DECL(COUNT_NUM);

    printf("plot yield_vs_nprocs 50 green\n");

    for (numchildren = 0; numchildren < 50; numchildren++) {
	// Do it once to warm up the cache and page it in:
	Scheduler::YieldProcessor();

	START_TIMER;
	Scheduler::YieldProcessor();
	END_TIMER;

	// Start another child process:
	test6_runchild();

	printf("%d %f %f %f\n", numchildren + 1,
	       AVG_TIME, MIN_TIME, MAX_TIME);
    }

    printf("xlabel Number of processes\n"
	   "ylabel Yield time (us)\n"
	   "title Yield time vs. number of processes\n"
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


    test6();
    return 0;
}
