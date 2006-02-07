#ifndef __SCHEDULER_H_
#define __SCHEDULER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Scheduler.H,v 1.108 2005/06/28 19:44:27 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic scheduler functionality.
 * **************************************************************************/

#include <scheduler/Thread.H>
#include <scheduler/SchedulerTimer.H>
#include <scheduler/VPSet.H>
#include <sync/MPMsgMgrDisabled.H>
#include <sync/MPMsgMgrEnabled.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <sys/VolatileState.H>
#include <misc/asm.h>
#include <sys/ppccore.H>

class Scheduler {
public:
    typedef void (*ThreadFunction)(uval);
    typedef void (*ThreadFunctionGeneral)(uval, char*);
    typedef void (*InterruptFunction)(uval);

    static void Init();

    static void ClassInit(DispatcherID dspid, Thread *thread,
			  MemoryMgrPrimitive *memory,
			  uval threadCount, uval threadAllocSize,
			  uval threadStackReservation,
			  ThreadFunction fct, uval fctArg);

    static void AllocForkInfo(uval &forkInfoUval);
    static void DeallocForkInfo(uval forkInfoUval);
    static void DisabledPreFork(uval forkInfoUval);
    static void DisabledPostFork(uval forkInfoUval,
				 ThreadFunction fct, uval fctArg);
    static uval IsForkSafeSelf();
    static void MakeForkSafeSelf();

    static void EnableEntryPoint(EntryPointNumber entry);
    static void DisableEntryPoint(EntryPointNumber entry);
    static void SetupSVCDirect();

    static void StoreProgInfo(ProcessID procID, char *name);

    // disable or enable scheduling
    // Take Care - while disabled, page faults and IPC's are expensive
    // Also, don't stay disabled very long or the process scheduler
    // may stop the whole process
    static void Disable();
    static void Enable();
    static uval IsDisabled();

    // these operations are all called enabled
    static void Yield();
    static void YieldProcessor();
    static void HandoffProcessor(CommID targetID);
    static void Block();
    static void Unblock(ThreadID t, uval makeFirst = 0);

    // this operation is called disabled - may be on scheduler stack
    static void DisabledUnblockOnThisDispatcher(ThreadID t,
						uval makeFirst = 0);

    // these operations are called on a thread but disabled
    static void DisabledSuspend();
    static void DisabledYield();
    static void DisabledBlock();

    static SysTime BlockWithTimeout(SysTime when, TimerEvent::Kind kind);
    static void DelayUntil(SysTime when, TimerEvent::Kind kind);
    static void DelaySecs(uval secs);
    static void DelayMicrosecs(uval us);

    static void ActivateSelf();
    static void DeactivateSelf();

    static uval IsMigratableSelf();

    typedef struct {
	Thread *curThread;
    } SystemSavedState;
    static void SystemEnter(SystemSavedState *saveAreaPtr);
    static uval SystemExit(SystemSavedState *saveAreaPtr);

    // Exit the current thread, and allow it to be re-used:
    static void DisabledExit() __attribute__ ((noreturn));
    static void Exit() __attribute__ ((noreturn));

    static DispatcherID GetDspID();
    static VPNum GetVP();

    static SysTime SysTimeNow();
    static SysTime TicksPerSecond();

    static ThreadID GetCurThread();
    static Thread *GetCurThreadPtr();
    static Thread *GetThread(ThreadID id);

    static void FreeThread(Thread *th);

    /*
     * Thread-creation routines.  The basic routine passes a single uval as
     * argument to the new thread, does not return the threadID of the
     * newly-created thread, and is to be called enabled.  Variants (all
     * combinations) pass a variable-length data buffer to the new thread,
     * return the new thread's ID, and/or are to be called disabled.
     */
    static SysStatus ScheduleFunction(ThreadFunction fct, uval data);
    static SysStatus ScheduleFunction(ThreadFunctionGeneral fct,
				      uval len, char *data);
    static SysStatus ScheduleFunction(ThreadFunction fct,
				      uval data, ThreadID &id);
    static SysStatus ScheduleFunction(ThreadFunctionGeneral fct,
				      uval len, char *data, ThreadID &id);
    static SysStatus DisabledScheduleFunction(ThreadFunction fct, uval data);
    static SysStatus DisabledScheduleFunction(ThreadFunctionGeneral fct,
					      uval len, char *data);
    static SysStatus DisabledScheduleFunction(ThreadFunction fct,
					      uval data, ThreadID &id);
    static SysStatus DisabledScheduleFunction(ThreadFunctionGeneral fct,
					      uval len, char *data,
					      ThreadID &id);

    static SysStatus ScheduleFunction(ThreadFunction fct, uval data,
				      Thread *thread);

    static void LaunchSandbox(VolatileState *vsp, NonvolatileState *nvsp);

