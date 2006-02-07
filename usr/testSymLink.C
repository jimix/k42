/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testSymLink.C,v 1.2 2003/08/29 17:54:25 okrieg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: test symbolic links
 ****************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
// #include <sys/sysIncs.H>

void
testLinkDirectory()
{
    int rc, fd;
    ssize_t res;
    char buf[1024];
    
    fprintf(stderr, "testing linking to directory\n");
    rc = mkdir("targetdir", 00777);
    if (rc != 0) {
	perror("ERROR:mkdir failed");
	_exit(1);
    }

    rc = symlink("targetdir", "linktotarget");
    if (rc != 0) {
	perror("ERROR:symlink failed");
	_exit(1);
    }
    
    fd = open("linktotarget", O_RDONLY);
    if (fd < 0) {
	perror("ERROR:symlink failed");
	fprintf(stderr, "ERROR:errno is %d\n", errno);
	_exit(1);
    }

    res = read(fd, buf, 1024);
    if (res < 0) {
	if (errno != EISDIR) {
	    fprintf(stderr, "ERROR:FAILURE: expected EISDIR when reading symlink"
		" pointing to directory\n");
	}
    } else {
	fprintf(stderr, "ERROR:FAILURE: succeeded in reading symlink pointing"
		" to directory\n");
    }

    rc = rmdir("targetdir");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }

    rc = unlink("linktotarget");
    if (rc != 0) {
	perror("ERROR:unlink of symlink failed");
	_exit(1);
    }
    fprintf(stderr, "done\n");
}

int
compareStatus(struct stat *st1, struct stat *st2, int fifdiff) 
{
    // fifdiff == 1 if failes when same
    if(st1->st_dev != st2->st_dev) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_dev differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_ino != st2->st_ino) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_ino differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_mode != st2->st_mode) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_mode differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_nlink != st2->st_nlink) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_nlink differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_uid != st2->st_uid) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_uid differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_gid != st2->st_gid) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_gid differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_rdev != st2->st_rdev) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_rdev differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_size != st2->st_size) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_size differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_blksize != st2->st_blksize) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_blksize differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_blocks != st2->st_blocks) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_blocks differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_atime != st2->st_atime) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_atime differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_mtime != st2->st_mtime) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_mtime differs\n");
	    _exit(1);
	}
	return 1;
    }
    if(st1->st_ctime != st2->st_ctime) {
	if (fifdiff) {
	    fprintf(stderr, "ERROR:st_ctime differs\n");
	    _exit(1);
	}
	return 1;
    }
    if (!fifdiff) {
	fprintf(stderr, "ERROR:compare stat succeeded\n");
	_exit(1);
    }
    return 0;
}

void
testLinkFile()
{
    int rc, fd;
    ssize_t res;
    struct stat st1, st2;
    char buf[1024];
    
    fprintf(stderr, "testing simple link to a file\n");
    fd = open("targetfile",  O_RDWR|O_CREAT, 00666);
    if (fd < 0) {
	perror("ERROR:open/create failed");
	_exit(1);
    }

    res = write(fd, buf, 256);
    if (res != 256) {
	perror("ERROR:failed writing to file\n");
	_exit(1);
    }

    rc = symlink("targetfile", "linktotarget");
    if (rc != 0) {
	perror("ERROR:symlink failed");
	_exit(1);
    }
    
    fd = open("linktotarget", O_RDONLY);
    if (fd < 0) {
	perror("ERROR:symlink failed");
	fprintf(stderr, "ERROR:errno is %d\n", errno);
	_exit(1);
    }

    res = read(fd, buf, 1024);
    if (res != 256) {
	perror("ERROR:read of link to file did not get same amount written\n");
	_exit(1);
    }

    // try stat on both files
    rc = stat("targetfile", &st1);
    if (rc != 0) {
	perror("ERROR:stat on target failed\n");
	_exit(1);
    }
    rc = stat("linktotarget", &st2);
    if (rc != 0) {
	perror("ERROR:stat on link failed\n");
	_exit(1);
    }

    compareStatus(&st1,&st2,1);

    rc = lstat("linktotarget", &st2);
    if (rc != 0) {
	perror("ERROR:stat on link failed\n");
	_exit(1);
    }
    compareStatus(&st1,&st2,0);

    rc = unlink("targetfile");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }

    rc = unlink("linktotarget");
    if (rc != 0) {
	perror("ERROR:unlink of symlink failed");
	_exit(1);
    }
    fprintf(stderr, "done\n");
}

void
testLinkMultiLinkFile()
{
    int rc;
    struct stat st1, st2;
    
    // a/b/c/d->a/b/e
    // f/g->a/b/c

    fprintf(stderr, "testing deep tree link to a file\n");
    rc = mkdir("a", 00777);
    if (rc != 0) {
	perror("ERROR:mkdir failed");
	_exit(1);
    }
    rc = mkdir("a/b", 00777);
    if (rc != 0) {
	perror("ERROR:mkdir failed");
	_exit(1);
    }
    rc = mkdir("a/b/c", 00777);
    if (rc != 0) {
	perror("ERROR:mkdir failed");
	_exit(1);
    }
    rc = mkdir("a/b/e", 00777);
    if (rc != 0) {
	perror("ERROR:mkdir failed");
	_exit(1);
    }
    rc = mkdir("f", 00777);
    if (rc != 0) {
	perror("ERROR:mkdir failed");
	_exit(1);
    }
    rc = symlink("../../../a/b/e", "a/b/c/d");
    if (rc != 0) {
	perror("ERROR:symlink failed");
	_exit(1);
    }

    rc = symlink("../a/b/c", "f/g");
    if (rc != 0) {
	perror("ERROR:symlink failed");
	_exit(1);
    }

    // try stat on both files
    rc = stat("a/b/c/d", &st1);
    if (rc != 0) {
	perror("ERROR:stat on target failed\n");
	_exit(1);
    }
    rc = stat("a/b/e", &st2);
    if (rc != 0) {
	perror("ERROR:stat on link failed\n");
	_exit(1);
    }
    compareStatus(&st1,&st2,1);

    rc = stat("f/g/d", &st2);
    if (rc != 0) {
	perror("ERROR:stat on link failed\n");
	_exit(1);
    }
    compareStatus(&st1,&st2,1);

    rc = lstat("a/b/c/d", &st1);
    if (rc != 0) {
	perror("ERROR:stat on target failed\n");
	_exit(1);
    }
    rc = lstat("a/b/e", &st2);
    if (rc != 0) {
	perror("ERROR:stat on link failed\n");
	_exit(1);
    }
    compareStatus(&st1,&st2,0);

    rc = lstat("f/g/d", &st1);
    if (rc != 0) {
	perror("ERROR:stat on target failed\n");
	_exit(1);
    }
    compareStatus(&st1,&st2,0);

    rc = unlink("a/b/c/d");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }
    rc = rmdir("a/b/e");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }
    rc = rmdir("a/b/c");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }
    rc = rmdir("a/b");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }
    rc = rmdir("a");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }
    rc = unlink("f/g");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }
    rc = rmdir("f");
    if (rc != 0) {
	perror("ERROR:rmdir failed");
	_exit(1);
    }
    fprintf(stderr, "done\n");
}

int
main()
{
    testLinkDirectory();
    testLinkFile();
    testLinkMultiLinkFile();
}
