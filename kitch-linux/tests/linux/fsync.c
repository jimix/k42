/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fsync.c,v 1.1 2005/08/01 20:42:14 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: running and timing sync so we can measure effect of
 *                     having a buffer cache deamon on
 * **************************************************************************/

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s file [file length] [-v]\n"
	    "\tWrite to file and fsync.\n"
	    "\t-v: verbose\n"
	    "\tFile length should be a multiple of 4k.\n"
	    "\tDefault for file length is 1MB\n", prog);
}

void
computeTime(struct timeval *bef, struct timeval *aft, struct timeval *result)
{
    result->tv_sec = 0;
    
    /* we could use timersub here, but timersub is only defined if not
     * building with -ansi or POSIX_SOURCE ...*/
    result->tv_sec = aft->tv_sec -  bef->tv_sec;
    result->tv_usec = aft->tv_usec - bef->tv_usec;
    if (result->tv_usec < 0) {
      --(result->tv_sec);
      result->tv_usec += 1000000;                                           \
    }   
}

int
main(int argc, char *argv[])
{
    int c, fd;
    long ret;
    extern int optind;
    const char *optlet = "v";
    char *endptr;
    int verbose = 0;
    char * file;
    long size = 1 << 20;
    long nb_pages;
    long i;
    
    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    return (1);
	}
    }

    if (optind == argc) {
	fprintf(stderr, "%s: No file given\n", argv[0]);
	usage(argv[0]);
	return (1);
    }
    file = argv[optind++];
    if (optind < argc) { // one more argument
	size = strtol(argv[optind], &endptr , 0);
	if (*endptr!= '\0') {
	    fprintf(stderr, "%s: invalid argument for request size %s\n", argv[0], argv[optind]);
	    usage(argv[0]);
	    return 1;
	}
	if (size % 0x1000 != 0) {
	    fprintf(stderr, "%s: invalid argument for request size %s: should be MULTIPLE OF 4K\n",
		    argv[0], argv[optind]);
	    usage(argv[0]);
	    return 1;
	}
    }
    nb_pages = size / 0x1000;

    fd = open (file, O_CREAT |O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed with %s\n",
		argv[0], file, strerror(errno));
	return 1;
    }

    char *buffer = (char*) malloc(0x1000); // allocate 4k
    if (buffer == NULL) {
	fprintf(stderr, "%s: malloc failed\n", argv[0]);
	return 1;
    }
    memset(buffer, 1, 0x1000);
    
    printf("%s: Writing %ld pages (%ld MB) to file %s\n", argv[0], nb_pages, nb_pages/256, file);

    struct timeval before;
    if (gettimeofday(&before, NULL) != 0) {
	fprintf(stderr, "%s: gettimeofday failed (%s)\n", argv[0], strerror(errno));
	return 1;
    }
    
    for (i = 0; i < nb_pages; i++) {
	ret = write(fd, buffer, 0x1000);
	if (ret == -1) {
	    fprintf(stderr, "%s: write for %s failed with %s\n",
		    argv[0], file, strerror(errno));
	    return 1;
	}    
	if (ret != 0x1000) {
	    fprintf(stderr, "%s: write returned %ld\n", argv[0], ret);
	    return 1;
	}
    }

    struct timeval afterWrite;
    if (gettimeofday(&afterWrite, NULL) != 0) {
	fprintf(stderr, "%s: gettimeofday failed (%s)\n", argv[0], strerror(errno));
	return 1;
    }

    ret = fsync(fd);
    if (ret == -1) {
	fprintf(stderr, "%s: fsync failed (%s)\n", argv[0], strerror(errno));
	return 1;
    }
	
    struct timeval afterFsync;
    if (gettimeofday(&afterFsync, NULL) != 0) {
	fprintf(stderr, "%s: gettimeofday failed (%s)\n", argv[0], strerror(errno));
	return 1;
    }

    struct timeval t1, t2;
    computeTime(&before, &afterWrite, &t1);
    computeTime(&afterWrite, &afterFsync, &t2);
    printf("%s:\n\tTime for writing: %ld sec %ld usec\n", argv[0], t1.tv_sec, t1.tv_usec);
    printf("\tTime for fsync:   %ld sec %ld usec\n", t2.tv_sec, t2.tv_usec);

    return 0;
}
