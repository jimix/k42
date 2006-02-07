/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mmap-ex.C,v 1.2 2005/03/16 17:20:53 awaterl Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#define MAP_LARGEPAGE 0x1000000
#define GPUL_LARGE_PAGE_SIZE 0x1000000L

int
main()
{
    void *base;
    uval *mb;

    base = mmap(0, 0x2000000, PROT_READ| PROT_WRITE, 
		(MAP_PRIVATE | MAP_ANONYMOUS | MAP_LARGEPAGE), 0,
		GPUL_LARGE_PAGE_SIZE);
    if ((sval) base == -1) {
        printf("Mmap Error");
    } else {
	mb = (uval *)base;
	mb [0] = 17;
    }

    printf("File mapped with page size 0x%lx\n", GPUL_LARGE_PAGE_SIZE);
}
