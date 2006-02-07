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
 * Module Description: stress test
 * **************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>

static int do_test(void)
{
    int i, fd, max = 64, result = 0;
    char c, k = 'K';
    long delta;
    struct timeval before, after;

    /* Get the time before the loop starts.  */
    if (gettimeofday(&before, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Start the stress loop.  */
    for (i = 0; i <= max; i++) {
	char fname[] = "stress.XXXXXX";

	/* First create a temporary file.  */
	if ((fd = mkstemp(fname)) == -1) {
	    printf("mkstemp call failed: %m\n");
	    return 1;
	}

	/* Now write a byte to it.  */
	if (write(fd, &k, 1) != 1) {
	    printf("write call failed: %m\n");
	    return 1;
	}

	/* Now close the file descriptor.  */
	close(fd);

	/* Open the file again.  */
	if ((fd = open(fname, O_RDONLY)) == -1) {
	    fprintf(stderr, "open() failed: %m\n");
	    ++result;
	    break;
	}

	/* Now read a byte from it. */
	if (read(fd, &c, 1) != 1) {
	    fprintf(stderr, "read() failed: %m\n");
	    ++result;
	    break;
	}

	/* Check that we got back what we put in.  */
	if (c != k) {
	    printf("%c != %c\n", c, k);
	    ++result;
	    break;
	}

	/* Fork off a child.  */
	if (fork() == 0) {
	    exit(0);
	}

	/* Wait for the child. */
	if (wait(NULL) < 0) {
	    printf("wait call failed: %m\n");
	    return 1;
	}

	/* Now close the file descriptor.  */
	close(fd);

	/* Now unlink the file.  */
	if (unlink(fname) != 0) {
	    printf("unlink call failed: %m\n");
	    return 1;
	}

	/* Print status update if appropriate.  */
	if (i == 0) {
	    printf("%-8s %-10s %-8s\n", "TESTS", "TIME", "RATE");
	} else if (i % 8 == 0) {
	    /* Get the current time.  */
	    if (gettimeofday(&after, NULL)) {
		printf("gettimeofday call failed: %m\n");
		return 1;
	    }

	    /* Calculate the elapsed time in nanoseconds.  */
	    delta = ((after.tv_sec * 1000000000) + (after.tv_usec * 1000)) -
	      ((before.tv_sec * 1000000000) + (before.tv_usec * 1000));

	    /* Print the running performance number.  */
	    printf("%-8i %-10lu %-8f\n", i, delta, 
		   i / ((double)delta / 1000000000));
	}
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
