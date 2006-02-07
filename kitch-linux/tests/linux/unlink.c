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
 * Module Description: test for unlink
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
    struct stat stat_buf;

    if (argc != 2) {
	usage(argv[0]);
	return 1;
    }

    path = argv[c];
    ret = unlink(path);

    if (ret == -1) {
	fprintf(stderr,"%s: unlink(%s) failed: %s\n",
		argv[0], path, strerror(errno));
	return 1;
    } else {
	// stat the file; should fail since the pathname does not exist anymore
	ret = stat(path, &stat_buf);
	if (ret == -1 && errno == ENOENT) {
	    printf("%s: success\n", argv[0]);
	} else {
	    if (ret != -1) {
		fprintf(stderr, "%s: unlink(%s) didn't "
			"succeed: stat returned nlink %d\n",
			argv[0], path, (int)stat_buf.st_nlink);
		return 1;
	    } else {
		fprintf(stderr, "%s: unlink(%s) didn't "
			"succeed: stat returned error %s\n",
			argv[0], path, strerror(errno));
		return 1;
	    }
	}
    }
    return 0;
}
