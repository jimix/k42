/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fileSharing.c,v 1.9 2003/07/28 15:27:18 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * ***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
//#include <time.h>
#include <stdlib.h>

#define NUMBER_TESTS 6

static void
usage(const char *prog)
{
    fprintf(stderr, "Usage: %s  [-v] [-t test_number]\n"
	    "\tDefault is to run all %d tests\n"
	    "-v: verbose\n", prog, NUMBER_TESTS);
}

static int verbose = 0;

#define verbose_out(MSG...)                                          \
    if (verbose) { fprintf(stdout, MSG); }

/* test1 : while we have the file open, we do a stat based on path */
static int
test1(char *file, char *prog)
{
    int fd;
    char buf[512];
    struct stat stat_buf;
    int j, ret;
    size_t sz;
    char testname[255];

    sprintf(testname, "%s:test1", prog);

    fd = open(file, O_TRUNC | O_RDWR | O_CREAT,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) succeeded\n", testname, file);

    verbose_out("%s: It is going to write\n", testname);
    sz = sprintf(buf, "this is fd %d\n", fd);
    j = write(fd, buf, sz);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s (count=%ld)\n", testname,
		file, strerror(errno), sz);
	return (1);
    } else if (j != sz) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %ld\n",
		testname, file, j, sz);
	return (1);
    }
    verbose_out("%s: writing succeeded\n", testname);

    ret = stat(file, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (stat_buf.st_size != sz) {
	fprintf(stderr, "%s: stat(%s) returned size %ld; should be %ld\n",
		testname, file, (long) stat_buf.st_size, (long) sz);
	return (1);
    }

    ret = lseek(fd, 0, SEEK_CUR);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (ret != sz) {
	fprintf(stderr, "%s: lseek for %s returned  %ld; should be %ld\n",
		testname, file, (long) ret, (long) sz);
	return (1);
    }

    /* remove file used in the test */
    ret = unlink(file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", testname, file,
		strerror(errno));
    }

    verbose_out("%s: succeeded\n", testname);

    return 0;
}

/* test4 : as test 1, but first stat then write then stat again */
static int
test4(char *file, char *prog)
{
    int fd;
    char buf[512];
    struct stat stat_buf;
    int j, ret;
    size_t sz = 0;
    char testname[255];

    sprintf(testname, "%s:test4", prog);

    fd = open(file, O_TRUNC | O_RDWR | O_CREAT,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) succeeded\n", testname, file);

    verbose_out("%s: it's going to do first stat\n", testname);
    ret = stat(file, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (stat_buf.st_size != sz) {
	fprintf(stderr, "%s: stat(%s) returned size %ld; should be %ld\n",
		testname, file, (long) stat_buf.st_size, (long) sz);
	return (1);
    }

    verbose_out("%s: It is going to write\n", testname);
    sz = sprintf(buf, "this is fd %d\n", fd);
    j = write(fd, buf, sz);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s (count=%ld)\n", testname,
		file, strerror(errno), sz);
	return (1);
    } else if (j != sz) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %ld\n",
		testname, file, j, sz);
	return (1);
    }
    verbose_out("%s: writing succeeded\n", testname);

    ret = stat(file, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (stat_buf.st_size != sz) {
	fprintf(stderr, "%s: stat(%s) returned size %ld; should be %ld\n",
		testname, file, (long) stat_buf.st_size, (long) sz);
	return (1);
    }

    ret = lseek(fd, 0, SEEK_CUR);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (ret != sz) {
	fprintf(stderr, "%s: lseek for %s returned  %ld; should be %ld\n",
		testname, file, (long) ret, (long) sz);
	return (1);
    }

    /* remove file used in the test */
    ret = unlink(file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", testname, file,
		strerror(errno));
    }

    verbose_out("%s: succeeded\n", testname);

    return 0;
}

