/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceControl.C,v 1.8 2005/06/28 19:42:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user program that simply sets the trask mask to trace
 *                     all events - used by knightly
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <misc/baseStdio.H>
#include <sys/SystemMiscWrapper.H>
#include <sys/systemAccess.H>
#include <sync/MPMsgMgr.H>
#include <scheduler/Scheduler.H>
#include <usr/ProgExec.H>
#include <trace/traceIncs.H>
#include <stdlib.h>
#include <stdio.h>

#define USAGE "traceControl\n\
  --help: print out this message\n\
  --mask <ALL, NONE, Value>: sets mask (hex value of mask) - \n\
  --start: starts the tracing daemon\n\
  --stop: stops the tracing daemon\n\
  --vp <#>: takes action on listed vps, 0 is local 1 is all default is 1\n\
  --dump: dumps events in buffers to files\n\
"

typedef enum {DUMP, MASK, NONE, RESET, START, STOP} Action;

int
main(int argc, char *argv[])
{
    NativeProcess();

    int argCount;
    SysStatus rc;
    uval mask=0, procs=1;
    uval action = NONE;

    argCount = 1;

    while (argCount < argc) {
	if (strcmp(argv[argCount], "--help") == 0) {
	    printf("%s", USAGE);
	} else if (strcmp(argv[argCount], "--mask") == 0) {
	    action = MASK;
	    argCount++;
	    if (strcmp(argv[argCount], "ALL") == 0) {
		mask = TRACE_ALL_MASK;
	    } else if (strcmp(argv[argCount], "NONE") == 0) {
		mask = 0;
	    } else {
		mask = strtoul(argv[argCount],NULL,0);
		printf("mask being set to 0x%lx\n", mask);
	    }
	} else if (strcmp(argv[argCount], "--start") == 0) {
	    action = START;
	} else if (strcmp(argv[argCount], "--stop") == 0) {
	    action = STOP;
	} else if (strcmp(argv[argCount], "--vp") == 0) {
	    argCount++;
	    procs = strtoul(argv[argCount],NULL,0);
	} else if (strcmp(argv[argCount], "--dump") == 0) {
	    action = DUMP;
	} else if (strcmp(argv[argCount], "--reset") == 0) {
	    action = RESET;
	} else {
	    printf("%s", USAGE);
	}
	argCount++;
    }

    if (! ((procs == 0) || (procs ==1))) {
	printf("error: unknown value for procs set to 0 or 1\n");
	exit(-1);
    }

    if (action == DUMP) {
	if (procs == 0) {
	    printf("warning: trace dump works across across processors\n");
	}
	rc = DREFGOBJ(TheSystemMiscRef)->traceDump();
	if (_FAILURE(rc)) {
	    printf("unknown error dumping trace buffers\n");
	}
    }

    if (action == MASK) {
	if (procs == 0) {
	    rc = DREFGOBJ(TheSystemMiscRef)->traceSetMask(mask);
	} else {
	    rc = DREFGOBJ(TheSystemMiscRef)->traceSetMaskAllProcs(mask);
	}
	if (_FAILURE(rc)) {
	    printf("unknown error setting trace mask\n");
	}
    }

    if (action == RESET) {
	if (procs == 0) {
	    rc = DREFGOBJ(TheSystemMiscRef)->traceReset();
	} else {
	    rc = DREFGOBJ(TheSystemMiscRef)->traceResetAllProcs();
	}
	if (_FAILURE(rc)) {
	    printf("unknown error resetting trace buffers\n");
	}
    }

    if (action == START) {
	rc = DREFGOBJ(TheSystemMiscRef)->traceStartTraceD(procs);
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) == EEXIST) {
		printf("traced already running\n");
	    } else {
		printf("unknown error starting traced\n");
	    }
	}
    }

    if (action == STOP) {
	if (procs == 0) {
	    rc = DREFGOBJ(TheSystemMiscRef)->traceStopTraceD();
	} else {
	    rc = DREFGOBJ(TheSystemMiscRef)->traceStopTraceDAllProcs();
	}
	if (_FAILURE(rc)) {
	    printf("unknown error stopping trace daemon\n");
	}
    }

    return 0;
}
