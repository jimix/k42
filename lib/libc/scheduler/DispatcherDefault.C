/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefault.C,v 1.201 2005/06/13 14:10:58 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Basic user-level dispatcher functionality.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "Thread.H"
#include "DispatcherDefault.H"
#include <limits.h>
#include <sys/BaseProcess.H>
#include <scheduler/Scheduler.H>
#include <scheduler/SchedulerTimer.H>
#include <scheduler/SchedulerService.H>
#include <sys/ppccore.H>
#include <cobj/XHandleTrans.H>
#include <sys/syscalls.H>
#include <sys/entryPoints.H>
#include <sys/Dispatcher.H>
#include <xobj/XBaseObj.H>
#include <misc/expedient.H>
#include <sync/MPMsgMgr.H>
#include <sync/BlockedThreadQueues.H>
#include <sync/SLock.H>
#include <misc/linkage.H>
#include <trace/traceScheduler.h>
#include <mem/Access.H>
#include <stub/StubFRComputation.H>
#include <stub/StubRegionDefault.H>
#include <emu/sandbox.H>

class DispatcherDefault::ThreadPool {
public:
    DEFINE_PRIMITIVE_NEW(DispatcherDefault::ThreadPool);

    // Maintain a pool of memory for thread stacks:
    uval threadAllocSz;
    uval threadStackReservation;
    uval memoryPool;
    uval memoryPoolTop;

    /*
     * We maintain of linked list of thread stacks that have been reclaimed
     * following a fork.  allocThread() checks the list before allocating a
     * new stack from memoryPool.  The list is protected by a lock, which
     * must be a spin lock because it is always acquired disabled.
     */
    SLock lock;
    uval reclaimedStacks;

    void init(MemoryMgrPrimitive *memory,
	      uval threadCount, uval threadAllocSz,
	      uval threadStackReservation) {
	this->threadAllocSz = threadAllocSz;
	this->threadStackReservation = threadStackReservation;
	// FIXME: This allocates a pool of memory large enough to hold
	// threadCount threads, shared by all dispatchers in this process.
	// But each dispatcher allocates a vector of pointers of the same
	// size.  So we are likely to run out of this resource first
	// if there are multiple dispatchers.
	memory->alloc(memoryPool, threadCount * threadAllocSz, PAGE_SIZE);
	memoryPoolTop = memoryPool + threadCount * threadAllocSz;
	lock.init();
	reclaimedStacks = 0;
    }

    // This allocator is far from ideal -- it would be better to
    // allocate memory locally on each processor.  But this is simple,
    // and it works.
    Thread * allocThread() {
	uval stack;
	Thread *thread;

	stack = 0;

	tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
	if (reclaimedStacks != 0) {
	    lock.acquire();
	    if (reclaimedStacks != 0) {
		stack = reclaimedStacks;
		reclaimedStacks = *((uval *) stack);
	    }
	    lock.release();
	}

	if (stack == 0) {
	    stack = FetchAndAdd(&memoryPool, threadAllocSz);
	    passertMsg(stack < memoryPoolTop, "Out of threads\n");
	}

#ifndef NDEBUG
	// Trash the stack:
	memset((void *) stack, 0xbf, threadAllocSz);
#endif /* #ifndef NDEBUG */

	thread = (Thread *) (stack + threadAllocSz - sizeof(Thread));
	thread->init((void *) thread,
		     (void *) (stack + threadStackReservation),
                     (void *) stack);

	DEBUG_CREATE_STACK_FENCE(thread);

	return thread;
    }

    void reclaim(Thread *thread) {
	tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
	uval stack;
	lock.acquire();
	stack = uval(thread) + sizeof(Thread) - threadAllocSz;
	*((uval *) stack) = reclaimedStacks;
	reclaimedStacks = stack;
	lock.release();
    }
};

/*static*/ DispatcherDefault::ThreadPool *
		    DispatcherDefault::TheThreadPool = NULL;

Thread *
DispatcherDefault::createThread()
{
    Thread *thread;

    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    thread = TheThreadPool->allocThread();
    allocThreadKey(thread);
    thread->activeCntP = NULL;

    return thread;
}

extern "C" Thread *
DispatcherDefault_CreateThread(DispatcherDefault *dispatcher)
{
    return dispatcher->createThread();
}

void
DispatcherDefault::initCore()
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    arch.init();

    // set flag on so init can do Primitive PPC's
    allowPrimitivePPCFlag = 1;
    sandboxShepherd = NULL;
}

void
DispatcherDefault::init(DispatcherID dspidArg, Thread *thread,
			MemoryMgrPrimitive *memory,
			uval threadCount, uval threadAllocSz,
			uval threadStackReservation,
			Scheduler::ThreadFunction fct, uval fctArg)
{
    uval mem;

    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    tassertMsg(dspidArg == dspid, "Wrong id in DispatcherDefault::init().\n");

    enum {SCHEDULER_STACK_SIZE = (2 * PAGE_SIZE)};
    memory->alloc(mem, SCHEDULER_STACK_SIZE, PAGE_SIZE);
    dispatcherStack = mem + SCHEDULER_STACK_SIZE;

    if (dspid == SysTypes::DSPID(0,0)) {
	TheThreadPool = new(memory) ThreadPool;
	TheThreadPool->init(memory, threadCount, threadAllocSz,
			    threadStackReservation);
    }

    memory->alloc(mem, threadCount * sizeof(Thread *), sizeof(Thread *));
    threadArray = (Thread **) mem;
    threadArraySize = threadCount;
    for (uval i = 0; i < threadArraySize; i++) {
	threadArray[i] = NULL;
    }

    nextThreadKey = 0;
    freeList = NULL;
    setReadyQueue(NULL);
    readyQueueTail = NULL;

    barredGroups = 0;

    for (uval i = 0; i < Dispatcher::NUM_PGFLT_IDS; i++) {
	pgfltList[i] = NULL;
    }

    for (uval i = 0; i < Dispatcher::NUM_IPC_RETRY_IDS; i++) {
	ipcRetryList[i] = NULL;
    }

    barredList = NULL;

    yieldRequested = 0;
    preemptRequested = 0;
    rescheduleNeeded = 0;

    // initialize all function pointers to null
    for (uval i = 0; i < SoftIntr::MAX_INTERRUPTS; i++) {
	interruptFunction[i] = NULL;
    }
    interruptFunction[SoftIntr::PREEMPT] = ProcessPreempt;
    interruptFunction[SoftIntr::PRINT_STATUS] = ProcessPrintStatus;
    interruptFunction[SoftIntr::PGFLT_COMPLETION] = ProcessPgfltCompletions;
    interruptFunction[SoftIntr::ASYNC_MSG] = ProcessAsyncMsgs;
    interruptFunction[SoftIntr::TIMER_EVENT] = SchedulerTimer::TimerInterrupt;
    interruptFunction[SoftIntr::IPC_RETRY_NOTIFY] = ProcessIPCRetryNotifs;
    interruptFunction[SoftIntr::PULSE] = ProcessPulse;

    timer.init(memory);

    asyncReadyHandlerExists = 0;
    asyncBusyHandlerCnt = 0;

    // turn off flag - we are about to enable for the first time
    // value should still be one.
    // N.B. we do not use the normal macro here - we never tried to
    // save the PPC page.
    tassertMsg(allowPrimitivePPCFlag == 1, "oops\n");
    allowPrimitivePPCFlag = 0;

    enum {DEBUG_STACK_SIZE = (2 * PAGE_SIZE)};
    memory->alloc(mem, sizeof(Thread), 8);
    debuggerThread = (Thread *) mem;
    debuggerThread->init(0, 0, 0);
    savedThread = NULL;
    memory->alloc(mem, DEBUG_STACK_SIZE, PAGE_SIZE);
    mainDebugStack = mem + DEBUG_STACK_SIZE;
    memory->alloc(mem, DEBUG_STACK_SIZE, PAGE_SIZE);
    auxDebugStack = mem + DEBUG_STACK_SIZE;
    currentDebugStack = mainDebugStack;

    if (thread == NULL) {
	disabledScheduleFunction(fct, fctArg);
    } else {
	disabledScheduleFunction(fct, fctArg, thread);
    }
    DispatcherDefault_GotoRunEntry(this);
}

