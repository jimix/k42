/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: select_simple.c,v 1.1 2004/06/20 04:51:33 apw Exp $
 *****************************************************************************/

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
    fd_set rfds;
    struct timeval tv;
    int ret;

    /* Clear the read file descriptor mask.  */
    FD_ZERO(&rfds);

    /* Set some bits in the first long.  */
    FD_SET(0, &rfds);
#if 0
    FD_SET(1, &rfds);
    FD_SET(8, &rfds);
    FD_SET(31, &rfds);
    FD_SET(63, &rfds);

    /* Set some bits in the second long.  */
    FD_SET(64, &rfds);
    FD_SET(65, &rfds);
    FD_SET(72, &rfds);
    FD_SET(95, &rfds);
    FD_SET(127, &rfds);
#endif


    /* Wait for at most about eight seconds.  */
    tv.tv_sec = 8;
    tv.tv_usec = 13;

    ret = select(1, &rfds, NULL, NULL, &tv);

    if (ret > 0) {
	printf("PASS: %i descriptors are ready to read from.\n", ret);

	if (!FD_ISSET(0, &rfds)) {
	    printf("FAIL: an fd other than 0 is marked as ready\n");
	    return 1;
	}
    }
    else if (ret == 0)
	printf("PASS: 0 descriptors are ready to read from.\n");
    else {
	printf("FAIL: select returned %i: %m\n", ret);
	return -ret;
    }

    return 0;
}
