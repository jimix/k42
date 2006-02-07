/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dev.c,v 1.3 2002/11/05 22:25:01 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: tests for special files in the /dev tree
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

/* one day our compiler will set this */
#define __USE_GNU
#include <fcntl.h>

#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>

static char *prog;

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-nt]\n"
	    "  Default: -nt\n"
	    "  -n    Test /dev/null\n"
	    "  -t    Test /dev/tty\n"
	    "\n", prog);
}

static int
dir_test(const char *fname)
{
    int fd;
    /* Cannot open as directory */
    fd = open (fname, O_DIRECTORY | O_RDONLY);
    if (fd != -1) {
	fprintf(stderr, "%s: %s: opened as a directory\n",
		prog, fname);
	close(fd);
	return 0;
    }
    return 1;
}

static gid_t
name2gid(const char *name)
{
    struct group *grp;

    grp = getgrnam(name);
    if (grp == NULL) {
	fprintf(stderr, "%s: getgrnam(\"%s\") failed: %s\n",
		prog, name, strerror(errno));
	exit(1);
    }
    return (grp->gr_gid);
}

static uid_t
name2uid(const char *name)
{
    struct passwd *pw;

    pw = getpwnam(name);
    if (pw == NULL) {
	fprintf(stderr, "%s: getpwnam(\"%s\") failed: %s\n",
		prog, name, strerror(errno));
	exit(1);
    }
    return (pw->pw_uid);
}


/* Compare stat values against a given reference */

static int
check_stat(const char *fname, struct stat *rs)
{
    struct stat stat_buf;

    int ret = 1;

    ret = stat(fname, &stat_buf);

    if (ret == -1) {
	fprintf(stderr, "%s: stat(\"%s\") failed: %s\n",
		prog, fname, strerror(errno));
	ret = 0;
    } else {
	/* Make sure we report all issues */

	if (stat_buf.st_mode != rs->st_mode) {
	    fprintf(stderr, "%s: %s: bad mode, expect %07o got %07o\n",
		   prog, fname, rs->st_mode, stat_buf.st_mode);
	    ret = 0;
	}
	if (stat_buf.st_nlink != rs->st_nlink) {
	    fprintf(stderr, "%s: %s: bad links, expect %u got %u\n",
		   prog, fname, (unsigned)rs->st_nlink, (unsigned)stat_buf.st_nlink);
	    ret = 0;
	}
	if (stat_buf.st_uid != rs->st_uid) {
	    fprintf(stderr, "%s: %s: bad uid, expect %u got %u\n",
		   prog, fname, rs->st_uid, stat_buf.st_uid);
	    ret = 0;
	}
	if (stat_buf.st_gid != rs->st_gid) {
	    fprintf(stderr, "%s: %s: bad gid, expect %u got %u\n",
		   prog, fname, rs->st_gid, stat_buf.st_gid);
	    ret = 0;
	}
	if (stat_buf.st_rdev != rs->st_rdev) {
	    fprintf(stderr, "%s: %s: bad rdev, expect %ld got %ld\n",
		   prog, fname, (long)rs->st_rdev, (long)stat_buf.st_rdev);
	    ret = 0;
	}
	if (stat_buf.st_size != rs->st_size) {
	    fprintf(stderr, "%s: %s: bad size, expect %lu got %lu\n",
		   prog, fname, rs->st_size, stat_buf.st_size);
	    ret = 0;
	}
	if (stat_buf.st_blksize != rs->st_blksize) {
	    fprintf(stderr, "%s: %s: bad blksize, expect %lu got %lu\n",
		   prog, fname, rs->st_blksize, stat_buf.st_blksize);
	    ret = 0;
	}
	if (stat_buf.st_blocks != rs->st_blocks) {
	    fprintf(stderr, "%s: %s: bad blocks, expect %lu got %lu\n",
		   prog, fname, rs->st_blocks, stat_buf.st_blocks);
	    ret = 0;
	}
	if (stat_buf.st_atime < rs->st_atime) {
	    fprintf(stderr, "%s: %s: atime (%ld) older then %ld\n",
		   prog, fname, (long)rs->st_atime, (long)stat_buf.st_atime);
	    ret = 0;
	}
	if (stat_buf.st_mtime < rs->st_mtime) {
	    fprintf(stderr, "%s: %s: mtime (%ld) older then %ld\n",
		   prog, fname, (long)rs->st_mtime, (long)stat_buf.st_mtime);
	    ret = 0;
	}
	if (stat_buf.st_ctime < rs->st_ctime) {
	    fprintf(stderr, "%s: %s: ctime (%ld) older then %ld\n",
		   prog, fname, (long)rs->st_ctime, (long)stat_buf.st_ctime);
	    ret = 0;
	}
    }
    return ret;
}