/*
 * used to pass some values from fork parent to child.
 * remember that the dispatcher is in the memory provided by
 * the kernel for each vp, so its content does not survive in the
 * child process.
 */
struct DispatcherDefault::ForkInfo {
    uval dispatcherStack;
    Thread **threadArray;
    uval threadArraySize;
    MPMsgMgrDisabled *disabledMsgMgr;
    MPMsgMgrEnabled *enabledMsgMgr;
    SchedulerTimer::ForkData timerState;
};

/*static*/ void
DispatcherDefault::AllocForkInfo(uval &forkInfoUval)
{
    forkInfoUval = (uval) AllocGlobal::alloc(sizeof(ForkInfo));
    tassertMsg(forkInfoUval != 0, "ForkInfo allocation failed.\n");
}

/*static*/ void
DispatcherDefault::DeallocForkInfo(uval forkInfoUval)
{
    AllocGlobal::free((void *) forkInfoUval, sizeof(ForkInfo));
}

void
DispatcherDefault::disabledPreFork(uval forkInfoUval)
{
    ForkInfo *const forkInfo = (ForkInfo *) forkInfoUval;
    forkInfo->dispatcherStack = dispatcherStack;
    forkInfo->threadArray = threadArray;
    forkInfo->threadArraySize = threadArraySize;
    forkInfo->disabledMsgMgr = disabledMsgMgr;
    forkInfo->enabledMsgMgr = enabledMsgMgr;
    timer.getPreForkData(&forkInfo->timerState);
}

void
DispatcherDefault::disabledPostFork(uval forkInfoUval,
				    Scheduler::ThreadFunction fct,
				    uval fctArg)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    ForkInfo *const forkInfo = (ForkInfo *) forkInfoUval;

    dispatcherStack = forkInfo->dispatcherStack;
    threadArray = forkInfo->threadArray;
    threadArraySize = forkInfo->threadArraySize;
    disabledMsgMgr = forkInfo->disabledMsgMgr;
    enabledMsgMgr = forkInfo->enabledMsgMgr;

    for (uval i = 0; i < threadArraySize; i++) {
	Thread *thread = threadArray[i];
	if ((thread != NULL) && (thread->extensionID == 0)) {
	    TheThreadPool->reclaim(thread);
	}
    }

    memset((void *) threadArray, 0, threadArraySize*sizeof(threadArray[0]));

    nextThreadKey = 0;
    freeList = NULL;
    setReadyQueue(NULL);
    readyQueueTail = NULL;

    for (uval i = 0; i < Dispatcher::NUM_PGFLT_IDS; i++) {
	pgfltList[i] = NULL;
    }

    for (uval i = 0; i < Dispatcher::NUM_IPC_RETRY_IDS; i++) {
	ipcRetryList[i] = NULL;
    }

    yieldRequested = 0;
    preemptRequested = 0;
    rescheduleNeeded = 0;

    // initialize all function pointers to null
    for (uval i = 0; i < SoftIntr::MAX_INTERRUPTS; i++) {
	interruptFunction[i] = NULL;
    }
    interruptFunction[SoftIntr::PREEMPT] = ProcessPreempt;
    interruptFunction[SoftIntr::PRINT_STATUS] = ProcessPrintStatus;
    interruptFunction[SoftIntr::PGFLT_COMPLETION] = ProcessPgfltCompletions;
    interruptFunction[SoftIntr::ASYNC_MSG] = ProcessAsyncMsgs;
    interruptFunction[SoftIntr::TIMER_EVENT] = SchedulerTimer::TimerInterrupt;
    interruptFunction[SoftIntr::IPC_RETRY_NOTIFY] = ProcessIPCRetryNotifs;
    interruptFunction[SoftIntr::PULSE] = ProcessPulse;

    timer.initPostFork(&forkInfo->timerState);

    disabledScheduleFunction(fct, fctArg);

    asyncReadyHandlerExists = 0;
    asyncBusyHandlerCnt = 0;

    // turn off flag - we are about to enable for the first time
    // value should still be one.
    // N.B. we do not use the normal macro here - we never tried to
    // save the PPC page.
    tassertMsg(allowPrimitivePPCFlag == 1, "oops\n");
    allowPrimitivePPCFlag = 0;
    DispatcherDefault_GotoRunEntry(this);
}

void
DispatcherDefault::enableEntryPoint(EntryPointNumber entry)
{
    EntryPointDesc *desc;
    SysStatus rc;

    switch (entry) {
    case RUN_ENTRY:       desc = &DispatcherDefault_RunEntry_Desc;       break;
    case INTERRUPT_ENTRY: desc = &DispatcherDefault_InterruptEntry_Desc; break;
    case TRAP_ENTRY:      desc = &DispatcherDefault_TrapEntry_Desc;      break;
    case PGFLT_ENTRY:     desc = &DispatcherDefault_PgfltEntry_Desc;     break;
    case IPC_CALL_ENTRY:  desc = &DispatcherDefault_IPCCallEntry_Desc;   break;
    case IPC_RTN_ENTRY:   desc = &DispatcherDefault_IPCReturnEntry_Desc; break;
    case IPC_FAULT_ENTRY: desc = &DispatcherDefault_IPCFaultEntry_Desc;  break;
    case SVC_ENTRY:       desc = &DispatcherDefault_SVCEntry_Desc;       break;
    default:
	passertMsg(0, "Unknown entry point number %d.\n", entry);
	return;
    }
    rc = DispatcherDefault_SetEntryPoint(entry, *desc);
    passertSilent(_SUCCESS(rc), BREAKPOINT) // SetEntryPoint failed
}

