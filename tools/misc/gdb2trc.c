/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: gdb2trc.c,v 1.1 2001/11/01 12:42:46 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Convert gdb output into form good for traceTool
 * **************************************************************************/

#include <stdio.h>
#include <unistd.h>
int main()
{
    int n;
    unsigned long long x,y,i;

    for (;;) {
        n = scanf("%llx: %llx %llx", &i,&x,&y);
        if (n != 3) break;
        write(1, &x, 8);
	write(1, &y, 8);
    }
    return 0;
}
