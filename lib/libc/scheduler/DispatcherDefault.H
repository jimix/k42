#ifndef __DISPATCHER_DEFAULT_H_
#define __DISPATCHER_DEFAULT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefault.H,v 1.129 2005/06/13 14:10:59 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic user-level dispatcher functionality.
 * **************************************************************************/

#include __MINC(DispatcherDefault.H)

#include <scheduler/Scheduler.H>
#include <scheduler/Thread.H>
#include <scheduler/SchedulerTimer.H>
#include <scheduler/DispatcherMgr.H>
#include <sync/MPMsgMgrDisabled.H>
#include <sync/MPMsgMgrEnabled.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <sys/Dispatcher.H>
#include <sys/KernelInfo.H>
#include <sys/extRegs.H>

#define DISPATCHER ((DispatcherDefault *) (extRegsLocal.dispatcher))

#ifndef NDEBUG
#define DEBUG_CREATE_STACK_FENCE(thread) thread->createStackFence()
#define DEBUG_CHECK_STACK(thread) thread->checkForStackOverflow()
#else /* #ifndef NDEBUG */
#define DEBUG_CREATE_STACK_FENCE(thread)
#define DEBUG_CHECK_STACK(thread)
#endif /* #ifndef NDEBUG */

class DispatcherDefault : protected Dispatcher {

    friend class Scheduler;
    friend class DispatcherMgr;
    friend class SchedulerTimer;
    friend class TimerEvent;
    friend void genConstants(void);
    friend class DispatcherDefaultExp;

protected:
    class ThreadPool;
    static ThreadPool *TheThreadPool;

    /*
     * The kernel treats the "hasWork" field in the Dispatcher base class as
     * a boolean indicating whether this dispatcher has work pending.  We use
     * the field as the head of our readyQueue because that gives us exactly
     * the right state transitions.  CurThread is not on readyQueue.  We
     * define readyQueue access functions to encapsulate the type cast.
     */
    void setReadyQueue(Thread *thread) { hasWork = uval(thread); }
    Thread *getReadyQueue() { return (Thread *) hasWork; }

    DispatcherDefaultArch arch;	// architecture-specific stuff
    DispatcherMgr::Published
		    published;	// facts distributed to all dispatchers
    uval dispatcherStack;	// stack for disabled dispatcher code
    Thread *sandboxShepherd;	// shepherd thread when in sandbox
    uval nextThreadKey;		// next thread key to be allocated
    Thread *freeList;		// list of available threads
    Thread *readyQueueTail;	// last thread on readyQueue (undefined if
				//                         readyQueue is NULL)
    uval16 barredGroups;	// thread groups that are currently locked out
    // the threadArray
    Thread **threadArray;
    uval threadArraySize;

    uval yieldRequested;		// thread has been asked to yield
    uval preemptRequested;		// process has been asked to yield
    uval rescheduleNeeded;		// yield or preempt is pending

    SchedulerTimer timer;		// scheduled events

    Thread *pgfltList[Dispatcher::NUM_PGFLT_IDS];
    Thread *ipcRetryList[Dispatcher::NUM_IPC_RETRY_IDS];
    Thread *barredList;

    MPMsgMgrDisabled *disabledMsgMgr;
    MPMsgMgrEnabled *enabledMsgMgr;

    uval allowPrimitivePPCFlag;		// true if PrimitivePPC is allowed

    /*
     * State variables needed for managing the threads that process incoming
     * asynchronous IPC messages.
     */
    uval asyncReadyHandlerExists;	// true if a handler thread has been
					//     created and hasn't yet picked
					//     up a message
    uval asyncBusyHandlerCnt;		// number of handler threads that are
					//     actively processing messages

    Thread *debuggerThread;		// dummy thread object for debugger
    Thread *savedThread;		// saved curThread while in debugger
    uval mainDebugStack;		// primary debugger stack
    uval auxDebugStack;			// auxiliary debugger stack
    uval currentDebugStack;		// debugger stack currently in use

