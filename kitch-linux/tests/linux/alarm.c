/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test for alarm() system call
 * **************************************************************************/

#include <time.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

/* Global variable which is modified by our signal hander.  */
volatile int x;

/* Signal handler whose job is to set global variable to 0.  */
void handler(int val)
{
    printf("signal handler called, setting x to 0 ...\n");

    x = 0;
}

/* Test that alarm() behaves correctly.  Return non-zero on error.  */
static int do_test(int seconds, long delay, long tolerance)
{
    int result = 0;
    long i, delta, desired;
    struct timespec req;
    struct timeval before, after;

    /* Set the global variable to non-zero, signal handler will reset.  */
    x = 1;

    /* First install our signal handler.  */
    if (signal(SIGALRM, handler) == SIG_ERR) {
	printf("signal call failed: %m\n");
	return 1;
    }
     
    /* Get the time before calling alarm.  */
    if (gettimeofday(&before, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Send off the alarm request, asking to be notified if interrupted.  */
    if (alarm(seconds)) {
	printf("FAIL: alarm(%d) returned error: %m\n", seconds);
	return 1;
    }

    /* Prepare our request structure.  */
    req.tv_sec = 0;
    req.tv_nsec = delay;

    /* Poll the global variable.  */
    for (i = 0; x; i++) {
	printf("sleeping for %lins for the %lith time\n", req.tv_nsec, i);
	nanosleep(&req, NULL);
    }

    /* Get the time after the alarm signal handler fired.  */
    if (gettimeofday(&after, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Calculate in nanoseconds how long we slept.  */
    delta = ((after.tv_sec * 1000000000) + (after.tv_usec * 1000)) -
	((before.tv_sec * 1000000000) + (before.tv_usec * 1000));

    /* Calculate in nanoseconds how long we should have slept.  */
    desired = seconds * 1000000000;

    /* Print status information to stdout.  */
    printf("actual alarm: %lins, desired alarm: %lins\n", delta, desired);

    /* Check that alarm was within our tolerance.  */
    if (delta < (desired - tolerance) || delta > (desired + tolerance)) {
	printf("FAIL: alarm was outside %lins tolerance\n", tolerance);
	++result;
    }

    /* Print result summary.  */
    if (result) {
	printf("FAIL: %i test failures\n", result);
    } else {
	printf("PASS: %i returned\n", result);
    }

    return result;
}

#define TEST_FUNCTION do_test ()

/* Replace main() with `#include "test-skeleton.c"' to fit in glibc.  */
int main(int argc, char **argv)
{
  /* Set 2s alarm, polling global variable in 100ms intervals,
     tolerating signal delivery offsets of plus or minus 10ms.  */
    return do_test(2, 100 * 1000000, 10 * 1000000);
}
