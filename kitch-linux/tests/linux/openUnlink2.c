/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Jimmi's test for open unlink.
 * ***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s  [-v] <file>\n"
	    "-v: verbose\n", prog);
}

static void
verbose_out(int verbose, const char *msg)
{
    if (verbose) {
	fprintf(stdout, "%s", msg);
    }
}

int
main(int argc, char *argv[])
{
    int c;
    extern int optind;
    const char *optlet = "v";
    int verbose = 0;
    int fd;
    char *file;
    int j;
    size_t sz;
    char buf[512];

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
	usage(argv[0]);
	return (1);
    }
    file = argv[optind];

    fd = open(file, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", argv[0], file,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "open() succeeded\n");

    sz = sprintf(buf, "this is line 1\n");
    j = write(fd, buf, sz);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s (count=%ld)\n", argv[0],
		file, strerror(errno), sz);
	return (1);
    } else if (j != sz) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %ld\n",
		argv[0], file, j, sz);
	return (1);
    }
    j = unlink(file);
    if (j == -1) {
	fprintf(stderr, "%s: unlink(): %s: %s\n", argv[0], file,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "unlink() succeeded\n");

    return (0);
}


