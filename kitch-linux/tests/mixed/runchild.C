/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: runchild.C,v 1.9 2005/06/28 19:44:15 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/
#include <sys/sysIncs.H>
//#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>
//#include <io/FileLinux.H>
//#include <stub/StubTestScheduler.H>
//#include "bench.H"

void
runchild (uval bg_domain, uval argc, char **argv)
{
    char *envp[] = { NULL };
    SysStatus rc;

    printf("Running: %s ",argv[0]);
    for (uval i=1; i<argc; i++)
	printf("%s ",argv[i]);
    printf("\n");

    ProgExec::ArgDesc *args;
    rc = ProgExec::ArgDesc::Create(argv[0], argv, envp, args);
    if (_SUCCESS(rc)) rc = runExecutable(args, 0);
    args->destroy();

    if (_FAILURE(rc)) {
	printf("%s [%ld]: Command not found\n",
	       argv[0], _SGENCD(rc));
	return;
    }
    printf("runchild: Done running command\n");
}

int
main (int argc, char **argv)
{
    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    if (argc < 3) {
	printf("Usage: %s <bg_domain> <bg_prog> ...\n",argv[0]);
	return -1;
    }
    char ** argvv = &(argv[2]);
    argc -= 2;

    runchild(atoi(argv[1]), argc, argvv);
    return 0;
}
