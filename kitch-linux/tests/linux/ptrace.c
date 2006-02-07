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
 * Module Description: test for ptrace system call
 * **************************************************************************/

#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

static int do_test(void)
{
    int result = 0;
    int pid, status, ret;

    if ((pid = fork()) == 0) {
	/* We are the child.  First call ptrace.  */
	if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
	    fprintf(stderr, "FAIL: child ptrace error: %m\n");
	    exit(1);
	}

	/* Now, instead of calling exec, raise SIGTRAP by hand.  */
	if (raise(SIGTRAP)) {
	    fprintf(stderr, "FAIL: child raise error: %m\n");
	    exit(1);
	}

	printf("PASS: child %i forked\n", getpid());
	exit(0);
    } else if (pid < 0) {
	fprintf(stderr, "FAIL: fork error: %m\n");
	++result;
    }

    if ((pid = wait(&status)) > 0) {
	if (WIFEXITED(status)) {
	    if ((ret = WEXITSTATUS(status)) == 0) {
		fprintf(stderr, "FAIL: child exited normally\n");
	    } else {
		fprintf(stderr, "FAIL: chld exited with: %i\n", ret);
	    }
	    ++result;
	} else if (WIFSIGNALED(status)) {
	    if ((ret = WTERMSIG(status)) == SIGTRAP) {
		fprintf(stderr, "FAIL: child got SIGTRAP but did not stop\n");
	    } else {
		fprintf(stderr, "FAIL: child got %i but did not stop\n", ret);
	    }
	    ++result;
	} else if (WIFSTOPPED(status)) {
	    if ((ret = WSTOPSIG(status)) == SIGTRAP) {
		printf("PASS: child %i stopped by SIGTRAP as expected\n", pid);
	    } else {
		fprintf(stderr, "FAIL: child %i stopped by: %i\n", pid, ret);
	    }
	} else {
	    fprintf(stderr, "FAIL: panic: should not be here!\n");
	    ++result;
	}
    } else {
	fprintf(stderr, "FAIL: error waiting for child: %m\n");
	++result;
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
