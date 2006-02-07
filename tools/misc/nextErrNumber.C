/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: nextErrNumber.C,v 1.8 2003/11/14 15:11:12 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define FILENAME "/u/kitchawa/.nextErrorNumber"

int
main(int argc, char **argv)
{
    int fd;
    int val;
    int code;
    char sys_str[64];
    char valstr[64];

    if (argc == 1) {
	if ((fd = open(FILENAME, O_RDWR|O_EXCL)) == -1) {
	    fprintf(stderr, "failed to exclusively open %s\n", FILENAME);
	    exit(-1);
	}

	read(fd, valstr, 64);
	sscanf(valstr, "%d", &val);



	val++;
	printf("Your error number is: %d\n", val);

	lseek(fd, 0, SEEK_SET);
	sprintf(valstr, "%d", val);
	write(fd, valstr, strlen(valstr));

	close(fd);
    }
    else if (argc == 3) {
	val = atoi(argv[1]);
	code = atoi(argv[2]);
	if (code == 22785) {
	    if ((fd = open(FILENAME, O_RDWR|O_EXCL|O_CREAT,0666)) == -1) {
		fprintf(stderr, "failed to exclusively open %s\n", FILENAME);
		fprintf(stderr, "This most likely occurred because the file already\n");
		fprintf(stderr, "  existed.  Make sure this is what you intend to do,\n");
		fprintf(stderr, "  if so, remove the file, and re-run\n");
		exit(-1);
	    }
	    sprintf(sys_str, "chmod 666 %s", FILENAME);
	    system(sys_str);
	    printf("setting next error number to %d\n", val);
	    write(fd, &val, sizeof(int));
	    close(fd);
	}
    }
    else {
	printf("usage: nextErrorNumber [set value] [verification code]\n");
    }

    return 0;
}