void
DispatcherDefault::disableEntryPoint(EntryPointNumber entry)
{
    EntryPointDesc desc;
    SysStatus rc;

    desc.nullify();
    rc = DispatcherDefault_SetEntryPoint(entry, desc);
    passertMsg(_SUCCESS(rc), "SetEntryPoint failed.\n");
}

/*static*/ void
DispatcherDefault::SetupSVCDirect()
{
    SysStatus rc;
    uval size;

    // Map in system-call entry table
    ObjectHandle callTableFROH;
    rc = StubFRComputation::_Create(callTableFROH);
    tassertMsg(_SUCCESS(rc), "SVCDirect FR creation failed.\n");

    size = PAGE_ROUND_UP(sizeof(DispatcherDefault_SVCDirect_Desc));

    rc = StubRegionDefault::_CreateFixedAddrLenExt(
	SVCDirectVectorAddr, size, callTableFROH, 0,
	(uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::ForkCopy+RegionType::KeepOnExec);
    tassertMsg(_SUCCESS(rc), "SVCDirect region creation failed.\n");

    *((EntryPointDesc *) SVCDirectVectorAddr) =
				DispatcherDefault_SVCDirect_Desc;
}

inline /*static*/ void
DispatcherDefault::ThreadBase(DispatcherDefault *dispatcher,
			      Scheduler::ThreadFunction fct,
			      uval data)
{
    Enable();
    CurrentThread->activate();
    fct(data);
    CurrentThread->deactivate();
    tassertMsg(!CurrentThread->isMigratable(), "Migratable!\n");
    Disable();
    dispatcher = DISPATCHER;	// must re-fetch;  may have migrated in fct
    dispatcher->freeThread(CurrentThread);
    DispatcherDefault_GotoRunEntry(dispatcher);
}

inline /*static*/ void
DispatcherDefault::ThreadBase(DispatcherDefault *dispatcher,
			      Scheduler::ThreadFunctionGeneral fct,
			      uval len, char *data)
{
    Enable();
    CurrentThread->activate();
    fct(len, data);
    CurrentThread->deactivate();
    tassertMsg(!CurrentThread->isMigratable(), "Migratable!\n");
    Disable();
    dispatcher = DISPATCHER;	// must re-fetch;  may have migrated in fct
    dispatcher->freeThread(CurrentThread);
    DispatcherDefault_GotoRunEntry(dispatcher);
}

extern "C" void
DispatcherDefault_ThreadBase(DispatcherDefault *dispatcher,
			     Scheduler::ThreadFunction fct,
			     uval data)
{
    DispatcherDefault::ThreadBase(dispatcher, fct, data);
}

extern "C" void
DispatcherDefault_ThreadBaseGeneral(DispatcherDefault *dispatcher,
				    Scheduler::ThreadFunctionGeneral fct,
				    uval len, char *data)
{
    DispatcherDefault::ThreadBase(dispatcher, fct, len, data);
}

SysStatus
DispatcherDefault::disabledScheduleFunction(Scheduler::ThreadFunction fct,
					    uval data)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    Thread *thread = allocThread();
    if (thread == NULL) {
	return _SERROR(1514, 0, ENOMEM);
    }
    TraceOSSchedulerThreadCreate(uval64(thread),uval64(fct));
    DispatcherDefault_InitThread(thread, fct, data);
    makeReady(thread);
    return 0;
}

SysStatus
DispatcherDefault::disabledScheduleFunction(
					Scheduler::ThreadFunctionGeneral fct,
					uval len, char *data)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    Thread *thread = allocThread();
    if (thread == NULL) {
	return _SERROR(1515, 0, ENOMEM);
    }
    TraceOSSchedulerThreadCreate(uval64(thread),uval64(fct));
    DispatcherDefault_InitThreadGeneral(thread, fct, len, data);
    makeReady(thread);
    return 0;
}

SysStatus
DispatcherDefault::disabledScheduleFunction(Scheduler::ThreadFunction fct,
					    uval data, ThreadID &id)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    Thread *thread = allocThread();
    if (thread == NULL) {
	return _SERROR(1516, 0, ENOMEM);
    }
    id = thread->getID();
    TraceOSSchedulerThreadCreate(uval64(thread),uval64(fct));
    DispatcherDefault_InitThread(thread, fct, data);
    makeReady(thread);
    return 0;
}

SysStatus
DispatcherDefault::disabledScheduleFunction(
					Scheduler::ThreadFunctionGeneral fct,
					uval len, char *data, ThreadID &id)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    Thread *thread = allocThread();
    if (thread == NULL) {
	return _SERROR(1518, 0, ENOMEM);
    }
    id = thread->getID();
    TraceOSSchedulerThreadCreate(uval64(thread),uval64(fct));
    DispatcherDefault_InitThreadGeneral(thread, fct, len, data);
    makeReady(thread);
    return 0;
}

SysStatus
DispatcherDefault::disabledScheduleFunction(Scheduler::ThreadFunction fct,
					    uval data, Thread *thread)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    DEBUG_CHECK_STACK(thread);
    allocThreadKey(thread);
    TraceOSSchedulerThreadCreate(uval64(thread),uval64(fct));
    DispatcherDefault_InitThread(thread, fct, data);
    makeReady(thread);
    return 0;
}

/*static*/ void
DispatcherDefault::LaunchSandbox(VolatileState *vsp, NonvolatileState *nvsp)
{
    tassertMsg(CurrentThread->extensionID != 0, "Thread is a base thread.\n");

    CurrentThread->deactivate();
    Disable();

    while (CurrentThread->upcallRequested()) {
	K42Linux_SandboxUpcall(vsp, nvsp);
    }

    DISPATCHER->sandboxShepherd = CurrentThread;	// enter sandbox mode

    DispatcherDefault_LaunchSandbox(vsp, nvsp);
    // NOTREACHED
}

/*static*/ void
DispatcherDefault::SystemEnter(Scheduler::SystemSavedState *saveAreaPtr)
{
    Disable();
    tassertMsg(DISPATCHER->sandboxShepherd != NULL,
	       "Not in sandbox mode.\n");
    saveAreaPtr->curThread = CurrentThread;
    CurrentThread = DISPATCHER->sandboxShepherd;
    DISPATCHER->sandboxShepherd = NULL;
    CurrentThread->bottomSP = 0;	// disable stack checking
    Enable();
    CurrentThread->activate();
}