/* test2 : open file, lseek, dup in fd2, get current position in fd2 */
static int
test2(char *file, char *prog)
{
    int fd, fddup;
    int ret;
    char testname[255];
    int position = 3;

    sprintf(testname, "%s:test2", prog);

    fd = open(file, O_TRUNC | O_RDWR | O_CREAT,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) succeeded\n", testname, file);

    ret = lseek(fd, position, SEEK_SET);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (ret != position) {
	fprintf(stderr, "%s: lseek for %s returned  %ld; should be %ld\n",
		testname, file, (long) ret, (long) position);
	return (1);
    }
    verbose_out("%s: first lseek succeeded\n", testname);

    fddup = dup(fd);
    if (fddup == -1) {
	fprintf(stderr, "%s: dup for %s failed: %s\n", prog, file,
		strerror(errno));
	return(1);
    }
    verbose_out("%s: dup succeeded\n", testname);

    ret = lseek(fd, 0, SEEK_CUR);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (ret != position) {
	fprintf(stderr, "%s: lseek using first fd (for %s) returned  %ld; "
		"should be %ld\n", testname, file, (long) ret, (long) position);
	return (1);
    }
    verbose_out("%s: lseek using first fd succeeded \n", testname);

    ret = lseek(fddup, 0, SEEK_CUR);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }

    if (ret != position) {
	fprintf(stderr, "%s: lseek using second fd (for %s) returned  %ld; "
		"should be %ld\n", testname, file, (long) ret, (long) position);
	return (1);
    }
    verbose_out("%s: lseek using second fd succeeded \n", testname);

    /* remove file used in the test */
    ret = unlink(file);
    if (ret == -1) {
	fprintf(stderr, "%s: unlink(%s) failed: %s\n", testname, file,
		strerror(errno));
    }

    verbose_out("%s: succeeded\n", testname);

    return 0;
}

/* test3: open file, write to it, fork. The child writes more stuff,
 * the parent sleeps and then fstat (it should have size reflecting
 * child writes */
static int
test3(char *file, char *prog)
{
    int fd;
    char buf[512];
    struct stat stat_buf;
    int j, ret;
    size_t sz;
    char testname[255];
    pid_t pid;

    sprintf(testname, "%s:test3", prog);

    fd = open(file, O_TRUNC | O_RDWR | O_CREAT,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) succeeded\n", testname, file);

    verbose_out("%s: It is going to write\n", testname);
    sz = sprintf(buf, "this is fd %d\n", fd);
    j = write(fd, buf, sz);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s (count=%ld)\n", testname,
		file, strerror(errno), sz);
	return (1);
    } else if (j != sz) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %ld\n",
		testname, file, j, sz);
	return (1);
    }
    verbose_out("%s: writing succeeded\n", testname);

    verbose_out("%s: it's going to fork\n", testname);
    if ((pid = fork()) == 0) {
	// child
	verbose_out("%s: fork() succeeded\n", testname);
	// write and sleep
	j = write(fd, buf, sz);
	if (j == -1) {
	    fprintf(stderr, "%s: write() %s: %s (count=%ld)\n", testname,
		    file, strerror(errno), sz);
	    return (1);
	} else if (j != sz) {
	    fprintf(stderr, "write(): %s: %s : returned %d, expected %ld\n",
		    testname, file, j, sz);
	    return (1);
	}
	verbose_out("%s:writing by child succeeded\n", testname);
	sleep(2);
	verbose_out("%s:child is finished\n", testname);
	exit(0);
    } else {
	// give oportunity to child to perform write
	sleep(1);
	ret = fstat(fd, &stat_buf);
	if (ret == -1) {
	    fprintf(stderr, "%s: fstat(): %s: %s\n", testname, file,
		    strerror(errno));
	    return (1);
	}
	if (stat_buf.st_size != 2*sz) {
	    fprintf(stderr, "%s: fstat(%s) returned size %ld; should be %ld\n",
		    testname, file, (long) stat_buf.st_size, (long) 2*sz);
	    return (1);
	}
    }

    verbose_out("%s: succeeded\n", testname);

    return 0;
}

