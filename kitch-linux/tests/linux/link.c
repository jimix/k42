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
 * Module Description: Tests of link and unlink
 * ***********************************************************************/

#include <stdio.h>
#define __USE_GNU
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-v] <file>\n"
	    "Tests link/unlink (2) calls.\n"
	    "<file> is a file to be open (potentially created) "
	    "during the test.\n"
	    "If created, file will be removed in the end of the test\n"
	    "-v is for verbose\n", prog);
}

static void
verbose_out(int verbose, const char *msg)
{
    if (verbose) {
	fprintf(stdout, "%s\n", msg);
    }
}

static int
check_nlink(char *prog, char *file, nlink_t new_value)
{
    struct stat buf;
    int ret;

    ret = stat(file, &buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(): %s: %s\n", prog, file, strerror(errno));
	return(1);
    }

    if (buf.st_nlink != new_value) {
	fprintf(stderr, "%s: expected nlink==%d, got %d\n", prog,
		(int) new_value, (int) buf.st_nlink);
	return (1);
    }

    return 0;
}

int
main(int argc, char *argv[])
{
    int fd;
    int ret, i;
    char *file;
    int c;
    extern int optind;
    const char *optlet = "v";
    int verbose = 0;
    char already_exists = 1;
    struct stat stat_buf;
    nlink_t orig_nlink;
    char buf[128];
    const int ntests = 5;

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

    // check if given file already exists, and its number of links
    ret = stat(file, &stat_buf);

    if (ret == -1) {
	if (errno == ENOENT) {
	    // file doesn't exist yet, let's create it
	    fd = open(file, O_RDONLY| O_CREAT | O_EXCL,
		      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
	    if (fd == -1) {
		fprintf(stderr, "%s: open(): %s: %s\n", argv[0], file,
			strerror(errno));
		return (1);
	    } else {
		verbose_out(verbose, "%s: File %s has been created.");
		already_exists = 0;
		orig_nlink = 1;
	    }
	} else {
	    // unexpected error
	    fprintf(stderr, "%s: open(): %s: %s\n", argv[0], file,
		    strerror(errno));
	    return (1);
	}
    } else {
	orig_nlink = stat_buf.st_nlink;
	sprintf(buf, "File %s has been opened. It has nlink==%d.",
		file, (int)orig_nlink);
	verbose_out(verbose, buf);
    }

    for (i=0; i < ntests; i++) {
	sprintf(buf, "%s-link-%d", file, i);
	ret = link (file, buf);
	if (ret == -1) {
	    fprintf(stderr, "%s: link(): %s, %s: %s\n", argv[0], file, buf,
		    strerror(errno));
	    return (1);
	}
	// stat to check if nlink is correct
	if (check_nlink(argv[0], file, orig_nlink+i+1)) return (1);
	if (check_nlink(argv[0], buf, orig_nlink+i+1)) return (1);
	verbose_out(verbose, "link call succeeded.");
    }

    for (i=0; i < ntests; i++) {
	sprintf(buf, "%s-link-%d", file, i);
	ret = unlink (buf);
	if (ret == -1) {
	    fprintf(stderr, "%s: unlink(): %s: %s\n", argv[0], buf,
		    strerror(errno));
	    return (1);
	}
	// stat to check if nlink is correct
	if (check_nlink(argv[0], file, orig_nlink+ntests-i-1)) return(1);
	// check if unlinked file went away
	ret = open(buf, 0);
	if (ret != -1) {
	    fprintf(stderr, "%s: after unlink() of %s it still exists\n",
		    argv[0], buf);
	}
	verbose_out(verbose, "unlink call succeeded.");
    }

    // create a directory so that we try to link/unlink it
    ret = mkdir("link-unlink-dir-test", S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(): link-unlink-dir-test: %s\n", argv[0],
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "mkdir for link-unlink-dir-test succeeded");


    // try link the directory: expected to fail!
    ret = link("link-unlink-dir-test", "link-wrong");
    if (ret != -1) {
	fprintf(stderr, "%s:Error: link call with a directory argument "
		"succeeded!", argv[0]);
	return (1);
    } else if (errno != EPERM) {
	fprintf(stderr, "%s:Error: link call with a directory argument "
		"failed with %s.", argv[0], strerror(errno));
	return (1);

    }
    verbose_out(verbose,
		"link call with a directory argument failed as expected");

    // try unlink the directory: expected to fail!
    ret = unlink("link-unlink-dir-test");
    if (ret != -1) {
	fprintf(stderr, "%s:Error: unlink call with a directory argument "
		"succeeded!", argv[0]);
	return (1);
    } else if (errno != EPERM) {
	fprintf(stderr, "%s:Error: unlink call with a directory argument "
		"failed with %s.", argv[0], strerror(errno));
	return (1);

    }
    verbose_out(verbose,
		"unlink call with a directory argument failed as expected");

    // clean up after test
    ret = rmdir("link-unlink-dir-test");
    if (ret == -1) {
	fprintf(stderr, "%s: rmdir(): link-unlink-dir-test : %s\n", argv[0],
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "rmdir of temp directory succeeded");

    verbose_out(verbose, "Test succeded.\n");

    return (0);
}