/*static*/ uval
DispatcherDefault::SystemExit(Scheduler::SystemSavedState *saveAreaPtr)
{
    uval upcallNeeded;

    CurrentThread->deactivate();
    Disable();
    tassertMsg(CurrentThread != NULL,
	       "SystemExit with NULL CurrentThread.\n");
    tassertMsg(DISPATCHER->sandboxShepherd == NULL,
	       "Already in sandbox mode.\n");
    upcallNeeded = CurrentThread->upcallRequested();
    // re-enable stack checking
    CurrentThread->bottomSP = CurrentThread->truebottomSP;
    DISPATCHER->sandboxShepherd = CurrentThread;
    CurrentThread = saveAreaPtr->curThread;

    /*
     * We have to Enable() at this point, but we have to be very careful.
     * We'll be enabling into sandbox mode, which means that we might be
     * migrated immediately after we clear the disabled flag and before we can
     * check for soft interrupts.  We have to remember the dispatcher on which
     * we were disabled and for whose pending interrupts we may be responsible.
     * We check for interrupts there, but if we find any, by the time we can
     * re-disable to process them, we may have migrated to a different
     * dispatcher.  It's okay.  If we didn't migrate (by far the usual case)
     * we'll have taken care of our responsibility.  And if we did migrate,
     * we'll simply have yielded once on our new dispatcher unnecessarily.  We
     * could only have migrated because of a software interrupt on the old
     * dispatcher, and any pending interrupts for which we were responsible
     * would have been handled at the same time as the one that initiated the
     * migration.  In effect, migration absolves us of all responsibility for
     * interrupts that arrived while we still had the original dispatcher
     * disabled.  We just have to be careful not to use the remembered
     * dispatcher pointer for anything other than reading the interrupt flags
     * word from our recent dispatcher.
     */
    DispatcherDefault *dispatcher;
    dispatcher = DISPATCHER;
    extRegsLocal.disabled = 0;
    while (dispatcher->wasInterrupted()) {
	extRegsLocal.disabled = 1;
	dispatcher = DISPATCHER;	// re-fetch dispatcher pointer
	CurrentThread = dispatcher->sandboxShepherd;
	dispatcher->sandboxShepherd = NULL;
	CurrentThread->bottomSP = 0;	// disable stack checking
	dispatcher->selfInterrupt();
	// re-enable stack checking
	CurrentThread->bottomSP = CurrentThread->truebottomSP;
	dispatcher->sandboxShepherd = CurrentThread;
	CurrentThread = saveAreaPtr->curThread;
	extRegsLocal.disabled = 0;
    }

    return upcallNeeded;
}

void
DispatcherDefault::disabledInterruptThread(Thread *thread,
					   Scheduler::InterruptFunction fct,
					   uval data)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    if (thread->pgfltID != uval(-1)) {
	/*
	 * The thread is blocked on a page fault.  Put it back on the ready
	 * queue so that it will run the interrupt function immediately.
	 * It may repeat the fault after processing the interrupt.
	 */
	if (pgfltList[thread->pgfltID] == thread) {
	    pgfltList[thread->pgfltID] = thread->next;
	} else {
	    Thread *list;
	    list = pgfltList[thread->pgfltID];
	    for (;;) {
		tassertMsg(list != NULL, "Thread not found on pgfltList.\n");
		if (list->next == thread) break;
		list = list->next;
	    }
	    list->next = thread->next;
	}
	thread->pgfltID = uval(-1);
	makeReady(thread);
    }

    DispatcherDefault_InterruptThread(thread, fct, data);
}

SysStatus
DispatcherDefault::disabledRemoveThread(Thread *thread)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    if (CurrentThread == thread ||     // Can't move current thread
	thread == NULL ||              // ...or no thread
	!thread->isMigratable() ||     // ...or marked non-migratable
	getReadyQueue() == NULL ||     // ...or non-ready thread
	thread->isActive()) {	       // ...or an active thread
	return _SERROR(1410, 0, EINVAL);
    }

    DEBUG_CHECK_STACK(thread);

    // Remove the thread from the readyQueue:
    if (thread == getReadyQueue()) {
	setReadyQueue(thread->next);
    } else {
	Thread *prev;
	prev = getReadyQueue();
	while (prev->next != thread) {
	    prev = prev->next;

	    if (prev == NULL) {
		// Thread not found in ready queue:
		return _SERROR(1411, 0, EINVAL);
	    }
	}
	prev->next = thread->next;
    }
    disabledFreeThreadKey(thread);
    return 0;
}

Thread *
DispatcherDefault::disabledRemoveThread()
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    Thread *thread, *prev;

    thread = getReadyQueue();
    prev = NULL;
    // Search for a migratable thread in the readyQueue:
    while (thread != NULL && !thread->isMigratable()) {
	prev = thread;
	thread = thread->next;
    }
    if (thread != NULL) {
	// Remove the thread from the readyQueue:
	tassertMsg(!thread->isActive(),
		   "Active thread can not be migratable.\n");

	if (prev != NULL) {
	    prev->next = thread->next;
	    if (readyQueueTail == thread) {
		readyQueueTail = prev;
	    }
	} else {
	    setReadyQueue(thread->next);
	}
	disabledFreeThreadKey(thread);

	DEBUG_CHECK_STACK(thread);
    }

    return thread;
}

SysStatus
DispatcherDefault::disabledAddThread(Thread *thread)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    tassertMsg(thread != NULL, "Migrated NULL thread.\n");
    tassertMsg(!thread->isActive(), "Migrated active thread.\n");

    allocThreadKey(thread);
    makeReady(thread);

    return 0;
}

void
DispatcherDefault::disabledSuspend()
{
    tassertMsg(GET_PPC_LENGTH() == 0,
	       "suspending while PPC page in use. dspid %ld, ppclen %lx\n",
	       dspid, GET_PPC_LENGTH());

    DEBUG_CHECK_STACK(CurrentThread);

    DispatcherDefault_Suspend(this);
}

void
DispatcherDefault::disabledYield()
{
    tassertMsg(GET_PPC_LENGTH() == 0,
	       "yielding while PPC page in use. dspid %ld, ppclen %lx\n",
	       dspid, GET_PPC_LENGTH());

    DEBUG_CHECK_STACK(CurrentThread);

    CurrentThread->yielding();
    makeReady(CurrentThread);
    DispatcherDefault_Suspend(this);
}

void
DispatcherDefault::disabledYieldProcessor()
{
    preemptRequested = 1;
    disabledYield();
}

