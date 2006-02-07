/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fileStream.c,v 1.2 2002/05/01 21:12:32 soules Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 *   Test of the interposition system.
 *
 * **************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

#define PAGE 4096

int
main(int argc, char *argv[])
{
    struct stat fdStat;
    int fd;
    int i;
    int single = 0; // do a single pass rather than looping
    char buf[PAGE];
    struct timeval start, end;

    if(argc < 2) {
        printf("Usage: fileStream [-s] <stream file>");
    }

    // check if called with the '-s' flag
    if(argc == 3) {
        single = 1;
    }

    // open the file using the last arg as the filename
    fd = open(argv[argc - 1], O_RDONLY);
    if(fd < 0) {
        printf("error opening file\n");
        return fd;
    }

    // read the entire file in page-size chunks
    fstat(fd, &fdStat);

    gettimeofday(&start, NULL);
    do {
        for(i = 0; i < fdStat.st_size; i += PAGE) {
            read(fd, buf, PAGE);
            if(!(i % (PAGE*PAGE))) {
                printf("."); fflush(stdout);
            }
        }
        lseek(fd, 0, SEEK_SET);
    } while (!single);
    gettimeofday(&end, NULL);

    if (end.tv_usec < start.tv_usec) {
        end.tv_sec--;
        end.tv_usec += 1000000;
    }
    printf("\n\ntime: %lu (%lu)\n",
           end.tv_sec - start.tv_sec,
           end.tv_usec - start.tv_usec);

    return 0;
}
