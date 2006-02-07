/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test for clock_gettime() libc call
 * **************************************************************************/

#include <stdio.h>
#include <time.h>
#include <errno.h>

static int do_test(void)
{
	struct timespec tp;
	
	if (clock_gettime(CLOCK_MONOTONIC, &tp)!=-1 || errno!=EINVAL) {
		fprintf(stderr, "Only CLOCK_REALTIME is supported. "
						"Should have returned EINVAL\n");
		return -1;
	}

	if (clock_gettime(CLOCK_REALTIME, &tp)!=0)
	{
		fprintf(stderr, "clock_gettime failed. errno: %i", errno);
		return -1;
	}	else {
		time_t t = tp.tv_sec;
		printf("clock_gettime: %lu %lu\n", tp.tv_sec, tp.tv_nsec);
		printf("%s\n", asctime(localtime(&t)));
		return 0;
	}

}

int main(int argc, char *argv[])
{
	return do_test();
}
