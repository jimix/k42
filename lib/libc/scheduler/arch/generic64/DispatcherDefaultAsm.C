/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultAsm.C,v 1.9 2004/06/28 17:01:29 rosnbrg Exp $
 *************************************************************************** */

#include <sys/sysIncs.H>
#include <scheduler/DispatcherDefault.H>

void DispatcherDefault_InitThread(Thread *thread, Scheduler::ThreadFunction fct,
				  uval data)
{
    /* empty body */
}

void DispatcherDefault_InitThreadGeneral(Thread *thread,
					 Scheduler::ThreadFunctionGeneral fct,
					 uval len, char *data)
{
    /* empty body */
}

void DispatcherDefault_InterruptThread(Thread *thread,
				       Scheduler::InterruptFunction fct,
				       uval data)
{
    /* empty body */
}

void DispatcherDefault_Suspend(DispatcherDefault *dispatcher) 
{
  /* empty body */
}

code DispatcherDefault_SuspendCore;
code DispatcherDefault_SuspendCore_Continue;

void DispatcherDefault_GotoRunEntry(DispatcherDefault *dispatcher)
{
  /* empty body */
}

SysStatus DispatcherDefault_SetEntryPoint(EntryPointNumber entryPoint,
					  EntryPointDesc entry)
{
    return 0;
}

void DispatcherDefault_HandoffProcessor(CommID targetID)
{
  /* empty body */
}

code DispatcherDefault_HandoffProcessorCore;


EntryPointDesc DispatcherDefault_RunEntry_Desc;
EntryPointDesc DispatcherDefault_InterruptEntry_Desc;
EntryPointDesc DispatcherDefault_TrapEntry_Desc;
EntryPointDesc DispatcherDefault_PgfltEntry_Desc;
EntryPointDesc DispatcherDefault_IPCCallEntry_Desc;
EntryPointDesc DispatcherDefault_IPCReturnEntry_Desc;
EntryPointDesc DispatcherDefault_IPCFaultEntry_Desc;
EntryPointDesc DispatcherDefault_SVCEntry_Desc;