void
DispatcherDefault::disabledHandoffProcessor(CommID targetID)
{
    tassertMsg(GET_PPC_LENGTH() == 0,
	       "handing off while PPC page in use. dspid %ld, ppclen %lx\n",
	       dspid, GET_PPC_LENGTH());

    DEBUG_CHECK_STACK(CurrentThread);

    CurrentThread->yielding();
    makeReady(CurrentThread);
    DispatcherDefault_HandoffProcessor(targetID);
}

void
DispatcherDefault::disabledBlock()
{
    tassertMsg(GET_PPC_LENGTH() == 0,
	       "blocking while PPC page in use. dspid %ld, ppclen %lx\n",
	       dspid, GET_PPC_LENGTH());
    DEBUG_CHECK_STACK(CurrentThread);

    if (CurrentThread->wasUnblocked) {
	CurrentThread->wasUnblocked = 0;
    } else {
	CurrentThread->isBlocked = 1;
	CurrentThread->blocking();
	DispatcherDefault_Suspend(this);
    }
}

void
DispatcherDefault::disabledUnblock(ThreadID thid, uval makeFirst)
{
    tassertMsg(Thread::GetDspID(thid) == dspid,
	       "thread not on this dispatcher\n");

    Thread *const thread = getThread(thid);

    if (thread == NULL) {
	/*
	 * The target thread must have migrated from this dispatcher.  The
	 * condition for which the thread was blocking must have been
	 * satisfied, allowing the thread to proceed, make itself migratable,
	 * and actually get itself migrated.  We can safely discard this
	 * unblock call because the target thread is obviously not waiting
	 * for it.  We're assuming, of course, that the thread was properly
	 * non-migratable when it was preparing to block.
	 */
	return;
    }

    DEBUG_CHECK_STACK(thread);

    if (thread->isBlocked) {
	thread->isBlocked = 0;
	if (makeFirst) {
	    makeReadyFirst(thread);
	} else {
	    makeReady(thread);
	}
    } else {
	thread->wasUnblocked = 1;
    }
}

struct DispatcherDefault::UnblockMsg : public MPMsgMgr::MsgAsync {
    ThreadID thid;
    uval makeFirst;

    virtual void handle() {
	// Note, blocked threads better not be migratable
	Scheduler::DisabledUnblockOnThisDispatcher(thid, makeFirst);
	free();
    }
};

void
DispatcherDefault::unblockRemote(ThreadID thid, uval makeFirst)
{
    DispatcherID targetDspID;
    SysStatus rc;

    targetDspID = Thread::GetDspID(thid);
    if (SysTypes::DSPID_VP_COMPARE(targetDspID, dspid) == 0) {
	rc = SchedulerService::Unblock(targetDspID, thid, makeFirst);
	tassertMsg(_SUCCESS(rc), "SchedServ::Unblock failed.\n");
    } else {
	UnblockMsg *const msg = new(getDisabledMsgMgr()) UnblockMsg;
	tassertMsg(msg != NULL, "message allocate failed.\n");
	msg->thid = thid;
	msg->makeFirst = makeFirst;
	rc = msg->send(targetDspID);
	tassertMsg(_SUCCESS(rc), "disabled msg send failed.\n");
    }
}

void
DispatcherDefault::disabledExit()
{
    tassertMsg(!CurrentThread->isActive(), "exiting while active.\n");
    tassertMsg(CurrentThread->getAltStack() == 0,
               "exiting with altStack != 0\n");
    tassertMsg(GET_PPC_LENGTH() == 0,
	       "exiting while PPC page in use.  dspid %ld, ppclen %lx\n",
	       dspid, GET_PPC_LENGTH());
    freeThread(CurrentThread);
    DispatcherDefault_GotoRunEntry(this);
}

void
DispatcherDefault::exit()
{
    Disable();
    disabledExit();
    // NOTREACHED
}

void
DispatcherDefault::interruptYield()
{
    DEBUG_CHECK_STACK(CurrentThread);

    PRESERVE_PPC_PAGE();

    CurrentThread->yielding();
    if (yieldRequested) {
	yieldRequested = 0;
	makeReady(CurrentThread);
    } else {
	makeReadyFirst(CurrentThread);
    }
    DispatcherDefault_Suspend(this);

    RESTORE_PPC_PAGE();
}

extern "C" void
DispatcherDefault_InterruptYield(DispatcherDefault *dispatcher)
{
    dispatcher->interruptYield();
}

void
DispatcherDefault::selfInterrupt()
{
    handleInterrupts();
    if (rescheduleNeeded) {
	rescheduleNeeded = 0;
	interruptYield();
    }
}

extern "C" void
DispatcherDefault_SelfInterrupt(DispatcherDefault *dispatcher)
{
    dispatcher->selfInterrupt();
}

inline void
DispatcherDefault::pgfltBlock(uval /*faultInfo*/,
			      uval /*faultAddr*/,
			      uval faultID)
{
    CurrentThread->pgfltID = faultID;
    CurrentThread->next = pgfltList[faultID];
    pgfltList[faultID] = CurrentThread;

    PRESERVE_PPC_PAGE();

    CurrentThread->blocking();
    DispatcherDefault_Suspend(this);

    RESTORE_PPC_PAGE();
}

extern "C" void
DispatcherDefault_PgfltBlock(DispatcherDefault *dispatcher,
			     uval faultInfo, uval faultAddr, uval faultID)
{
    dispatcher->pgfltBlock(faultInfo, faultAddr, faultID);
}

inline void
DispatcherDefault::handleInterrupts()
{
    SoftIntr todo;
    SoftIntr::IntrType intr;

    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");
    todo = interrupts.fetchAndClear();
    while (todo.pending()) {
	intr = todo.getAndClearFirstPending();
#ifdef DEBUG_SOFT_INTERRUPTS
	(void) FetchAndAddSigned(&interrupts.outstanding[intr], -1);
#endif /* #ifdef DEBUG_SOFT_INTERRUPTS */
	tassertSilent(interruptFunction[intr] != NULL, BREAKPOINT);
	interruptFunction[intr](intr);
    }
}

extern "C" void
DispatcherDefault_HandleInterrupts(DispatcherDefault *dispatcher)
{
    dispatcher->handleInterrupts();
}

extern "C" void
DispatcherDefault_TraceCurThread()
{
    TraceOSSchedulerCurThread(uval64(CurrentThread));
}

extern "C" void
DispatcherDefault_TracePPCXObjFct(uval func)
{
    TraceOSSchedulerPPCXobjFCT(
		    uval64(CurrentThread), uval64(func));
}

/*static*/ void
DispatcherDefault::ProcessPreempt(SoftIntr::IntrType)
{
    DISPATCHER->preemptRequested = 1;
    DISPATCHER->rescheduleNeeded = 1;
}

