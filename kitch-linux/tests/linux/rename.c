/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: rename.c,v 1.11 2005/07/11 20:52:56 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Tests rename(2)
 * **************************************************************************/
#include <stdio.h>
#include <limits.h>
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
    fprintf(stderr, "Usage: %s [-v] oldpath newpath\n"
	    "    or %s [-v] file1 file2 dir1 dir2\n"
	    "\tIf only two arguments (besides -v) are provided,\n"
	    "\t   rename(oldpath, newpath) is tested.\n"
	    "\tIf four arguments (besides -v) are provided\n"
	    "\t\t(all arguments should be existing files/dirs)\n"
	    "\t\tseveral tests of rename(2) are executed:\n"
	    "\t\t file1 to file1.mv.tmp (file1.mv.tmp is created, and then\n"
	    "\t\t                        moved back to file1)\n"
	    "\t\t file1 to file2 (should succeed, replacing original file2)\n"
	    "\t\t dir1 to dir1.mv.tmp (dir1.mv.tmp created, then moved back)\n"
	    "\t\t dir1 to dir2 (should succeed, replacing original dir2)\n"
	    "\t\t file2 to dir2 (should fail)\n"
	    "\t\t creates dir1, makes dir2 not empty then test file2 to \n"
	    "\t\t     dir2/file1 (should fail)\n"
	    "\t\t dir1 to dir2 (first dir1 is created; should fail since\n"
	    "\t\t               dir1 is not empty)\n"
	    "\t-v: verbose\n", prog, prog);
}

static void
verbose_out(int verbose, const char *msg)
{
    if (verbose) {
	fprintf(stdout, "%s\n", msg);
    }
}

// does cd .. and rmdir dir
static int cleanUpDir(char *dir, char *prog)
{
    int ret;
    ret = chdir("..");
    if (ret != 0) {
	fprintf(stderr, "%s: chdir(..) returned error: %s\n",
		prog, strerror(errno));
	return 1;
    }
    ret = rmdir(dir);
    if (ret != 0) {
	fprintf(stderr, "%s: rmdir(%s) returned error: %s\n",
		prog, dir, strerror(errno));
	return 1;
    }
    return 0;
}

int
check_results(char *prog, char *old, ino_t oldino, char *new)
{
    int ret;
    struct stat stat_buf;
    char old_resolved[PATH_MAX], new_resolved[PATH_MAX];
    char *retp;

    // old and new are the same ??
    retp = realpath(old, old_resolved);
    if (retp == NULL) {
	if (errno != ENOENT) {
	    fprintf(stderr, "%s: realpath(%s) returned error: %s\n", prog, old,
		    strerror(errno));
	    return 1;
	} else {
	    // couldn't find it, has been moved, things are cool so far
	}
    } else {
	// old is there, let's see if newname is the same
	retp = realpath(new, new_resolved);
	if (retp == NULL) {
	    fprintf(stderr, "%s: realpath(%s) returned error: %s\n", prog, new,
		    strerror(errno));
	    return 1;
	}
	// if we don't have the same file, things are going wrong
	if (strcmp(old_resolved, new_resolved) != 0) {
	    // old has not been removed
	    fprintf(stderr, "%s: file %s was expected to disappear since file "
		    "has been renamed\n", prog, old);
	}
    }
    ret = stat(new, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", prog, new,
		strerror(errno));
	return 1;
    }
    if (oldino != stat_buf.st_ino) {
	fprintf(stderr, "%s: after rename %s has inode %ld, %s had inode "
		"%ld\n", prog, new, (long)stat_buf.st_ino, old, (long)oldino);
	return 1;
    }
    return 0;
}

int
do_check_file(char *prog, char *file, ino_t *ino)
{
    struct stat stat_buf;
    int ret;
    int fd;

    fd = open(file, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "?%s: open(%s) failed: %s\n", prog, file,
		strerror(errno));
	return 1;
    }
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: fstat(%s) failed: %s\n", prog, file,
		strerror(errno));
	return 1;
    }
    if (!S_ISREG(stat_buf.st_mode)) {
	fprintf(stderr, "%s: %s is not a regular file as expected\n", prog,
		file);
	return 1;
    }
    *ino = stat_buf.st_ino;
    close(fd);
    return 0;
}

