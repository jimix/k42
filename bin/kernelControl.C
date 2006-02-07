/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: kernelControl.C,v 1.5 2005/06/28 19:42:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user program that simply sets the trask mask to trace
 *                     all events - used by knightly
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/SystemMiscWrapper.H>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>
#include <stdlib.h>
#include <stdio.h>

#define USAGE "kernelControl [--help] [--optimize | value]\n\
  --help: print out this message\n\
  --optimize:  sets all flags to most optimal values\n\
  value : numeric value to set flags to\n"



int
main(int argc, char *argv[])
{
    NativeProcess();

    int argCount;
    SysStatus rc;
    uval mask=0;

    argCount = 1;

    while (argCount < argc) {
	if (strcmp(argv[argCount], "--help") == 0) {
	    printf("%s", USAGE);
	    return -1;
	} else {
	    mask = strtoul(argv[argCount],NULL,0);
	}
	argCount++;
    }

    rc = DREFGOBJ(TheSystemMiscRef)->setControlFlags(mask);

    return _FAILURE(rc);
}
