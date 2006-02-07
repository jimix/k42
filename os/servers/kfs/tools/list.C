/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: list.C,v 1.3 2003/08/21 17:42:50 dilma Exp $
 *****************************************************************************/
#include "fs.H"
#include "FileDisk.H"
#include "SuperBlock.H"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
    Disk *disk;
    uval disknum = 0;
    sval error;

    if(argc != 2 && argc != 3) {
        printf("Usage: %s <disk> [<disknum>]\n", argv[0]);
        return -1;
    }

    if (argc == 3) {
	char *endptr;
	disknum = (uval) strtol(argv[2], &endptr, 10);
	if (*endptr != '\0' || errno == ERANGE ) {
	    printf("%s: disknum provided (%s) is invalid\n",
		   argv[0], argv[2]);
	    return (1);
	}
	if (disknum != 0 && disknum != 1) {
	    printf("%s: we didn't expect disknum provided (%s)\n",
		   argv[0], argv[2]);
	    return (1);
	}
    }

    // create a disk to use
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
	printf("%s: open(%s) failed: %s\n", argv[0], argv[1], strerror(error));
	return -1;
    }
    disk = new FileDisk(fd);

    // validate/list the contents of disk
    return validateDiskKFS(disk);
}