    static uval const ASYNC_BUSY_HANDLER_MAX = 10;


    DispatcherDefault() { /* empty body */ }

    void initCore();

    void init(DispatcherID dspid, Thread *thread,
	      MemoryMgrPrimitive *memory,
	      uval threadCount, uval threadAllocSize,
	      uval threadStackReservation,
	      Scheduler::ThreadFunction fct, uval fctArg);

    struct ForkInfo;
    static void AllocForkInfo(uval &forkInfoUval);
    static void DeallocForkInfo(uval forkInfoUval);
    void disabledPreFork(uval forkInfoUval);
    void disabledPostFork(uval forkInfoUval,
			  Scheduler::ThreadFunction fct, uval fctArg);

    void enableEntryPoint(EntryPointNumber entry);
    void disableEntryPoint(EntryPointNumber entry);

    static const uval SVCDirectVectorAddr = 0x4000;
    static void SetupSVCDirect();

    uval setAllowPrimitivePPC(uval set) {
	uval oldval = allowPrimitivePPCFlag;
	allowPrimitivePPCFlag = set;
	return oldval;
    }

    Thread *allocThread() {
	Thread *thread;
	if (freeList != NULL) {
	    thread = freeList;
	    freeList = thread->next;
	} else {
	    thread = createThread();
	}
	return thread;
    }

    void allocThreadKey(Thread *thread) {
	uval key;

	key = nextThreadKey;
	while (threadArray[key] != NULL) {
	    key = (key + 1) % threadArraySize;
	    passertMsg(key != nextThreadKey, "Ran out of thread keys.\n");
	}
	nextThreadKey = (key + 1) % threadArraySize;

	threadArray[key] = thread;
	thread->setID(dspid, key);
    }

    void disabledFreeThreadKey(Thread *thread) {
	DEBUG_CHECK_STACK(thread);
	threadArray[thread->getKey()] = NULL;
	thread->setID(Scheduler::NullThreadID);
    }

    void freeThreadKey(Thread *thread) {
	Disable();
	disabledFreeThreadKey(thread);
	Enable();
    }

    void freeThread(Thread *thread) {
	tassertMsg(!thread->isActive(), "Thread is active.\n");
	tassertMsg(thread->groups == 0, "Thread is still in a group.\n");
	DEBUG_CHECK_STACK(thread);
	if (thread->extensionID == 0) {
	    thread->next = freeList;
	    freeList = thread;
	} else {
	    disabledFreeThreadKey(thread);
	}
    }

    void makeReady(Thread *thread) {
	thread->next = NULL;
	if (getReadyQueue() == NULL) {
	    setReadyQueue(thread);
	} else {
	    readyQueueTail->next = thread;
	}
	readyQueueTail = thread;
    }

    void makeReadyFirst(Thread *thread) {
	thread->next = getReadyQueue();
	setReadyQueue(thread);
	if (thread->next == NULL) {
	    readyQueueTail = thread;
	}
    }

    Thread *getReadyThread() {
	Thread *thread = getReadyQueue();
	if (thread != NULL) {
	    setReadyQueue(thread->next);
	}
	return thread;
    }

    uval hasExtraThreads() {
	Thread *thread = getReadyQueue();

	while (thread != NULL) {
	    if (thread->isMigratable()) {
		return 1;
	    }
	    thread = thread->next;
	}
	return 0;
    }

    static uval IsDisabled() { return extRegsLocal.disabled; }

    uval wasInterrupted() {
	return interrupts.pending();
    }

    static void Disable() {
	tassertMsg(!IsDisabled(), "Dispatcher already disabled.\n");
	extRegsLocal.disabled = 1;
    }

