/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: chown.c,v 1.4 2005/05/11 01:18:05 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test for chown, but right now only for group id
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-f] group-id file [[file] ...]\n"
	    "\t-f\tUse open(2)/fchown(2) instead of chown(2)\n\n"
	    "so far only works for groups\n", prog);
}

int
main(int argc, char *argv[])
{

    int c;
    extern int optind;
    const char *optlet = "f";
    char *endptr;
    int use_chown = 1;
    gid_t gid;

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'f':
	    use_chown = 0;
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

    gid = (gid_t) strtol(argv[c], &endptr, 10);
    if (*endptr != '\0' || errno == ERANGE ) {
	fprintf(stderr, "%s: group id %s is invalid\n",
		argv[0], argv[c]);
	usage(argv[0]);
	return (1);
    }

    for (c++; c < argc; c++) {
	struct stat stat_buf;
	int ret;
	char *func;
	int fd = -1;

	if (use_chown) {
	    ret = chown(argv[c], -1, gid);
	    func = "chown";
	} else {
	    fd = open (argv[c], O_RDONLY);
	    if (fd == -1) {
		func = "open";
		ret = fd;
	    } else {
		ret = fchown(fd, -1, gid);
		func = "fchown";
		close(fd);
	    }
	}

	if (ret == -1) {
	    fprintf(stderr,"%s: %s(%s) failed: %s\n",
		    argv[0], func, argv[c], strerror(errno));
	    if (errno == ENOSYS) {
		return (1);
	    }
	} else {
	    // check to see if it has been changed
	    if (use_chown) {
		ret = stat(argv[c], &stat_buf);
	    } else {
		ret = fstat(fd, &stat_buf);
	    } 
	    if (ret == -1) {
		fprintf(stderr,"%s, stat(%s) failed: %s\n",
			argv[0], argv[c], strerror(errno));
	    } else {
		if (gid == stat_buf.st_gid) {
		    printf("%s: success\n", argv[0]);
		} else {
		    fprintf(stderr, "%s: chown(%s) to group %d didn't "
			    "succeed: stat returned group id %d", 
			    argv[0], argv[c], gid, stat_buf.st_gid);
		}		    
	    }
	}
    }
    return 0;
}
