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
 * Module Description: test for pwrite system call
 * **************************************************************************/

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

static int do_test(void)
{
    int fd, result = 0;
    char c = 'Z';
    char fname[] = "pwrite.XXXXXX";
    long long max = 0xABCDABCDELL;

    /* First create a temporary file.  */
    if ((fd = mkstemp(fname)) == -1) {
	printf("mkstemp call failed: %m\n");
	return 1;
    }

    printf("\n--> %i %p %i %llx\n", fd, &c, 1, max);

    /* Now write a byte to it.  */
    if (pwrite(fd, &c, 1, max) != 1) {
	printf("write call failed: %m\n");
	return 1;
    }

    /* Now close the file descriptor.  */
    close(fd);

    /* Now unlink the file.  */
    if (unlink(fname) != 0) {
	printf("unlink call failed: %m\n");
	return 1;
    }

    return result;
}

#define TEST_FUNCTION do_test ()

/* Replace main() with `#include "test-skeleton.c"' to fit in glibc.  */
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
