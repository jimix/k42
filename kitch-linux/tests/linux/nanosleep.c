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
 * Module Description: test for nanosleep() system call
 * **************************************************************************/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

/* Test that nanosleep() behaves correctly.  Return non-zero on error.  */
static int do_test(void)
{
    int result = 0;
    const int seconds = 1, nanoseconds = 13;
    long delta, desired;
    struct timespec req, rem;
    struct timeval before, after;

    /* Prepare our request structure.  */
    req.tv_sec = seconds;
    req.tv_nsec = nanoseconds;

    /* Get the time before calling nanosleep.  */
    if (gettimeofday(&before, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Send off the sleep request, asking to be notified if interrupted.  */
    if (nanosleep(&req, &rem)) {
	printf("FAIL: nanosleep(%p, %p) returned error: %m\n", &req, &rem);
	result++;
    }

    /* Get the time after calling nanosleep.  */
    if (gettimeofday(&after, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Calculate in nanoseconds how long we slept.  */
    delta = ((after.tv_sec * 1000000000) + (after.tv_usec * 1000)) -
	((before.tv_sec * 1000000000) + (before.tv_usec * 1000));

    /* Calculate in nanoseconds how long we should have slept.  */
    desired = (seconds * 1000000000) + nanoseconds;

    /* Print status information to stdout.  */
    printf("actual sleep: %lins, desired sleep: %lins\n", delta, desired);

    /* Did we sleep for at least the time we requested?  */
    if (delta < desired) {
	printf("FAIL: did not sleep for at least %lins\n", desired);
	++result;
    }

    /* Print result summary.  */
    if (result) {
	printf("FAIL: %i test failures\n", result);
    } else {
	printf("PASS: %i test failures\n", result);
    }

    return result;
}

#define TEST_FUNCTION do_test ()

/* Replace main() with `#include "test-skeleton.c"' to fit in glibc.  */
int main(int argc, char **argv)
{
    return do_test();
}
