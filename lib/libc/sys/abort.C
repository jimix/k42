/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: abort.C,v 1.22 2004/04/06 21:00:32 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: User-level versions of abort() and breakpoint().
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <sys/Dispatcher.H>
#include <sys/KernelInfo.H>
#include <sys/extRegs.H>
#include <scheduler/Scheduler.H>

void
baseAbort(void)
{
    while (1) {};
}

ThreadID UserThreadToUnblock = Scheduler::NullThreadID;
uval printTimerStatus = 0;

void
breakpoint(void)
{
    if (extRegsLocal.dispatcher != (Dispatcher *)-1) {
	err_printf("breakpoint pid 0x%lx, progName %s\n",
		   extRegsLocal.dispatcher->processID,
		   extRegsLocal.dispatcher->progName);
    } else {
	err_printf("breakpoint in kernel\n");
    }

    if (KernelInfo::OnSim()) {
	SIMOS_BREAKPOINT;
	SIMOS_BREAKPOINT;	// FIXME: on MP simos, attempts to attach from
				// gdb after the system hits a breakpoint kick
				// the system out of the breakpoint.  Double
				// the breakpoint in hopes of keeping the
				// system here for analysis.
    } else {
	BREAKPOINT;
    }

    /*
     * A hook for unblocking a thread from the debugger.  Set this variable
     * to the ThreadID (not the thread pointer) of the thread you want to
     * examine, and then set a breakpoint that the target thread will hit when
     * it runs.
     */
    if (UserThreadToUnblock != Scheduler::NullThreadID) {
	Scheduler::Unblock(UserThreadToUnblock);
	UserThreadToUnblock = Scheduler::NullThreadID;
    }
    if (printTimerStatus) {
	printTimerStatus=0;
	SchedulerTimer::PrintStatus();
    }
}

void EnterDebugger()
{
    Scheduler::EnterDebugger();
}

void ExitDebugger()
{
    Scheduler::ExitDebugger();
}

uval InDebugger()
{
    return Scheduler::InDebugger();
}
