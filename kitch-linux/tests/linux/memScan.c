/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memScan.c,v 1.8 2005/03/18 00:56:45 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 *   Scan through large memory region.
 *   See comments on end of the file about how long this takes to run.
 *
 * **************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
    int numPages;
    volatile int* ptr;
    int i;
    int j = 0;
    int k = 0;
    if (argc < 2) {
        printf("Usage: memScan <num pages>");
	return -1;
    }
    numPages = atoi(argv[1]);

    ptr = (int*)malloc(4096 * (numPages+1));
    ptr = (int*)((((unsigned long)ptr)& ~4095ULL)+4096);

    for (j=0; j < numPages; ++j) {
	for (k=0; k<1024; ++k) {
	    ptr[j*1024+k] = j*1024+k;
	}
	if (j%100==0) {
	    write(1,"#",1);
	}
    }

    i=2;
    while (i!=0) {
	int x=0;
	for (j=numPages-1; j >=0; --j) {

	    for (k=0; k<1024; ++k) {
		if (ptr[j*1024+k] != j*1024+k) {
		    printf("Mismatch: %x vs %x at %p\n",ptr[j*1024+k],
			   j*1024+k, &ptr[j*1024+k]);
		}
	    }
	    ++x;
	    if (j%100==0) {
		write(1,".",1);
	    }
	    if (x%1000==0) {
		printf("%d\n",x);
	    }
	}
	--i;
    }
    return 0;
}

/***********************************************************************
 *          Info about how long this program takes to run
 *
 * You will need paging enabled to be able to run this program.
 *
 * The info below refers to running with argument 400000.
 *
 * 10/27/04 - Orran ran fullDeb (victim k9, with nfs) in 41 minutes.
 * 10/28/04 - Dilma increased the number of max outstanding requests from
 *            32 to 64. On victim k1, with nfs:
 *            noDeb    42min
 *            fullDeb  44min
 * 10/29/04 - Orran ran fullDeb on K9 with nfs in 29 minutes with only 32 
 *            outstanding requests, 64 failed, am debugging, second try was
 *            36 minutes.
 * 11/01/04 - dilma ran fullDeb on k1 with nfs (the floor system, withc 32
 *            max outstanding requests) 64 min.
 *
 *  3/15/05 - dilma ran fullDeb on k1 with nfs, 72m24s.
 *            did again (after reboot), 63m44s.
 *
 *  3/18/05 - dilma ran fullDeb on k1 with KFS, 45m10s
 *
 ***********************************************************************
 */
