/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Scheduler.C,v 1.85 2005/06/13 14:10:59 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic scheduler functionality.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DispatcherDefault.H"
#include "Scheduler.H"

#ifndef	NDEBUG

/*static*/ void
Scheduler::AssertDisabled()
{
    tassertMsg(DispatcherDefault::IsDisabled(), "Enabled!\n");
}

/*static*/ void
Scheduler::AssertEnabled()
{
    tassertMsg(!DispatcherDefault::IsDisabled(), "Disabled!\n");
}

/*static*/ void
Scheduler::AssertNonMigratable()
{
    tassertMsg(!CurrentThread->isMigratable(), "Migratable!\n");
}

/*static*/ void
Scheduler::AssertDisabledOrNonMigratable()
{
    tassertMsg(DispatcherDefault::IsDisabled() ||
			!CurrentThread->isMigratable(),
	       "Enabled and migratable!\n");
}

#define DISABLED AssertDisabled()
#define ENABLED AssertEnabled()
#define NON_MIGRATABLE AssertNonMigratable()
#define DISABLED_OR_NON_MIGRATABLE AssertDisabledOrNonMigratable()

#else	// NDEBUG

#define DISABLED
#define ENABLED
#define NON_MIGRATABLE
#define DISABLED_OR_NON_MIGRATABLE

#endif	// NDEBUG

/*static*/ void
Scheduler::Init()
{
    DISABLED;
    DISPATCHER->initCore();
}

/*static*/ void
Scheduler::ClassInit(DispatcherID dspid, Thread *thread,
		     MemoryMgrPrimitive *memory,
		     uval threadCount, uval threadAllocSize,
		     uval threadStackReservation,
		     ThreadFunction fct, uval fctArg)
{
    DISABLED;
    DISPATCHER->init(dspid, thread, memory,
		     threadCount, threadAllocSize, threadStackReservation,
		     fct, fctArg);
}

/*static*/ void
Scheduler::AllocForkInfo(uval &forkInfoUval)
{
    ENABLED;
    DispatcherDefault::AllocForkInfo(forkInfoUval);
}

/*static*/ void
Scheduler::DeallocForkInfo(uval forkInfoUval)
{
    ENABLED;
    DispatcherDefault::DeallocForkInfo(forkInfoUval);
}

/*static*/ void
Scheduler::DisabledPreFork(uval forkInfoUval)
{
    DISABLED;
    DISPATCHER->disabledPreFork(forkInfoUval);
}

/*static*/ void
Scheduler::DisabledPostFork(uval forkInfoUval,
			    ThreadFunction fct, uval fctArg)
{
    DISABLED;
    DISPATCHER->disabledPostFork(forkInfoUval, fct, fctArg);
}

/*static*/ uval
Scheduler::IsForkSafeSelf()
{
    ENABLED;
    return CurrentThread->isForkSafe();
}

/*static*/ void
Scheduler::MakeForkSafeSelf()
{
    ENABLED;
    CurrentThread->makeForkSafe();
}

/*static*/ void
Scheduler::EnableEntryPoint(EntryPointNumber entry)
{
    DISABLED_OR_NON_MIGRATABLE;
    DISPATCHER->enableEntryPoint(entry);
}

/*static*/ void
Scheduler::DisableEntryPoint(EntryPointNumber entry)
{
    DISABLED_OR_NON_MIGRATABLE;
    DISPATCHER->disableEntryPoint(entry);
}

/*static*/ void
Scheduler::SetupSVCDirect()
{
    ENABLED;
    DispatcherDefault::SetupSVCDirect();
}

/*static*/ void
Scheduler::StoreProgInfo(ProcessID procID, char *name)
{
    DISABLED_OR_NON_MIGRATABLE;
    DISPATCHER->storeProgInfo(procID, name);
}

// disable or enable scheduling
// Take Care - while disabled, page faults and IPC's are expensive
// Also, don't stay disabled very long or the process scheduler
// may stop the whole process
/*static*/ void
Scheduler::Disable()
{
    ENABLED; NON_MIGRATABLE;
    DispatcherDefault::Disable();
}

/*static*/ void
Scheduler::Enable()
{
    DISABLED; NON_MIGRATABLE;
    DispatcherDefault::Enable();
}

