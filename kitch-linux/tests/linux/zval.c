/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: zval.c,v 1.2 2005/01/06 20:54:28 awaterl Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Turn on or off Mambo ztracing.
 * **************************************************************************/

#include <stdio.h>
#include <string.h>


long long sim_stats1(int commandcode)
{
  int command = commandcode; 
  register unsigned long long  c asm ("r3") = command;
  register unsigned long a1 asm ("r4") = 0;
  asm volatile (".long 0x000EAEB0" : "=r" (c): "r" (c), "r" (a1));
  return (long long )0;
}

#define USAGE "zval <start|stop>\n"

int
main(int argc, char **argv)
{
    if (argc != 2) {
	printf(USAGE);
	return(1);
    }
    
    if (strcmp(argv[1], "start") == 0) {
	sim_stats1(142);
    } else if (strcmp(argv[1], "stop") == 0) {
	sim_stats1(143);
    } else {
	printf("unknown argument\n");
	printf(USAGE);
    }
    return(0);
}
