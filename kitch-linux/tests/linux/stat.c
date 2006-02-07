/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: stat.c,v 1.13 2004/04/29 01:50:21 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Dumps linux style stat/statfs
 * **************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-of] file [[file] ...]\n"
	    "\t print statistics about file/file system (stat(2)/statfs(2)\n"
	    "\t-f\tDoes not stat the file itself but instead the filesystem"
	    "\t  \twhere the file is located\n"
	    "\t-o\tUse open(2)/fstat/fstatfs(2) instead of stat/statfs(2)\n\n",
	    prog);
}

static void
printstat(char *file, struct stat* stat_buf)
{
    puts(file);
    printf ("\tst_dev\t\t%lu\n"
	    "\tst_ino\t\t%lu\n"
	    "\tst_mode\t\t%07o\n"
	    "\tst_nlink\t%u\n"
	    "\tst_uid\t\t%u\n"
	    "\tst_gid\t\t%u\n"
	    "\tst_rdev\t\t%ld\n"
	    "\tst_size\t\t%lu\n"
	    "\tst_blksize\t%lu\n"
	    "\tst_blocks\t%lu\n"
	    "\tst_atime\t%s"
	    "\tst_mtime\t%s"
	    "\tst_ctime\t%s",
	    (long)stat_buf->st_dev,
	    stat_buf->st_ino,
	    stat_buf->st_mode,
	    (unsigned)stat_buf->st_nlink,
	    stat_buf->st_uid,
	    stat_buf->st_gid,
	    (long)stat_buf->st_rdev,
	    stat_buf->st_size,
	    stat_buf->st_blksize,
	    stat_buf->st_blocks,
	    ctime(&stat_buf->st_atime),
	    ctime(&stat_buf->st_mtime),
	    ctime(&stat_buf->st_ctime));

}

static void
printstatfs(char *file, struct statfs *statfs_buf)
{
    puts(file);
    printf("\tf_type\t\t0x%lx\n"
	   "\tf_bsize\t\t%lu\n"
	   "\tf_blocks\t%lu\n"
	   "\tf_bfree\t\t%lu\n"
	   "\tf_bavail\t%lu\n"
	   "\tf_files\t\t%lu\n"
	   "\tf_ffree\t\t%lu\n"
	   "\tf_fsid\t\t%u %u\n"
	   "\tf_namelen\t%lu\n",
	   (long int)statfs_buf->f_type, (unsigned long) statfs_buf->f_bsize,
	   statfs_buf->f_blocks, statfs_buf->f_bfree, statfs_buf->f_bavail,
	   statfs_buf->f_files, statfs_buf->f_ffree,
	   statfs_buf->f_fsid.__val[0], statfs_buf->f_fsid.__val[1],
	   (unsigned long) statfs_buf->f_namelen);
}

int
main(int argc, char *argv[])
{

    int c;
    extern int optind;
    const char *optlet = "of";
    int stat_file = 1;
    int use_open = 0;

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'o':
	    use_open = 1;
	    break;
	case 'f':
	    stat_file = 0;
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    return (1);
	}
    }

    if (optind == argc) {
	fprintf(stderr, "%s: No file given\n", argv[0]);
	usage(argv[0]);
	return (1);
    }
    c = optind;

    for (; c < argc; c++) {
	struct stat stat_buf;
	struct statfs statfs_buf;
	int ret;
	char *func;

	if (use_open == 1) {
	    int fd;
	    fd = open (argv[c], O_RDONLY);
	    if (fd == -1) {
		func = "open";
		ret = fd;
	    } else {
		if (stat_file == 1) {
		    ret = fstat(fd, &stat_buf);
		    func = "fstat";
		} else {
		    ret = fstatfs(fd, &statfs_buf);
		    func = "fstatfs";
		}
		close(fd);
	    }
	} else {
	    if (stat_file == 1) {
		ret = stat(argv[c], &stat_buf);
		func = "stat";
	    } else {
		ret = statfs(argv[c], &statfs_buf);
		func = "statfs";
	    }
	}

	if (ret == -1) {
	    fprintf(stderr,"%s: %s(%s) failed: %s\n",
		    argv[0], func, argv[c], strerror(errno));
	    return (1);
	} else {
	    if (stat_file == 1) {
		printstat(argv[c], &stat_buf);
	    } else {
		printstatfs(argv[c], &statfs_buf);
	    }
	}
    }
    return 0;
}
