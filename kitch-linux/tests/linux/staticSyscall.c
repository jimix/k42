/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: staticSyscall.c,v 1.3 2003/12/09 19:58:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Scans memory, deduces page-fault times
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <asm/unistd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#define __NR_my_getpid	__NR_getpid
#define __NR_my_open	__NR_open

_syscall0(int, my_getpid);

/*
 * HACK: The unistd.h machinery causes compilation errors in _syscall3().
 *       We avoid them by redefining __syscall_clobbers.
 */
#undef __syscall_clobbers
#define __syscall_clobbers \
	"r6", "r7", "r8", "r9", "r10", "r11", "r12"

_syscall3(int,my_open,const char *,file,int,flag,int,mode);

int
main(int argc, char **argv)
{
    int pid, fd;

    pid = my_getpid();
    fprintf(stderr, "pid = %d (should be %d).\n", pid, getpid());

    fd = my_open("no_such_file", O_RDWR, 0644);
    if (fd < 0) {
	fprintf(stderr, "my_open(\"no_such_file\") failed as expected, "
		"fd = %d, errno = %d.\n", fd, errno);
	perror("\t\tmy_open");
    } else {
	fprintf(stderr, "my_open(\"no_such_file\") succeeded unexpectedly, "
		"fd = %d.\n", fd);
    }

    fd = my_open("new_file", O_RDWR|O_CREAT|O_EXCL, 0644);
    if (fd >= 0) {
	fprintf(stderr, "my_open(\"new_file\") succeeded as expected, "
		"fd = %d.\n", fd);
    } else {
	fprintf(stderr, "my_open(\"new_file\") failed unexpectedly, "
		"fd = %d, errno = %d.\n", fd, errno);
	perror("\t\tmy_open");
    }

    fd = my_open("new_file", O_RDWR|O_CREAT|O_EXCL, 0644);
    if (fd < 0) {
	fprintf(stderr, "my_open(\"new_file\") failed as expected, "
		"fd = %d, errno = %d.\n", fd, errno);
	perror("\t\tmy_open");
    } else {
	fprintf(stderr, "my_open(\"new_file\") succeeded unexpectedly, "
		"fd = %d.\n", fd);
    }

    (void) unlink("new_file");

    struct timeval start;
    struct timeval end;
    unsigned long long delta;
    int i;

    gettimeofday(&start,NULL);
    for (i = 0; i < 1000000; i++) {
	(void) getpid();
    }
    gettimeofday(&end,NULL);

    delta = ((end.tv_sec * 1000000ull) + end.tv_usec) -
		((start.tv_sec * 1000000ull) + start.tv_usec);
    fprintf(stderr, "1000000 getpid()'s took %lld microseconds.\n", delta);

    gettimeofday(&start,NULL);
    for (i = 0; i < 1000000; i++) {
	(void) my_getpid();
    }
    gettimeofday(&end,NULL);

    delta = ((end.tv_sec * 1000000ull) + end.tv_usec) -
		((start.tv_sec * 1000000ull) + start.tv_usec);
    fprintf(stderr, "1000000 my_getpid()'s took %lld microseconds.\n", delta);

    return 0;
}
