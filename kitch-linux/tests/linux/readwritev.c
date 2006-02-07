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
 * Module Description: test for readv and writev system calls
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

static int do_test(void)
{
    int fd, i, bytes, result = 0;
    char fname[] = "tst-readwritev.XXXXXX";
    char out1[] = "123";
    char out2[] = "456789";
    char in1[strlen(out1)];
    char in2[strlen(out2)];
    struct iovec iov[2];

    /* Create a temporary file.  */
    if ((fd = mkstemp(fname)) == -1) {
	printf("mkstemp call failed: %m\n");
	return 1;
    }

    /* Set up our iovec structure for writing.  */
    iov[0].iov_base = out1;
    iov[0].iov_len = strlen(out1);
    iov[1].iov_base = out2;
    iov[1].iov_len = strlen(out2);

    /* Now write some data to the temporary file.  */
    if ((bytes = writev(fd, iov, 2)) != strlen(out1) + strlen(out2)) {
	printf("writev call returned %i: %m\n", bytes);
	return 1;
    }

    /* Close the file.  */
    if (close(fd)) {
	printf("close call failed: %m\n");
	return 1;
    }

    /* Reopen it.  */
    if ((fd = open(fname, O_RDONLY)) < 0) {
	printf("open call failed: %m\n");
	return 1;
    }

    /* Set up our iovec structure for reading.  */
    iov[0].iov_base = in1;
    iov[0].iov_len = strlen(out1);
    iov[1].iov_base = in2;
    iov[1].iov_len = strlen(out2);

    /* Clear the arrays.  */
    memset(in1, 0, strlen(out1));
    memset(in2, 0, strlen(out2));

    /* Try to read back the data we put in.  */
    if ((bytes = readv(fd, iov, 2)) != strlen(out1) + strlen(out2)) {
	printf("readv call returned %i: %m\n", bytes);
	return 1;
    }

    /* Close the file.  */
    if (close(fd)) {
	printf("close call failed: %m\n");
	return 1;
    }

    /* Did we get out what we put in for the first array?  */
    for (i = 0; i < strlen(out1); i++) {
	printf("%c %c\n", out1[i], in1[i]);
	if (out1[i] != in1[i]) {
	    printf("FAIL: bytes do not match\n");
	    return 1;
	}
    }

    /* Did we get out what we put in for the second array?  */
    for (i = 0; i < strlen(out2); i++) {
	printf("%c %c\n", out2[i], in2[i]);
	if (out2[i] != in2[i]) {
	    printf("FAIL: bytes do not match\n");
	    return 1;
	}
    }

    /* Delete the temporary file.  */
    if (unlink(fname)) {
	printf("unlink call failed: %m\n");
	return 1;
    }

    return result;
}

#ifdef _LIBC

/* If we are compiling inside glibc, use its regression test framework.  */
#define TEST_FUNCTION do_test ()
#include "test-skeleton.c"

#else

/* Otherwise, define our own entry point.  */
int main(int argc, char **argv)
{
    int result = do_test();

    /* Print result summary.  */
    if (result) {
	printf("FAIL: %i test failures\n", result);
    } else {
	printf("PASS: %i test failures\n", result);
    }

    return result;
}

#endif
