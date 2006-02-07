#ifndef __SCHEDULER_SERVICE_H_
#define __SCHEDULER_SERVICE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SchedulerService.H,v 1.10 2005/01/26 03:21:51 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides status of all threads.
 * **************************************************************************/

#ifndef EXPORT_SCHEDULERSERVICE
#define EXPORT_SCHEDULERSERVICE

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

#include <sys/ppccore.H>
#include <scheduler/Thread.H>
#include <scheduler/Scheduler.H>
#include <cobj/XHandleTrans.H>

#endif /* #ifndef EXPORT_SCHEDULERSERVICE */

#include "meta/MetaSchedulerService.H"
#include "stub/StubSchedulerService.H"

class SchedulerService : public Obj {
private:
    static ProcessID PID;
    static void InitStub(StubSchedulerService &stub, DispatcherID targetDspID) {
	stub.initOHWithCommID(
	    SysTypes::WILD_COMMID(SysTypes::COMMID(PID, targetDspID)),
	    XHANDLE_MAKE_NOSEQNO(CObjGlobals::SchedulerServiceIndex));
    }

public:
    enum {MAX_THREADS = ((PPCPAGE_LENGTH_MAX - 2*sizeof(uval)) /
						sizeof(Thread::Status))};

    DEFINE_PINNEDLOCALSTRICT_NEW(SchedulerService);
    DEFINE_ACCESS_SERVER(SchedulerService, 0, 0);

public:
    static void ClassInit(ProcessID pid);
    static void PostFork(ProcessID pid);

    static SysStatus Unblock(DispatcherID targetDspID,
			     ThreadID thid, uval makeFirst);
    static SysStatus CallFunction(DispatcherID targetDspID,
				  SysStatusFunctionUval fct, uval data);
    static SysStatus ScheduleFunction(DispatcherID targetDspID,
				      Scheduler::ThreadFunction fct,
				      uval data);
    static SysStatus AddThread(DispatcherID targetDspID, Thread *thread);

__xpublic:
    virtual SysStatus _getStatus(__inout uval &keyIterator,
				 __out uval &numThreads,
				 __in uval maxThreads,
				 __outbuf(numThreads:maxThreads)
				     Thread::Status *threadStatus) __xa(none);
    virtual __async SysStatus _doBreakpoint() __xa(none);
    virtual __async SysStatus _dumpCObjTable() __xa(none);
    virtual  SysStatus _initCODescs(__out ObjectHandle &oh,
                                    __CALLER_PID pid) __xa(none);
    virtual __async SysStatus _dumpLeakInfo() __xa(none);
    virtual __async SysStatus _resetLeakInfo() __xa(none);

    virtual SysStatus _unblock(ThreadID thid, uval makeFirst);
    virtual SysStatus _callFunction(SysStatusFunctionUval fct, uval data);
    virtual SysStatus _scheduleFunction(Scheduler::ThreadFunction fct,
					uval data);
    virtual SysStatus _addThread(uval threadUval);
};

#endif /* #ifndef __SCHEDULER_SERVICE_H_ */
