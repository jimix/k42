/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: awaittime.C,v 1.1 2003/03/24 19:35:18 rosnbrg Exp $
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
/*
 * hack to synchronize start times
 */
int
main(int argc, char *argv[])
{
    unsigned long timeis;
    unsigned long targettime;

    targettime = strtoul(argv[1],NULL,0);

    do {
	struct timeval start = {0,0};
	gettimeofday(&start, NULL);
	timeis = (unsigned long)start.tv_sec*1000000 + start.tv_usec;
    } while (timeis < targettime);

    return 0;
}
