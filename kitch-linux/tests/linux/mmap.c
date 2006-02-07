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
#include <sys/mman.h>
#include <sys/sendfile.h>

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
    int fd, rfd;
    char *file, *rfile;
    int i, ret;
    off_t off;
    pid_t pid;
    char* addr, *raddr;
    char buf[4096];
    void *first, *second;
    
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

    // verify that mmap to a busy location works
    printf("region allocation errors expected for next test\n");
    first = mmap(0, 4096, PROT_WRITE, MAP_ANONYMOUS, 0, 0);
    second = mmap(first, 4096, PROT_WRITE, MAP_ANONYMOUS, 0, 0);

    file = argv[optind];
    
    fd = open(file, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", argv[0], file,
		strerror(errno));
	return (1);
    }

    verbose_out(verbose, "open() succeeded, going to mmap\n");
    
    addr = mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED) {
	fprintf(stderr, "%s: mmap(): %s: %s\n", argv[0], file,
		strerror(errno));
	return (1);
    }
    
    if (optind+1 < argc) {
	rfile = argv[optind+1];
    
	rfd = open(rfile, O_RDONLY, 0);

	if (rfd == -1) {
	    fprintf(stderr, "%s: open(): %s: %s\n", argv[0], rfile,
		    strerror(errno));
	    return (1);
	}

	verbose_out(verbose, "open() succeeded, going to mmap\n");

	raddr = mmap(0, 4096, PROT_READ, MAP_SHARED, rfd, 0);

	if (raddr == MAP_FAILED) {
	    fprintf(stderr, "%s: mmap(): %s: %s\n", argv[0], rfile,
		    strerror(errno));
	    return (1);
	}
    }  else {
	rfd = -1;
	rfile = 0; raddr = 0;		// prevent compile warnings
    }

    verbose_out(verbose, "It is going to write\n");
    for (i = 0; i < 4096; i++) {
	addr[i] = i;
    }
    verbose_out(verbose, "writing succeeded\n");

    verbose_out(verbose, "it's going to fork\n");

    if ((pid = fork()) == 0) {
	// child
	verbose_out(verbose, "fork() succeeded\n");
	for (i = 0; i < 4096; i++) {
	    if(addr[i] != (i&0xff)) {
		fprintf(stderr,
			"compare failed: byte %d contained %d\n",
			i, addr[i]);
		return (1);
	    }
	}
    
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
	verbose_out(verbose, "lseek() in child succeeded\n");

	verbose_out(verbose, "Child is going to read\n");

	i = read(fd, buf, 4096);
	
	if (i == -1) {
	    fprintf(stderr, "%s: read(): %s: %s\n", argv[0], file,
		    strerror(errno));
	    return (1);
	} else if (i != 4096) {
	    fprintf(stderr, "%s: file length wrong", argv[0]);
	    return (1);
	}

	for (i = 0; i < 4096; i++) {
	    if(buf[i] != (i&0xff)) {
		fprintf(stderr,
			"compare failed: byte %d contained %d\n",
			i, buf[i]);
		return (1);
	    }
	}
    
	verbose_out(verbose, "reading in child process succeeded\n");

	if (rfd != -1) {
	    ssize_t count;
	    off_t offset;
	    
	    fprintf(stdout, "read of first line of %s\n", rfile);
	    for(i = 0;raddr[i] != '\n' && i < 128; i++) {
		buf[i] = raddr[i];
	    }
	    buf[i] = 0;
	    fprintf(stdout, "%s\n", buf);

	    // now test sendfile
	    ftruncate(fd, 0);
	    lseek(fd, 0, SEEK_SET);
	    offset = 0;
	    count = sendfile(fd, rfd, &offset, 1000000);
	    if(count == -1) {
		fprintf(stderr, "%s: sendfile(): %s: %s\n", argv[0], file,
			strerror(errno));
		return (1);
	    }
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
    
