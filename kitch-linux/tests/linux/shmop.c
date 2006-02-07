/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: shmop.c,v 1.4 2004/02/17 15:36:15 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: for shmat(2) and friends
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   /* for fork, execlp, getopt */
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

static const char *prog;

static void
usage(void)
{
    fprintf(stderr, "Usage: "
	    "%s [-] [-p <path>] [-c <char>] [-s <size>] [-i <shmid>]\n"
	    "    -p <path>    Path to use for calculating key (see ftok(3).\n"
	    "                 IPC_PRIVATE if no path specified.\n"
	    "    -c <char>    Char to use for calculating key (see ftok(3).\n"
	    "    -s <size>    Number of bytes to shmget(2).\n"
	    "    -i <shmid>   do not shmget(2) and use <shmid> to shmat(2).\n"
	    "\n", prog);
}

int
main(int argc, char *argv[])
{
    int shmid = -1;
    int dochild = 1;
    key_t key;
    void *mem;
    char *addr;
    size_t sz = 256*1024;
    const char *optlet = "p:c:s:i:";
    int c;
    char kc = 'K';
    char *kp = NULL;
    pid_t pid;

    prog = argv[0];

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'p':
	    kp = strdup(optarg);
	    if (kp == NULL) {
		kp = "/";
	    }
	    break;
	case 'c':
	    kc = *optarg;
	    break;
	case 's':
	    sz = strtol(optarg, (char **)NULL, 10);
	    break;
	case 'i':
	    shmid = strtol(optarg, (char **)NULL, 10);
	    dochild = 0;
	    break;
	case '?':
	default:
	    usage();
	    return (1);
	}
    }

    if(kp) {
	key = ftok(kp, kc);
    } else {
	key = IPC_PRIVATE;
    }

    if (shmid == -1) {
	shmid = shmget(key, sz , (key == IPC_PRIVATE)?0:IPC_CREAT);
    }
    if (shmid == -1) {
	fprintf(stderr, "shmget(%d): failed: %s\n",
		key, strerror(errno));
	return 1;
    }

    mem = shmat(shmid, NULL, 0);
    if (mem == NULL) {
	fprintf(stderr, "shmat(%d): failed: %s\n",
		shmid, strerror(errno));
	return 1;
    }

    printf("mem = %p\n", mem);

    if (!dochild) {
	puts("--- Testing Pages (shmat only) ---");
	fflush(stdout);
	addr = mem;
	while ((uintptr_t)addr < ((uintptr_t)mem + sz)) {
	    if (*addr != kc) {
		fprintf(stderr, "memory corrupt\%p n", addr);
		return 1;
	    }
	    addr += 1024;
	}
	puts("--- Testing Pages (shmat only): Success ---");
    } else {
	puts("--- Touching pages ---");
	fflush(stdout);
	addr = mem;
	while ((uintptr_t)addr < ((uintptr_t)mem + sz)) {
	    *addr = kc;
	    addr += 1024;
	}
	puts("--- Touching pages: Success ---");
	pid = fork();
	if (pid == 0) {
	    puts("--- Testing Pages (fork) ---");
	    fflush(stdout);
	    addr = mem;
	    while ((uintptr_t)addr < ((uintptr_t)mem + sz)) {
		if (*addr != kc) {
		    fprintf(stderr, "memory corrupt\%p \n", addr);
		    return 1;
		}
		addr += 1024;
	    }
	    puts("--- Testing Pages (fork): Success ---");
	} else if (pid > 0) {
	    char c[] = {kc, '\0'};
	    char sid[20];
	    snprintf(sid, sizeof(sid), "%d", shmid);
	    printf("exec()ing: %s -i %s -c %s\n",
		   prog, sid, c);
	    fflush(stdout);
	    execlp(prog, prog, "-i", sid, "-c", c, NULL);
	} else {
	    fprintf(stderr, "fork(): failed: %s\n", strerror(errno));
	    return 1;
	}
    }
    return 0;
}
