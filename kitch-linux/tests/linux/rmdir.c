/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: rmdir.c,v 1.3 2003/02/05 15:43:37 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Tests rmdir in the case that the directory is being
 * used (i.e., there are processes with the file open)
 * **************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#define MAX_NAME_SIZE 128

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s dir\n"
	    "\tTwo tests:\n"
	    "\t\tmkdir, create file, rmdir (should fail)\n"
	    "\t\tmkdir, open directory, fstat, fchmod, rmdir, fstat (DISABLED"
	    " for now)\n", prog);
}

int main(int argc, char *argv[])
{
    char *dir, file[MAX_NAME_SIZE];
#ifdef SECOND_TEST_ENABLED
    struct stat stat_buf;
#endif // #ifdef SECOND_TEST_ENABLED

    int ret, fd;

    if (argc != 2) {
	fprintf(stderr, "%s: wrong number of arguments\n", argv[0]);
	usage(argv[0]);
	return 1;
    }
    dir = argv[1];

    ret = mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", argv[0], dir,
		strerror(errno));
	return 1;
    }

    if (strlen(dir) > MAX_NAME_SIZE - strlen("/file")) {
	fprintf(stderr, "%s: dir name provided (%s) too long "
		"(max size is %d)\n", argv[0], dir, MAX_NAME_SIZE);
	return 1;
    }

    sprintf(file, "%s/file", dir);
    fd = open(file, O_CREAT);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", argv[0], file,
		strerror(errno));
	return 1;
    }

    ret = rmdir(dir);
    if (ret != -1) {
	fprintf(stderr, "%s: rmdir(%s) should have failed (dir not empty!)\n",
		argv[0], dir);
    }

#ifdef SECOND_TEST_ENABLED
    ret = mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", argv[0], dir,
		strerror(errno));
	return 1;
    }
    fd = open(dir, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", argv[0], dir,
		strerror(errno));
	return 1;
    }
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: fstat on %s failed: %s\n", argv[0], dir,
		strerror(errno));
	return 1;
    }
    ret = fchmod(fd, S_IRUSR | S_IWUSR | S_IXUSR);
    if (ret == -1) {
	fprintf(stderr, "%s: fchmod on %s failed: %s\n", argv[0], dir,
		strerror(errno));
	return 1;
    }

    // rmdir (directory should not go away until clients go away)
    ret = rmdir(dir);
    if (ret == -1) {
	fprintf(stderr, "%s: rmdir(%s) failed: %s\n", argv[0], dir,
		strerror(errno));
	return 1;
    }

    // TEMPORARY, SHOULD NOT BE COMMITED: making sure we need to go to NFS for this
    sleep(30);

    // fstat again
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: invocation of fstat on %s (after rmdir) failed: "
		"%s\n", argv[0], dir, strerror(errno));
	return 1;
    }
    // check mode to make sure it's up to date
    if ((stat_buf.st_mode & 0777) != (S_IRUSR | S_IWUSR | S_IXUSR)) {
	fprintf(stderr, "%s: permission bits value (%o) different from expected "
		"(%o)\n", argv[0], stat_buf.st_mode &0777,
		S_IRUSR | S_IWUSR | S_IXUSR);
	return 1;
    }
#endif // #ifdef SECOND_TEST_ENABLED

    return 0;
}