/*static*/ uval
Scheduler::IsDisabled()
{
    DISABLED_OR_NON_MIGRATABLE;
    return DispatcherDefault::IsDisabled();
}

// these operations are all called enabled
/*static*/ void
Scheduler::Yield()
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->yield();
}

/*static*/ void
Scheduler::YieldProcessor()
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->yieldProcessor();
}

/*static*/ void
Scheduler::HandoffProcessor(CommID targetID)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->handoffProcessor(targetID);
}

/*static*/ void
Scheduler::Block()
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->block();
}

/*static*/ void
Scheduler::Unblock(ThreadID t, uval makeFirst)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->unblock(t, makeFirst);
}

// this operation is called disabled - may be on scheduler stack
/*static*/ void
Scheduler::DisabledUnblockOnThisDispatcher(ThreadID t, uval makeFirst)
{
    DISABLED;
    DISPATCHER->disabledUnblock(t, makeFirst);
}

// this operation is called on a thread but disabled
/*static*/ void
Scheduler::DisabledSuspend()
{
    DISABLED;
    DISPATCHER->disabledSuspend();
}

// this operation is called on a thread but disabled
/*static*/ void
Scheduler::DisabledYield()
{
    DISABLED;
    DISPATCHER->disabledYield();
}

// this operation is called on a thread but disabled
/*static*/ void
Scheduler::DisabledBlock()
{
    DISABLED;
    DISPATCHER->disabledBlock();
}

/*static*/ SysTime
Scheduler::BlockWithTimeout(SysTime when, TimerEvent::Kind kind)
{
    ENABLED; NON_MIGRATABLE;
    return DISPATCHER->blockWithTimeout(when, kind);
}

/*static*/ void
Scheduler::DelayUntil(SysTime when, TimerEvent::Kind kind)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->delayUntil(when, kind);
}

/*static*/ void
Scheduler::DelaySecs(uval secs)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->delaySecs(secs);
}

/*static*/ void
Scheduler::DelayMicrosecs(uval us)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->delayMicrosecs(us);
}

/*static*/ void
Scheduler::ActivateSelf()
{
    ENABLED; NON_MIGRATABLE;
    CurrentThread->activate();
}

/*static*/ void
Scheduler::DeactivateSelf()
{
    ENABLED; NON_MIGRATABLE;
    CurrentThread->deactivate();
}

/*static*/ uval
Scheduler::IsMigratableSelf()
{
    return CurrentThread->isMigratable();
}

/*static*/ void
Scheduler::SystemEnter(SystemSavedState *saveAreaPtr)
{
    DispatcherDefault::SystemEnter(saveAreaPtr);
}

/*static*/ uval
Scheduler::SystemExit(SystemSavedState *saveAreaPtr)
{
    return DispatcherDefault::SystemExit(saveAreaPtr);
}

// Exit the current thread, and allow it to be re-used:
/*static*/ void
Scheduler::DisabledExit()
{
    DISABLED;
    DISPATCHER->disabledExit();
}

// Exit the current thread, and allow it to be re-used:
/*static*/ void
Scheduler::Exit()
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->exit();
}

/*static*/ DispatcherID
Scheduler::GetDspID()
{
    DISABLED_OR_NON_MIGRATABLE;
    return DISPATCHER->getDspID();
}

/*static*/ VPNum
Scheduler::GetVP()
{
    DISABLED_OR_NON_MIGRATABLE;
    return SysTypes::VP_FROM_COMMID(DISPATCHER->getDspID());
}

/*static*/ SysTime
Scheduler::SysTimeNow()
{
    // no constraints
    return SchedulerTimer::SysTimeNow();
}

/*static*/ SysTime
Scheduler::TicksPerSecond()
{
    // no constraints
    return SchedulerTimer::TicksPerSecond();
}

/*static*/ ThreadID
Scheduler::GetCurThread()
{
    DISABLED_OR_NON_MIGRATABLE;
    return CurrentThread->getID();
}


/*static*/ void
Scheduler::SetThreadSpecificUvalSelf(uval n)
{
    // no constraints on caller; just operations on current thread
    CurrentThread->setThreadSpecificUval(n);
}

/*static*/ uval
Scheduler::GetThreadSpecificUvalSelf()
{
    // no constraints on caller; just operations on current thread
    return CurrentThread->getThreadSpecificUval();
}

