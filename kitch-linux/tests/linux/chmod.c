/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: chmod.c,v 1.3 2002/11/05 22:25:01 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test for chmod and fchmod
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
    fprintf(stderr, "Usage: %s [-f] octal_mode file [[file] ...]\n"
	    "\t-f\tUse open(2)/fchmod(2) instead of chmod(2)\n\n", prog);
}

int
main(int argc, char *argv[])
{

    int c;
    extern int optind;
    const char *optlet = "f";
    char *endptr;
    int use_chmod = 1;
    mode_t mode;

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'f':
	    use_chmod = 0;
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

    mode = (mode_t) strtol(argv[c], &endptr, 8);
    if (*endptr != '\0' || errno == ERANGE ) {
	fprintf(stderr, "%s: mode %s is invalid\n",
		argv[0], argv[c]);
	usage(argv[0]);
	return (1);
    }

    for (c++; c < argc; c++) {
	struct stat stat_buf;
	int ret;
	char *func;
	int fd = -1;

	if (use_chmod) {
	    ret = chmod(argv[c], mode);
	    func = "chmod";
	} else {
	    fd = open (argv[c], O_RDONLY);
	    if (fd == -1) {
		func = "open";
		ret = fd;
	    } else {
		ret = fchmod(fd, mode);
		func = "fchmod";
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
	    if (use_chmod) {
		ret = stat(argv[c], &stat_buf);
	    } else {
		ret = fstat(fd, &stat_buf);
	    } 
	    if (ret == -1) {
		fprintf(stderr,"%s, stat(%s) failed: %s\n",
			argv[0], argv[c], strerror(errno));
	    } else {
		if (mode == (stat_buf.st_mode  & (S_IRWXU|S_IRWXG|S_IRWXO))) {
		    printf("%s: success\n", argv[0]);
		} else {
		    fprintf(stderr, "%s: chmod(%s) for mode %07o didn't "
			    "succeed: stat returned mode %07o", 
			    argv[0], argv[c], mode, 
			    stat_buf.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO));
		}		    
	    }
	}
    }
    return 0;
}