/*  test5: open file, write, another open, fstat has to return new size */
static int
test5(char *file, char *prog)
{
    int fd, fd2;
    char buf[512];
    struct stat stat_buf;
    int j, ret;
    size_t sz;
    char testname[255];

    sprintf(testname, "%s:test5", prog);

    fd = open(file, O_TRUNC | O_RDWR | O_CREAT,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) succeeded\n", testname, file);

    verbose_out("%s: It is going to write\n", testname);
    sz = sprintf(buf, "this is fd %d\n", fd);
    j = write(fd, buf, sz);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s (count=%ld)\n", testname,
		file, strerror(errno), sz);
	return (1);
    } else if (j != sz) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %ld\n",
		testname, file, j, sz);
	return (1);
    }
    verbose_out("%s: writing succeeded\n", testname);

    verbose_out("%s: it's going to open again\n", testname);

    fd2 = open(file, O_RDWR | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd2 == -1) {
	fprintf(stderr, "%s: second open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: second open(%s) succeeded\n", testname, file);

    ret = fstat(fd2, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: fstat(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    if (stat_buf.st_size != sz) {
	fprintf(stderr, "%s: fstat(%s) returned size %ld; should be %ld\n",
		testname, file, (long) stat_buf.st_size, (long) sz);
	return (1);
    }

    verbose_out("%s: succeeded\n", testname);

    return 0;
}

/* test6: generates a file; three open file for read, perform read;
 * then one for write. The writer
 * modifies file, other guys with open for read finally read and
 * they should see up-to-date data */
static int
test6(char *file, char *prog)
{
    int fd, fdr[3], fdw;
    char buf[512], bufr[512];
    int i, j, n, ret;
    ssize_t size;
    struct stat stat_buf;
    char testname[255];

    sprintf(testname, "%s:test6", prog);

    fd = open(file, O_TRUNC | O_RDWR | O_CREAT,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) succeeded\n", testname, file);

    verbose_out("%s: It is going to write 8k of '?' to the file\n", testname);
    memset(buf, '?', sizeof(buf));
    for (i = 0; i < 16; i++) {
	j = write(fd, buf, sizeof(buf));
	if (j == -1) {
	    fprintf(stderr, "%s: write() %s: %s (i is %d)\n", testname,
		    file, strerror(errno), i);
	    return (1);
	} else if (j != sizeof(buf)) {
	    fprintf(stderr, "write(): %s: %s : returned %d, expected %ld "
		    "(i is %d)\n", testname, file, j, sizeof(buf), i);
	    return (1);
	}
    }
    close(fd);
    verbose_out("%s: file %s closed\n", testname, file);

    for (i = 0 ; i < 3; i++) {
	verbose_out("%s: it's going to open for read i %d\n", testname, i);

	fdr[i] = open(file, O_RDONLY);

	if (fdr[i] == -1) {
	    fprintf(stderr, "%s: open() for i %d: %s: %s\n", testname, i, file,
		    strerror(errno));
	    return (1);
	}
	verbose_out("%s: i %d open(%s) succeeded\n", testname, i, file);
	verbose_out("%s: it's going to read 1 byte from fdr[%d]\n", testname, i);
	j = read(fdr[i], bufr, 1);
	if (j == -1) {
	    fprintf(stderr, "%s: read() for fdr[%d]: %s\n", testname,
		    i, strerror(errno));
	    return (1);
	} else if (j != 1) {
	    fprintf(stderr, "%s: read() for fdr[%d] returned %d, expected %d\n",
		    testname, i , j, 1);
	    return (1);
	}
    }

    verbose_out("%s: it's going to open for write\n", testname);
    fdw = open(file, O_RDWR);
    if (fdw == -1) {
	fprintf(stderr, "%s: open() for write: %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) for write succeeded\n", testname, file);

    verbose_out("%s: going to write in the begining of file\n", testname);

    sprintf(buf, "TESTING: %s", file);
    n = strlen(buf);
    verbose_out("buf is %s, n is %d\n", buf, n);
    j = write(fdw, buf, n);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s\n", testname,
		file, strerror(errno));
	return (1);
    } else if (j != n) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %d "
		"(i is %d)\n", testname, file, j, n, i);
	return (1);
    }

    for (i = 0; i < 3; i++) {
	ret = lseek(fdr[i], 0, SEEK_SET);
	if (ret == -1) {
	    fprintf(stderr, "%s: lseek on fdr[i] failed: %s\n", testname,
		    strerror(errno));
	    return (1);
	}

	j = read(fdr[i], bufr, n);
	if (j == -1) {
	    fprintf(stderr, "%s: read() %s: %s (i %d)\n", testname,
	    file, strerror(errno), i);
	    return (1);
	} else if (j != n) {
	    fprintf(stderr, "read(): %s: %s : returned %d, expected %d "
		    "(i is %d)\n", testname, file, j, n, i);
	    return (1);
	}
	if (strncmp(bufr, buf, n) != 0) {
	    fprintf(stderr, "%s: read string doesn't match expected. Initial "
		    "char read are %c%c%c\n", testname, bufr[0], bufr[1],
		    bufr[2]);
	    return (1);
	}
    }
    verbose_out("%s: in terms of data file sharing succeeded\n", testname);

    // writing more so we change file size
    verbose_out("%s: writing to the end of the file\n", testname);
    ret = lseek(fdw, 0, SEEK_END);
    if (ret == -1) {
	fprintf(stderr, "%s: lseek on fdw failed: %s\n", testname,
		strerror(errno));
	return (1);
    }
    j = write(fdw, buf, n);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s\n", testname,
		file, strerror(errno));
	return (1);
    } else if (j != n) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %d "
		"(i is %d)\n", testname, file, j, n, i);
	return (1);
    }

    ret = fstat(fdw, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: fstat on fdw failed with %s\n", testname,
		strerror(errno));
	return (1);
    }
    size = stat_buf.st_size;

    verbose_out("%s: Now comparing size as seen through other file "
		"descriptors\n", testname);
    for (i = 0; i < 3; i ++) {
	ret = fstat(fdr[i], &stat_buf);
	if (ret == -1) {
	    fprintf(stderr, "%s: fstat on fdr[%d] failed with %s\n", testname,
		    i, strerror(errno));
	    return (1);
	}
	if (stat_buf.st_size != size) {
	    fprintf(stderr, "%s: fstat on fdr[%d] returned %d, expected %d\n",
		    testname, i, (int) stat_buf.st_size, (int) size);
	    return (1);
	}
    }
    verbose_out("%s: succeeded\n", testname);

    return 0;
}