    static void Enable() {
	tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
	tassertMsg(CurrentThread != NULL,
	           "Enabling while not on a thread.\n");

	extRegsLocal.disabled = 0;
	while (DISPATCHER->wasInterrupted()) {
	    extRegsLocal.disabled = 1;
	    DISPATCHER->selfInterrupt();
	    extRegsLocal.disabled = 0;
	}
    }

    void disabledSuspend();
    void disabledYield();
    void disabledYieldProcessor();
    void disabledHandoffProcessor(CommID targetID);
    void disabledBlock();
    void disabledUnblock(ThreadID thid, uval makeFirst);

    // these operations are all called enabled

    struct UnblockMsg;
    void unblockRemote(ThreadID thid, uval makeFirst);

    void yield() {
	Disable();
	disabledYield();
	Enable();
    }

    void yieldProcessor() {
	Disable();
	disabledYieldProcessor();
	Enable();
    }

    void handoffProcessor(CommID targetID) {
	Disable();
	disabledHandoffProcessor(targetID);
	Enable();
    }

    void block() {
	Disable();
	disabledBlock();
	Enable();
    }

    void unblock(ThreadID thid, uval makeFirst) {
	if (Thread::GetDspID(thid) == dspid) {
	    Disable();
	    disabledUnblock(thid, makeFirst);
	    Enable();
	} else {
	    unblockRemote(thid, makeFirst);
	}
    }

    SysTime blockWithTimeout(SysTime when, TimerEvent::Kind kind);
    void delayUntil(SysTime when, TimerEvent::Kind kind);
    void delaySecs(uval secs);
    void delayMicrosecs(uval ms);

    void disabledExit() __attribute__ ((noreturn));
    void exit() __attribute__ ((noreturn));

#if 0
    void setMaxThreads(uval max);
#endif /* #if 0 */

    SysStatus disabledScheduleFunction(Scheduler::ThreadFunction fct,
				       uval data);
    SysStatus disabledScheduleFunction(Scheduler::ThreadFunctionGeneral fct,
				       uval len, char *data);
    SysStatus disabledScheduleFunction(Scheduler::ThreadFunction fct,
				       uval data, ThreadID &id);
    SysStatus disabledScheduleFunction(Scheduler::ThreadFunctionGeneral fct,
				       uval len, char *data, ThreadID &id);

    SysStatus disabledScheduleFunction(Scheduler::ThreadFunction fct,
				       uval data, Thread *thread);

    SysStatus scheduleFunction(Scheduler::ThreadFunction fct,
			       uval data) {
	Disable();
	SysStatus rc = disabledScheduleFunction(fct, data);
	Enable();
	return rc;
    }

    SysStatus scheduleFunction(Scheduler::ThreadFunctionGeneral fct,
			       uval len, char *data) {
	Disable();
	SysStatus rc = disabledScheduleFunction(fct, len, data);
	Enable();
	return rc;
    }

    SysStatus scheduleFunction(Scheduler::ThreadFunction fct,
			       uval data, ThreadID &id) {
	Disable();
	SysStatus rc = disabledScheduleFunction(fct, data, id);
	Enable();
	return rc;
    }

    SysStatus scheduleFunction(Scheduler::ThreadFunctionGeneral fct,
			       uval len, char *data, ThreadID &id) {
	Disable();
	SysStatus rc = disabledScheduleFunction(fct, len, data, id);
	Enable();
	return rc;
    }

    SysStatus scheduleFunction(Scheduler::ThreadFunction fct,
			       uval data, Thread *thread) {
	Disable();
	SysStatus rc = disabledScheduleFunction(fct, data, thread);
	Enable();
	return rc;
    }


    static void LaunchSandbox(VolatileState *vsp, NonvolatileState *nvsp);

    static void SystemEnter(Scheduler::SystemSavedState *saveAreaPtr);
    static uval SystemExit(Scheduler::SystemSavedState *saveAreaPtr);

    void disabledInterruptThread(Thread *thread,
				 Scheduler::InterruptFunction fct, uval data);