/*static*/ void
DispatcherDefault::ProcessPulse(SoftIntr::IntrType)
{
    DISPATCHER->yieldRequested = 1;
    DISPATCHER->rescheduleNeeded = 1;
}

/*static*/ void
DispatcherDefault::DoPrintStatus(uval /*dummy*/)
{
    Scheduler::PrintStatus();
}

/*static*/ void
DispatcherDefault::ProcessPrintStatus(SoftIntr::IntrType)
{
    /*
     * Print status on a thread, rather than directly, so that any thread we
     * happen to have interrupted will be included in the report.
     */
    Scheduler::DisabledScheduleFunction(DoPrintStatus, 0);
}

/*static*/ void
DispatcherDefault::ProcessPgfltCompletions(SoftIntr::IntrType)
{
    uval i, j, id;
    uval64 bits;
    Thread *thrd, *next;

    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    for (i = 0; i < Dispatcher::NUM_PGFLT_ID_WORDS; i++) {
	bits = FetchAndClear64(&DISPATCHER->pgfltCompleted[i]);
	j = 0;
	while (bits != 0) {
	    if ((bits & 0xffffffff) == 0) {bits >>= 32; j += 32;}
	    if ((bits &     0xffff) == 0) {bits >>= 16; j += 16;}
	    if ((bits &       0xff) == 0) {bits >>=  8; j +=  8;}
	    if ((bits &        0xf) == 0) {bits >>=  4; j +=  4;}
	    if ((bits &        0x3) == 0) {bits >>=  2; j +=  2;}
	    if ((bits &        0x1) == 0) {bits >>=  1; j +=  1;}

	    id = ((sizeof(uval))*8) * i + j;
	    thrd = DISPATCHER->pgfltList[id];
	    while (thrd != NULL) {
		next = thrd->next;
		thrd->pgfltID = uval(-1);
		DISPATCHER->makeReady(thrd);
		thrd = next;
	    }
	    DISPATCHER->pgfltList[id] = NULL;

	    bits >>= 1;
	    j++;
	}
    }
}

/*static*/ void
DispatcherDefault::ProcessIPCRetryNotifs(SoftIntr::IntrType)
{
    uval64 targets;
    uval targetBit;
    Thread *thrd, *next;

    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    targets = FetchAndClear64Volatile(&DISPATCHER->ipcRetry);
    targetBit = 0;
    while (targets != 0) {
	if ((targets & 0xffffffff) == 0) {targets >>= 32; targetBit += 32;}
	if ((targets &     0xffff) == 0) {targets >>= 16; targetBit += 16;}
	if ((targets &       0xff) == 0) {targets >>=  8; targetBit +=  8;}
	if ((targets &        0xf) == 0) {targets >>=  4; targetBit +=  4;}
	if ((targets &        0x3) == 0) {targets >>=  2; targetBit +=  2;}
	if ((targets &        0x1) == 0) {targets >>=  1; targetBit +=  1;}

	thrd = DISPATCHER->ipcRetryList[targetBit];
	while (thrd != NULL) {
	    next = thrd->next;
	    DISPATCHER->makeReady(thrd);
	    thrd = next;
	}
	DISPATCHER->ipcRetryList[targetBit] = NULL;

	targets >>= 1;
	targetBit++;
    }
}

/*
 * Asynchronous Message-Handling Strategy
 *
 * Ideally, when messages appear in the asynchronous IPC buffer, we'd
 * create a single thread which would process the messages one by one and
 * then terminate.  However, the thread may block for a page fault or lock
 * while processing a message, and in that case we want a new thread to
 * jump in and start processing any subsequent messages in the buffer.
 * Of course that thread may also block.  We're willing to have as many as
 * ASYNC_BUSY_HANDLER_MAX threads processing async messages.
 *
 * Since we can't predict whether a thread will block while processing a
 * message, we try to keep one uncommitted thread on the ready queue as
 * long as there are messages still in the buffer.  With luck, that thread
 * will find nothing to do when it actually runs and can simply terminate.
 * If it picks up a message, however, and there are yet more messages in
 * the buffer, it must create a new uncommitted ready thread before
 * starting to process its message, unless we've reached the
 * ASYNC_BUSY_HANDLER_MAX limit.  If we've reached the limit, we'll let
 * messages sit in the buffer until the existing pool of threads can get to
 * them.  If the buffer fills up, senders will have to deal with the
 * back-pressure.
 *
 * When we receive an ASYNC_MSG soft interrupt, we simply create an
 * initial ready thread to start the ball rolling.  If a ready thread
 * already exists, we needn't do anything.
 *
 * Synchronization:  all the state variables (asyncReadyHandlerExists and
 * asyncBusyHandlerCnt), as well as the message buffer itself, are manipulated
 * only on this dispatcher and in disabled mode.
 */

void
DispatcherDefault::createAsyncMsgHandler()
{
    SysStatus rc;

    if (asyncBusyHandlerCnt >= ASYNC_BUSY_HANDLER_MAX) {
	/*
	 * We have enough handlers running.  Don't create another even though
	 * there are messages in the buffer.
	 */
	asyncReadyHandlerExists = 0;
    } else {
	rc = Scheduler::DisabledScheduleFunction(AsyncMsgHandler, uval(this));
	passertMsg(_SUCCESS(rc),
		   "Could not create an AsyncMsgHandler thread.\n");
	/*
	 * We successfully created a ready handler.
	 */
	asyncReadyHandlerExists = 1;
    }
}

