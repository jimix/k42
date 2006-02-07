/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: resMgrStats.C,v 1.2 2004/01/05 22:03:06 bob Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:  program to set or toggle resMgrStatsFlag
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <sys/ProcessLinuxClient.H>
#include <sys/ResMgrWrapper.H>
#include <sys/KernelScheduler.H>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage()
{
    printf("Usage: resMgrStats [key]\n");
    printf("By default resMgrStats toggles the value of the Stats Flag.\n");
    printf("If key is 0 or 1 it set the value to that.\n");
    printf("If key is 2 it prints out the uid info.\n");
    printf("If key is 3 it prints out condensed pid info.\n");
    printf("If key is 4 it prints out full pid info.\n");
}

int
main(int argc, char **argv)
{
    SysStatus rc;
    int key;

    if (argc > 2) {
	usage();
	return -1;
    }

    if (argc == 2) {
	key = atoi(argv[1]);
	if ((key < 0) || (key > 4)) {
	    printf("error: key must between 0 and 4\n");
	    return -1;
	}
	rc = DREFGOBJ(TheResourceManagerRef)->setStatsFlag(key);
    } else {
	rc = DREFGOBJ(TheResourceManagerRef)->toggleStatsFlag();
    }

    return 0;
}