    SysStatus disabledRemoveThread(Thread *thread);
    Thread *disabledRemoveThread();
    SysStatus disabledAddThread(Thread *thread);

    SysStatus addThread(Thread *thread) {
	Disable();
	SysStatus rc = disabledAddThread(thread);
	Enable();
	return rc;
    }

    void resumeThread(Thread *thread) {
	Disable();
	makeReady(thread);
	Enable();
    }

    // array of soft interrupt pointers
    SoftIntr::IntrFunc interruptFunction[SoftIntr::MAX_INTERRUPTS];

    static void ProcessPreempt(SoftIntr::IntrType);
    static void ProcessPulse(SoftIntr::IntrType);
    static void DoPrintStatus(uval dummy);
    static void ProcessPrintStatus(SoftIntr::IntrType);
    static void ProcessPgfltCompletions(SoftIntr::IntrType);
    static void ProcessIPCRetryNotifs(SoftIntr::IntrType);
    static void ProcessAsyncMsgs(SoftIntr::IntrType);

    void createAsyncMsgHandler();
    void asyncMsgHandler();

    static void AsyncMsgHandler(uval dummy);

    Thread *getThread(ThreadID id) {
	return threadArray[Thread::GetKey(id)];
    }
    ThreadID getCurThread() {return CurrentThread->getID();}

    void setDisabledMsgMgr(MPMsgMgrDisabled *mgr) {disabledMsgMgr = mgr;}
    void setEnabledMsgMgr(MPMsgMgrEnabled *mgr) {enabledMsgMgr = mgr;}
    MPMsgMgrDisabled *getDisabledMsgMgr() {return disabledMsgMgr;}
    MPMsgMgrEnabled *getEnabledMsgMgr() {return enabledMsgMgr;}

    void disabledJoinGroupSelf(Thread::Group g);
    void disabledLeaveGroupSelf(Thread::Group g);
    void disabledBarGroup(Thread::Group g);
    void disabledUnbarGroup(Thread::Group g);
    void disabledSetGroupsSelf(uval mask);
    uval disabledReplaceBarMask(uval mask);

    uval getBarredGroups() { return barredGroups; }

    void getStatus(uval &keyIterator, uval &numThreads,
		   uval maxThreads, Thread::Status *status);
    void printStatus();

    void enterDebugger() {
	currentDebugStack = auxDebugStack;
	savedThread = CurrentThread;
	CurrentThread = debuggerThread;
    }

    void exitDebugger() {
	currentDebugStack = mainDebugStack;
	CurrentThread = savedThread;
	savedThread = NULL;
    }

    uval inDebugger() { return currentDebugStack == auxDebugStack; }

public:
    /*
     * Routines that are invoked (indirectly) from assembly language, and
     * that therefore must be public.
     */
    Thread *createThread();
    static void ThreadBase(DispatcherDefault *dispatcher,
			   Scheduler::ThreadFunction fct,
			   uval data);
    static void ThreadBase(DispatcherDefault *dispatcher,
			   Scheduler::ThreadFunctionGeneral fct,
			   uval len, char *data);
    void interruptYield();
    void selfInterrupt();
    void pgfltBlock(uval faultInfo, uval faultAddr, uval faultID);
    void handleInterrupts();
};

/*
 * DispatcherDefault.C routines called from DispatcherDefaultAsm.S.
 */
extern "C" Thread *DispatcherDefault_CreateThread(DispatcherDefault
								*dispatcher);
extern "C" void DispatcherDefault_ThreadBase(DispatcherDefault *dispatcher,
					     Scheduler::ThreadFunction fct,
					     uval data);
extern "C" void DispatcherDefault_ThreadBaseGeneral(
					DispatcherDefault *dispatcher,
					Scheduler::ThreadFunctionGeneral fct,
					uval len, char *data);
