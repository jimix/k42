/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: truncate.c,v 1.11 2005/08/01 20:41:32 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: tests related to truncating a file
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

static char buf[5000];

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-fv] file length [[file length]...]\n"
	    "\t-f\tUse open(2) followed by ftruncate(2) instead of "
	    "truncate(2)\n"
	    "\t-v: verbose\n\n" , prog);
}

int
main(int argc, char *argv[])
{

    int c, i, fd, ret;
    extern int optind;
    const char *optlet = "fv";
    char *endptr;
    int use_truncate = 1;
    off_t len;
    struct stat stat_buf;
    int verbose = 0;
    
    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'f':
	    use_truncate = 0;
	    break;
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
    c = optind;

    for (; c < argc; c++) {
	char *file;
	char *func;
	file = argv[c++];
	if (c == argc) {
	    fprintf(stderr, "%s: Missing lenth for file %s\n",
		    argv[0], file);
	    return (1);
	}
	len = (off_t) strtol(argv[c], &endptr, 10);
	if (*endptr != '\0' || errno == ERANGE ) {
	    fprintf(stderr, "%s: length %s provided for file %s is invalid\n",
		    argv[0], argv[c], file);
	    usage(argv[0]);
	    return (1);
	}

	if (use_truncate) {
	    ret = truncate(file, len);
	    func = "truncate";
	} else {
	    fd = open (file, O_WRONLY);
	    if (fd == -1) {
		func = "open";
		ret = fd;
	    } else {
		ret = ftruncate(fd, len);
		func = "ftruncate";
		close(fd);
	    }
	}

	if (ret == -1) {
	    fprintf(stderr,"%s: %s(%s) failed: %s\n",
		    argv[0], func, file, strerror(errno));
	    return (1);
	} else {
	    // stat the file to check its size
	    ret = stat(file, &stat_buf);
	    if (ret == -1) {
		fprintf(stderr,"%s, stat(%s) failed: %s\n",
			argv[0], file, strerror(errno));
		return(1);
	    } else {
		if (stat_buf.st_size != len) {
		    fprintf(stderr, "%s: truncate(%s) to len %ld didn't "
			    "succeed: stat returned len %ld", 
			    argv[0], file, (long)len, (long)stat_buf.st_size);
		    return(1);
		}		    
	    }			
	}

	if (verbose) {
	    fprintf(stdout, "%s: truncate for file %s succeeded\n", argv[0],
		    file);
	}
    }

    // last test: write, lseek, truncate, write (only for the first file
    // in the list provided by the user)
    for(i=0; i<5000; buf[i++]=i);
    
    fd = open(argv[optind], O_RDWR | O_CREAT | O_TRUNC,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if (fd == -1) {
	fprintf(stderr,"%s: open(%s) failed: %s\n",
		argv[0], argv[optind], strerror(errno));
	return(1);
    }
    
    ret = write(fd, buf, 5000);
    if (ret != 5000) {
	if (ret != -1) {
	    fprintf(stderr,"%s: write in file %s failed: returned %d "
		    "instead of 5000\n", argv[0], argv[optind], ret);
	    return(1);
	} else {
	    fprintf(stderr,"%s: write(%s) failed: %s\n",
		    argv[0], argv[optind], strerror(errno));
	    return(1);
	}
    }

    if (verbose) {
	fprintf(stdout, "%s: first write for file %s succeeded\n", argv[0],
		argv[optind]);
    }

    ret = ftruncate(fd, 0);
    if (ret == -1 ) {
	fprintf(stderr,"%s: ftruncate(%s) failed: %s\n",
		argv[0], argv[optind], strerror(errno));
	return(1);
    }

    if (verbose) {
	fprintf(stdout, "%s: ftruncate for file %s succeeded\n", argv[0],
		argv[optind]);
    }
    
    ret = write(fd, buf, 5000);
    if (ret != 5000) {
	if (ret != -1) {
	    fprintf(stderr,"%s: write in file %s failed: returned %d "
		    "instead of 5000\n", argv[0], argv[optind], ret);
	    return(1);
	} else {
	    fprintf(stderr,"%s: write(%s) failed: %s\n",
		    argv[0], argv[optind], strerror(errno));
	    return(1);
	}
    }

    if (verbose) {
	fprintf(stdout, "%s: second write for file %s succeeded\n", argv[0],
		argv[optind]);
    }

    // stat the file to check its size

    ret = stat(argv[optind], &stat_buf);
    if (ret == -1) {
	fprintf(stderr,"%s, stat(%s) failed: %s\n",
		argv[0], argv[optind], strerror(errno));
	return(1);
    } else {
	if (stat_buf.st_size == 10000) {
	    printf("%s: success\n", argv[0]);
	} else {
	    fprintf(stderr, "%s: stat returned size %ld; expected 10000\n", 
		    argv[0], (long)stat_buf.st_size);
	    return(1);
	}	    
    }

    if (verbose) {
	fprintf(stdout, "%s: succeeded\n", argv[0]);
    }

    return 0;
}
