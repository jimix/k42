/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Thread.C,v 1.57 2004/06/28 17:01:26 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic Thread Definition
 * **************************************************************************/

#include "sys/sysIncs.H"
#include "sys/syscalls.H"
#include "sys/Dispatcher.H"
#include "Thread.H"
#include "Scheduler.H"

void
Thread::init(void *baseOfStack, void *bottomOfStack, void* truebottom)
{
    attachment = NULL;
    next = NULL;
    targetID = SysTypes::COMMID_NULL;
    curSP = 0;
    wasUnblocked = 0;
    isBlocked = 0;
    migratable = 0;
    extensionID = 0;
    groups = 0;
    activeCntP = NULL;
    pgfltID = uval(-1);
    startSP = (uval)baseOfStack;
    threadID = Scheduler::NullThreadID;
    threadSpecificUval = 0;
    bottomSP = (uval)bottomOfStack;
    truebottomSP = (uval)truebottom;
    altStack = 0;
    upcallNeeded = 0;
}

void
Thread::Status::print()
{
    char *stateStr[] = {"UNKNOWN", "FREE", "RUNNING", "READY",
			"BLOCKED", "PPC_BLOCKED", "PGFLT_BLOCKED",
			"IPC_RETRY_BLOCKED", "BARRED"};
    uval i;
    if (Status::FREE == state) {
	return;
    }
    err_printf("        id 0x%lx, ptr 0x%lx, attachment 0x%lx, state %s\n",
					id, ptr, attachment, stateStr[state]);
    err_printf("            thread data 0x%lx, groups 0x%lx\n",
					threadData, groups);
    err_printf("            generation %ld, targetID 0x%lx\n",
					generation, targetID);
    err_printf("            call chain:  %lx", callChain[0]);
    uval const addrsPerLine = (callChain[0] < 0x100000000ULL) ? 6 : 3;
    for (i = 1; i < Thread::Status::CALL_CHAIN_DEPTH; i++) {
	if (callChain[i] == 0) break;
	if ((i % addrsPerLine) == 0) err_printf("\n                        ");
	err_printf(" %lx", callChain[i]);
    }
    err_printf("\n");
}