void
DispatcherDefault::asyncMsgHandler()
{
    SysStatus rc;
    ProcessID badge;
    XHandle xh;
    uval method;
    XBaseObj *xobj;

    /*
     * This thread runs disabled except when actually processing a message.
     * An interface that starts a thread disabled and expects it to return
     * disabled would save us a few cycles here.
     */
    Disable();

    /*
     * At this point we are the current "ready" handler, not yet busy
     * processing a message.
     */
    tassertMsg(asyncReadyHandlerExists,
	       "Unexpected async msg handler.\n");

    /*
     * Try to grab a message from one of the async buffers.  fetchMsg() sets
     * the length before copying the data, so the data will be safe in the
     * PPC page.
     */
    rc = asyncBufferLocal.fetchMsg(badge, xh, method,
				   PPCPAGE_LENGTH, PPCPAGE_DATA);
    if (_SUCCESS(rc)) goto GotAMessage;
    rc = asyncBufferRemote.fetchMsg(badge, xh, method,
				    PPCPAGE_LENGTH, PPCPAGE_DATA);
    if (_SUCCESS(rc)) goto GotAMessage;

    /*
     * The buffers are empty.  (Or at least they were empty a moment ago.  If
     * a message has since arrived, we'll get a new soft interrupt.)  This
     * thread can terminate, leaving no "ready" handler.
     */
    asyncReadyHandlerExists = 0;
    Enable();
    return;

GotAMessage:
    /*
     * We picked up a message, so we're now "busy" and not "ready".
     */
    asyncBusyHandlerCnt++;

    if (asyncBufferLocal.isEmpty() && asyncBufferRemote.isEmpty()) {
	/*
	 * There are no further messages in the buffer, so we don't need
	 * a ready handler.  (Again, if a message has since arrived, we'll
	 * get a new soft interrupt.)
	 */
	asyncReadyHandlerExists = 0;
    } else {
	/*
	 * There are additional messages in at least one of the buffers, so
	 * create a new ready handler to keep the ball rolling if we happen
	 * to block.
	 */
	createAsyncMsgHandler();
    }

    for (;;) {
	/*
	 * Process the message we've grabbed in enabled mode.
	 */
	Enable();
	rc = DREFGOBJ(TheXHandleTransRef)->xhandleToXObj(xh, xobj);
	if (_SUCCESS(rc)) {
	    if ((method >= XBaseObj::FIRST_METHOD) &&
					(method < xobj->__nummeth)) {
		typedef SysStatus (*XObjMethod)(XBaseObj *, ProcessID);
		XObjMethod func;
		func = (XObjMethod)
			(XBaseObj::GetFTable(xobj))[method].getFunc();
		(void) func(xobj, badge);
	    }
	}
	/*
	 * We silently drop any asynchronous request that is invalid in
	 * any way.
	 */

	/*
	 * We may not have called the X-Object method, and even if we called
	 * it, it may not have cleared the PPC page, so we clear it here to
	 * be sure in either case.
	 */
	RESET_PPC();
	Disable();

	/*
	 * Try to grab another message from either buffer.
	 */
	rc = asyncBufferLocal.fetchMsg(badge, xh, method,
				       PPCPAGE_LENGTH, PPCPAGE_DATA);
	if (_SUCCESS(rc)) continue;
	rc = asyncBufferRemote.fetchMsg(badge, xh, method,
					PPCPAGE_LENGTH, PPCPAGE_DATA);
	if (_SUCCESS(rc)) continue;
	break;
    }

    /*
     * There are no more messages in the buffer, so we're no longer busy.
     * Enabling before returning is a bit of a waste.  See comment at top of
     * this function.
     */
    asyncBusyHandlerCnt--;
    Enable();
}

/*static*/ void
DispatcherDefault::AsyncMsgHandler(uval /*dummy*/)
{
    DISPATCHER->asyncMsgHandler();
}

/*static*/ void
DispatcherDefault::ProcessAsyncMsgs(SoftIntr::IntrType intr)
{
    tassertMsg(IsDisabled(), "Dispatcher not disabled.\n");

    if (!DISPATCHER->asyncReadyHandlerExists) {
	DISPATCHER->createAsyncMsgHandler();
    }
}

void
DispatcherDefault::disabledJoinGroupSelf(Thread::Group g)
{
    tassertMsg(g < Thread::NUM_GROUPS, "Invalid group number.\n");
    CurrentThread->joinGroup(g);
    if ((barredGroups & (uval16(1) << g)) != 0) {
	/*
	 * The group we just joined is already barred.  We could put ourselves
	 * directly on the barred list and suspend, but yield will do that for
	 * us.  There's a good chance the group will be unbarred before we
	 * reach the head of the ready queue.
	 */
	disabledYield();
    }
}

void
DispatcherDefault::disabledSetGroupsSelf(uval mask)
{
    CurrentThread->setGroups(mask);
    if ((barredGroups & mask) != 0) {
	/*
	 * The group we just joined is already barred.  We could put ourselves
	 * directly on the barred list and suspend, but yield will do that for
	 * us.  There's a good chance the group will be unbarred before we
	 * reach the head of the ready queue.
	 */
	disabledYield();
    }
}

void
DispatcherDefault::disabledLeaveGroupSelf(Thread::Group g)
{
    tassertMsg(g < Thread::NUM_GROUPS, "Invalid group number.\n");
    CurrentThread->leaveGroup(g);
}

void
DispatcherDefault::disabledBarGroup(Thread::Group g)
{
    tassertMsg(g < Thread::NUM_GROUPS, "Invalid group number.\n");
    barredGroups |= (uval16(1) << g);
}

void
DispatcherDefault::disabledUnbarGroup(Thread::Group g)
{
    Thread *thrd, *prev, *next;

    tassertMsg(g < Thread::NUM_GROUPS, "Invalid group number.\n");
    barredGroups &= ~(uval16(1) << g);
    prev = NULL;
    thrd = barredList;
    while (thrd != NULL) {
	next = thrd->next;
	if ((thrd->groups & barredGroups) != 0) {
	    // still barred
	    prev = thrd;
	} else {
	    // no longer barred
	    makeReady(thrd);
	    if (prev == NULL) {
		barredList = next;
	    } else {
		prev->next = next;
	    }
	}
	thrd = next;
    }
}

uval
DispatcherDefault::disabledReplaceBarMask(uval newMask)
{
    uval oldGroups = barredGroups;
    uval activated = barredGroups & ~newMask;
    uval i = 1;
    while (activated) {
	if ( activated & 1ULL) {
	    disabledUnbarGroup((Thread::Group)i);
	}
	i <<= 1;
	activated >>= 1;
    }
    barredGroups = newMask;
    return oldGroups;
}

class TimerEventDelay : public TimerEvent {
    ThreadID waiter;			// thread to be unblocked
					// must be in same dispatcher
					// as thread that schedules event
public:
    static SysTime BlockWithTimeout(SysTime when, TimerEvent::Kind kind);
    static void DelayUntil(SysTime when, TimerEvent::Kind kind);
    static void DelaySecs(uval secs);
    static void DelayMicrosecs(uval usecs);
    virtual void handleEvent();
    DEFINE_PINNEDLOCALSTRICT_NEW(TimerEventDelay);
};

void
TimerEventDelay::handleEvent()
{
    /*
     * Event will be fired on same processor as requested,
     * we are exploiting this to be able to handle the unblock
     * disabled.
     */
    ThreadID td;
    td = waiter;
    waiter = Scheduler::NullThreadID;
    Scheduler::DisabledUnblockOnThisDispatcher(td);
};

