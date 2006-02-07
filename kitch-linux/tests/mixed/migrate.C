/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: migrate.C,v 1.15 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Time how long migration takes.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include <stub/StubWire.H>
#include <sys/systemAccess.H>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "bench.H"

// Note:  with a value of "50" this caused K42 to hang.  We need to debug this.
#define MAX_THREADS 50

double times[MAX_THREADS];

extern "C" int pthread_setconcurrency(int level);

void *
test_thread (void *v_param)
{
    uval thread_num = (uval)v_param;
    VPNum curVP;
    int done = 0;
    TIMER_DECL(1);

    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    curVP = Scheduler::GetVP();

    // Wait for all the threads to start:
    sleep(10);

    // Have the threads wake up one at a time:
    sleep(thread_num);

    // Loop around doing a whole bunch of nothing, consuming CPU.
    // Hopefully the migration manager will load balance these
    // threads:
    while (!done) {
	// Check for test end:
	pthread_testcancel();

	START_TIMER;
	SystemExit(&saveArea);
	sched_yield();
	SystemEnter(&saveArea);
	END_TIMER;

	if (curVP != Scheduler::GetVP()) {
	    times[thread_num] = AVG_TIME;
	    done = 1;
	}
    }
    return NULL;
}

void
migrate_test()
{
    printf("########################################\n"
	   "# Migration Test\n"
	   "#    -> Measures time taken for a thread to migrate from\n"
	   "#       one VP to another.  Prints individual times for\n"
	   "#       each thread (large outliers due to other processes\n"
	   "#       awakening during the migration, 0's due to threads that\n"
	   "#       do not migrate).\n");

    unsigned long count;
    pthread_t thread;
    int status;
    TIMER_DECL(5000);

    // warm up cache:
    Scheduler::SysTimeNow();

    // Since we are not doing a bunch of repetitions of each
    // migration, first find out if our timer overhead is significant
    // in our measurements:
    START_TIMER;
    Scheduler::SysTimeNow();
    END_TIMER;
    printf("Timer overhead = %f %f %f\n", AVG_TIME, MIN_TIME, MAX_TIME);

    // Create a whole bunch of threads:
    for (count = 0; count < MAX_THREADS; count++) {
	times[count] = 0;
	status = pthread_create (&thread, NULL, test_thread, (void *)count);
	if (status != 0) {
	    printf ("create status = %d, count = %lu: %s\n", status, count,
		    strerror (errno));
	    return;
	}
    }

    // Allow threads to start up and block migration:
    sleep(2);

    // Tell pthreads to use more than one VP:
    pthread_setconcurrency(2);

    // Go to sleep, allowing our threads to move around some:
    sleep(MAX_THREADS+20);

    // Print the results:
    for (count = 0; count < MAX_THREADS; count++) {
	printf("Thread %lu migrated in %f us\n", count, times[count]);
    }
}

int
main (void)
{
    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    printf("###############################################################\n"
	   "# K42 Threading Microbenchmarks -- migration\n"
	   "# \n"
	   "# Number of CPUs: %ld\n"
	   "# Ticks per second: %ld\n"
	   "###############################################################\n",
	   DREFGOBJ(TheProcessRef)->ppCount(),
	   (unsigned long)Scheduler::TicksPerSecond());

    // Stop interrupts for thinwire:
    printf("About to stop daemon...\n");
    StubWire::SuspendDaemon();
    printf("...stopped.\n");

    migrate_test();

    printf("About to start daemon...\n");
    StubWire::RestartDaemon();
    printf("...started.\n");

    return 0;
}