    static void DisabledInterruptThread(Thread *thread,
					InterruptFunction fct, uval data);

    // Functions to migrate a thread from one Scheduler to another:
    static SysStatus DisabledRemoveThread(Thread *thread);
    static Thread *DisabledRemoveThread();
    static SysStatus AddThread(Thread *thread);
    static SysStatus DisabledAddThread(Thread *thread);
    static void ResumeThread(Thread *thread);
    // Does the dispatcher have a migratable thread in its ready queue?
    static uval HasExtraThreads();

    // Allow this thread key to be recycled -- this implies that the
    // given thread will never be run again.
    static void FreeThreadKey(Thread *thread);
    static void DisabledFreeThreadKey(Thread *thread);

    // set function pointer for handing interrupt on this processor
    static void SetSoftIntrFunction(SoftIntr::IntrType interruptNumber,
				    SoftIntr::IntrFunc fct);

    static void SetDisabledMsgMgr(MPMsgMgrDisabled *mgr);
    static void SetEnabledMsgMgr(MPMsgMgrEnabled *mgr);
    static MPMsgMgrDisabled *GetDisabledMsgMgr();
    static MPMsgMgrEnabled *GetEnabledMsgMgr();

    static uval SetAllowPrimitivePPC(uval set);

    static void DisabledJoinGroupSelf(Thread::Group g);
    static void DisabledLeaveGroupSelf(Thread::Group g);
    static void DisabledBarGroup(Thread::Group g);
    static void DisabledUnbarGroup(Thread::Group g);
    static uval DisabledReplaceBarMask(uval newMask);

    static uval GetGroupsSelf();
    static void DisabledSetGroupsSelf(uval newGroups);
    static uval GetBarredGroups();

    static void GetStatus(uval &keyIterator, uval &numThreads,
			  uval maxThreads, Thread::Status *threadStatus);
    static void PrintStatus();

    static const ThreadID NullThreadID = ThreadID(-1);

    // get/set thread specific state
    static void SetThreadSpecificUvalSelf(uval n);
    static uval GetThreadSpecificUvalSelf();

    // GDB support routines
    static void EnterDebugger();
    static void ExitDebugger();
    static uval InDebugger();

    /*
     * Number of resource domains the implementation can manage.  Also, one
     * more than the largest valid RDNum, since we number from 0.
     */
    static const RDNum RDLimit = 4;

    /*
     * Number of VPs the implementation can manage.  Also, one more than
     * the largest valid VPNum, since we number from 0.
     */
    static const VPNum VPLimit = VPSet::VPLimit;

    /*
     * Initialization code needs to know how much space to reserve for each
     * dispatcher, but it doesn't otherwise need to know what Dispatcher
     * subclass is actually being used.  For now we define a hardwired
     * constant that is big enough for the Dispatcher subclasses we currently
     * have.  If we ever have subclasses with wildly different space
     * requirements we'll have to do something more dynamic.
     */
    static const uval DISPATCHER_SPACE = 2 * PAGE_SIZE;

    /*
     * In a similar vein, "fork" needs to provide space which the dispatcher
     * can use to preserve critical information, but the amount of space
     * needed depends on what Dispatcher subclass is actually in use.  We
     * define a constant that is "big enough" for now.
     */
    static const uval FORK_INFO_SPACE = 32 * sizeof(uval);

#ifndef NDEBUG
    // used only for assertion checking.
    static void AssertDisabled();
    static void AssertEnabled();
    static void AssertNonMigratable();
    static void AssertDisabledOrNonMigratable();
#else
    static void AssertDisabled() {};
    static void AssertEnabled() {};
    static void AssertNonMigratable() {};
    static void AssertDisabledOrNonMigratable() {};
#endif /* #ifndef NDEBUG */
};

/*
 * Primitive PPCs are PPCs which occur while disabled.  These should not
 * normally be used, since they perform poorly.  Additionally, there is a
 * technical problem if a primitive PPC is used while the PPC page is busy,
 * which can happen in a disabled interrupt handler.  To avoid this
 * very-hard-to-detect bug, we require that primitive PPC be guarded by
 * an ALLOW_PRIMITIVE_PPC/UN_ALLOW_PRIMITIVE_PPC pair.
 */
#define ALLOW_PRIMITIVE_PPC() \
    PRESERVE_PPC_PAGE(); \
    uval savedAllowPrimitivePPC = Scheduler::SetAllowPrimitivePPC(1);

#define UN_ALLOW_PRIMITIVE_PPC(itrSave) \
    (void) Scheduler::SetAllowPrimitivePPC(savedAllowPrimitivePPC); \
    RESTORE_PPC_PAGE();

#endif /* #ifndef __SCHEDULER_H_ */