// return 1 if timeout happened.
/*static*/ SysTime
TimerEventDelay::BlockWithTimeout(SysTime when, TimerEvent::Kind kind)
{
    TimerEventDelay ted;
    ted.waiter = Scheduler::GetCurThread();
    ted.scheduleEvent(when, kind);
    Scheduler::Block();
    if (ted.waiter != Scheduler::NullThreadID) {
	ted.scheduleEvent(SysTime(-1), TimerEvent::reset);
	//return absolute target timeout - makes re-blocking easy
	//this depends on fact that scheduleEvent writes abs targ in when
	return ted.when;
    }
    return 0;
}

/*static*/ void
TimerEventDelay::DelayUntil(SysTime when, TimerEvent::Kind kind)
{
    TimerEventDelay ted;

    if (traceSchedulerEnabled()) {
	uval callChain[5];
	GetCallChainSelf(0, callChain, 5);
	TraceOSSchedulerDelay(
			when - Scheduler::SysTimeNow(),
			callChain[0], callChain[1], callChain[2],
			callChain[3], callChain[4]);
    }

    ted.waiter = Scheduler::GetCurThread();
    ted.scheduleEvent(when, kind);
    while (ted.waiter != Scheduler::NullThreadID) {
	Scheduler::Block();
    }
}

/*static*/ void
TimerEventDelay::DelaySecs(uval secs)
{
    uval64 s = secs;		// in case uval is 32 bits
    SysTime interval;
    interval = s * Scheduler::TicksPerSecond();
    TimerEventDelay::DelayUntil(interval, TimerEvent::relative);
}

/*static*/ void
TimerEventDelay::DelayMicrosecs(uval usecs)
{
    uval64 us = usecs;	// in case uval is 32 bits
    SysTime interval;
    if (us > (uval64(1) << 32)) {
	// lose some precision but avoid overflow
	interval = (us / 1000000) * Scheduler::TicksPerSecond();
    } else {
	interval = (us * Scheduler::TicksPerSecond()) / 1000000;
    }
    TimerEventDelay::DelayUntil(interval, TimerEvent::relative);
}

// return absolute timeout target time if unblocked, 0 if timed out
// this lets caller block again to complete the interval
SysTime
DispatcherDefault::blockWithTimeout(SysTime when, TimerEvent::Kind kind)
{
    return TimerEventDelay::BlockWithTimeout(when, kind);
}

void
DispatcherDefault::delayUntil(SysTime when, TimerEvent::Kind kind)
{
    TimerEventDelay::DelayUntil(when, kind);
}

void
DispatcherDefault::delaySecs(uval secs)
{
    TimerEventDelay::DelaySecs(secs);
}

void
DispatcherDefault::delayMicrosecs(uval usecs)
{
    TimerEventDelay::DelayMicrosecs(usecs);
}

void
DispatcherDefault::getStatus(uval &keyIterator, uval &numThreads,
			     uval maxThreads, Thread::Status *status)
{
    Thread::Status::State *threadState;
    uval key, id;
    Thread *thread, *curTh;
    Thread::Status *stat;

    threadState =
	(Thread::Status::State *)
	    allocLocalStrict(threadArraySize * sizeof(Thread::Status::State));
    tassertMsg(threadState != NULL, "Couldn't allocate threadState array.\n");

    Disable();
    curTh = CurrentThread;

    for (key = 0; key < threadArraySize; key++) {
	threadState[key] = Thread::Status::UNKNOWN;
    }

    if (curTh != NULL) {
	key = curTh->getKey();
	threadState[key] = Thread::Status::RUNNING;
    }

    for (thread = getReadyQueue(); thread != NULL; thread = thread->next) {
	key = thread->getKey();
	threadState[key] = Thread::Status::READY;
    }

    for (thread = freeList; thread != NULL; thread = thread->next) {
	key = thread->getKey();
	threadState[key] = Thread::Status::FREE;
    }

    for (id = 0; id < Dispatcher::NUM_PGFLT_IDS; id++) {
	for (thread = pgfltList[id]; thread != NULL; thread = thread->next) {
	    key = thread->getKey();
	    threadState[key] = Thread::Status::PGFLT_BLOCKED;
	}
    }

    for (id = 0; id < Dispatcher::NUM_IPC_RETRY_IDS; id++) {
	for (thread = ipcRetryList[id]; thread != NULL; thread=thread->next) {
	    key = thread->getKey();
	    threadState[key] = Thread::Status::IPC_RETRY_BLOCKED;
	}
    }

    for (thread = barredList; thread != NULL; thread=thread->next) {
	key = thread->getKey();
	threadState[key] = Thread::Status::BARRED;
    }

    numThreads = 0;

    for (key = keyIterator; key < threadArraySize; key++) {
	thread = threadArray[key];
	if (thread == NULL) continue;

	if (numThreads >= maxThreads) break;
	stat = &status[numThreads++];

	if (thread->isBlocked) {
	    stat->state = Thread::Status::BLOCKED;
	} else if (thread->targetID != SysTypes::COMMID_NULL) {
	    stat->state = Thread::Status::PPC_BLOCKED;
	} else {
	    stat->state = threadState[key];
	}

	stat->attachment = uval(thread->attachment);
	stat->id = thread->getID();
	stat->ptr = uval(thread);
	stat->generation = thread->generation();
	stat->targetID = thread->targetID;
	stat->threadData = thread->getThreadSpecificUval();
	stat->groups = thread->groups;

	switch (stat->state) {
	case Thread::Status::UNKNOWN:
	case Thread::Status::FREE:
	    memset(stat->callChain, 0,
		   Thread::Status::CALL_CHAIN_DEPTH * sizeof(uval));
	    break;
	case Thread::Status::RUNNING:
	    GetCallChainSelf(0, stat->callChain,
			     Thread::Status::CALL_CHAIN_DEPTH);
	    break;
	case Thread::Status::READY:
	case Thread::Status::BLOCKED:
	case Thread::Status::PPC_BLOCKED:
	case Thread::Status::PGFLT_BLOCKED:
	case Thread::Status::IPC_RETRY_BLOCKED:
	case Thread::Status::BARRED:
	    GetCallChain(thread->curSP, stat->callChain,
			 Thread::Status::CALL_CHAIN_DEPTH);
	    break;
	}
    }

    keyIterator = key;

    Enable();
    freeLocalStrict((void *) threadState,
		    threadArraySize * sizeof(Thread::Status::State));
}

void
DispatcherDefault::printStatus()
{
    enum {CHUNK_SIZE = 16};
    uval numThreads, keyIter, i;
    Thread::Status status[CHUNK_SIZE];
    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    err_printf("Thread status for process %ld (%s), rd %ld, vp %ld:\n",
	       processID, progName, rd, vp);

    keyIter = 0;
    do {
	getStatus(keyIter, numThreads, CHUNK_SIZE, status);
	for (i = 0; i < numThreads; i++) {
	    status[i].print();
	}
    } while (numThreads > 0);
}
