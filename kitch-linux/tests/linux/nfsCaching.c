/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: nfsCaching.c,v 1.5 2002/11/05 22:25:02 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Runs tests for dealing with NFS invalidations on server
 * test1: write to a file, stat, fake remote write, stat, check size
 * test2: write to a file, stat, fake remote unlink, fstat should get ESTALE,
 *        then stat should get ENOENT
 * test3: write to a file, stat, fake remote link to other name, fake remote
 *        unlink of original name, open & write to the file again, then stat
 *        (should get size as it just wrote, not original size plus recently
 *        written amount)
 * test4: open/creat, fchown, remote unlink, remote open/create; fchown (should
 *        fail), fchown (fail again), chown (should succeed)
 * test5: open/creat, fstat, remote unlink, remote open/create; fstat (should
 *        fail), fstat (fail again), stat (should succeed)
 * test6: mkdir, open, fstat, fchown, remote rmdir, remote mkdir; fchown (should
 *        fail), chown (should succeed)
 * test7: mkdir, open, fstat, remote rmdir, remote mkdir; fstat (should
 *        fail), stat (should succeed)
 * ***********************************************************************/

#include <stdio.h>
#define __USE_GNU
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/vfs.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>

#define NUMBER_TESTS  7

static void
usage(const char *prog)
{
    fprintf(stdout, "Usage: %s  [-v] [-t n>  <n>\n"
	    "\tRun nfs cache validation tests\n"
	    "\t-v: verbose\n"
	    "\t-t n: runs only test number n (n should be in 1-%d\n", prog,
	    NUMBER_TESTS);
}

static void
verbose_out(int verbose, const char *msg)
{
    if (verbose) {
	fprintf(stdout, "%s\n", msg);
    }
}

static void
let_time_pass(int nsegs, int verbose)
{
    time_t time1, time2;
    time1 = time(NULL);
    if (verbose) {
	fprintf(stdout, "It'll invoke sleep(%d). Time is 0x%lx\n",
		nsegs, (long) time1);
    }
    sleep(nsegs);
    time2 = time(NULL);
    if (verbose) {
	fprintf(stdout, "Returned from sleep. Difference in time is %d\n",
		(int)(time2 - time1));
    }

}
// write to a file, stat, fake remote write, stat, check size
static int
test1(int verbose, char *prog, char *remote_cwd)
{
    int i, fd, ret, fakefd;
    char buf[256];
    char remote_file[256], file[32];
    struct stat stat_buf;
    char test_name[128];

    if (verbose) {
	fprintf(stdout, "%s: started test1 (write to a file, stat, fake"
		" remote write, stat, check size)\n", prog);
    }

    sprintf(test_name, "%s:test1", prog);
    sprintf(file, "file1");
    for (i = 0; i < 256; buf[i++] = i);

    fd = open(file, O_RDWR | O_CREAT| O_TRUNC,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    ret = write(fd, buf, sizeof(buf));
    if (ret != sizeof(buf)) {
	if (ret == -1) {
	    fprintf(stderr, "%s: write(%s) failed: %s\n", test_name, file,
		    strerror(errno));
	} else {
	    fprintf(stderr, "%s: write(%s) returned %d, expected %ld\n",
		    test_name, file, ret, sizeof(buf));
	}
	return 1;
    }

    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }
    // silly test to make sure stat got us right value
    if (stat_buf.st_size != sizeof(buf)) {
	fprintf(stderr, "%s: stat(%s) returned %ld, expected %ld\n", test_name,
		file, stat_buf.st_size, sizeof(buf));
	return 1;
    }

    // make sure write got to remote server
    ret = fsync(fd);
    if (ret == -1) {
	fprintf(stderr, "%s: fsync for %s failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    // fake remote write through opening via other NFS mound
    sprintf(remote_file, "%s/%s", remote_cwd, file);
    fakefd = open(remote_file, O_RDWR,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fakefd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }
    ret = lseek(fakefd, 0, SEEK_END);
    if (ret != sizeof(buf)) {
	if (ret == -1) {
	    fprintf(stderr, "%s: lseek end for %s failed: %s\n", test_name,
		    remote_file, strerror(errno));
	} else {
	    fprintf(stderr, "%s: lseek for %s returned %d, expected %ld\n",
		    test_name, remote_file, ret, sizeof(buf));
	}
	return 1;
    }

    // wait before writing, otherwise we could have same mtime
    let_time_pass(3, verbose);

    ret = write(fakefd, buf, sizeof(buf));
    if (ret != sizeof(buf)) {
	if (ret == -1) {
	    fprintf(stderr, "%s: write for %s failed: %s\n", test_name,
		    remote_file, strerror(errno));
	} else {
	    fprintf(stderr, "%s: write for %s returned %d, expected %ld\n",
		    test_name, remote_file, ret, sizeof(buf));
	}
	return 1;
    }

    // make sure write got to remote server
    ret = fsync(fakefd);
    if (ret == -1) {
	fprintf(stderr, "%s: fsync for %s failed: %s\n", test_name,
		remote_file, strerror(errno));
	return 1;
    }

    // let some time to pass, to make sure we are over nfs cache timeout (that
    // now should be still TIMEOUT_MIN, something like 3 sec
    let_time_pass(30, verbose);

    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) after sleep failed: %s\n", test_name,
		file, strerror(errno));
	return 1;
    }
    // size should be 2*sizeof(buf)
    if (stat_buf.st_size != 2*sizeof(buf)) {
	fprintf(stderr, "%s: we didn't catch remote change: got size %ld, "
		"expected %ld\n", test_name, stat_buf.st_size, 2*sizeof(buf));
	return 1;
    }

    // clean up
    close(fd);
    close(fakefd);
    ret = unlink(file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    verbose_out(verbose, "test1 succeeded\n");
    return 0;
}

