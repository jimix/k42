/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dir.c,v 1.14 2002/11/05 22:25:01 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: ls(1) the almost linux way
 * assumes the following options:
 *   -U: do not sort; list entries in directory order
 *   -n: list numeric UIDs and GIDs instead of names
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
#include <dirent.h>

typedef enum {
    H_ALL	= 0x01,
    H_LONG	= 0x02
} how_t;

static const char *prog;

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [-alg] [-f <fd>] [FILE]...\n"
	    "  -a       do not hide entries starting with `.'\n"
	    "  -l       use a long listing format\n"
	    "  -f <fd>  use stdin to read directory (BIG HACK)\n"
	    "\n", prog);
}

static char const *perms[] = {
    "---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"
};

// if it works for gcc 2.x, should work for 3.x
#if (__GNUC__>= 2)
static char const ftype[] = {
    [S_IFIFO >>12 ]	= 'p',
    [S_IFCHR >> 12]	= 'c',
    [S_IFDIR >> 12]	= 'd',
    [S_IFBLK >> 12]	= 'b',
    [S_IFREG >> 12]	= '-',
    [S_IFLNK >> 12]	= 'l',
    [S_IFSOCK >> 12]	= 's'
};
#else /* #if (__GNUC__>= 2) */
#error "requires GNU C 2.x or better"
#endif /* #if (__GNUC__>= 2) */

static char *
emode(mode_t mode, char *buf)
{
    int others	= mode & S_IRWXO ;
    int group	= (mode & S_IRWXG) >> 3;
    int user	= (mode & S_IRWXU) >> 6;
    int aux	= (mode & (S_ISUID|S_ISGID|S_ISVTX)) >> 9;
    int type	= (mode & S_IFMT) >> 12;

    char *bp = buf;

    if (ftype[type] == '\0') {
	*bp++ = '?';
    } else {
	*bp++ = ftype[type];
    }

    /* do permisions */
    (void)strcpy(bp, perms[user]);
    bp += 3;
    (void)strcpy(bp, perms[group]);
    bp += 3;
    (void)strcpy(bp, perms[others]);
    bp += 3;
    /* null terminate */
    bp = '\0';

    if (aux & S_ISVTX) {
	if (user & S_IXOTH) {
	    buf[9] = 't';
	} else {
	    buf[9] = 'T';
	}
    }
    if (aux & S_ISGID) {
	if (user & S_IXGRP) {
	    buf[6] = 's';
	} else {
	    buf[6] = 'S';
	}
    }
    if (aux & S_ISUID) {
	if (user & S_IXUSR) {
	    buf[3] = 's';
	} else {
	    buf[3] = 'S';
	}
    }
    return buf;
}

static int
print_it(const char *path, struct stat *s, how_t how)
{
    int ret;
    if (how & H_LONG) {
	char t[128];
	char m[sizeof("drwxrwxrwx")];
	struct tm *tm;

	tm = localtime(&s->st_mtime);
	if (tm == NULL) {
	    fprintf(stderr, "%s: localtime(): %s\n",
		    prog, strerror(errno));
	    exit(1);
	} else if (errno == ENOENT) {
	    // FIXME: localtime(3) cannot find timezone file
	    // We know it does not exist yet.
	    errno = 0;
	}


	/* do this cause asctime(3) and ctime(3) append a newline */
	(void) strftime(t, sizeof (t), "%b %d %T", tm);

	printf("%s %3u %5u %7u %12lu %s %s\n",
	       emode(s->st_mode, m),
	       (unsigned)s->st_nlink, s->st_uid, s->st_gid, s->st_size,
	       t, path);
	ret = 0;
    } else {
	ret = printf("%s\n", path);
    }
    return (ret);
}

static int
list_it(const char *path, how_t how, int use_fd)
{
    DIR *dirp;
    struct stat sb;
    int ret = 0;
    int saved_errno;
    
    dirp = opendir(path);

    if (dirp != NULL) {
	struct dirent *dent;

	if (use_fd != -1) {
	    /* WARNING: This is a total hack
	     * It depends on the FD being the first member in the struct.
	     */
	    int dirfd;

	    if ((fstat(use_fd, &sb) == 0) && S_ISDIR(sb.st_mode)) {
		dirfd = *(int *)dirp;

		if (dup2(use_fd, dirfd) == -1) {
		    fprintf(stderr, "%s: dup2(%d, %d) failed: %s\n",
			    prog, use_fd, dirfd, strerror(errno));
		    return 1;
		}
	    } else {
		fprintf(stderr, "%s: fd[%d] is not s directory\n",
			prog, use_fd);
		return 1;
	    }

	}

	saved_errno = errno;
	if (errno == EBADF) {
	    errno = 0;
	}
	while ((dent = readdir(dirp)) != NULL) {
	    char full_path[PATH_MAX]; /* yeah I'm lazy right now */

	    if (!(how & H_ALL) && dent->d_name[0] == '.') {
		continue;
	    }
	    (void)snprintf(full_path, sizeof (full_path),
		     "%s/%s", path, dent->d_name);
	    if (stat(full_path, &sb) == -1) {
		fprintf(stderr, "%s: stat(%s): %s\n",
			prog, full_path, strerror(errno));
		ret = 1;
	    } else {
		print_it(dent->d_name, &sb, how);
	    }
	}
	/* check errno */
	if (errno == EBADF) {
	    /* technically this is the only possible errno */
	    fprintf(stderr, "%s: readdir(%s): %s\n",
		    prog, path, strerror(errno));
	    ret = 1;
	} else if (errno != ENOSYS && errno != 0) {
	    /* FIXME
	     * since there are several unsupported system calls
	     * an errno could be set to ENOSYS. At some point this
	     * should always be just an else.
	     */
	    fprintf(stderr, "%s: WILD ERRNO readdir(%s): %s\n",
		    prog, path, strerror(errno));
	    ret = 1;
	    // restore to original errno value
	    errno = saved_errno;
	}
    } else if (errno == ENOTDIR) {
	/* it's a file */
	if (stat(path, &sb) == -1) {
	    fprintf(stderr, "%s: stat(%s): %s\n",
		    prog, path, strerror(errno));
	    ret = 1;
	} else {
	    print_it(path, &sb, how);
	}
    } else {
	    fprintf(stderr, "%s: opendir(%s): %s\n",
		    prog, path, strerror(errno));
	    ret = 1;
    }
    return (ret);
}

int
main(int argc, char *argv[])
{

    int c;
    extern int optind;
    const char *optstring = "algf:";
    how_t how = 0;
    int exit_status = 0;
    int use_fd = -1;

    prog = argv[0];

    while ((c = getopt(argc, argv, optstring)) != EOF) {
	switch (c) {
	case 'a':
	    how |= H_ALL;
	    break;
	case 'l':
	    how |= H_LONG;
	    break;
	case 'f':
	    use_fd = strtol(optarg, (char **)NULL, 10);
	    break;
	case '?':
	default:
	    usage();
	    return (1);
	}
    }

    if (optind == argc) {
	/* no args use current directory */
	if (list_it(".", how, use_fd) != 0) {
	    exit_status = 1;
	}
    } else if (optind + 1 == argc) {
	if (list_it(argv[optind], how, use_fd) != 0) {
		exit_status = 1;
	}
    } else {
	int i = optind;

	while (i < argc) {
	    printf("%s:\n", argv[i]);
	    if (list_it(argv[i++], how, use_fd) != 0) {
		exit_status = 1;
	    }
	}
    }

    //FIXME: This should not be necessary
    (void) fflush(stdout);
    return exit_status;
}
