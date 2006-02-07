/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Utility program to download a new kernel image for
 *                     fast reboot.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stub/StubRegionDefault.H>

int
loadImage(const char *prog, const char *image)
{
    SysStatus rc;
    int ret, fd;
    uval imageAddr, imageSize;
    struct timeval start;
    struct timeval end;

    fprintf(stderr, "Starting image download\n");
    fd = open(image, O_RDONLY);
    if (fd == -1) {
	fprintf(stderr, "%s: open(%s) failed: %s\n",
		prog, image, strerror(errno));
	return 1;
    }

    rc = StubRegionDefault::_CreateRebootImage(imageAddr, imageSize);

    if (!_SUCCESS(rc)) {
	fprintf(stderr, "%s: _CreateRebootImage() failed, rc 0x%lx\n",
		prog, rc);
	return 1;
    }

    gettimeofday(&start,NULL);
    ret = read(fd, (void *) imageAddr, imageSize);
    if (ret == -1) {
	fprintf(stderr, "%s: read(%s) failed:%s\n",
		prog, image, strerror(errno));
	return 1;
    }
    gettimeofday(&end,NULL);

    if (end.tv_usec<start.tv_usec) {
	end.tv_usec+=1000000;
    }

    fprintf(stderr, "%s: downloaded %s, %d b %ld.%06ld s\n",
	    prog, image, ret,
	    end.tv_sec-start.tv_sec,end.tv_usec-start.tv_usec);

    return 0;
}
