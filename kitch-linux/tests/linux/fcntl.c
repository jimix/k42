/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fcntl.c,v 1.9 2004/05/27 22:01:14 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Test for fcntl(2) locking
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static const char *prog;

static void
usage(void)
{
    fprintf(stderr, "Usage: "
	    "%s \n"
	    "\n", prog);
}

int
main(int argc, char *argv[])
{
    const char *optlet = "a"; /* fake it so we take options later */
    const char *path = "fcntl.test";
    int c;
    pid_t pid, mypid;
    int fd, rc;
    struct flock f = {
	l_type:		F_WRLCK,
	l_whence:	SEEK_SET,
	l_start:	0,
	l_len:		0, /* to end of file */
	l_pid:		0 /* ignored */
    };

    char *lock_msg[]={
	 [F_RDLCK] = "reader lock",
	 [F_WRLCK] = "writer lock",
	 [F_UNLCK] = "unlocked"
    };

    prog = argv[0];

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'a':
	    /* fake, do nothing */
	    break;
	case '?':
	default:
	    usage();
	    return (1);
	}
    }

    fd = open(path, O_RDWR | O_CREAT);
    if (fd == -1) {
	printf("open() failed: %s\n", strerror(errno));
	return 1;
    }

    mypid = getpid();
    rc = fcntl(fd, F_SETLK, &f);
    if (rc == -1) {
	fprintf(stderr, "fcntl(F_SETLK) after open() failed: %s\n",
		strerror(errno));
	close (fd);
	return 1;
    } else {
	printf("parent[%u]: aquired lock type: %s.\n",
	       mypid, lock_msg[f.l_type]);
    }

    pid = fork();
    if (pid == 0) {
	mypid = getpid();

	rc = fcntl(fd, F_SETLK, &f);
	if (!(rc == -1 && errno == EAGAIN)) {
	    printf("FAIL: child should not have been granted lock\n");
	    return 1;
	}
	printf("PASS: child was correctly denied lock: %m\n");

	rc = fcntl(fd, F_GETLK, &f);
	if (rc == -1) {
	    fprintf(stderr, "child[%u]: fcntl(F_GETLK)  failed: %s\n",
		    mypid, strerror(errno));
	    return -1;
	}
	printf("child[%u]: lock is owned by %u.\n",
	       mypid, f.l_pid);
		
	printf("child[%u]: will wait for lock\n", mypid);
	rc = fcntl(fd, F_SETLKW, &f);

	if (rc == -1) {
	    fprintf(stderr, "child[%u]: fcntl(F_SETLKW)  failed: %s\n",
		    mypid, strerror(errno));
	    return -1;
	}
	printf("child[%u]: got the lock thankyou\n", mypid);

	return 0;

    } else if (pid > 0) {
	int child, status, ret;

	printf("parent[%u]: sleep(5) then release the lock\n", mypid);
	sleep(5);
	printf("parent[%u]: releasing the lock\n", mypid);
	f.l_type = F_UNLCK;
	rc = fcntl(fd, F_SETLK, &f);
	if (rc == -1) {
	    fprintf(stderr, "parent[%u]: fcntl(F_SETLK) F_UNLBK  failed: %s\n",
		    mypid, strerror(errno));
	    return -1;
	}
	printf("parent[%u]: released the lock\n", mypid);

	if ((child = wait(&status)) > 0) {
	    printf("parent[%u]: reaped child whose pid=%u\n", mypid, child);

	    if (WIFEXITED(status)) {
		if ((ret = WEXITSTATUS (status)) == 0) {
		    printf("PASS: child exited with no error\n");
		} else {
		    fprintf(stderr, "FAIL: child returned %i\n", ret);
		    return -1;
		}
	    } else {
		fprintf(stderr,"wait(): unexpected status\n");
		return -1;
	    }
	} else {
	    fprintf(stderr, "wait(): failed: %s\n", strerror(errno));
	    return -1;
	}
    } else {
	fprintf(stderr, "fork(): failed: %s\n", strerror(errno));
	return 1;
    }

    return 0;
}