int
do_check_dir(char *prog, char *dir, ino_t *ino)
{
    struct stat stat_buf;
    int ret, fd;

    // we could open with O_DIRECTORY (then no need to stat it to check
    // if it's a directory), but to run this same test on AIX we have to
    // avoid such "Linux-ism"
    fd = open(dir, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", prog, dir,
		strerror(errno));
	return 1;
    }
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: fstat(%s) failed: %s\n", prog, dir,
		strerror(errno));
	return 1;
    }
    if (!S_ISDIR(stat_buf.st_mode)) {
	fprintf(stderr, "%s: %s is not a regular dir as expected(although\n"
		"it has been opened with O_DIRECTORY!)\n", prog, dir);
	return 1;
    }
    *ino = stat_buf.st_ino;
    close(fd);
    return 0;
}

int
main(int argc, char *argv[])
{

    int c;
    extern int optind;
    const char *optlet = "v";
    int verbose = 0;
    char *file1, *file2, *dir1, *dir2, *file2lastpart;
    char file1tmp[255], dir1tmp[255], file[255];
    ino_t f1ino, f2ino, d1ino, d2ino;
    int fd;
    int ret;

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'v':
	    verbose = 1;
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    return (1);
	}
    }

    if (argc == optind + 2) {
	// simple test, just invoke rename, check result and return
	// first get inode from oldpath so later we can check if rename
	// did its job correctly
	struct stat stat_buf;
	ino_t ino;
	char *old = argv[optind];
	char *new = argv[optind+1];
	ret = stat(old, &stat_buf);
	if (ret == -1) {
	    fprintf(stderr, "%s: stat(%s) returned error: %s\n", argv[0], old,
		    strerror(errno));
	    return 1;
	}
	ino = stat_buf.st_ino;
	ret = rename(old, new);
	if (ret == -1) {
	    if (errno == ENOSYS) {
		fprintf(stdout, "%s: rename not available for this file "
			"system\n", argv[0]);
		// for now let's consider this acceptable in terms of
		// regress.sh success
		return 0;
	    }
	    fprintf(stderr, "%s: rename(%s, %s) returned error: %s\n", argv[0],
		    old, new, strerror(errno));
	    return 1;
	}
	if (check_results(argv[0], old, ino, new) != 0) return 1;
	fprintf(stdout, "%s: success\n", argv[0]);
	return 0;
    }

    // more than 2 path arguments, so lots of testa are performed
    if (optind +3 >= argc) {
	fprintf(stderr, "%s: Arguments missing\n", argv[0]);
	usage(argv[0]);
	return (1);
    }
    file1 = argv[optind];
    file2 = argv[optind+1];
    dir1 = argv[optind+2];
    dir2 = argv[optind+3];

    /* opening files and directories. They're supposed to exist */
    if (do_check_file(argv[0], file1, &f1ino) == 1) {
	return 1;
    }
    if (do_check_file(argv[0], file2, &f2ino) == 1) {
	return 1;
    }
    if (do_check_dir(argv[0], dir1, &d1ino) == 1) {
	return 1;
    }
    if (do_check_dir(argv[0], dir2, &d2ino) == 1) {
	return 1;
    }
    verbose_out(verbose, "Files/dirs arguments ok\n");

    /* test mv file1 to file1.mv.tmp */
    sprintf(file1tmp,"%s.mv.tmp", file1);
    // make sure file1tmp does not exist
    fd = open(file1tmp, O_RDONLY);
    if (fd != -1) {
	// get rid of it
	ret = unlink(file1tmp);
	fd = open(file1tmp, O_RDONLY);
	if (fd != -1) {
	    fprintf(stderr, "%s: file %s still exists after unlink!?\n",
		    argv[0], file1tmp);
	    return 1;
	}
	if (verbose == 1) {
	    fprintf(stdout, "%s: file %s removed\n", argv[0], file1tmp);
	}
    }
    ret = rename(file1, file1tmp);
    if (ret == -1) {
	if (errno == ENOSYS) {
	    fprintf(stdout, "%s: rename not available for this file system\n",
		    argv[0]);
	    // for now let's consider this acceptable in terms of regress.sh
	    // success
	    return 0;
	}
	fprintf(stderr, "%s: rename(%s, %s) returned error: %s\n", argv[0],
		file1, file1tmp, strerror(errno));
	return 1;
    }
    if (check_results(argv[0], file1, f1ino, file1tmp) != 0) return 1;
    if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) succeeded\n", argv[0], file1,
		file1tmp);
    }

    // move file1tmp back to file1
    ret = rename(file1tmp, file1);
    if (ret == -1) {
	fprintf(stderr, "%s: rename(%s, %s) returned error: %s\n", argv[0],
		file1tmp, file1, strerror(errno));
	return 1;
    }
    if (check_results(argv[0], file1tmp, f1ino, file1) != 0) return 1;
    if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) succeeded\n", argv[0], file1tmp,
		file1);
    }

    /* test mv file1 to file2: file 2 should disappear */
    ret = rename(file1, file2);
    if (ret == -1) {
	fprintf(stderr, "%s: rename(%s, %s) returned error: %s\n", argv[0],
		file1, file2, strerror(errno));
	return 1;
    }
    if (check_results(argv[0], file1, f1ino, file2) != 0) return 1;
    if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) succeeded\n", argv[0], file1,
		file2);
    }

    /* test mv dir1 to dir1.mv.tmp */
    sprintf(dir1tmp,"%s.mv.tmp", dir1);
    // make sure dir1tmp does not exist
    fd = open(dir1tmp, O_RDONLY);
    if (fd != -1) {
	// get rid of it
	ret = rmdir(dir1tmp);
	fd = open(dir1tmp, O_RDONLY);
	if (fd != -1) {
	    fprintf(stderr, "%s: file %s still exists after rmdir!?\n",
		    argv[0], file1tmp);
	    return 1;
	}
	verbose_out(verbose, "removed file dir1.mv.mp");
    }
    ret = rename(dir1, dir1tmp);
    if (ret == -1) {
	fprintf(stderr, "%s: rename(%s, %s) returned error: %s\n", argv[0],
		dir1, dir1tmp, strerror(errno));
	return 1;
    }
    if (check_results(argv[0], dir1, d1ino, dir1tmp) != 0) return 1;
    if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) succeeded\n", argv[0], dir1,
		dir1tmp);
    }

    // move back to dir1
    ret = rename(dir1tmp, dir1);
    if (ret == -1) {
	fprintf(stderr, "%s: rename(%s, %s) returned error: %s\n", argv[0],
		dir1tmp, dir1, strerror(errno));
	return 1;
    }
    if (check_results(argv[0], dir1tmp, d1ino, dir1) != 0) return 1;
    if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) succeeded\n", argv[0], dir1tmp,
		dir1);
    }

    // move dir1 to dir2 (replacing dir2)
    ret = rename(dir1, dir2);
    if (ret == -1) {
	fprintf(stderr, "%s: rename(%s, %s) returned error: %s\n", argv[0],
		dir1, dir2, strerror(errno));
	return 1;
    }
    if (check_results(argv[0], dir1, d1ino, dir2) != 0) return 1;
    if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) succeeded\n", argv[0], dir1,
		dir2);
    }

    // file2 to dir2: should fail
    ret = rename(file2, dir2);
    if (ret != -1) {
	fprintf(stderr, "%s: rename(%s, %s) didn't return an error as "
		"expected\n", argv[0], file2, dir2);
	return 1;
    } else if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) returned error as expected with %s\n",
		argv[0], file2, dir2, strerror(errno));
    }

    // testing moving directory to another not empty
    // first creates dir1
    ret = mkdir(dir1, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", argv[0], dir1,
		strerror(errno));
	return 1;
    }
    if (verbose == 1) {
	struct stat stat_buf;
	ret = stat(dir1, &stat_buf);
	if (ret == -1) {
	    fprintf(stderr, "%s: sstat(%s) failed: %s\n", argv[0], file,
		    strerror(errno));
	    return 1;
	}
	fprintf(stdout, "%s: mkdir(%s) succeeded (dir has mode 0x%x)\n",
		argv[0], dir1, (unsigned int) stat_buf.st_mode);
    }
    // second makes dir2 not empty by moving file2 into it
    file2lastpart = strrchr(file2, '/');
    if (file2lastpart == NULL) { // no / in string file2
	file2lastpart = file2;
    } else {
	file2lastpart++;
    }
    sprintf(file, "%s/%s", dir2, file2lastpart);
    ret = rename(file2, file);
    if (ret == -1) {
	fprintf(stderr, "%s: rename(%s, %s) failed: %s\n", argv[0], file2,
		file, strerror(errno));
	return 1;
    }
    if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) succeeded\n", argv[0], file2,
		file);
    }

    // finally tries dir1 to dir2 (should fail)
    ret = rename(dir1, dir2);
    if (ret != -1) {
	fprintf(stderr, "%s: rename(%s, %s) expected to return error %s not "
		"empty\n", argv[0], dir1, dir2, dir2);
	return 1;
    } else if (verbose == 1) {
	fprintf(stdout, "%s: rename(%s, %s) returned error as expected with %s\n",
		argv[0], dir1, dir2, strerror(errno));
    }

    // checking trying to move directory into one of its own subdirectories
    struct stat stat_buf;
    ret = stat("foodir", &stat_buf);
    if (ret == 0) {
	unlink("foodir");
    }
    ret =  mkdir("foodir", S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", argv[0], "foodir", strerror(errno));
	return 1;
    }
    ret = rename("foodir", "foodir/subdir");
    if (ret != -1) {
	fprintf(stderr, "%s: rename(%s, %s) should return -1, but it returned %d\n",
		argv[0], "foodir", "foodir/subdir", ret);
	return 1;
    }
    if (errno != EINVAL) {
	fprintf(stderr, "%s: rename(%s, %s) should return EINVAL, but it returned %d(%s)\n",
		argv[0], "foodir", "foodir/subdir", errno, strerror(errno));
	return 1;
    }
    
    fprintf(stdout, "%s: rename(foodir, foodir/subdir) failed with EINVAL as "
	    "expected(%s)\n", argv[0], strerror(errno));
 
    // checking with scenario used by ispell
    ret = chdir("foodir");
    if (ret != 0) {
	fprintf(stderr, "%s: chdir(\"foodir\") returned error: %s\n", argv[0], strerror(errno));
	return 1;
    }
    fprintf(stdout, "%s: succcessfully did chdir(\"foodir\")\n", argv[0]);
    char *names[4] = {"", ".bak", "", ""};
    int idx = 0;
    do {
	ret = rename(names[idx], names[idx+1]);
	if (ret != -1) {
	    fprintf(stderr, "%s: rename(\"%s\", \"%s\") should return -1, but it returned "
		    "%d\n", argv[0], names[idx], names[idx+1], ret);
	    (void)cleanUpDir("foodir", argv[0]);
	    return 1;
	} else if (errno != EINVAL) {
	    fprintf(stderr, "%s: rename(\"%s\", \"%s\") should return EINVAL, "
		    "but it returned %d(%s)\n", argv[0], names[idx], names[idx+1],
		    errno, strerror(errno));
	    (void)cleanUpDir("foodir", argv[0]);
	    return 1;
	}
	fprintf(stdout, "%s: rename(\"%s\", \"%s\") failed as expected with %s\n",
		    argv[0], names[idx], names[idx+1], strerror(errno));
	
	idx += 2;
    } while (idx < 4);
    
    if (cleanUpDir("foodir", argv[0]) != 0) {
	fprintf(stdout, "%s: failed in the end.\n", argv[0]);
    }
    
    fprintf(stdout, "%s: success\n", argv[0]);

    return 0;
}
