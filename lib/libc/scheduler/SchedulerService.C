/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SchedulerService.C,v 1.11 2005/01/26 03:21:51 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides status of all threads.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include "SchedulerService.H"
#include "defines/mem_debug.H"
#include <cobj/COListServer.H>

/* static */ ProcessID SchedulerService::PID = ProcessID(-1);

/* virtual __async */ SysStatus
SchedulerService::_doBreakpoint()
{
    breakpoint();
    return 0;
}

/* virtual __async */ SysStatus
SchedulerService::_resetLeakInfo()
{
#ifdef DEBUG_LEAK
    allocLeakProof->reset();
#endif /* #ifdef DEBUG_LEAK */

    return 0;
}

/* virtual __async */ SysStatus
SchedulerService::_dumpLeakInfo()
{
#ifdef DEBUG_LEAK
    allocLeakProof->print();
    allocLeakProof->reset();
#endif /* #ifdef DEBUG_LEAK */
    return 0;
}

/* virtual __async */ SysStatus
SchedulerService::_dumpCObjTable()
{
    DREFGOBJ(TheCOSMgrRef)->print();
    return 0;
}

/* virtual */ SysStatus
SchedulerService::_initCODescs(__out ObjectHandle &oh, __CALLER_PID pid)
{
    err_printf("SchedulerService::_initCOList: called\n");
    SysStatus rc=COListServer::Create(oh, pid);
    return rc;
}

/* virtual */ SysStatus
SchedulerService::_getStatus(uval &keyIterator, uval &numThreads,
			    uval maxThreads, Thread::Status *threadStatus)
{
    Scheduler::GetStatus(keyIterator, numThreads, maxThreads, threadStatus);
    return 0;
}

/* virtual */ SysStatus
SchedulerService::_unblock(ThreadID thid, uval makeFirst)
{
    Scheduler::Unblock(thid, makeFirst);
    return 0;
}

/* virtual */ SysStatus
SchedulerService::_callFunction(SysStatusFunctionUval fct, uval data)
{
    return fct(data);
}

/* virtual */ SysStatus
SchedulerService::_scheduleFunction(Scheduler::ThreadFunction fct, uval data)
{
    return Scheduler::ScheduleFunction(fct, data);
}

/* virtual */ SysStatus
SchedulerService::_addThread(uval threadUval)
{
    return Scheduler::AddThread((Thread *) threadUval);
}

/* static */ SysStatus
SchedulerService::Unblock(DispatcherID targetDspID,
			  ThreadID thid, uval makeFirst)
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    InitStub(schedServ, targetDspID);
    return schedServ._unblock(thid, makeFirst);
}

/* static */ SysStatus
SchedulerService::CallFunction(DispatcherID targetDspID,
			       SysStatusFunctionUval fct, uval data)
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    InitStub(schedServ, targetDspID);
    return schedServ._callFunction(fct, data);
}

/* static */ SysStatus
SchedulerService::ScheduleFunction(DispatcherID targetDspID,
				   Scheduler::ThreadFunction fct,
				   uval data)
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    InitStub(schedServ, targetDspID);
    return schedServ._scheduleFunction(fct, data);
}

/* static */ SysStatus
SchedulerService::AddThread(DispatcherID targetDspID, Thread *thread)
{
    StubSchedulerService schedServ(StubObj::UNINITIALIZED);
    InitStub(schedServ, targetDspID);
    return schedServ._addThread(uval(thread));
}


/* static */ void
SchedulerService::ClassInit(ProcessID pid)
{
    SchedulerService *const rep = new SchedulerService();
    tassert(rep != NULL, err_printf("new SchedulerService failed\n"));

    CObjRootSingleRepPinned::Create(rep, GOBJ(TheSchedulerServiceRef));

    MetaSchedulerService::
	createXHandle((ObjRef)GOBJ(TheSchedulerServiceRef),
		      pid, MetaObj::globalHandle, 0);
    PID = pid;
}

/* static */ void
SchedulerService::PostFork(ProcessID pid)
{
    MetaSchedulerService::
	createXHandle((ObjRef)GOBJ(TheSchedulerServiceRef),
		      pid, MetaObj::globalHandle, 0);
    PID = pid;
}
