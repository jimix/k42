/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: contend.C,v 1.18 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include <stub/StubTestScheduler.H>
#include <sys/systemAccess.H>
#include "bench.H"
#include <stdio.h>

void contend_runchild(uval bg_domain, char **argv)
{
    char *envp[] = { NULL };
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

uval Iteration()
{
    uval i, sum;
    sum = 0;
    for (i = 0; i < 100000; i++) sum += i;
    return sum;
}

#define COUNT_NUM 5000l

void contend(char **argv)
{
    printf("########################################\n"
	   "# Contention benchmark: Inter- and intra-domain contention\n"
	   "#    -> Runs a varying number of background processes in\n"
	   "#       specified domains, and times a spin loop (checks that\n"
	   "#       domains are scheduled properly, measures overhead)\n"
	   "# [data in plotorX format]\n");

    uval numchildren;
    TIMER_DECL(COUNT_NUM);

    uval const num_bg_progs = atoi(argv[1]);
    uval const bg_domain = atoi(argv[2]);
    char *const bg_prog = argv[3];

    printf("# count %ld, num_bg_progs %ld, bg_domain %ld, bg_prog \"%s\".\n",
	   COUNT_NUM, num_bg_progs, bg_domain, bg_prog);

    printf("plot elapsed_vs_nbgprogs %ld green\n", num_bg_progs + 1);

    for (numchildren = 0; numchildren <= num_bg_progs; numchildren++) {
	// Do it once to warm up the cache and page it in:
	(void) Iteration();

	START_TIMER;
	(void) Iteration();
	END_TIMER;

	printf("%ld %f %f %f\n", numchildren, AVG_TIME, MIN_TIME, MAX_TIME);

	// Start another child process:
	contend_runchild(bg_domain, argv+4);

    }

    printf("xlabel Number of background processes\n"
	   "ylabel Iteration time (us)\n"
	   "title Iteration time vs. number of processes\n"
	   "# All times averaged over %ld iterations\n", COUNT_NUM);
}

int
main(int argc, char **argv)
{
    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    printf("###############################################################\n"
	   "# K42 CPU Contention Benchmark\n"
	   "# \n"
	   "# Number of CPUs: %ld\n"
	   "# Ticks per second: %ld\n"
	   "###############################################################\n",
	   DREFGOBJ(TheProcessRef)->ppCount(),
	   (unsigned long)Scheduler::TicksPerSecond());


    if (argc < 5) {
	printf("Usage: %s <iters> <bg_cnt> <bg_domain> <bg_prog> ...\n",
	       argv[0]);
	return -1;
    }
    contend(argv);
    return 0;
}
