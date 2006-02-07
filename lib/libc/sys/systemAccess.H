#ifndef __SYSTEM_ACCESS_H_
#define __SYSTEM_ACCESS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: systemAccess.H,v 1.3 2005/06/29 16:12:53 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides access to K42 interfaces for mixed-mode
 *                     programs.
 * **************************************************************************/

#include <scheduler/Scheduler.H>

typedef Scheduler::SystemSavedState SystemSavedState;

extern "C" void SystemEnter(SystemSavedState *saveAreaPtr);
extern "C" void SystemExit(SystemSavedState *saveAreaPtr);

inline void
SystemEnter(SystemSavedState *saveAreaPtr)
{
    Scheduler::SystemEnter(saveAreaPtr);
}

extern "C" int getpid();

inline void
SystemExit(SystemSavedState *saveAreaPtr)
{
    if (Scheduler::SystemExit(saveAreaPtr)) {
	/*
	 * SystemEnter and SystemExit don't switch stacks as a true system
	 * call would.  But now we have to make an upcall on the shepherd
	 * thread, probably to deliver a signal, and for that we have to
	 * make a first-class transition to the shepherd, including its
	 * stack.  The simplest way to accomplish that is to make an actual
	 * call to a harmless syscall such as getpid().
	 */
	(void) getpid();
    }
}

inline void
NativeProcess()
{
    SystemSavedState saveArea;
    SystemEnter(&saveArea);
    tassertMsg(saveArea.curThread == NULL,
	       "NativeProcess called with non-NULL CurrentThread.\n");
}

#endif /* #ifndef __SYSTEM_ACCESS_H_ */
