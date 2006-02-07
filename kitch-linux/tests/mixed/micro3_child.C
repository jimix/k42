/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: micro3_child.C,v 1.13 2005/06/28 19:44:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assorted microbenchmarks
 * **************************************************************************/

#include <stdio.h>
#ifndef K42
#include <sched.h>
#else
#include <sys/sysIncs.H>
#include <misc/hardware.H>
#include <scheduler/Scheduler.H>
#include <sys/systemAccess.H>
#endif

int
main (void)
{
#ifndef K42
    printf("Child running\n");

    while(1) {
	yield();
    }
#else
    // Enter the K42 environment once and for all.
    // FIXME: This is no longer adequate for mixed-mode programs.
    SystemSavedState saveArea;
    SystemEnter(&saveArea);

    printf("Child running\n");

    while(1) {
	Scheduler::YieldProcessor();
    }
#endif
    return 0;
}
