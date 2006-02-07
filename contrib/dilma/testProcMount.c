/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <mntent.h>

int
main(int argc, char *argv[])
{
    if (argc == 1) {
	printf("Usage: %s path\n", argv[0]);
	return 1;
    }

    FILE *mtab;
    /*
    mtab = fopen("/proc/mounts", "r");
    char line[256];
    char *ret;
    printf("calling flockfile\n");
    flockfile(mtab);
    printf("Reading from /proc/mounts\n");
    ret = fgets_unlocked(line, sizeof(line), mtab);
    while (ret != NULL) {
	printf("Got line *%s*\n", line);
	ret = fgets_unlocked(line, sizeof(line), mtab);
    }
    funlockfile(mtab);
    fclose(mtab);
    */
    mtab = fopen("/proc/mounts", "r");
    if (mtab == NULL) {
	printf("error on open of /proc/mounts??\n");
	return 1;
    }
    
    printf("Trying getmntent_r\n");
    struct mntent mntbuf;
    char tmpbuf[1024];
    struct mntent *retbuf = getmntent_r(mtab, &mntbuf, tmpbuf, sizeof(tmpbuf));
    printf("getmnetent_r returned %p\n", retbuf);
    
    printf("Trying fstatvfs\n");
    char *file = argv[1];
    int fd = open(file, O_RDONLY);
    struct statvfs buf;
    int r = fstatvfs(fd, &buf);
    if (r == 0) {
	printf("fstatvfs succeeded\n");
    } else {
	printf("fstatvfs failed with %s (ret was %d)\n", strerror(errno), r);
    }

    printf("Trying statvfs\n");
    r = statvfs(file, &buf);
    if (r == 0) {
	printf("statvfs succeeded\n");
    } else {
	printf("statvfs failed with %s (ret was %d)\n", strerror(errno), r);
    }

    return 0;
}
