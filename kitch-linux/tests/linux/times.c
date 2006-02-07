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
 * Module Description: test for times() system call
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>

/* Test that times() behaves correctly.  Return non-zero on error.  */
static int do_test(void)
{
    int i, result = 0;
    double j = 1.0;
    clock_t ticks;
    struct tms buf;

    /* Use at least some resources with a busy loop.  We make the
     * rand() call to ensure that an optimizing compiler cannot
     * replace the whole loop.  This is a difficult CS problem,
     * actually, how to make the below loop provably use at least some
     * resources, taking into account fast computers with nanosecond
     * granularity.  Polling time() does not work, since a hard
     * realtime process could take all the processor during the
     * interval.  The below works for now, make the upper bound bigger
     * when faster computers make that necessary.
     */
    for (i = 1; i < 1024 * 1024; i++) {
	j = j + (0.123 * (rand() % 3));
    }

    /* Fire off the system call.  */
    if ((ticks = times(&buf)) < 0) {
	fprintf(stderr, "FAIL: times returned error: %m\n");
	++result;
    }

    /* Dump some debugging information to stdout.  */
    printf ("i = %i, j = %f\n"
            "%li <- times(%p) = {\n"
            " .tms_utime  %li\n .tms_stime  %li\n"
	    " .tms_cutime %li\n .tms_cstime %li\n}\n",
	    i, j, ticks, &buf, buf.tms_utime, buf.tms_stime,
	    buf.tms_cutime, buf.tms_cstime);

    /* Our user and system time should not be zero.  */
    if (buf.tms_utime == 0 && buf.tms_stime == 0) {
	fprintf(stderr, "FAIL: utime and stime should not both be 0\n");
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
