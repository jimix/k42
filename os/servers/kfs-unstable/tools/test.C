/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: test.C,v 1.3 2004/03/07 00:47:24 lbsoares Exp $
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
#include <stdlib.h>			// srand, rand

#define TEST_MAX_FILES 1000

int
main(int argc, char *argv[])
{
    int i;
    char dirs[9][60] = {"/", "/bin/", "/sbin/", "/tests/linux/",
                        "/usr/bin/", "/usr/local/bin/", "/etc/",
                        "/usr/local/", "/tests/pthread/"};
    char files[TEST_MAX_FILES][60];
    char fileName[1024];
    char oldPath[1024], newPath[1024];
    Disk *disk;

    if (argc != 2) {
        printf("Usage: %s <disk>\n", argv[0]);
        return -1;
    }
    // do a random set of file creations and links
    srand(5555);
    for(i = 0; i < TEST_MAX_FILES; i++) {
        // create a disk to use
#if defined (KFS_TOOLS) && defined(PLATFORM_Darwin)
	int fd = open(argv[1], O_RDWR);
#else
	int fd = open64(argv[1], O_RDWR);
#endif
	if (fd == -1) {
	    printf("%s: open(%s) failed with %s\n", argv[0], argv[1],
		   strerror(errno));
	    return 1;
	}
        disk = new FileDisk(fd, 262144, OS_BLOCK_SIZE);

        fd = open("big_file", O_RDONLY);

        strcpy(files[i], dirs[rand() % 9]);
        sprintf(fileName, "%d%d%d", i, i, i);
        strcat(files[i], fileName);

        strcpy(newPath, files[i]);

        if(i < 20 || (rand() % 2)) {
            createFileKFS(argv[0], disk, fd, newPath, 0x1FF, 0, 0);
        } else {
            strcpy(oldPath, files[rand() % i]);
            linkFileKFS(argv[0], disk, oldPath, newPath);
        }

        close(fd);
        delete disk;
    }
}
