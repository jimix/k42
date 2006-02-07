/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: printtime.C,v 1.1 2003/03/24 04:45:41 marc Exp $
 *****************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
/*
 * hack to print high resolution time for timing
 */
int
main()
{
    unsigned long timeis;
    struct timeval start = {0,0};
    gettimeofday(&start, NULL);
    timeis = (unsigned long)start.tv_sec*1000000 + start.tv_usec;
    printf("%ld\n", timeis);
    return 0;
}
