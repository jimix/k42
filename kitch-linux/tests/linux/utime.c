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
 * Module Description: test for utime() system call
 * **************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>

/* Return 1 if `path' is on an NFS filesystem, return 0 otherwise.  */
static int is_on_nfs(char *path)
{
    const int NFS_SUPER_MAGIC = 0x6969;
    struct statfs buf;

    if (statfs(path, &buf) == -1) {
	fprintf(stderr, "statfs returned error: %m\n");
	return 1;
    }

    return buf.f_type == NFS_SUPER_MAGIC;
}

/* Test that utime() behaves correctly.  Return non-zero on error.  */
static int do_test(int do_utimes)
{
    char fname[] = "tst-utime.XXXXXX";
    struct timeval now, before, after;
    struct stat buf;
    struct utimbuf past;
    struct timeval times[2];
    int result = 0;

    /* First create a temporary file.  */
    if (mkstemp(fname) == -1) {
	printf("mkstemp call failed: %m\n");
	return 1;
    }

    /* Get the current time.  */
    if (gettimeofday(&now, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Set up utimes data structure to refer to a time in the past.  */
    times[0].tv_sec = 13;
    times[0].tv_usec = 0;
    times[1].tv_sec = 101;
    times[1].tv_usec = 0;

    /* Make the utime data structure mirror the above.  */
    past.actime = times[0].tv_sec;
    past.modtime = times[1].tv_sec;

    /* Now set file access and modification times equal to time in past.  */
    if (do_utimes) {
	if (utimes(fname, times)) {
	    printf("utimes call failed: %m\n");
	    return 1;
	}
    } else {
	if (utime(fname, &past)) {
	    printf("utime call failed: %m\n");
	    return 1;
	}
    }

    /* Get current file information now that utime has updated it.  */
    if (stat(fname, &buf)) {
	printf("stat call failed: %m\n");
	return 1;
    }

    /* Dump some debugging information to stdout.  */
    printf
	("%li stat(%s)\n .st_atime %li\n .st_mtime %li\n .st_ctime %li\n",
	 now.tv_sec, fname, buf.st_atime, buf.st_mtime, buf.st_ctime);

    /* Does the access time of the file now equal the time in the past?  */
    if (buf.st_atime != past.actime) {
	printf("FAIL: stat(%s).st_atime = %li, but should be = %li\n",
	       fname, buf.st_atime, past.actime);
	++result;
    }

    /* Does the modify time of the file now equal the time in the past?  */
    if (buf.st_mtime != past.modtime) {
	printf("FAIL: stat(%s).st_mtime = %li, but should be = %li\n",
	       fname, buf.st_mtime, past.modtime);
	++result;
    }

    /* Does the change time of the file now equal the current time?  */
    if (buf.st_ctime < now.tv_sec) {
	printf("FAIL: stat(%s).st_ctime = %li, but should be >= %li\n",
	       fname, buf.st_ctime, now.tv_sec);
	++result;
    }

    /* Save the timestamp just before calling utime again.  */
    if (gettimeofday(&before, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Now return the access and modification times to the current time.  */
    if (do_utimes) {
	if (utimes(fname, NULL)) {
	    printf("utimes call failed: %m\n");
	    return 1;
	}
    } else {
	if (utime(fname, NULL)) {
	    printf("utime call failed: %m\n");
	    return 1;
	}
    }

    /* Save the timestamp just after calling utime again.  */
    if (gettimeofday(&after, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    /* Get current file information now that utime has updated it.  */
    if (stat(fname, &buf)) {
	printf("stat call failed: %m\n");
	return 1;
    }

    /* Dump some debugging information to stdout.  */
    printf
	("%li stat(%s)\n .st_atime %li\n .st_mtime %li\n .st_ctime %li\n",
	 after.tv_sec, fname, buf.st_atime, buf.st_mtime, buf.st_ctime);

    /* Did utime set file access time to before first timestamp?  */
    if (buf.st_atime < before.tv_sec) {
	printf("FAIL: stat(%s).st_atime = %li, but should be >= %li\n",
	       fname, buf.st_atime, before.tv_sec);
	++result;
    }

    /* Did utime set file modify time to before first timestamp?  */
    if (buf.st_mtime < before.tv_sec) {
	printf("FAIL: stat(%s).st_mtime = %li, but should be >= %li\n",
	       fname, buf.st_mtime, before.tv_sec);
	++result;
    }

    /* Did utime set file change times to before first timestamp?  */
    if (buf.st_ctime < before.tv_sec) {
	printf("FAIL: stat(%s).st_ctime = %li, but should be >= %li\n",
	       fname, buf.st_ctime, before.tv_sec);
	++result;
    }

    /* Did utime set file access time to after second timestamp?  */
    if (buf.st_atime > after.tv_sec) {
	printf("FAIL: stat(%s).st_atime = %li, but should be <= %li\n",
	       fname, buf.st_atime, after.tv_sec);
	++result;
    }

    /* Did utime set file modify time to after second timestamp?  */
    if (buf.st_mtime > after.tv_sec) {
	printf("FAIL: stat(%s).st_mtime = %li, but should be <= %li\n",
	       fname, buf.st_mtime, after.tv_sec);
	++result;
    }

    /* Did utime set file change time to after second timestamp?  */
    if (buf.st_ctime > after.tv_sec) {
	printf("FAIL: stat(%s).st_ctime = %li, but should be <= %li\n",
	       fname, buf.st_ctime, after.tv_sec);
	++result;
    }

    /* Print result summary.  */
    if (result) {
	printf("FAIL: %i test failures\n", result);
    } else {
	printf("PASS: %i test failures\n", result);

	/* Delete the temporary file only if all tests passed.  */
	if (unlink(fname)) {
	    printf("unlink call failed: %m\n");
	    return 1;
	}
    }

    return result;
}

#define TEST_FUNCTION do_test ()

/* Replace main() with `#include "test-skeleton.c"' to fit in glibc.  */
int main(int argc, char **argv)
{
    int err, do_utimes = 0;

    /* Test the obsolete utimes function if argument given.  */
    if (argc > 1)
      do_utimes = 1;

    /* Fire off the test.  */
    err = do_test(do_utimes);

    /* Be forgiving if we are on NFS.  */
    if (err && is_on_nfs(".")) {
	printf("SKIP: ignoring errors since . is on an NFS filesystem\n");
	err = 0;
    }

    return err;
}