extern "C" void DispatcherDefault_InterruptYield(DispatcherDefault *dispatcher);
extern "C" void DispatcherDefault_SelfInterrupt(DispatcherDefault *dispatcher);
extern "C" void DispatcherDefault_PgfltBlock(DispatcherDefault *dispatcher,
					     uval faultInfo,
					     uval faultAddr,
					     uval faultID);
extern "C" void DispatcherDefault_HandleInterrupts(DispatcherDefault
							    *dispatcher);
extern "C" void DispatcherDefault_TraceCurThread();
extern "C" void DispatcherDefault_TracePPCXObjFct(uval func);

/*
 * DispatcherDefaultAsm.S routines called from DispatcherDefault.C.
 */
extern "C" void DispatcherDefault_InitThread(Thread *thread,
					     Scheduler::ThreadFunction fct,
					     uval data);
extern "C" void DispatcherDefault_InitThreadGeneral(
					Thread *thread,
					Scheduler::ThreadFunctionGeneral fct,
					uval len, char *data);
extern "C" void DispatcherDefault_LaunchSandbox(VolatileState *vsp,
						NonvolatileState *nvsp);
extern "C" void
    DispatcherDefault_InterruptThread(Thread *thread,
				      Scheduler::InterruptFunction fct,
				      uval data);
extern "C" void DispatcherDefault_Suspend(DispatcherDefault *dispatcher);
extern "C" void DispatcherDefault_GotoRunEntry(DispatcherDefault *dispatcher)
    __attribute__ ((noreturn));
extern "C" SysStatus
    DispatcherDefault_SetEntryPoint(EntryPointNumber entryPoint,
				    EntryPointDesc entry);
extern "C" void DispatcherDefault_HandoffProcessor(CommID targetID);

/*
 * DispatcherDefaultAsm.S descriptors for all entry points.
 */
extern EntryPointDesc DispatcherDefault_RunEntry_Desc;
extern EntryPointDesc DispatcherDefault_InterruptEntry_Desc;
extern EntryPointDesc DispatcherDefault_TrapEntry_Desc;
extern EntryPointDesc DispatcherDefault_PgfltEntry_Desc;
extern EntryPointDesc DispatcherDefault_IPCCallEntry_Desc;
extern EntryPointDesc DispatcherDefault_IPCReturnEntry_Desc;
extern EntryPointDesc DispatcherDefault_IPCFaultEntry_Desc;
extern EntryPointDesc DispatcherDefault_SVCEntry_Desc;

/*
 * DispatcherDefaultAsm.S descriptor for the direct SVC entry point.
 */
extern EntryPointDesc DispatcherDefault_SVCDirect_Desc;

/*
 * DispatcherDefaultAsm.S code fragments used in DispatcherDefaultExp.C.
 */
extern code DispatcherDefault_SuspendCore;
extern code DispatcherDefault_SuspendCore_Continue;
extern code DispatcherDefault_RunEntry;
extern code DispatcherDefault_InterruptEntry;
extern code DispatcherDefault_TrapEntry;
extern code DispatcherDefault_PgfltEntry;
extern code DispatcherDefault_ProcessInterrupts;
extern code DispatcherDefault_SyscallYieldProcessor;
extern code DispatcherDefault_SyscallHandoffProcessor;
extern code DispatcherDefault_PPCClient;
extern code DispatcherDefault_PPCPrimitiveClientCore;
extern code DispatcherDefault_PPCClientCore;
extern code DispatcherDefault_PPCClientCore_Continue;
extern code DispatcherDefault_IPCCallEntry;
extern code DispatcherDefault_PPCServerOnThread;
extern code DispatcherDefault_InvokeXObjMethod;
extern code DispatcherDefault_IPCReturnEntry;
extern code DispatcherDefault_IPCFaultEntry;
extern code DispatcherDefault_IPCFaultOnThread;
extern code DispatcherDefault_SyscallIPCCall;
extern code DispatcherDefault_SyscallIPCReturn;

#endif /* #ifndef __DISPATCHER_DEFAULT_H_ */
