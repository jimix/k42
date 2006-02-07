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
 * Module Description: measures the rate at which the system can sustain
 *                     streaming gettimeofday system calls.
 * **************************************************************************/

#include <stdio.h>
#include <sys/time.h>

int main(int argc, char **argv)
{
    int i, sample = 1000000, max = 10000000;
    long delta;
    struct timeval before, after;

    if (gettimeofday(&before, NULL)) {
	printf("gettimeofday call failed: %m\n");
	return 1;
    }

    for (i = 0; i <= max; i++) {
	if (gettimeofday(&after, NULL)) {
	    printf("gettimeofday call failed: %m\n");
	    return 1;
	}

	if (i == 0) {
	    printf("%-9s %-10s %-8s\n", "TESTS", "TIME", "RATE");
	} else if (i % sample == 0) {
	    delta = ((after.tv_sec * 1000000000) + (after.tv_usec * 1000)) -
	      ((before.tv_sec * 1000000000) + (before.tv_usec * 1000));
	    printf("%-9i %-10lu %-8f\n", i, delta, 
		   i / ((double)delta / 1000000000));
	}
    }

    return 0;
}