/* write to a file, stat, fake remote unlink, fstat should get ESTALE,
 * then stat should get ENOENT */
static int
test2(int verbose, char *prog, char *remote_cwd)
{
    int i, fd, ret;
    char buf[256];
    char remote_file[256], file[32];
    struct stat stat_buf;
    char test_name[128];

    if (verbose) {
	fprintf(stdout, "%s: started test2 (write to a file, stat, fake"
		" remote unlink, fstat should get ESTALE, then stat "
		"(should get size 0))\n", prog);
    }

    sprintf(test_name, "%s:test2", prog);
    sprintf(file, "file2");
    for (i = 0; i < 256; buf[i++] = i);

    fd = open(file, O_RDWR | O_CREAT| O_TRUNC,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    ret = write(fd, buf, sizeof(buf));
    if (ret != sizeof(buf)) {
	if (ret == -1) {
	    fprintf(stderr, "%s: write(%s) failed: %s\n", test_name, file,
		    strerror(errno));
	} else {
	    fprintf(stderr, "%s: write(%s) returned %d, expected %ld\n",
		    test_name, file, ret, sizeof(buf));
	}
	return 1;
    }

    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }
    // silly test to make sure stat got us write value
    if (stat_buf.st_size != sizeof(buf)) {
	fprintf(stderr, "%s: stat(%s) returned %ld, expected %ld\n", test_name,
		file, stat_buf.st_size, sizeof(buf));
	return 1;
    }

    // make sure write got to remote server
    ret = fsync(fd);
    if (ret == -1) {
	fprintf(stderr, "%s: fsync for %s failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    // fake remote unlink through opening via other NFS mound
    sprintf(remote_file, "%s/%s", remote_cwd, file);
    ret = unlink(remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }

    // let some time to pass, to make sure we are over nfs cache timeout (that
    // now should be still TIMEOUT_MIN, something like 3 sec
    let_time_pass(30, verbose);

    ret = fstat(fd, &stat_buf);
    if (ret != -1) {
	fprintf(stderr, "%s: fstat for %s expected to failure with STALE"
		" after unlink(%s), but it returned with size %d\n",
		test_name, file, remote_file, (int) stat_buf.st_size);
	return 1;
    } else if (errno != ESTALE) {
	fprintf(stderr, "%s: fstat for %s expected to failure with STALE"
		" after unlink(%s), but got error %s\n", test_name, file,
		remote_file, strerror(errno));
	return 1;
    }

    ret = stat(file, &stat_buf);
    if (ret != -1) {
	fprintf(stderr, "%s: stat(%s) expected to failure with ENOENT"
		" after unlink(%s), but it returned with size %d\n",
		test_name, file, remote_file, (int) stat_buf.st_size);
	return 1;
    } else if (errno != ENOENT) {
	fprintf(stderr, "%s: stat(%s) expected to failure with ENOENT"
		" after unlink(%s), but got error %s\n", test_name, file,
		remote_file, strerror(errno));
	return 1;
    }

    close(fd);
    verbose_out(verbose, "test2 succeeded\n");

    return 0;
}

/* write to a file, fstat, fake remote link to other name, fake remote
 * unlink of original name, open & write to the file again, fstat (should get
 * size as it just wrote, not original size plus recently written amount) */
static int
test3(int verbose, char *prog, char *remote_cwd)
{
    int i, fd, ret;
    char buf[256];
    char remote_file[256], file[32], other_remote_file[256];
    struct stat stat_buf;
    char test_name[128];

    if (verbose) {
	fprintf(stdout, "%s: started test3 (write to a file, fstat, fake"
		" remote link other name, fake remote unlink of original "
		"name, open & write to the file again, fstat (should get "
		"size as it just wrote, not original size plus recently "
		"written amout)\n", prog);
    }

    sprintf(test_name, "%s:test3", prog);
    sprintf(file, "file3");
    for (i = 0; i < 256; buf[i++] = i);

    fd = open(file, O_RDWR | O_CREAT| O_TRUNC,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    ret = write(fd, buf, sizeof(buf));
    if (ret != sizeof(buf)) {
	if (ret == -1) {
	    fprintf(stderr, "%s: write(%s) failed: %s\n", test_name, file,
		    strerror(errno));
	} else {
	    fprintf(stderr, "%s: write(%s) returned %d, expected %ld\n",
		    test_name, file, ret, sizeof(buf));
	}
	return 1;
    }

    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }
    // silly test to make sure stat got us write value
    if (stat_buf.st_size != sizeof(buf)) {
	fprintf(stderr, "%s: stat(%s) returned %ld, expected %ld\n", test_name,
		file, stat_buf.st_size, sizeof(buf));
	return 1;
    }

    // make sure write got to remote server
    ret = fsync(fd);
    if (ret == -1) {
	fprintf(stderr, "%s: fsync for %s failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    // fake remote link to other name
    sprintf(remote_file, "%s/%s", remote_cwd, file);
    sprintf(other_remote_file, "%s/%s-other", remote_cwd, file);
    ret = link(remote_file, other_remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: link(%s, %s) failed: %s\n", test_name,
		remote_file, other_remote_file, strerror(errno));
	return 1;
    }

    //fake remote unlink of original name
    ret = unlink(remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }

    // let some time to pass, to make sure we are over nfs cache timeout (that
    // now should be still TIMEOUT_MIN, something like 3 sec
    let_time_pass(30, verbose);

    // open and write again
    fd = open(file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    ret = write(fd, buf, sizeof(buf));
    if (ret != sizeof(buf)) {
	if (ret == -1) {
	    fprintf(stderr, "%s: write(%s) failed: %s\n", test_name, file,
		    strerror(errno));
	} else {
	    fprintf(stderr, "%s: write(%s) returned %d, expected %ld\n",
		    test_name, file, ret, sizeof(buf));
	}
	return 1;
    }

    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }
    // silly test to make sure stat got us write value
    if (stat_buf.st_size != sizeof(buf)) {
	fprintf(stderr, "%s: stat(%s) returned %ld, expected %ld\n", test_name,
		file, stat_buf.st_size, sizeof(buf));
	return 1;
    }

    close(fd);
    /* get rid of files used in the test */
    ret = unlink(other_remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name,
		other_remote_file, strerror(errno));
	return 1;
    }
    ret = unlink(file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name,
		file, strerror(errno));
	return 1;
    }
    verbose_out(verbose, "test3 succeeded\n");

    return 0;
}

/* open/creat, fchown, remote unlink, remote open/create; fchown (should
 * fail), fchown (fail again) chown (should succeed) */
static int
test4(int verbose, char *prog, char *remote_cwd)
{
    int fd, ret, fd_remote;
    char remote_file[256], file[32];
    struct stat stat_buf;
    char test_name[128];

    if (verbose) {
	fprintf(stdout, "%s: started test4 ( open/creat, fchown, remote unlink,"
		" remote open/create; fchown (should fail), fchown (fail"
		" again), chown (should succeed)\n", prog);
    }

    sprintf(test_name, "%s:test4", prog);
    sprintf(file, "file4");
    sprintf(remote_file, "%s/%s", remote_cwd, file);

    fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    /* stat to get current uid, so we ask to change for the same value (we
     * want to check if the operationg went through, so this is enough) */
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    // fchown
    ret = fchown(fd, stat_buf.st_uid, stat_buf.st_gid);
    if (ret == -1) {
	fprintf(stderr, "%s: fchown for %s failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    // fake remote unlink
    ret = unlink(remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }

    fd_remote = open(remote_file, O_RDWR | O_CREAT| O_TRUNC,
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd_remote == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }

    // do another fchown (should fail)
    ret = fchown(fd, stat_buf.st_uid, stat_buf.st_gid);
    if (ret != -1) {
	fprintf(stderr, "%s: fchown for %s was expected to fail, but it "
		"succeeded\n", test_name, file);
	return 1;
    }

    // fchown once again (should fail)
    ret = fchown(fd, stat_buf.st_uid, stat_buf.st_gid);
    if (ret != -1) {
	fprintf(stderr, "%s: fchown for %s was expected to fail, but it "
		"succeeded\n", test_name, file);
	return 1;
    }

    ret = chown(file, stat_buf.st_uid, stat_buf.st_gid);
    if (ret == -1) {
	fprintf(stderr, "%s: chown(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    close(fd_remote);
    ret = unlink(remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }

    verbose_out(verbose, "test4 succeeded\n");

    return 0;
}

/* open/creat, fstat, remote unlink, remote open/create; fstat (should
 * fail), fstat (fail again), stat (should succeed) */
static int
test5(int verbose, char *prog, char *remote_cwd)
{
    int fd, ret, fd_remote;
    char remote_file[256], file[32];
    struct stat stat_buf;
    char test_name[128];

    if (verbose) {
	fprintf(stdout, "%s: started test5 ( open/creat, fstat, remote unlink,"
		" remote open/create; fstat (should fail), fstat (fail again), "
		"stat (should succeed)\n", prog);
    }

    sprintf(test_name, "%s:test5", prog);
    sprintf(file, "file5");
    sprintf(remote_file, "%s/%s", remote_cwd, file);

    fd = open(file, O_RDWR | O_CREAT | O_TRUNC,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    /* stat to get current uid, so we ask to change for the same value (we
     * want to check if the operationg went through, so this is enough) */
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    // fake remote unlink
    ret = unlink(remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }

    fd_remote = open(remote_file, O_RDWR | O_CREAT| O_TRUNC,
		     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (fd_remote == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }

    // let some time to pass, to make sure we are over nfs cache timeout (that
    // now should be still TIMEOUT_MIN, something like 3 sec
    let_time_pass(30, verbose);

    // do another fstat (should fail)
    ret = fstat(fd, &stat_buf);
    if (ret != -1) {
	fprintf(stderr, "%s: fstat for %s was expected to fail, but it "
		"succeeded\n", test_name, file);
	return 1;
    }

    // once again fstat (should fail)
    ret = fstat(fd, &stat_buf);
    if (ret != -1) {
	fprintf(stderr, "%s: fstat for %s was expected to fail, but it "
		"succeeded\n", test_name, file);
	return 1;
    }

    ret = stat(file, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, file,
		strerror(errno));
	return 1;
    }

    close(fd_remote);
    ret = unlink(remote_file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", test_name, remote_file,
		strerror(errno));
	return 1;
    }
    verbose_out(verbose, "test5 succeeded\n");

    return 0;
}

/* mkdir, open, fstat, fchown, remote rmdir, remote mkdir; fchown (should
 * fail), chown (should succeed) */
static int
test6(int verbose, char *prog, char *remote_cwd)
{
    int fd, ret;
    char remote_dir[256], dir[32];
    struct stat stat_buf;
    char test_name[128];

    if (verbose) {
	fprintf(stdout, "%s: started test6 (mkdir, open, fstat ,fchown, remote "
		"rmdir, remote mkdir; fchown (should fail), chown (should "
		"succeed)\n", prog);
    }

    sprintf(test_name, "%s:test6", prog);
    sprintf(dir, "dir6");
    sprintf(remote_dir, "%s/%s", remote_cwd, dir);

    ret = mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }
    fd = open(dir, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }

    /* stat to get current uid, so we ask to change for the same value (we
     * want to check if the operationg went through, so this is enough) */
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }

    // fchown
    ret = fchown(fd, stat_buf.st_uid, stat_buf.st_gid);
    if (ret == -1) {
	fprintf(stderr, "%s: fchown for %s failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }

    // fake remote rmdir
    ret = rmdir(remote_dir);
    if (ret == -1) {
	fprintf(stderr, "%s: rmdir(%s) failed: %s\n", test_name, remote_dir,
		strerror(errno));
	return 1;
    }

    ret = mkdir(remote_dir, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", test_name, remote_dir,
		strerror(errno));
	return 1;
    }

    // do another fchown (should fail)
    ret = fchown(fd, stat_buf.st_uid, stat_buf.st_gid);
    if (ret != -1) {
	fprintf(stderr, "%s: fchown for %s was expected to fail, but it "
		"succeeded\n", test_name, dir);
	return 1;
    }

    ret = chown(dir, stat_buf.st_uid, stat_buf.st_gid);
    if (ret == -1) {
	fprintf(stderr, "%s: chown(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }

    // remove directory
    ret = rmdir(remote_dir);
    if (ret == -1) {
	fprintf(stderr, "%s: final rmdir(%s) failed: %s\n", test_name, remote_dir,
		strerror(errno));
	return 1;
    }

    verbose_out(verbose, "test6 succeeded\n");

    return 0;
}

/* mkdir, open, fstat, remote rmdir, remote mkdir; fstat (should
 * fail), stat (should succeed) */
static int
test7(int verbose, char *prog, char *remote_cwd)
{
    int fd, ret;
    char remote_dir[256], dir[32];
    struct stat stat_buf;
    char test_name[128];

    if (verbose) {
	fprintf(stdout, "%s: started test7 (mkdir, open, fstat, remote "
		"rmdir, remote mkdir; fstat (should fail), stat (should "
		"succeed)\n", prog);
    }

    sprintf(test_name, "%s:test7", prog);
    sprintf(dir, "dir7");
    sprintf(remote_dir, "%s/%s", remote_cwd, dir);

    ret = mkdir(dir, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }
    fd = open(dir, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }

    /* stat to get current uid, so we ask to change for the same value (we
     * want to check if the operationg went through, so this is enough) */
    ret = fstat(fd, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }

    // fake remote rmdir
    ret = rmdir(remote_dir);
    if (ret == -1) {
	fprintf(stderr, "%s: rmdir(%s) failed: %s\n", test_name, remote_dir,
		strerror(errno));
	return 1;
    }

    ret = mkdir(remote_dir, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (ret == -1) {
	fprintf(stderr, "%s: mkdir(%s) failed: %s\n", test_name, remote_dir,
		strerror(errno));
	return 1;
    }

    // let some time to pass, to make sure we are over nfs cache timeout (that
    // now should be still TIMEOUT_MIN, something like 3 sec
    let_time_pass(30, verbose);

    // do another fstat (should fail)
    ret = fstat(fd, &stat_buf);
    if (ret != -1) {
	fprintf(stderr, "%s: fstat for %s was expected to fail, but it "
		"succeeded\n", test_name, dir);
	return 1;
    }

    ret = stat(dir, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(%s) failed: %s\n", test_name, dir,
		strerror(errno));
	return 1;
    }

    // remove directory
    ret = rmdir(remote_dir);
    if (ret == -1) {
	fprintf(stderr, "%s: final rmdir(%s) failed: %s\n", test_name, remote_dir,
		strerror(errno));
	return 1;
    }

    verbose_out(verbose, "test7 succeeded\n");

    return 0;
}

int
main(int argc, char *argv[])
{
    int c, i;
    extern int optind;
    const char *optlet = "vt";
    char *endptr;
    int verbose = 0, test_selected = 0;
    struct statfs statfs_buf;
    char buf[256], remote_cwd[256];
    char *retc;
    int dfd;
    typedef int (*Test_function_ptr)(int, char*, char*);
    Test_function_ptr test_functions[NUMBER_TESTS]
	= {test1, test2, test3, test4, test5, test6, test7};

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'v':
	    verbose = 1;
	    break;
	case 't':
	    test_selected = (int) strtol(argv[optind], &endptr, 10);
	    if (*endptr != '\0' || errno == ERANGE) {
		fprintf(stderr, "%s: test number provided (%s) is invalid\n",
			argv[0], argv[optind]);
		usage(argv[0]);
		return 1;
	    } else if (test_selected < 1 || test_selected > NUMBER_TESTS) {
		fprintf(stderr, "%s: test number should be 1-%d (is %d)\n",
			argv[0], NUMBER_TESTS, test_selected);
		usage(argv[0]);
		return 1;
	    }
	    optind++;
	    break;
	case '?':
	default:
	    usage(argv[0]);
	    return (1);
	}
    }

    if (optind != argc) {
	usage(argv[0]);
	return 1;
    }

    if (statfs(".", &statfs_buf) == -1) {
	fprintf(stderr, "%s: statfs failed: %s\n", argv[0], strerror(errno));
	return 1;
    }

    if (statfs_buf.f_type != 0x6969) {
	fprintf(stderr, "%s: Error. This program should run from a NFS\n"
		" mounted directory\n", argv[0]);
	return 1;
    }

    verbose_out(verbose, "Check to see if program is running from a NFS"
		" file system succeeded\n");

    // we know we are in a NFS mounted directory. We need to know
    // which directory so we can mess with the same files through the
    // second NFS mount
    retc = getcwd(buf, sizeof(buf));
    if (retc == NULL) {
	fprintf(stderr, "%s: getcwd failed, buffer (size %d) too small\n",
		argv[0], (int) sizeof(buf));
	return 1;
    }
    if (verbose) {
	fprintf(stdout, "getcwd returned %s\n", buf);
    }

    if (strlen(buf) + strlen("/nfs-otherRoot") >= sizeof(buf)) {
	fprintf(stderr, "%s: buffer (size %d) too small for "
		"/nfs-otherRoot%s\n", argv[0], (int) sizeof(buf), buf);
	return 1;
    }

    sprintf(remote_cwd, "/nfs-otherRoot%s", buf);
    dfd = open(remote_cwd, O_DIRECTORY);
    if (dfd == -1) {
	fprintf(stderr, "%s: opening directory %s failed: %s\n",
		argv[0], remote_cwd, strerror(errno));
	return 1;
    }
    verbose_out(verbose, "Opening directory corresponding cwd in "
		"/nfs-otherRoot tree succeeded\n");

    if (test_selected != 0) {
	if (test_functions[test_selected-1](verbose, argv[0], remote_cwd) != 0) {
	    return 1;
	}
    } else { // run all tests
	for (i=0; i < NUMBER_TESTS; i++) {
	    if (test_functions[i](verbose, argv[0], remote_cwd) != 0) {
		return 1;
	    }
	}
    }

    fprintf(stdout, "%s: success\n", argv[0]);

    return 0;
}
