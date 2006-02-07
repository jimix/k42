/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: multiseg.c,v 1.2 2004/12/20 15:43:22 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <stdio.h>

char myData[8] = {'a','b','c','d','e','f','g','h'};
char myBss[8];

int
main()
{
    myBss[0] = myData[0]+10;
    printf("Hello world 4 data %c bss %c\n",(char)myData[0], (char)myBss[0]);
    return 0;
}