/*static*/ void
Scheduler::EnterDebugger()
{
    DISPATCHER->enterDebugger();
}

/*static*/ void
Scheduler::ExitDebugger()
{
    DISPATCHER->exitDebugger();
}

/*static*/ uval
Scheduler::InDebugger()
{
    return DISPATCHER->inDebugger();
}

/*static*/ Thread *
Scheduler::GetCurThreadPtr()
{
    return CurrentThread;
}

/*static*/ Thread *
Scheduler::GetThread(ThreadID id)
{
    DISABLED_OR_NON_MIGRATABLE;
    return DISPATCHER->getThread(id);
}

/*static*/ void
Scheduler::FreeThread(Thread *th)
{
    DISABLED_OR_NON_MIGRATABLE;
    DISPATCHER->freeThread(th);
}

/*
 * Thread-creation routines.  The basic routine passes a single uval as
 * argument to the new thread, does not return the threadID of the
 * newly-created thread, and is to be called enabled.  Variants (all
 * combinations) pass a variable-length data buffer to the new thread,
 * return the new thread's ID, and/or are to be called disabled.
 */
/*static*/ SysStatus
Scheduler::ScheduleFunction(ThreadFunction fct, uval data)
{
    ENABLED; NON_MIGRATABLE;
    return DISPATCHER->scheduleFunction(fct, data);
}

/*static*/ SysStatus
Scheduler::ScheduleFunction(ThreadFunctionGeneral fct,
			    uval len, char *data)
{
    ENABLED; NON_MIGRATABLE;
    return DISPATCHER->scheduleFunction(fct, len, data);
}

/*static*/ SysStatus
Scheduler::ScheduleFunction(ThreadFunction fct,
			    uval data, ThreadID &id)
{
    ENABLED; NON_MIGRATABLE;
    return DISPATCHER->scheduleFunction(fct, data, id);
}

/*static*/ SysStatus
Scheduler::ScheduleFunction(ThreadFunctionGeneral fct,
			    uval len, char *data, ThreadID &id)
{
    ENABLED; NON_MIGRATABLE;
    return DISPATCHER->scheduleFunction(fct, len, data, id);
}

/*static*/ SysStatus
Scheduler::DisabledScheduleFunction(ThreadFunction fct, uval data)
{
    DISABLED;
    return DISPATCHER->disabledScheduleFunction(fct, data);
}

/*static*/ SysStatus
Scheduler::DisabledScheduleFunction(ThreadFunctionGeneral fct,
				    uval len, char *data)
{
    DISABLED;
    return DISPATCHER->disabledScheduleFunction(fct, len, data);
}

/*static*/ SysStatus
Scheduler::DisabledScheduleFunction(ThreadFunction fct,
				    uval data, ThreadID &id)
{
    DISABLED;
    return DISPATCHER->disabledScheduleFunction(fct, data, id);
}

/*static*/ SysStatus
Scheduler::DisabledScheduleFunction(ThreadFunctionGeneral fct,
				    uval len, char *data, ThreadID &id)
{
    DISABLED;
    return DISPATCHER->disabledScheduleFunction(fct, len, data, id);
}

/*static*/ SysStatus
Scheduler::ScheduleFunction(ThreadFunction fct,
			    uval data, Thread *thread)
{
    ENABLED; NON_MIGRATABLE;
    return DISPATCHER->scheduleFunction(fct, data, thread);
}

/*static*/ void
Scheduler::LaunchSandbox(VolatileState *vsp, NonvolatileState *nvsp)
{
    ENABLED; NON_MIGRATABLE;
    DispatcherDefault::LaunchSandbox(vsp, nvsp);
}

/*static*/ void
Scheduler::DisabledInterruptThread(Thread *thread,
				   InterruptFunction fct, uval data)
{
    DISABLED;
    DISPATCHER->disabledInterruptThread(thread, fct, data);
}

// Functions to migrate a thread from one Scheduler to another:
/*static*/ SysStatus
Scheduler::DisabledRemoveThread(Thread *thread)
{
    DISABLED;
    return DISPATCHER->disabledRemoveThread(thread);
}

