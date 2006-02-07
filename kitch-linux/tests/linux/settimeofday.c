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
 * Module Description: test for settimeofday() system call
 * **************************************************************************/

#include <stdio.h>
#include <sys/time.h>

/* Test that settimeofday() behaves correctly.  Return non-zero on error.  */
static int do_test(void)
{
    int result = 0, ret;
    struct timeval then;

    /* Set time to near the epoch.  */
    then.tv_sec = 1;
    then.tv_usec = 13;

    /* Fire off the syscall.  */
    if ((ret = settimeofday(&then, NULL)) < 0) {
	fprintf(stderr, "FAIL: settimeofday returned %d: %m\n", ret);
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
