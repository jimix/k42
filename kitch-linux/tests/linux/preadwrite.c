/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: preadwrite.c,v 1.1 2004/02/19 15:46:05 dilma Exp $
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: tests pread/pwrite
 * **************************************************************************/

#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s\n"
	    "\tTests pwrite/pread\n", prog);
}

int
main(int argc, char *argv[])
{
    char buf[4], rbuf[33];
    int i, ret;
    int fd;
    char *filename = "ptest_file";

    if (argc != 1) {
	usage(argv[0]);
	return(1);
    }

    fd = open(filename, O_CREAT | O_EXCL | O_RDWR,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: Couldn't create test file %s: %s.\n"
		"Remove it before running the test\n", argv[0], filename,
		strerror(errno));
	return(1);
    }

    // populate the file from end to the beginning using pwrite. The
    // file will look like the string "A00A01A02A3 ...A99"
    for (i=99; i>=0; i--) {
	// build the string to be stored in the file
	sprintf(buf, "A%02d", i);
	ret = pwrite(fd, buf, 3, 3*i);
	if (ret == -1) {
	    fprintf(stderr, "%s: pwrite on position %d failed: %s\n",
		    argv[0], 3*i, strerror(errno));
	    goto ret_label;
	} else if (ret != 3) {
	    fprintf(stderr, "%s: pwrite on position %d failed: returned %d\n",
		    argv[0], 3*i, ret);
	    goto ret_label;
	}
    }

#if 0
    // read sequentially in chunk sizes of 31 bytes and print
    do {
	ret = read(fd, rbuf, 31);
	if (ret == -1) {
	    fprintf(stderr, "%s: read error: %s\n", argv[0], strerror(errno));
	    goto ret_label;
	} else if (ret == 0) {
	    break;
	}

	rbuf[ret] = '\0';
	printf("%s", rbuf);
    } while (1);
    printf("\n");
#endif

    // read sequentially in chunks of 4 bytes and check if got expected value.
    // During this process, also read the file with pread in an order that
    // retrieves elements in the sequence: 0, 99, 1, 98, 2, 97, 3, 96 ...

    ret = lseek(fd, 0, SEEK_SET);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek to begining of file resulted in error: "
		"%s\n", argv[0], strerror(errno));
	goto ret_label;
    } else if (ret != 0) {
	fprintf(stderr, "%s: lseek to begining of file returned %d\n",
		argv[0], ret);
	goto ret_label;
    }
    char expected[5];
    i = 0;
    do {
	// proceed with sequencial order
	sprintf(expected, "A%02d", i);
	ret = read(fd, rbuf, strlen(expected));
	if (ret == -1) {
	    fprintf(stderr, "%s: read error: %s\n", argv[0], strerror(errno));
	    goto ret_label;
	} else if (ret == 0) {
	    // end of data
	    if (i != 100) {
		fprintf(stderr,"%s: data not as expected, got to i %d\n",
			argv[0], i);
		goto ret_label;
	    }
	    break;
	} else if (ret != strlen(expected)) {
	    fprintf(stderr, "%s: read error: ret %d, expected %d\n", argv[0],
		    ret, (int) strlen(expected));
	    goto ret_label;
	}

	rbuf[ret] = '\0';
	if (strncmp(rbuf, expected, ret) != 0) {
	    fprintf(stderr, "%s: read value %s, expected %s\n", argv[0],
		    rbuf, expected);
	    goto ret_label;
	}

	// proceed with the other order (0, 99, 1, 98, ...) through pread
	int number;
	if (i % 2 == 0) {
	    number = i/2;
	} else {
	    number = 99 - i/2;
	}
	sprintf(expected, "A%02d", number);
	//printf("expected in weird order is now %s\n", expected);
	ret = pread(fd, rbuf, strlen(expected), number*3);
	if (ret == -1) {
	    fprintf(stderr, "%s: pread error: %s\n", argv[0], strerror(errno));
	    goto ret_label;
	} else if (ret == 0) {
	    // end of data
	    if (i != 100) {
		fprintf(stderr,"%s: data not as expected, got to i %d\n",
			argv[0], i);
		goto ret_label;
	    }
	    break;
	} else if (ret != strlen(expected)) {
	    fprintf(stderr, "%s: pread error: ret %d, expected %d\n", argv[0],
		    ret, (int) strlen(expected));
	    goto ret_label;
	}

	rbuf[ret] = '\0';
	if (strncmp(rbuf, expected, ret) != 0) {
	    fprintf(stderr, "%s: pread value %s, expected %s\n", argv[0],
		    rbuf, expected);
	    goto ret_label;
	}

	i++;
    } while (1);

    printf("%s: Test succeeded.\n", argv[0]);

  ret_label:
    // cleanup
    unlink(filename);

    return(0);
}