/*static*/ Thread *
Scheduler::DisabledRemoveThread()
{
    DISABLED;
    return DISPATCHER->disabledRemoveThread();
}

/*static*/ SysStatus
Scheduler::AddThread(Thread *thread)
{
    ENABLED; NON_MIGRATABLE;
    return DISPATCHER->addThread(thread);
}

/*static*/ SysStatus
Scheduler::DisabledAddThread(Thread *thread)
{
    DISABLED;
    return DISPATCHER->disabledAddThread(thread);
}

/*static*/ void
Scheduler::ResumeThread(Thread *thread)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->resumeThread(thread);
}

// Does the dispatcher have a migratable thread in its ready
// queue?
/*static*/ uval
Scheduler::HasExtraThreads()
{
    DISABLED;
    return DISPATCHER->hasExtraThreads();
}

// Allow this thread key to be recycled -- this implies that the
// given thread will never be run again.
/*static*/ void
Scheduler::DisabledFreeThreadKey(Thread *thread)
{
    DISABLED;
    DISPATCHER->disabledFreeThreadKey(thread);
}

// Allow this thread key to be recycled -- this implies that the
// given thread will never be run again.
/*static*/ void
Scheduler::FreeThreadKey(Thread *thread)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->freeThreadKey(thread);
}

// set function pointer for handing interrupt on this processor
/*static*/ void
Scheduler::SetSoftIntrFunction(SoftIntr::IntrType interruptNumber,
			       SoftIntr::IntrFunc fct)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->interruptFunction[interruptNumber] = fct;
}

/*static*/ void
Scheduler::SetDisabledMsgMgr(MPMsgMgrDisabled *mgr)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->setDisabledMsgMgr(mgr);
}

/*static*/ void
Scheduler::SetEnabledMsgMgr(MPMsgMgrEnabled *mgr)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->setEnabledMsgMgr(mgr);
}

/*static*/ MPMsgMgrDisabled *
Scheduler::GetDisabledMsgMgr()
{
    DISABLED_OR_NON_MIGRATABLE;
    return DISPATCHER->getDisabledMsgMgr();
}

/*static*/ MPMsgMgrEnabled *
Scheduler::GetEnabledMsgMgr()
{
    DISABLED_OR_NON_MIGRATABLE;
    return DISPATCHER->getEnabledMsgMgr();
}

/*static*/ uval
Scheduler::SetAllowPrimitivePPC(uval set)
{
    DISABLED;
    return DISPATCHER->setAllowPrimitivePPC(set);
}

/*static*/ void
Scheduler::DisabledJoinGroupSelf(Thread::Group g)
{
    DISABLED;
    DISPATCHER->disabledJoinGroupSelf(g);
}

/*static*/ void
Scheduler::DisabledLeaveGroupSelf(Thread::Group g)
{
    DISABLED;
    DISPATCHER->disabledLeaveGroupSelf(g);
}

/*static*/ void
Scheduler::DisabledBarGroup(Thread::Group g)
{
    DISABLED;
    DISPATCHER->disabledBarGroup(g);
}

/*static*/ void
Scheduler::DisabledUnbarGroup(Thread::Group g)
{
    DISABLED;
    DISPATCHER->disabledUnbarGroup(g);
}

/*static*/ uval
Scheduler::DisabledReplaceBarMask(uval newMask)
{
    DISABLED;
    return DISPATCHER->disabledReplaceBarMask(newMask);
}

/*static*/ void
Scheduler::DisabledSetGroupsSelf(uval newGroups)
{
    DISABLED;
    DISPATCHER->disabledSetGroupsSelf(newGroups);
}

/*static*/ uval
Scheduler::GetGroupsSelf()
{
    return CurrentThread->getGroups();
}

/*static*/ uval
Scheduler::GetBarredGroups()
{
    DISABLED_OR_NON_MIGRATABLE;
    return DISPATCHER->getBarredGroups();
}

/*static*/ void
Scheduler::GetStatus(uval &keyIterator, uval &numThreads,
		     uval maxThreads, Thread::Status *threadStatus)
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->getStatus(keyIterator, numThreads, maxThreads, threadStatus);
}

/*static*/ void
Scheduler::PrintStatus()
{
    ENABLED; NON_MIGRATABLE;
    DISPATCHER->printStatus();
}
