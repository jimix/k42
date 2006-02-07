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
 * Module Description: test for statfs() system call
 * **************************************************************************/

#include <stdio.h>
#include <sys/vfs.h>

/* Test that statfs() behaves correctly.  Return non-zero on error.  */
static int do_test(void)
{
    int result = 0;
    struct statfs buf;
    char *path = ".";

    /* Fire off the system call.  */
    if (statfs(path, &buf) != 0) {
	fprintf(stderr, "statfs returned error: %m\n");
	++result;
    }

    /* Dump the structure we got back from the syscall.  */
    printf("statfs(\"%s\") = {\n"
	   " .f_type    0x%-8lX\n"
	   " .f_bsize   %-8li\n"
	   " .f_blocks  %-8lu\n"
	   " .f_bfree   %-8lu\n"
	   " .f_bavail  %-8lu\n"
	   " .f_files   %-8lu\n"
	   " .f_ffree   %-8lu\n"
	   " .f_fsid    %-8li %-8li\n"
	   " .f_namelen %-8li\n"
	   " .f_frsize  %-8li\n"
	   " .f_spare   %-8li %-8li %-8li %-8li %-8li\n"
	   "}\n",
	   path,
	   (long) buf.f_type,
	   (long) buf.f_bsize,
	   buf.f_blocks,
	   buf.f_bfree,
	   buf.f_bavail,
	   buf.f_files,
	   buf.f_ffree,
	   (long) buf.f_fsid.__val[0],
	   (long) buf.f_fsid.__val[1],
	   (long) buf.f_namelen,
	   (long) buf.f_frsize,
	   (long) buf.f_spare[0],
	   (long) buf.f_spare[1],
	   (long) buf.f_spare[2],
	   (long) buf.f_spare[3], (long) buf.f_spare[4]);

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