int
main(int argc, char *argv[])
{
    int c, i, ret;
    extern int optind;
    const char *optlet = "vt";
    char file[255];
    struct statfs statfs_buf;
    int test_selected = 0;
    char *endptr;
    typedef int (*Test_function_ptr)(char*, char*);
    Test_function_ptr test_functions[NUMBER_TESTS]
	= {test1, test2, test3, test4, test5, test6};

    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	case 'v':
	    verbose = 1;
	    break;
	case 't':
	    if (optind == argc) {
		fprintf(stderr, "%s: missing test number\n", argv[0]);
		usage(argv[0]);
		return (1);
	    }
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
	return (1);
    }

    ret = statfs(".", &statfs_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: statfs returned error: %s\n", argv[0],
		strerror(errno));
	return (1);
    }

    if (statfs_buf.f_type == 0x6969) { // NFS_SUPER_MAGIC
	fprintf(stdout, "%s: No need to run this test in a NFS file system\n",
		argv[0]);
	return 0;
    } else if (statfs_buf.f_type == 0x42) { // KFS_SUPER_MAGIC
	fprintf(stdout, "%s: Not running this test in a KFS file system\n",
		argv[0]);
	return 0;
    }

    if (test_selected != 0) {
	sprintf(file, "foo%d", test_selected);
	if (test_functions[test_selected-1](file, argv[0]) != 0) {
	    return 1;
	}
    } else { // run all tests
	for (i=0; i < NUMBER_TESTS; i++) {
	    sprintf(file, "foo%d", i+1);
	    if (test_functions[i](file, argv[0]) != 0) {
		return 1;
	    }
	}
    }

    fprintf(stdout, "%s: Success\n", argv[0]);
    return 0;
}

