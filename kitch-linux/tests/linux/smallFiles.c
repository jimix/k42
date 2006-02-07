/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: smallFiles.c,v 1.2 2002/11/05 22:25:03 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * ***********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
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
    fprintf(stderr, "Usage: %s  [-v] [-t test_number] <n>\n"
	    "\tDefault is to run all %d tests\n"
	    "-v: verbose\n", prog, NUMBER_TESTS);
}

static int verbose = 0;

#define verbose_out(MSG...)                                          \
    if (verbose) { fprintf(stdout, MSG); }

/* test1: open truncate a file, and keep writing to it until it gets
 * larget than 4k */
static int
test1(char *file, char *prog)
{
    int fd;
    char buf[128];
    int j;
    size_t sz;
    char testname[255];
    int total_size = 0;

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
    while (total_size < 0xf00) {
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
	total_size += j;
    }
    verbose_out("%s: writing succeeded\n", testname);
    return 0;
}

/* test2 : open a small file for read, it should be a string of given char */
static int
test2(char *test, char *file, int expected_len, char expected_contents,
      char *prog)
{
    int fd;
    char buf[128];
    ssize_t sz;
    int size;
    char testname[255];

    sprintf(testname, "%s:%s", prog, test);

    fd = open(file, O_RDONLY);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: open(%s) succeeded\n", testname, file);

    verbose_out("%s: It is going to read\n", testname);
    size = 0;
    while ((sz = read(fd, buf, 128)) != 0) {
	int i;
	size += sz;
	if (size > expected_len) {
	    fprintf(stderr, "%s: %s has size (%d) greater than expected(%d)\n",
		    testname, file, expected_len, size);
	    return (1);
	}
	// FIXME: too stupid
	for (i = 0; i < sz; i++) {
	    if (buf[i] != expected_contents) {
		fprintf(stderr, "%s: data read different from expected\n", prog);
		return (1);
	    }
	}
    }
    if (size != expected_len) {
	    fprintf(stderr, "%s: %s has size (%d) different from expected(%d)\n",
		    testname, file, expected_len, size);
	    return (1);
    }

    verbose_out("%s succeeded.\n", testname);

    return 0;
}

/* test4: open file, write some to it, fork. The child writes more stuff,
 * the parent sleeps and then fstat (it should have size reflecting
 * child writes */
static int
test4(char *file, char *prog)
{
    int fd;
    char buf[512];
    struct stat stat_buf;
    int j, ret;
    size_t sz;
    char testname[255];
    pid_t pid;

    sprintf(testname, "%s:test4", prog);

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

static int
test6(char *file, int file_size, int file_content, char *prog)
{
    int fd1, fd2;
    char buf1[2], buf2[2];
    int i, j;
    char testname[255];

    sprintf(testname, "%s:test6", prog);

    fd1 = open(file, O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if (fd1 == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: first open(%s) succeeded\n", testname, file);

    fd2 = open(file, O_RDWR, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if (fd2 == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", testname, file,
		strerror(errno));
	return (1);
    }
    verbose_out("%s: second open(%s) succeeded\n", testname, file);

    buf1[0] = 'd';
    buf1[1] = '\0';

    j = write(fd1, buf1, 2);
    if (j == -1) {
	fprintf(stderr, "%s: write() %s: %s\n", testname,
		file, strerror(errno));
	return (1);
    } else if (j != 2) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %d\n",
		testname, file, j, 2);
	return (1);
    }
    verbose_out("%s: write succeeded\n", testname);

    i = read(fd2, buf2, 2);
    if (i == -1) {
	fprintf(stderr, "%s: write() %s: %s\n", testname,
		file, strerror(errno));
	return (1);
    } else if (i != 2) {
	fprintf(stderr, "write(): %s: %s : returned %d, expected %d\n",
		testname, file, i, 2);
	return (1);
    }
    verbose_out("%s: read succeeded\n", testname);
    if (strncmp(buf1, buf2, 2) != 0) {
	fprintf(stderr, "%s: values read differ from values written\n",
		testname);
    }

    verbose_out("%s: succeeded\n", testname);
    return 0;
}

static int
prepare_file(char *file, int file_size, char file_content, char *prog)
{
    int fd;
    int ret;
    ssize_t sz;
    int total_written = 0;
    char buf[128];
    struct stat stat_buf;

    fd = open(file, O_TRUNC | O_RDWR | O_CREAT,
	      S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

    if (fd == -1) {
	fprintf(stderr, "%s: open(): %s: %s\n", prog, file,
		strerror(errno));
	return (1);
    }

    memset(buf, file_content, 128);
    while (total_written != file_size) {
	int len = (file_size > 128 ?  128 : file_size);
	sz = write(fd, buf, len);
	if (sz != len) {
	    fprintf(stderr, "%s: error in prepared_file file_size %d\n",
		    prog, file_size);
	    return 1;
	}
	total_written += len;
    }
    close(fd);

    ret = stat(file, &stat_buf);
    if (ret == -1) {
	fprintf(stderr, "%s: stat(): %s: %s\n", prog, file,
		strerror(errno));
	return (1);
    } else if (stat_buf.st_size != file_size) {
	fprintf(stderr, "%s: stat() returned %d, file_size is %d\n",
		prog, (int) stat_buf.st_size, file_size);
	return (1);
    }
    return 0;
}

static void
delete_file(char *file, char *prog)
{
    if (unlink(file) == -1) {
	fprintf(stderr, "%s: unlink(): %s\n", file, strerror(errno));
    }
    verbose_out("File %s has been deleted\n", file);
}

typedef int (*Test_function_ptr)(char*, char*);
static Test_function_ptr test_functions[NUMBER_TESTS] = {test1, NULL, NULL,
							 test4, test5};

static int
invoke_test(int test_selected, char *file, char *prog)
{
    int ret;
    int file_size;
    char file_content;
    switch (test_selected) {
    case 2:
	file_size = 1024;
	file_content = '?';
	ret = prepare_file("small_file", file_size, file_content, prog);
	if (ret != 0) return 1;
	ret = test2("test2", "small_file", file_size, file_content, prog);
	// clean up
	delete_file("small_file", prog);
	break;
    case 3:
	file_size = 10240;
	file_content = '!';
	ret = prepare_file("large_file", file_size, file_content, prog);
	if (ret != 0) return 1;
	ret = test2("test3", "large_file", file_size, file_content, prog);
	// clean up
	delete_file("large_file", prog);
	break;
    case 6:
	file_size = 128;
	file_content = '*';
	ret = prepare_file("small_file", file_size, file_content, prog);
	if (ret != 0) return 1;
	ret = test6("small_file", file_size, file_content, prog);
	// clean up
	delete_file("small_file", prog);
	break;
    default:
	ret = test_functions[test_selected-1](file, prog);
    }
    return ret;
}

int
main(int argc, char *argv[])
{
    int c, i;
    extern int optind;
    const char *optlet = "vt";
    char file[255];
    int test_selected = 0;
    char *endptr;
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

    verbose_out("Starting tests\n");

    if (test_selected != 0) {
	sprintf(file, "foo%d", test_selected);
	if (invoke_test(test_selected, file, argv[0]) != 0) {
	    return 1;
	}
    } else { // run all tests
	for (i=1; i <= NUMBER_TESTS; i++) {
	    sprintf(file, "foo%d", i+1);
	    if (invoke_test(i, file, argv[0]) != 0) {
		return 1;
	    }
	}
    }

    fprintf(stdout, "%s: Success\n", argv[0]);
    return 0;
}

