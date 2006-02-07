/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: select_files_devices.c,v 1.1 2005/01/25 03:14:21 cyeoh Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Tests select works properly on normal files and 
   /dev/null, /dev/zero
 ****************************************************************************/
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int test_file(char *filename)
{
    int fd;
    fd_set readSet, nullSet;
    int fdCount;
    struct timeval timeout;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
	printf("Could not open file %s\n", filename);
	return 0;
    }

    FD_ZERO(&readSet);
    FD_SET(fd, &readSet);
    FD_ZERO(&nullSet);

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    fdCount = select(fd+1, &readSet, &nullSet, &nullSet, &timeout);

    if (fdCount==0) {
	/* Timeout. Nothing to read when there should have been */
	return 0;
    }

    close(fd);
    return 1;
}


int main(int argc, char *argv[])
{
    int test_passes = 1;

    if (!test_file("/dev/zero")) {
	printf("Select test failed on /dev/zero");
	test_passes = 0;
    }
    if (!test_file("/dev/null")) {
	printf("Select test failed on /dev/null\n");
	test_passes = 0;
    }
    if (!test_file(argv[0])) {
	printf("Select test failed on %s (ordinary file)\n", argv[0]);
	test_passes = 0;
    }

    exit(!test_passes);
}
