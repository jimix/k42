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
 * Module Description: test for access() system call
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
    fprintf(stderr, "Usage: %s path \n", prog);
}

int
main(int argc, char *argv[])
{
    int ret;
    int c = 1;
    char *path;

    if (argc != 2) {
	usage(argv[0]);
	return 1;
    }

    path = argv[c];
    ret = access(path, 0775);

    printf("access: %s %d %s\n", path, ret, strerror(errno));
    return 0;
}
