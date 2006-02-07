/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * ***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s  [-v] <file> <n>\n"
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
    int i, j, ret;
    size_t sz;
    off_t off;
    char buf[32];
    pid_t pid;

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

    verbose_out(verbose, "It is going to write \"hi there\"\n");
    sz = sprintf(buf, "hi there\n");
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

    verbose_out(verbose, "writing succeeded.\n");

    // seek to beginning, read 3 initial chars
    off = lseek(fd, 0, SEEK_SET);
    if (off == -1) {
	fprintf(stderr, "%s: lseek(): %s: %s\n", argv[0], file,
		    strerror(errno));
	return (1);
    }
    if (off != 0) {
	fprintf(stderr, "%s: lseek() didn't returned 0 as expected\n",
		argv[0]);
	return (1);
    }

    sprintf(buf, "*********\n");
    j = read(fd, buf, 3);
    if (j == -1) {
	fprintf(stderr, "%s: read() %s: %s\n", argv[0],
		file, strerror(errno));
	return (1);
    } else if (j != 3) {
	fprintf(stderr, "read(): %s: %s : returned %d, expected 3\n",
		argv[0], file, j);
	return (1);
    }

    if (strncmp(buf, "hi ", 3) != 0) {
	fprintf(stderr, "%s: read() didn't returned expected data "
		"(got %3s, expected %s)\n", argv[0], buf, "hi ");
	return (1);
    }

    verbose_out(verbose, "it's going to fork\n");

    if ((pid = fork()) == 0) {
	// child
	verbose_out(verbose, "fork() succeeded\n");

	verbose_out(verbose, "Child is going to read\n");

	sprintf(buf, "*********\n");
	j = read(fd, buf, 5);
	if (j == -1) {
	    fprintf(stderr, "%s: read() %s: %s\n", argv[0],
		    file, strerror(errno));
	    return (1);
	} else if (j != 5) {
	    fprintf(stderr, "read(): %s: %s : returned %d, expected 5\n",
		    argv[0], file, j);
	    return (1);
	}

	if (strncmp(buf, "there", 5) != 0) {
	    fprintf(stderr, "%s: read() didn't returned expected data"
		    " (got %5s, expected %s)\n", argv[0], buf, "there");
	    return (1);
	}
	return 0;
    } else if (pid != -1) {
	i = wait(&ret);
	if (i == -1) {
	    fprintf(stderr, "%s: wait failed:%s\n", argv[0], strerror(errno));
	    return(1);
	}
    } else {
	fprintf(stderr, "%s: fork failed:%s\n", argv[0], strerror(errno));
	return(1);
    }

    if (!ret) {
	verbose_out(verbose, "Test succeeded.\n");
    }

    return (ret);
}