static int
dev_tty(void)
{
    struct stat ref;
    const char *fname = "/dev/tty";
    int fd;

    ref.st_mode		= 0020666;
    ref.st_nlink	= 1;
    ref.st_uid		= name2uid("root");
    ref.st_gid		= name2gid("root");
    ref.st_rdev		= makedev(5, 0);
    ref.st_size		= 0;
    ref.st_blksize	= 4096;
    ref.st_blocks	= 0;
    /* minimum time */
    ref.st_atime	= time(NULL);
    ref.st_mtime	= 0;
    ref.st_ctime	= 0;

    if (check_stat(fname, &ref)) {
	return 0;
    }

    if (dir_test(fname)) {
	return 0;
    }

    /* open for real now */
    fd = open (fname, O_RDWR|O_NOCTTY);
    if (fd == -1) {
	fprintf(stderr, "%s: %s: open() failed: %s\n",
		prog, fname, strerror(errno));
	return 0;
    }
    return 1;
}

static int
dev_null(void)
{
    struct stat ref;
    const char *fname = "/dev/null";
    char buf[256];
    int fd;
    int ret = 1;
    ssize_t sz;

    ref.st_mode		= 0020666;
    ref.st_nlink	= 1;
    ref.st_uid		= name2uid("root");
    ref.st_gid		= name2gid("mem");
    ref.st_rdev		= makedev(1, 3);
    ref.st_size		= 0;
    ref.st_blksize	= 4096;
    ref.st_blocks	= 0;
    /* minimum time */
    ref.st_atime	= 0;
    ref.st_mtime	= 0;
    ref.st_ctime	= 0;

    if (check_stat(fname, &ref)) {
	return 0;
    }

    if (dir_test(fname)) {
	return 0;
    }

    fd = open (fname, O_RDWR);
    if (fd == -1) {
	fprintf(stderr, "%s: %s: open() failed: %s\n",
		prog, fname, strerror(errno));
	return 0;
    }

    sz = read(fd, buf, 256);
    if (sz == -1) {
	fprintf(stderr,"%s: read failed: %s\n",
		prog, strerror(errno));
	close(fd);
	return 0;
    }
    if (sz != 0) {
	fprintf(stderr,"%s: expected to read 0, got %ld\n",
		prog, (long)sz);
	ret = 0;
    }

    sz = write(fd, buf, 256);
    if (sz == -1) {
	fprintf(stderr,"%s: write failed: %s\n",
		prog, strerror(errno));
	close(fd);
	return 0;
    }
    if (sz != 256) {
	/*
	 * this may not be a valid test when we support interuptable
	 * system calls.
	 */
	fprintf(stderr,"%s: expected to write 256, got %ld\n",
		prog, (long)sz);
	ret = 0;
    }

    close(fd);

    return ret;
}

int
main(int argc, char *argv[])
{
    int c;
    const char *optlet = "nt";
    int do_null = 0;
    int do_tty = 0;
    int ret = 0;

    prog = argv[0];

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 't':
	    do_tty = 1;
	    break;
	case 'n':
	    do_null = 1;
	    break;
	case '?':
	default:
	    usage();
	    return (1);
	}
    }

    if ((!do_null) && (!do_tty)) {
	do_null = do_tty = 1;
    }

    if (do_tty) {
	if (!dev_tty()) {
	    ret = 1;
	}
    }
    if (do_null) {
	if (!dev_null()) {
	    ret = 1;
	}
    }
    return ret;
}
