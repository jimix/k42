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
 * Module Description: Tests open in terms of errors related to open (2)
 * flags. The program uses mkdir, rmdir also.
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
    fprintf(stderr, "Usage: %s [-v] <file> <dir>\n"
	    "<file> is a file to be created during the test\n"
	    "<dir> is a directory to be created during the test\n"
	    "Created file/dir will be removed in the end of the test\n"
	    "-v is for verbose\n", prog);
}

static void
verbose_out(int verbose, const char *msg)
{
    if (verbose) {
	fprintf(stdout, "%s\n", msg);
    }
}

int
main(int argc, char *argv[])
{
    int fd, fd2;
    int ret;
    char *file, *dir;
    int c;
    extern int optind;
    const char *optlet = "v";
    int verbose = 0;

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

    if (optind >= argc - 1) {
	usage(argv[0]);
	return (1);
    }
    file = argv[optind++];
    dir = argv[optind];
    
    // opens file with O_CREAT, O_EXCL
    fd = open(file, O_RDONLY| O_CREAT | O_EXCL,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", argv[0], file,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose,
		"open file with O_CREAT | O_EXCL succeeded as expected");
    
    // try again open with O_CREAT, O_EXCL: expected to fail!
    fd2 = open(file, O_RDONLY| O_CREAT | O_EXCL,
	       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if (fd2 != -1) {
	fprintf(stderr, "%s:Error: open with O_CREAT | O_EXCL on existing file"
		" (%s)succeed\n", argv[0], file);
	return (1);
    }
    verbose_out(verbose,
		"open file again with O_CREAT | O_EXCL failed as expected");

    // try open of existing file with O_DIRECTORY: expected to fail!
    fd2 = open(file, O_DIRECTORY);
    if (fd2 != -1) {
	fprintf(stderr, "%s:Error: open with O_DIRECTORY on  file"
		" (%s)succeed\n", argv[0], file);
	return (1);
    }
    verbose_out(verbose, "open file with O_DIRECTORY failed as expected");
    close(fd);

    ret = unlink(file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(): %s : %s\n", argv[0], file,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "unlink file succeeded as expected");
    
    // create directory
    ret = mkdir(dir, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(): %s : %s\n", argv[0], dir,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "mkdir succeeded as expected");
	
    // open directory O_RDWR: expected to fail
    fd = open(dir, O_RDWR);
    if (fd != -1) {
	fprintf(stderr, "%s: Error: open with O_RDWR to a directory"
		" (%s)succeed\n", argv[0], dir);
	return (1);
    }
    verbose_out(verbose, "open dir with O_RDWR failed as expected");
    
    // open directory O_WRONLY: expected to fail
    fd = open(dir, O_WRONLY);
    if (fd != -1) {
	fprintf(stderr, "%s:Error: open with O_WRONLY to a directory"
		" (%s)succeed\n", argv[0], dir);
	return (1);
    }
    verbose_out(verbose, "open dir with O_WRONLY failed as expected");

    // open directory with O_TRUNC, should fail
    fd = open(dir, O_TRUNC);
    if (fd != -1) {
	fprintf(stderr, "%s:Error: open with O_TRUNC to a directory"
		" (%s)succeed\n", argv[0], dir);
	return (1);
    }
    verbose_out(verbose, "open dir with O_TRUNC failed as expected");

    // open directory; should succeed
    fd = open(dir, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s : %s\n", argv[0], dir,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "open dir with O_RDONLY succeeded as expected");
    
    // open directory with O_DIRECTORY; should succeed
    fd2 = open(dir, O_DIRECTORY);
    if (fd2 == -1) {
	fprintf(stderr, "%s: open() with O_DIRECTORY: %s : %s\n", argv[0], dir,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "open dir with O_DIRECTORY succeeded as expected");

    close(fd);
    close(fd2);

    ret = rmdir(dir);
    if (ret == -1) {
	fprintf(stderr, "%s: rmdir(): %s : %s\n", argv[0], dir,
		strerror(errno));
	return (1);
    }
    verbose_out(verbose, "rmdir succeeded as expected");

    verbose_out(verbose, "Test succeded.\n");
    
    return (0);
}
    
