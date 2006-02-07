/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: libksup.C,v 1.38 2005/03/15 02:39:43 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: PwrPC support routines needed by machine-independent
 * libksup.C.
 * **************************************************************************/

// Not really a ".C" file.  Don't include <kernIncs.H>.
#include <sys/KernelInfo.H>	// for OnSim()
#include <init/kernel.H>	// for _OnSim
#include "simos.H"

#include <sys/hcall.h>

#include <sys/IOChan.H>

void
baseAbort(void)
{
    breakpoint();
    while (1) {};
}

ThreadID KernelThreadToUnblock = Scheduler::NullThreadID;

void
breakpoint(void)
{
    if (_OnSim) {
	SIMOS_BREAKPOINT;
    } else {
	BREAKPOINT;
    }

    /*
     * A hook for unblocking a thread from the debugger.  Set this variable
     * to the ThreadID (not the thread pointer) of the thread you want to
     * examine, and then set a breakpoint that the target thread will hit when
     * it runs.
     */
    if (KernelThreadToUnblock != Scheduler::NullThreadID) {
	Scheduler::Unblock(KernelThreadToUnblock);
	KernelThreadToUnblock = Scheduler::NullThreadID;
    }
}
