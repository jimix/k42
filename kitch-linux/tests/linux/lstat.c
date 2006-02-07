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
 * Module Description: test for lstat system call
 * **************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

/* Pretty print a struct stat.  */
static int dump_struct_stat(char *file, struct stat *left)
{
    printf("stat(%s) = {\n"
	   " .st_dev:     %lu\n"
	   " .st_ino:     %lu\n"
	   " .st_mode:    %07o\n"
	   " .st_nlink:   %u\n"
	   " .st_uid:     %u\n"
	   " .st_gid:     %u\n"
	   " .st_rdev:    %ld\n"
	   " .st_size:    %lu\n"
	   " .st_blksize: %lu\n"
	   " .st_blocks:  %lu\n"
	   " .st_atime:   %s"
	   " .st_mtime:   %s"
	   " .st_ctime:   %s"
	   "}\n",
	   file,
	   (long) left->st_dev,
	   left->st_ino,
	   left->st_mode,
	   (unsigned) left->st_nlink,
	   left->st_uid,
	   left->st_gid,
	   (long) left->st_rdev,
	   left->st_size,
	   left->st_blksize,
	   left->st_blocks,
	   ctime(&left->st_atime),
	   ctime(&left->st_mtime), ctime(&left->st_ctime));

    return 0;
}

/* Return 1 if the two stat structs are not identical.  */
static int cmp_struct_stat(struct stat *left, struct stat *right)
{
    int result = 0;

    if (left->st_dev != right->st_dev) {
	printf("(left->st_dev = %li) != (right->st_dev = %li)\n",
	       (long) left->st_dev, (long) right->st_dev);
	++result;
    }

    if (left->st_ino != right->st_ino) {
	printf("(left->st_ino = %lu) != (right->st_ino = %lu)\n",
	       left->st_ino, right->st_ino);
	++result;
    }

    /* The above two are sufficient for comparison for our purposes.  */

    return result;
}

/* Test that lstat syscall behaves correctly.  Return non-zero on error. */
static int do_test(void)
{
    int result = 0;
    char fname[] = "tst-lstat.XXXXXX";
    char lname[] = "tst-lstat.link";
    struct stat buf1, buf2;

    /* First create a temporary file.  */
    if (mkstemp(fname) == -1) {
	printf("mkstemp call failed: %m\n");
	return 1;
    }

    /* Get the file information from stat.  */
    if (stat(fname, &buf1)) {
	printf("stat call failed: %m\n");
	return 1;
    }

    /* Dump some debugging information to stdout.  */
    dump_struct_stat(fname, &buf1);

    /* Now get the file information from lstat.  */
    if (lstat(fname, &buf2)) {
	printf("lstat call failed: %m\n");
	return 1;
    }

    /* Dump some debugging information to stdout.  */
    dump_struct_stat(fname, &buf2);

    /* The two stat structures should be identical.  */
    if (cmp_struct_stat(&buf1, &buf2)) {
	printf("FAIL: stat(%s) != lstat(%s)\n", fname, fname);
	++result;
    }

    /* Now create a symbolic link to our temporary file.  */
    if (symlink(fname, lname)) {
	printf("FAIL: symlink error: %m\n");
	return 1;
    }

    /* Get the symbolic link information from stat.  */
    if (stat(lname, &buf1)) {
	printf("stat call failed: %m\n");
	return 1;
    }

    /* Dump some debugging information to stdout.  */
    dump_struct_stat(lname, &buf1);

    /* Now get the symbolic link information from lstat.  */
    if (lstat(lname, &buf2)) {
	printf("lstat call failed: %m\n");
	return 1;
    }

    /* Dump some debugging information to stdout.  */
    dump_struct_stat(lname, &buf2);

    /* The two stat structures should NOT be identical.  */
    if (cmp_struct_stat(&buf1, &buf2) == 0) {
	printf("FAIL: stat(%s) == lstat(%s)\n", lname, lname);
	++result;
    }

    /* Delete the temporary file.  */
    if (unlink(fname)) {
	printf("unlink call failed: %m\n");
	return 1;
    }

    /* Delete the symbolic link to temporary file.  */
    if (unlink(lname)) {
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
