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
 * Module Description: Testing of (1) discarding pages when a file is found to
 * be modified in the remote server and (2) lseek. This program reads a few
 * bytes, and wait for input. After getting input, it lseek to the begining,
 * and reads again. Second read should provide new content of file.
 * NOTE: lseek is not supported yet, so second read will continue from the
 *       point where first left.
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

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-f] file\n", prog);
}


int
main(int argc, char *argv[])
{

    int ret, fd;
    struct stat stat_buf;
    int c;
    char buf[200];

    if (argc != 2) {
	usage(argv[0]);
	return (1);
    }

    fd = open (argv[1], O_RDWR);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", 
		argv[0], argv[1], strerror(errno));
	return (1);
    }

    ret = read(fd, buf, 10);
    if (ret == -1) {
	fprintf(stderr, "%s: first read(%s) failed:%s\n", 
		argv[0], argv[1], strerror(errno));
	return (1);
    }

    buf[ret] = '\0';
    printf("value read: %s\n\n", buf);

    printf("Continue?");
    c = getchar();

    printf("It is going to read from begining again\n");
    ret = lseek(fd, 0, SEEK_SET);
    if (ret == -1) {
	fprintf(stderr,"%s: lseek failed:%s\n", argv[0], strerror(errno));
	return (1);
    }
    ret = read(fd, buf, 10);
    if (ret == -1) {
	printf("%s: second read failed:%s\n", argv[0], strerror(errno));
	return (1);
    }
    buf[ret] = '\0';
    printf("value read is %s\n", buf);

    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	    fprintf(stderr, "%s: fstat failed: %s\n", 
		    argv[0], strerror(errno));
    }
    printf("fstat returned size %ld\n", stat_buf.st_size);
    return 0;
}
