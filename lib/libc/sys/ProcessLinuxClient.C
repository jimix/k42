/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessLinuxClient.C,v 1.82 2005/08/11 20:20:45 rosnbrg Exp $
 *****************************************************************************/
#include <sys/sysIncs.H>
#include "ProcessLinuxClient.H"
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include <scheduler/SchedulerService.H>
#include <scheduler/DispatcherMgr.H>
#include <sync/BlockedThreadQueues.H>
#include <sys/ProcessSet.H>
#include <sys/KernelInfo.H>
#include <usr/ProgExec.H>
#include <stub/StubProcessServer.H>
#include <meta/MetaProcessServer.H>
#include <alloc/PageAllocator.H>

#include <stdlib.h>
#include <sched.h>

/*
 * Define AGGRESSIVE_SIGNAL to cause signal target to go to the head of the
 * ready queue.
 */
#undef AGGRESSIVE_SIGNAL

struct ProcessLinuxClient::ThreadClone : public ThreadMigratable {
    BLock lock;
    pid_t clonePID;
    pid_t parentPID;
    int parentSignal;
    enum {ALIVE, EXITING, ZOMBIE} state;
    sval exitStatus;
    uval sigSuspended;
    SigSet sigBlocked;
    SigSet sigPending;
    SigSet sigSuspendSaveBlocked;
    uval cloneStartPending;
    ProcessLinuxClient::CloneStartArg *cloneStartArg;
    uval signalsPending;
    uval migrationPending;
    DispatcherID migrateDspID;
    uval yieldCount;
    uval blockCount;

    void initClone() {
	extensionID = GetThreadExtID();	// mark thread as being a ThreadClone
	migratable = 1;
	lock.init();
	clonePID = -1;
	parentPID = -1;
	parentSignal = 0;
	state = ALIVE;
	exitStatus = 0;
	sigSuspended = 0;
	sigBlocked.empty();
	sigPending.empty();
	sigSuspendSaveBlocked.empty();
	cloneStartPending = 0;
	cloneStartArg = NULL;
	signalsPending = 0;
	migrationPending = 0;
	migrateDspID = DispatcherID(-1);
	yieldCount = 0;
	blockCount = 0;
    }

    void sandboxUpcall(VolatileState *vsp, NonvolatileState *nvsp) {
	while (upcallRequested()) {
	    clearUpcallRequest();

	    if (cloneStartPending) {
		cloneStartPending = 0;
		ProcessLinuxClient::DisabledCloneStart(vsp, nvsp,
						       cloneStartArg);
	    }

	    if (signalsPending) {
		signalsPending = 0;
		ProcessLinuxClient::DisabledPushSignals(vsp, nvsp);
	    }

	    if (migrationPending) {
		migrationPending = 0;
		Scheduler::DisabledScheduleFunction(
				ProcessLinuxClient::MoveThread, uval(this));
		Scheduler::DisabledSuspend();
	    }
	}
    }

    uval syscallSignalsPending() {
	return signalsPending;
    }

    void requestCloneStart(ProcessLinuxClient::CloneStartArg *cloneArg) {
	tassertMsg(!cloneStartPending, "CloneStart request already pending.\n");
	cloneStartPending = 1;
	cloneStartArg = cloneArg;
	requestUpcall();
    }

    void requestSignals() {
	if (!signalsPending) {
	    signalsPending = 1;
	    requestUpcall();
	    // Unblock the target in case it's blocked in an interruptable
	    // system call.
	    Scheduler::DisabledUnblockOnThisDispatcher(getID()
#ifdef AGGRESSIVE_SIGNAL
							, /*makeFirst*/ 1
#endif
						      );
	}
    }

    void requestMigration(DispatcherID targetDspID) {
	if (!migrationPending) {
	    migrationPending = 1;
	    migrateDspID = targetDspID;
	    requestUpcall();
	}
    }

    static const RDNum IO_DOMAIN  = 0;
    static const RDNum CPU_DOMAIN = 1;

    static const uval BLOCK_THRESHOLD = 3;
    static const uval YIELD_THRESHOLD = 3;

    /*
     * Record the fact that the thread blocked, and request a migration if
     * appropriate.
     */
    virtual void blocked() {
	if (KernelInfo::ControlFlagIsSet(KernelInfo::
					    DISABLE_IO_CPU_MIGRATION)) {
	    return;
	}
	yieldCount = 0;
	blockCount++;
	if ((blockCount == BLOCK_THRESHOLD) && (getRD() == CPU_DOMAIN)) {
	    // We've blocked several times without yielding, and we're in the
	    // CPU-intensive domain.  Request migration to the IO domain.
	    requestMigration(SysTypes::DSPID(IO_DOMAIN, getVP()));
	}
    }

    /*
     * Record the fact that the thread yielded, and request a migration if
     * appropriate.
     */
    virtual void yielded() {
	if (KernelInfo::ControlFlagIsSet(KernelInfo::
					    DISABLE_IO_CPU_MIGRATION)) {
	    return;
	}
	blockCount = 0;
	yieldCount++;
	if ((yieldCount == YIELD_THRESHOLD) && (getRD() == IO_DOMAIN)) {
	    // We've yielded several times without blocking, and we're in the
	    // IO-intensive domain.  Request migration to the CPU domain.
	    requestMigration(SysTypes::DSPID(CPU_DOMAIN, getVP()));
	}
    }

    virtual uval isCurrentlyMigratable() {
	return 0;
    }

    static const uval THREAD_CLONE_SIZE = 128*1024;

    void * operator new(size_t size, uval space) {
	tassertMsg(size == sizeof(ThreadClone), "Wrong size.\n");
	return (void *) space;
    }

    static void Destroy(ThreadClone *thread) {
	SysStatus rc;
	uval stack;
	stack = uval(thread) + sizeof(ThreadClone) - THREAD_CLONE_SIZE;
	rc = DREFGOBJ(ThePageAllocatorRef)->
			    deallocPages(stack, THREAD_CLONE_SIZE);
	passertMsg(_SUCCESS(rc), "ThreadClone deallocation failed.\n");
    }

    static ThreadClone *MakeThreadClone(uval stack) {
	uval objspace;
	ThreadClone *thread;
    #ifndef NDEBUG
	// Trash the stack:
	memset((void *) stack, 0xbf, THREAD_CLONE_SIZE);
    #endif
	objspace = (stack + THREAD_CLONE_SIZE - sizeof(ThreadClone));
	thread = new(objspace) ThreadClone;
	thread->init((void *) thread, (void *) stack, (void *) stack);
	thread->initClone();
	return thread;
    }

    static ThreadClone *Create() {
	SysStatus rc;
	uval stack;
	rc = DREFGOBJ(ThePageAllocatorRef)->
			    allocPages(stack, THREAD_CLONE_SIZE);
	passertMsg(_SUCCESS(rc), "ThreadClone allocation failed.\n");
	return MakeThreadClone(stack);
    }

    static ThreadClone *Create(MemoryMgrPrimitive *memory) {
	uval stack;
	memory->alloc(stack, ThreadClone::THREAD_CLONE_SIZE, PAGE_SIZE);
	return MakeThreadClone(stack);
    }
};

/*
 * LOCKING:  We use a RequestCountWithStop as a sort of readers/writers
 *           lock on the ProcessLinuxClient object.  The counter is
 *           stop'ed during updates to global data (especially updates
 *           to the cloneThread array).  The counter is enter'ed during
 *           requests that affect a particular clone but don't change
 *           global data.  A lock in the clone structure protects updates
 *           to the clone.  A clone lock should be acquired only after
 *           the requests counter has been enter'ed.  Clone locks are not
 *           needed if the requests counter is stop'ed.
 */

class AutoStop {
    RequestCountWithStop *reqsP;
public:
    DEFINE_NOOP_NEW(AutoStop);
    AutoStop(RequestCountWithStop *p) : reqsP(p) { (void) reqsP->stop(); }
    ~AutoStop() { reqsP->restart(); }
};

class AutoEnter {
    RequestCountWithStop *reqsP;
public:
    DEFINE_NOOP_NEW(AutoEnter);
    AutoEnter(RequestCountWithStop *p) : reqsP(p) { (void) reqsP->enter(); }
    ~AutoEnter() { reqsP->leave(); }
};

/*static*/ ProcessLinuxClient::ThreadClone *
ProcessLinuxClient::GetSelf()
{
    Thread *self = Scheduler::GetCurThreadPtr();
    tassertMsg(self->extensionID == GetThreadExtID(),
	       "Wrong thread extensionID.\n");
    return (ThreadClone *) self;
}

/*static*/ void
ProcessLinuxClient::SyscallDeactivate()
{
    Thread *self = Scheduler::GetCurThreadPtr();
    if (self->extensionID == GetThreadExtID()) {
	Scheduler::DeactivateSelf();
    }
}

/*static*/ void
ProcessLinuxClient::SyscallActivate()
{
    Thread *self = Scheduler::GetCurThreadPtr();
    if (self->extensionID == GetThreadExtID()) {
	Scheduler::ActivateSelf();
    }
}

/*static*/ void
ProcessLinuxClient::SyscallBlock()
{
    Thread *self = Scheduler::GetCurThreadPtr();
    if (self->extensionID == GetThreadExtID()) {
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    } else {
	Scheduler::Block();
    }
}

/*static*/ uval
ProcessLinuxClient::SyscallSignalsPending()
{
    Thread *self = Scheduler::GetCurThreadPtr();
    if (self->extensionID == GetThreadExtID()) {
	return ((ThreadClone *) self)->syscallSignalsPending();
    }
    return 0;
}

/*static*/ SysStatus
ProcessLinuxClient::AddThread(uval threadUval)
{
    Thread *const thread = (Thread *) threadUval;
    Scheduler::DisabledAddThread(thread);
    return 0;
}

/*virtual*/ SysStatus
ProcessLinuxClient::moveThread(Thread *baseThread)
{
    DispatcherID dspid;
    SysStatus rc, retRC;

    tassertMsg(baseThread->extensionID == GetThreadExtID(),
	       "Wrong thread extensionID.\n");
    ThreadClone *const thread = (ThreadClone *) baseThread;

    tassertMsg(!thread->isActive(), "Migrating active thread.\n");

    dspid = thread->migrateDspID;
    thread->migrateDspID = DispatcherID(-1);

    Dispatcher *dsp;
    rc = DREFGOBJ(TheDispatcherMgrRef)->getDispatcher(dspid, dsp);
    tassertMsg(_SUCCESS(rc), "getDispatcher failed.\n");
    if (dsp == NULL) {
	/*
	 * We create the CPU-intensive dispatcher lazily.  That's the
	 * only reason the target dispatcher might not exist yet.
	 */
	tassertMsg(SysTypes::RD_FROM_DSPID(dspid) == ThreadClone::CPU_DOMAIN,
		   "Non-existent dispatcher not in CPU_DOMAIN.\n");
	tassertMsg(SysTypes::VP_FROM_DSPID(dspid) == Scheduler::GetVP(),
		   "Non-existent dispatcher is on another VP.\n");

	/*
	 * Create the target dispatcher.  It may exist by now.
	 */
        rc = ProgExec::CreateDispatcher(dspid);
        passertMsg(_SUCCESS(rc) || (_SGENCD(rc) == EEXIST),
                   "CreateDispatcher failed: %lx\n",rc);
    }

    /*
     * Hold the lock for the entire migration, so that locked_deliverSignals()
     * will not see a bad threadID.
     */
    AutoEnter ae(&requests);
    AutoLock<BLock> al(&thread->lock);

    Scheduler::FreeThreadKey(thread);

    if (SysTypes::DSPID_VP_COMPARE(dspid, Scheduler::GetDspID()) == 0) {
	rc = SchedulerService::AddThread(dspid, thread);
	tassertMsg(_SUCCESS(rc), "SchedServ::AddThread failed.\n");
    } else {
	rc = MPMsgMgr::SendSyncUval(Scheduler::GetDisabledMsgMgr(),
				    dspid, AddThread, uval(thread), retRC);
	tassertMsg(_SUCCESS(rc) && _SUCCESS(retRC),
		   "SendSyncUval(AddThread) failed\n");
    }

    return 0;
}

/*static*/ void
ProcessLinuxClient::MoveThread(uval threadUval)
{
    ThreadClone *const thread = (ThreadClone *) threadUval;
    (void) DREFGOBJ(TheProcessLinuxRef)->moveThread(thread);
}

/*static*/ void
ProcessLinuxClient::SandboxUpcall(VolatileState *vsp, NonvolatileState *nvsp)
{
    ThreadClone *self = GetSelf();
    self->sandboxUpcall(vsp, nvsp);
}

/*static*/ void
ProcessLinuxClient::SandboxTrap(uval trapNumber, uval trapInfo,
				uval trapAuxInfo,
				VolatileState *vsp, NonvolatileState *nvsp)
{
    DisabledPushTrap(trapNumber, trapInfo, trapAuxInfo, vsp, nvsp);
}

/*static*/ Thread *
ProcessLinuxClient::AllocThread(MemoryMgrPrimitive *memory)
{
    return ThreadClone::ThreadClone::Create(memory);
}

/*static*/ SysStatus
ProcessLinuxClient::ClassInit(ObjectHandle oh)
{
    uval i;
    SysStatus rc;
    LinuxInfo linuxInfo;

    ProcessLinuxClient *ptr = new ProcessLinuxClient();
    // ptr->requests is initialized via constructor
    ptr->stub.setOH(oh);
    CObjRootSingleRep::Create(ptr, (RepRef)GOBJ(TheProcessLinuxRef));

    ThreadClone *self = GetSelf();
    rc = ptr->stub._getInfoLinuxPid(0, linuxInfo);
    if (_SUCCESS(rc)) {
	ptr->registerCallback();
	ptr->processPID = linuxInfo.pid;
	self->clonePID = linuxInfo.pid;
	self->parentPID = linuxInfo.ppid;
    } else {
	ptr->processPID = pid_t(-1);
	self->clonePID = pid_t(-1);
	self->parentPID = pid_t(-1);
    }

    ptr->did_exec = 0;
    ptr->exitHook = NULL;

    ptr->vpLimit = _SGETUVAL(DREFGOBJ(TheProcessRef)->ppCount());
    ptr->cloneVPCount = 1;
    ptr->cloneCount = 1;
    ptr->cloneCountMax = 1;
    ptr->nextCloneHint = 1;
    ptr->cloneZombieCount = 0;
    ptr->cloneThread = &ptr->cloneThread_0;
    ptr->cloneThread[0] = self;

    for (i = 1; i < _NSIG; i++) {
	ptr->sigAction[i].sa_handler = SIG_DFL;
    }
    ptr->sigIgnored.empty();
    ptr->sigIgnored.set(SIGCONT);
    ptr->sigIgnored.set(SIGCHLD);
    ptr->sigIgnored.set(SIGWINCH);

    return 0;
}

/* virtual */ SysStatus
ProcessLinuxClient::setVPLimit(uval limit)
{
    if ((limit < 1) || (limit > Scheduler::VPLimit)) {
	return _SERROR(2778, 0, EINVAL);
    }
    vpLimit = limit;
    return 0;
}

/* virtual */ SysStatus
ProcessLinuxClient::internalExec()
{
    AutoStop as(&requests);

    /*
     * see more on this kludge, which is in the posix spec, in
     * ProcessLinuxServer.
     */
    did_exec = 1;

    sigIgnored.empty();
    //Reset signal handlers to default
    for (uval i = 1; i < _NSIG; i++) {
	if (sigAction[i].sa_handler != SIG_IGN) {
	    sigAction[i].sa_handler = SIG_DFL;
	} else {
	    sigIgnored.set(i);
	}
	sigAction[i].sa_flags = 0;
	memset(&sigAction[i].sa_mask,0,sizeof(sigset_t));
    }
    sigIgnored.set(SIGCONT);
    sigIgnored.set(SIGCHLD);
    sigIgnored.set(SIGWINCH);

    ThreadClone *self = GetSelf();
    self->cloneStartPending = 0;
    self->cloneStartArg = NULL;
    self->signalsPending = 0;
    self->migrationPending = 0;
    self->migrateDspID = DispatcherID(-1);

    return 0;
}

SysStatus
ProcessLinuxClient::becomeInit()
{
    SysStatus rc;

    AutoStop as(&requests);

    rc = stub._becomeInit();
    _IF_FAILURE_RET(rc);

    registerCallback();

    processPID = 1;

    ThreadClone *self = GetSelf();
    self->clonePID = 1;
    self->parentPID = 1;

    return 0;
}

SysStatus
ProcessLinuxClient::becomeLinuxProcess()
{
    SysStatus rc;
    LinuxInfo linuxInfo;

    AutoStop as(&requests);

    rc = stub._becomeLinuxProcess();
    _IF_FAILURE_RET(rc);

    registerCallback();

    rc = stub._getInfoLinuxPid(0, linuxInfo);
    tassertMsg(_SUCCESS(rc), "getInfoLinuxPid() failed.\n");
    processPID = linuxInfo.pid;

    ThreadClone *self = GetSelf();
    self->clonePID = linuxInfo.pid;
    self->parentPID = linuxInfo.ppid;

    return 0;
}

/*virtual*/ SysStatus
ProcessLinuxClient::destroyOSData(uval data)
{
    if (data != 0) {
	delete (LinuxCredsHolder *) data;
    }
    return 0;
}

SysStatus
ProcessLinuxClient::preFork(ObjectHandle childProcessOH,
			    ObjectHandle& childProcessLinuxOH,
			    pid_t& childPID)
{
    AutoStop as(&requests);
    SysStatus rc;
    ObjectHandle oh;
    StubProcessServer childProcStub(StubObj::UNINITIALIZED);
    childProcStub.setOH(childProcessOH);
    rc = childProcStub._giveAccess(oh, stub.getPid(),
				   MetaProcessServer::destroy, MetaObj::none);
    _IF_FAILURE_RET(rc);

    //N.B. this call sets childProcessLinuxOH and childPID
    return stub._createChild(oh, childProcessLinuxOH, childPID);
}

SysStatus
ProcessLinuxClient::postFork(ObjectHandle processLinuxOH,
			     pid_t pid, Thread *forkThread)
{
    tassertMsg(forkThread->extensionID == GetThreadExtID(),
	       "Wrong thread extensionID.\n");
    ThreadClone *const mainThread = (ThreadClone *) forkThread;

    AutoStop as(&requests);
    tassertMsg(processLinuxOH.valid(),
	       "fork did not provide ProcessLinux ObjectHandle\n");
    stub.setOH(processLinuxOH);

    mainThread->parentPID = mainThread->clonePID;
    mainThread->parentSignal = 0;
    mainThread->clonePID = pid;

    // Free the old array if it was dynamically allocated.  (The initial
    // 1-element array is embedded in the structure.)
    if (cloneCountMax > 1) {
	freeGlobal(cloneThread, cloneCountMax * sizeof(ThreadClone*));
    }

    processPID = pid;
    did_exec = 0;
    exitHook = NULL;

    cloneVPCount = 1;
    cloneCount = 1;
    cloneCountMax = 1;
    nextCloneHint = 1;
    cloneZombieCount = 0;
    cloneThread = &cloneThread_0;
    cloneThread[0] = mainThread;

    // POSIX semantics are that children do not inherit alarms
    delete currAlarm;
    currAlarm = NULL;

    registerCallback();
    return 0;
}

SysStatus
ProcessLinuxClient::registerExitHook(ExitHook hook)
{
    tassertMsg(exitHook == NULL, "An exit hook is already registered.\n");
    exitHook = hook;
    return 0;
}

SysStatus
ProcessLinuxClient::createExecProcess(ObjectHandle execProcessOH,
				      ObjectHandle& execProcessLinuxOH)

{
    AutoStop as(&requests);
    //N.B. this call sets execProcessLinuxOH
    SysStatus rc;
    ObjectHandle oh;
    rc = Obj::GiveAccessByClient(execProcessOH, oh, stub.getPid());
    _IF_FAILURE_RET(rc);
    return stub._createExecProcess(oh, execProcessLinuxOH);
}

SysStatus
ProcessLinuxClient::waitpid(pid_t& waitfor, sval& status, uval options)
{
    uval id;

    if (options&__WCLONE) {
	if ((int(options) != int(WNOHANG|__WCLONE)) || (waitfor != -1)) {
	    /*
	     * We only support the combination of options that linuxthreads
	     * actually uses.
	     */
	    return _SERROR(2324, 0, EINVAL);
	}
	AutoStop as(&requests);
	if (cloneZombieCount > 0) {
	    for (id = 0; id < cloneCountMax; id++) {
		if ((cloneThread[id] != NULL) &&
			(cloneThread[id]->state == ThreadClone::ZOMBIE)) break;
	    }
	    tassertMsg(id < cloneCountMax, "No cloneZombies found.\n");
	    status = cloneThread[id]->exitStatus;
	    waitfor = cloneThread[id]->clonePID;
	    ThreadClone::Destroy(cloneThread[id]);
	    cloneThread[id] = NULL;
	    cloneCount--;
	    cloneZombieCount--;
	} else {
	    waitfor = 0;
	}
	return 0;
    }

    // no need to lock
    return stub._waitpid(waitfor, status);
}

/*static*/ SysStatus
ProcessLinuxClient::WaitPIDInternal(pid_t& pid, sval& status, uval options)
{
    SysStatus rc;
    BlockedThreadQueues::Element qe;
    pid_t pid_in = pid;
    DREFGOBJ(TheBlockedThreadQueuesRef)->
	    addCurThreadToQueue(&qe, GOBJ(TheProcessLinuxRef));
    while (1) {
	rc = DREFGOBJ(TheProcessLinuxRef)->waitpid(pid, status, options);
	if (_FAILURE(rc) || (pid != 0)) break;
	pid = pid_in;	// restore for next call
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
    DREFGOBJ(TheBlockedThreadQueuesRef)->
	    removeCurThreadFromQueue(&qe, GOBJ(TheProcessLinuxRef));
    return rc;
}

void
ProcessLinuxClient::getHandler(__sighandler_t& handler, uval& flags,
			       sval& sig, SigSet& oldmask, uval& more)
{
    (void) requests.enter(); // can't use AutoEnter because we may not return

    ThreadClone *self = GetSelf();

    self->lock.acquire(); // can't use AutoLock because we may not return

    for (;;) {
	sig = self->sigPending.getAndClearNextNonblocked(self->sigBlocked);

	if (sig == 0) break;

	handler = sigAction[sig].sa_handler;
	flags = sigAction[sig].sa_flags;

	if (handler == SIG_IGN) {
	    passertMsg(0, "Ignored signal wasn't discarded soon enough.\n");
	    continue;
	}

	if (handler == SIG_DFL) {
	    if (processPID == 1) continue;	// copy linux kludge

	    switch (sig) {
	    case SIGSTOP:
	    case SIGKILL:
		// These signals should have been handled in the server.
		passertMsg(0, "Unexpected STOP or KILL signal.\n");
		continue;

	    case SIGTSTP:
	    case SIGTTIN:
	    case SIGTTOU:
	    {
		static int sigCount = 0;
		++sigCount;
		if (sigCount<5)
		    // These signals stop the process by default.
		    err_printf("SIGTSTP, SIGTTIN and SIGTTOU NYI.  Ignoring.\n");
		continue;
	    }
	    case SIGCONT:
	    case SIGCHLD:
	    case SIGWINCH:
		// These signals are ignored by default.
		passertMsg(0, "Ignored signal wasn't discarded soon enough.\n");
		continue;

	    case SIGQUIT:
	    case SIGILL:
	    case SIGABRT:
	    case SIGFPE:
	    case SIGSEGV:
	    case SIGBUS:
	    case SIGSYS:
	    case SIGTRAP:
	    case SIGXCPU:
	    case SIGXFSZ:
	    // (duplicate of SIGABRT) case SIGIOT:
		// These signals dump core and kill the process by default.
		err_printf("Core dumps NYI.  Exiting without dump.\n");
		/* fall through */

	    default:
		// All other signals just kill the process by default.
		self->lock.release();
		requests.leave();
		exit(__W_EXITCODE(1, sig));
		// NOTREACHED
	    }
	}

	if (self->sigSuspended) {
	    self->sigBlocked = self->sigSuspendSaveBlocked;
	    self->sigSuspended = 0;
	}
	oldmask = self->sigBlocked;
	self->sigBlocked.orIn(&sigAction[sig].sa_mask);
	if (!(sigAction[sig].sa_flags & SA_NODEFER)) {
	    self->sigBlocked.set(sig);
	}

	more = self->sigPending.anyNonblocked(self->sigBlocked);

	if (sigAction[sig].sa_flags & SA_ONESHOT) {
	    // reset to default
	    sigAction[sig].sa_handler = SIG_DFL;
	    if ((sig == SIGCONT) || (sig == SIGCHLD) || (sig == SIGWINCH)) {
		sigIgnored.set(sig);
	    }
	}

	break;
    }

    self->lock.release();
    requests.leave();
}

/*virtual*/ SysStatus
ProcessLinuxClient::handlerFinished(SigSet& oldmask)
{
    ThreadClone *self = GetSelf();
    AutoEnter ae(&requests);
    AutoLock<BLock> al(&self->lock);
    self->sigBlocked = oldmask;
    return 0;
}

void
ProcessLinuxClient::forceSignalSelf(sval sig)
{
    ThreadClone *self = GetSelf();
    AutoEnter ae(&requests);
    AutoLock<BLock> al(&self->lock);

    // Make sure the signal is not blocked or ignored.
    self->sigBlocked.clear(sig);
    if (sigIgnored.isSet(sig)) {
	sigIgnored.clear(sig);
	sigAction[sig].sa_handler = SIG_DFL;
    }

    // Now make the signal pending.
    self->sigPending.set(sig);
}

/*virtual*/ SysStatus
ProcessLinuxClient::pushSignals(VolatileState *vsp, NonvolatileState *nvsp)
{
    __sighandler_t handler;
    uval flags;
    sval sig;
    SigSet oldmask;
    uval more;

    do {
	getHandler(handler, flags, sig, oldmask, more);
	if (sig == 0) break;
	SignalUtils::PushSignal(vsp, nvsp, handler, flags, sig, oldmask.bits);
    } while (more);

    return 0;
}

// FIXME:  Get rid of this declaration when the FIXME below goes away.
extern "C" void GDBStub_KernelTrap(uval trapNumber, uval trapInfo,
				   uval trapAuxInfo,
				   VolatileState *vsp,
				   NonvolatileState *nvsp);

/*virtual*/ SysStatus
ProcessLinuxClient::pushTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
			     VolatileState *vsp, NonvolatileState *nvsp)
{
    sval sig;
    SignalUtils::ConvertTrapToSignal(trapNumber, trapInfo, trapAuxInfo,
				     vsp, nvsp, sig);

    // FIXME:	We don't currently support ptrace debugging, and until we do
    //		it's at least somewhat useful to use gdb-stub debugging even
    //		in sandbox mode.  So we invoke the stub if there's no handler
    //		installed for the signal.  Note that we're looking at the
    //		sigAction array with no locking whatsoever, which is bogus but
    //		probably won't bite us.  It turns out that the kernel interface
    //		to the stub is the appropriate one to use here.
    if ((sig != 0) && (sigAction[sig].sa_handler == SIG_DFL)) {
	GDBStub_KernelTrap(trapNumber, trapInfo, trapAuxInfo, vsp, nvsp);
	return 0;
    }

    if (sig != 0) {
	forceSignalSelf(sig);
	(void) pushSignals(vsp, nvsp);
    }

    return 0;
}

/*static*/ void
ProcessLinuxClient::DisabledPushSignals(VolatileState *vsp,
					NonvolatileState *nvsp)
{
    Scheduler::Enable();
    Scheduler::ActivateSelf();

    (void) DREFGOBJ(TheProcessLinuxRef)->pushSignals(vsp, nvsp);

    Scheduler::DeactivateSelf();
    Scheduler::Disable();
}

/*static*/ void
ProcessLinuxClient::DisabledPushTrap(uval trapNumber, uval trapInfo,
				     uval trapAuxInfo,
				     VolatileState *vsp,
				     NonvolatileState *nvsp)
{
    Scheduler::Enable();
    Scheduler::ActivateSelf();

    (void) DREFGOBJ(TheProcessLinuxRef)->pushTrap(trapNumber, trapInfo,
						  trapAuxInfo, vsp, nvsp);

    Scheduler::DeactivateSelf();
    Scheduler::Disable();
}

void
ProcessLinuxClient::locked_deliverSignals(ThreadClone *thread)
{
    DispatcherID targetDspID;

    _ASSERT_HELD(thread->lock);

    thread->sigPending.clearNonblockedAndIgnored(thread->sigBlocked,
						 sigIgnored);

    if (!thread->sigPending.anyNonblocked(thread->sigBlocked)) {
	// No non-blocked signals are pending.  There's nothing to be done.
	return;
    }

    if (thread->state != ThreadClone::ALIVE) {
	// Don't deliver signals to a clone that has already terminated.
	return;
    }

    /*
     * The target thread won't exit or migrate while we hold the lock, so we
     * can look at its threadID safely.
     */
    targetDspID = thread->getDspID();
    if (targetDspID == Scheduler::GetDspID()) {
	// Thread is on this dispatcher.  Disable and interrupt it.
	Scheduler::Disable();
	thread->requestSignals();
	Scheduler::Enable();
    } else {
	// Thread is on another dispatcher.  Forward the request.
	MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(), targetDspID,
				DeliverSignals, uval(thread->clonePID));
    }
}

SysStatus
ProcessLinuxClient::deliverSignals(pid_t pid)
{
    AutoEnter ae(&requests);
    uval id;
    ThreadClone *thread;

    id = (pid >> CLONE_ID_SHIFT) & CLONE_ID_MASK;
    if (id < cloneCountMax) {
	thread = cloneThread[id];
	if (thread != NULL) {
	    AutoLock<BLock> al(&thread->lock);
	    locked_deliverSignals(thread);
	    return 0;
	}
    }
    return _SERROR(2339, 0, ESRCH);
}

/*static*/ SysStatus
ProcessLinuxClient::DeliverSignals(uval pidUval)
{
    pid_t const pid = (pid_t) pidUval;

    return DREFGOBJ(TheProcessLinuxRef)->deliverSignals(pid);
}

__async SysStatus
ProcessLinuxClient::_acceptSignals(SigSet set)
{
    ThreadClone *thread;

    AutoEnter ae(&requests);

    if (set.isSet(SIGCHLD)) {
	DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll(getRef());
    }

    thread = cloneThread[0];
    AutoLock<BLock> al(&thread->lock);
    thread->sigPending.orInSigSet(&set);
    locked_deliverSignals(thread);

    return 0;
}

SysStatus
ProcessLinuxClient::registerCallback()
{
    // no lock - called when nothing else is happening
    ObjectHandle callbackOH;
    SysStatus rc;
    rc = giveAccessByServer(callbackOH, stub.getPid());
    tassert(_SUCCESS(rc), err_printf("do cleanup code\n"));

    rc = stub._registerCallback(callbackOH);
    if (_FAILURE(rc)) {
	if (_SGENCD(rc) == ESRCH) return 0;	// not a linux process
	tassert(_SUCCESS(rc),
		err_printf("error register callback rc=(%ld,%ld,%ld)\n",
			   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc)));
    }
    return rc;
}

/*virtual*/ SysStatus
ProcessLinuxClient::terminateClone(Thread *baseThread)
{
    tassertMsg(baseThread->extensionID == GetThreadExtID(),
	       "Wrong thread extensionID.\n");
    ThreadClone *const thread = (ThreadClone *) baseThread;

    (void) requests.stop();
    cloneZombieCount++;
    thread->state = ThreadClone::ZOMBIE;
    pid_t pid = thread->parentPID;
    int sig = thread->parentSignal;
    requests.restart();

    if ((sig < 1) || (sig > _NSIG)) {
	return 0;
    }

    return signalClone(pid, sig);
}

/*static*/ void
ProcessLinuxClient::TerminateClone(uval threadUval)
{
    ThreadClone *const thread = (ThreadClone *) threadUval;
    (void) DREFGOBJ(TheProcessLinuxRef)->terminateClone(thread);
}

//N.B. status is encoded version - see wait.h
//     currenty coding is status<<8 | signal | x'80' if core dump
SysStatus
ProcessLinuxClient::exit(sval status)
{
    SysStatus rc;

    ThreadClone *self = GetSelf();
    if (self->clonePID != processPID) {
	/*
	 * We can't change our own state to ZOMBIE, because if we did, someone
	 * could harvest us and deallocate our space before we've actually
	 * stopped running.  But we can't leave ourselves ALIVE either because
	 * someone may attempt to deliver a signal to us.  So we change our
	 * state to an intermediate EXITING, holding the lock in order to
	 * synchronize with locked_deliverSignals().  Then we disable, we
	 * create an auxiliary thread to complete the process of turning
	 * ourself into a zombie, and we exit.  The disabling ensures that the
	 * auxiliary thread won't run until we're really and truly terminated.
	 */
	(void) requests.enter();
	self->lock.acquire();
	self->state = ThreadClone::EXITING;
	self->exitStatus = status;
	self->lock.release();
	requests.leave();
	Scheduler::DeactivateSelf();
	Scheduler::Disable();
	Scheduler::DisabledScheduleFunction(TerminateClone, uval(self));
	Scheduler::DisabledExit();
	// NOTREACHED
    }

    // no need to lock
    rc = stub._exit(status);
    tassertMsg(_SUCCESS(rc), "ProcessLinux server _exit() failed.\n");

    if (exitHook != NULL) {
	exitHook();
    }

    DREFGOBJ(TheProcessRef)->kill();
    // NOTREACHED
    return 0;	// keep the compiler happy
}

SysStatus
ProcessLinuxClient::getpid(pid_t& pid)
{
    // no need to lock
    ThreadClone *self = GetSelf();
    pid = self->clonePID;
    return 0;
}


SysStatus
ProcessLinuxClient::getppid(pid_t& pid)
{
    // no need to lock
    ThreadClone *self = GetSelf();
    if (self->clonePID == processPID) {
	// main clone parent can change because of parent death
	// so must always ask server
	SysStatus rc;
	LinuxInfo linuxInfo;
	rc = stub._getInfoLinuxPid(0, linuxInfo);
	pid = linuxInfo.ppid;
	return rc;
    }
    pid = self->parentPID;
    return 0;
}

SysStatus
ProcessLinuxClient::getpgid(pid_t& pid)
{
    // no need to lock
    SysStatus rc;
    LinuxInfo linuxInfo;
    rc = stub._getInfoLinuxPid(pid, linuxInfo);
    pid = linuxInfo.pgrp;
    return rc;
}

SysStatus
ProcessLinuxClient::signalClone(pid_t pid, sval sig)
{
    uval id;
    ThreadClone *thread;

    tassertMsg((uval(pid) & ~(CLONE_ID_MASK<<CLONE_ID_SHIFT)) ==
							uval(processPID),
	       "target is not a clone in this process.\n");

    AutoEnter ae(&requests);
    id = (pid >> CLONE_ID_SHIFT) & CLONE_ID_MASK;
    if (id < cloneCountMax) {
	thread = cloneThread[id];
	if (thread != NULL) {
	    AutoLock<BLock> al(&thread->lock);
	    thread->sigPending.set(sig);
	    locked_deliverSignals(thread);
	    return 0;
	}
    }

    // Clone not found.
    return _SERROR(2335, 0, ESRCH);
}

SysStatus
ProcessLinuxClient::kill(pid_t pid, sval sig)
{
    SysStatus rc;

    if ((sig < 0) || (sig > _NSIG)) {
	return _SERROR(2338, 0, EINVAL);
    }

    if (((uval(pid) & ~(CLONE_ID_MASK<<CLONE_ID_SHIFT)) != uval(processPID)) ||
				(sig == SIGKILL) || (sig == SIGSTOP)) {
	// Signal is for another process, or is one that can't be caught
	// or ignored.  Send the request to the server.
	// no need to lock
	return stub._kill(pid, sig);
    }

    rc = signalClone(pid, sig);

#ifdef AGGRESSIVE_SIGNAL
    Scheduler::Yield();	// Give the target a chance to run.
#endif

    return rc;
}

SysStatus
ProcessLinuxClient::setsid()
{
    // no need to lock
    return stub._setsid();
}

SysStatus
ProcessLinuxClient::setpgid(pid_t pid, pid_t pgid)
{
    // no need to lock
    return stub._setpgid(pid, pgid, did_exec);
}

SysStatus
ProcessLinuxClient::set_uids_gids(
    set_uids_gids_type type,
    uid_t uid, uid_t euid, uid_t suid, uid_t fsuid,
    gid_t gid, gid_t egid, gid_t sgid, gid_t fsgid)
{
    // no need to lock
    return stub._set_uids_gids(type, uid, euid, suid, fsuid,
			       gid, egid, sgid, fsgid);
}

/* virtual */
SysStatus
ProcessLinuxClient::insecure_setuidgid(uid_t euid, gid_t egid)
{
    return stub._insecure_setuidgid(euid, egid);
}

struct ProcessLinuxClient::CloneStartArg {
    DEFINE_GLOBALPADDED_NEW(ProcessLinuxClient::CloneStartArg);
    uval stackPtr;
    ThreadClone *thread;
    VPNum targetVP;
    VolatileState vs;
    NonvolatileState nvs;
};

/*static*/ void
ProcessLinuxClient::LaunchClone(uval cloneArgUval)
{
    CloneStartArg *const cloneArg = (CloneStartArg *) cloneArgUval;
    VolatileState vs;
    NonvolatileState nvs;

    vs = cloneArg->vs;
    nvs = cloneArg->nvs;

    vs.setStackPtr(cloneArg->stackPtr);
    vs.setReturnCode(0);	// clone returns 0 in child

    delete cloneArg;

    Scheduler::LaunchSandbox(&vs, &nvs);
    // NOT REACHED
}

/*static*/ SysStatus
ProcessLinuxClient::CloneStart(uval cloneArgUval)
{
    CloneStartArg *const cloneArg = (CloneStartArg *) cloneArgUval;
    SysStatus rc;

    rc = Scheduler::ScheduleFunction(LaunchClone, cloneArgUval,
				     cloneArg->thread);
    tassertMsg(_SUCCESS(rc), "ScheduleFunction failed.\n");
    return 0;
}

/*static*/ void
ProcessLinuxClient::DisabledCloneStart(VolatileState *vsp,
				       NonvolatileState *nvsp,
				       CloneStartArg *cloneArg)
{
    SysStatus rc;

    Scheduler::Enable();
    Scheduler::ActivateSelf();

    cloneArg->vs = *vsp;
    cloneArg->nvs = *nvsp;

    if (Scheduler::GetVP() == cloneArg->targetVP) {
	(void) CloneStart(uval(cloneArg));
    } else {
	rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
				     SysTypes::DSPID(0, cloneArg->targetVP),
				     CloneStart, uval(cloneArg));
	tassertMsg(_SUCCESS(rc), "SendAsyncUval() failed.\n");
    }

    Scheduler::DeactivateSelf();
    Scheduler::Disable();
}

void
ProcessLinuxClient::cloneCommon(ThreadClone *&thread)
{
    uval id;
    ThreadClone **newCloneThread;

    thread = ThreadClone::Create();

    /*
     * Expand the cloneThread array if necessary.
     */
    if (cloneCount >= cloneCountMax) {
	passertMsg(cloneCountMax <= CLONE_ID_MASK, "Too many clones.\n");
	newCloneThread = (ThreadClone **) allocGlobal((2 * cloneCountMax) *
							sizeof(ThreadClone *));
	// copy existing array elements
	for (id = 0; id < cloneCountMax; id++) {
	    newCloneThread[id] = cloneThread[id];
	}
	// initialize new elements
	for (id = cloneCountMax; id < (2 * cloneCountMax); id++) {
	    newCloneThread[id] = NULL;
	}

	// Free the old array if it was dynamically allocated.  (The initial
	// 1-element array is embedded in the structure.)
	if (cloneCountMax > 1) {
	    freeGlobal(cloneThread, cloneCountMax * sizeof(ThreadClone*));
	}
	cloneThread = newCloneThread;
	cloneCountMax = (2 * cloneCountMax);
    }

    /*
     * Allocate a clone id for this thread.
     */
    do {
	id = nextCloneHint;
	nextCloneHint = (nextCloneHint + 1) % cloneCountMax;
    } while (cloneThread[id] != NULL);
    cloneThread[id] = thread;
    cloneCount++;

    thread->clonePID = processPID | (id << CLONE_ID_SHIFT);
}

SysStatus
ProcessLinuxClient::clone(pid_t& pid, int flags, void *child_stack,
			  void *parent_tid, void *tls, void *child_tid)
{
    ThreadClone *thread;
    CloneStartArg *cloneArg;
    uval id;
    VPNum targetVP;
    SysStatus rc;

    if ((flags & ~CSIGNAL) !=
	    (CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND)) {
	// This implementation only supports the cloning of everything.
	return _SERROR(2318, 0, ENOSYS);
    }

    AutoStop as(&requests);

    cloneCommon(thread);

    ThreadClone *self = GetSelf();

    thread->parentSignal = flags & CSIGNAL;
    thread->parentPID = self->clonePID;
    thread->sigBlocked = self->sigBlocked;

    /*
     * Create new VPs for new clones, up to vpLimit (by default, the number of
     * physical processors on the machine).  There's a bit of a kludge here.
     * Clone id 0 represents the main program and deserves a processor.  We
     * shouldn't count on it, but clone id 1 is always linuxthreads' manager
     * thread, which doesn't need its own processor.  We subtract 1 from
     * cloneCount and from id when making VP calculations, resulting in the
     * main program and the manager thread sharing VP 0, and subsequent
     * pthreads getting new VPs.
     */
    if (((cloneCount - 1) > cloneVPCount) && (cloneVPCount < vpLimit)) {
	rc = ProgExec::CreateVP(cloneVPCount);
	passertMsg(_SUCCESS(rc), "CreateVP() failed.\n");
	cloneVPCount++;
    }

    /*
     * Distribute new clones to different VPs.  See comment above.
     */
    id = (thread->clonePID >> CLONE_ID_SHIFT) & CLONE_ID_MASK;
    targetVP = (id - 1) % cloneVPCount;

    cloneArg = new CloneStartArg;
    cloneArg->stackPtr = uval(child_stack);
    cloneArg->thread = thread;
    cloneArg->targetVP = targetVP;

    Scheduler::Disable();
    self->requestCloneStart(cloneArg);
    Scheduler::Enable();

    pid = thread->clonePID;
    return 0;
}

SysStatus
ProcessLinuxClient::cloneNative(pid_t& pid,
				Scheduler::ThreadFunction fct, uval fctArg)
{
    ThreadClone *thread;
    SysStatus rc;

    AutoStop as(&requests);

    cloneCommon(thread);

    thread->parentSignal = 0;
    thread->parentPID = processPID;
    thread->sigBlocked.empty();

    pid = thread->clonePID;

    rc = Scheduler::ScheduleFunction(fct, fctArg, thread);
    tassertMsg(_SUCCESS(rc), "ScheduleFunction failed.\n");

    return 0;
}

SysStatus
ProcessLinuxClient::getResourceUsage(
    pid_t about, struct BaseProcess::ResourceUsage& resourceUsage)
{
    return stub._getResourceUsage(about, resourceUsage);
}

/* static */ void
ProcessLinuxClient::TimerEventAlarm::Worker(uval target)
{
    pid_t pid = pid_t(target);

    DREFGOBJ(TheProcessLinuxRef)->kill(pid, SIGALRM);
}

void
ProcessLinuxClient::TimerEventAlarm::handleEvent()
{
    Scheduler::DisabledScheduleFunction(
        ProcessLinuxClient::TimerEventAlarm::Worker, uval(victim));
}

uval
ProcessLinuxClient::TimerEventAlarm::timeRemaining()
{
    SysTime now = TimerEvent::scheduleEvent(0, TimerEvent::queryNow);

    if (expiration < now) return 0;

    uval multiplier = Scheduler::TicksPerSecond();
    uval remain = ((expiration - now) / multiplier) + 1;

    return remain;
}

SysTime
ProcessLinuxClient::TimerEventAlarm::cancelAlarm()
{
    this->expiration = TimerEvent::scheduleEvent(0, TimerEvent::reset);
    return this->expiration;
}

SysTime
ProcessLinuxClient::TimerEventAlarm::scheduleAlarm(uval seconds, pid_t caller)
{
    uval multiplier = Scheduler::TicksPerSecond();
    uval delay = multiplier * seconds;

    this->victim = caller;
    this->inception = TimerEvent::scheduleEvent(delay, TimerEvent::relative);
    this->expiration = this->inception + delay;

    return this->inception;
}

SysStatus
ProcessLinuxClient::alarm(pid_t about, uval seconds)
{
    SysStatus rc = 0;
    SysTime now;

    if (currAlarm) {
        rc = currAlarm->timeRemaining();
	currAlarm->cancelAlarm();
    }

    if (seconds > 0) {
        if (currAlarm == NULL) {
            currAlarm = new TimerEventAlarm;
	}
        now = currAlarm->scheduleAlarm(seconds, GetSelf()->clonePID);
    }

    return rc;
}

SysStatus
ProcessLinuxClient::getInfoLinuxPid(
    pid_t about, struct ProcessLinux::LinuxInfo& linuxInfo)
{
    // no need to lock
    return stub._getInfoLinuxPid(about, linuxInfo);
}

SysStatus
ProcessLinuxClient::getInfoNativePid(
    ProcessID k42_pid, struct ProcessLinux::LinuxInfo& linuxInfo)
{
    // no need to lock
    return stub._getInfoNativePid(k42_pid, linuxInfo);
}

/*
 *FIXME MAA
 * This implementation goes to the server every time.
 * Should be replaced with caching the info when it is immutable
 * in the OSData field of the ProcessWrapper.
 * But to do this, we must deal with a process desctruction call
 * back so we can free the cached structure when its use count goes
 * to zero.
 * I think we must also deal with late references during destruction
 * by freeing on token circulation
 */
SysStatus
ProcessLinuxClient::getCredsPointerNativePid(
    ProcessID k42_pid, ProcessLinux::creds_t*& linuxCredsPtr)
{
    // no need to lock
    SysStatus rc;
    LinuxInfo linuxInfo;
    LinuxCredsHolder* linuxCredsHolder;

    rc = stub._getInfoNativePid(k42_pid, linuxInfo);
    if (_FAILURE(rc)) {
	//FIXME
	// for now assert root creds for any non linux task
	linuxCredsHolder = new LinuxCredsHolder;
	if (!linuxCredsHolder) return _SERROR(2560, 0, ENOMEM);
	linuxCredsHolder->linuxCreds.rootPrivs();
	linuxCredsPtr = &(linuxCredsHolder->linuxCreds);
	rc = 0;
	return rc;
    }
    linuxCredsHolder = new LinuxCredsHolder;
    if (!linuxCredsHolder) return _SERROR(2561, 0, ENOMEM);
    linuxCredsHolder->linuxCreds = linuxInfo.creds;
    linuxCredsPtr = &(linuxCredsHolder->linuxCreds);
    return 0;
}

SysStatus
ProcessLinuxClient::releaseCredsPointer(
    ProcessLinux::creds_t* linuxCredsPtr)
{
    // no need to lock
    LinuxCredsHolder *linuxCredsHolder;
    linuxCredsHolder = (LinuxCredsHolder*) linuxCredsPtr;
    delete linuxCredsHolder;
    return 0;
}

SysStatus
ProcessLinuxClient::setTimeOfDay(uval sec, uval usec)
{
    // no need to lock
    return stub._setTimeOfDay(sec, usec);
}

/*
 * job control methods used by privileged tty servers
 */

/* virtual */ SysStatus
ProcessLinuxClient::addTTY(uval ttyNum, uval ttyData)
{
    // no need to lock
    return stub._addTTY(ttyNum, ttyData);
}

/* virtual */ SysStatus
ProcessLinuxClient::removeTTY(uval ttyToken)
{
    // no need to lock
    return stub._removeTTY(ttyToken);
}

/* virtual */ SysStatus
ProcessLinuxClient::setCtrlTTY(uval ttyNum, ProcessID processID)
{
    // no need to lock
    return stub._setCtrlTTY(ttyNum, processID);
}

/* virtual */ SysStatus
ProcessLinuxClient::sigaction(int sig, const struct sigaction* act,
			      struct sigaction* oldact, uval sigsetsize)

{
    AutoStop as(&requests);

    if ((sig < 1) || (sig > _NSIG)) {
	return _SERROR(1827, 0, EINVAL);
    }

    if (oldact != NULL) {
	*oldact = sigAction[sig];
    }

    if (act != NULL) {
	sigAction[sig] = *act;

	/*
	 * POSIX requires that we proactively discard any pending signals
	 * whose action is now SIG_IGN, or whose action is now SIG_DFL and
	 * whose default action is "ignore".  Blecch.
	 */
	if ((act->sa_handler == SIG_IGN) ||
	    ((act->sa_handler == SIG_DFL) &&
	     ((sig == SIGCONT) || (sig == SIGCHLD) || (sig == SIGWINCH))))
	{
	    for (uval id = 0; id < cloneCountMax; id++) {
		if (cloneThread[id] != NULL) {
		    cloneThread[id]->sigPending.clear(sig);
		}
	    }
	    sigIgnored.set(sig);
	} else {
	    sigIgnored.clear(sig);
	}
    }

    return 0;
}

/* virtual */ SysStatus
ProcessLinuxClient::sigprocmask(int how, const sigset_t* set, sigset_t* oldset,
				uval sigsetsize)
{
    SysStatus rc;

    ThreadClone *self = GetSelf();

    AutoEnter ae(&requests);
    AutoLock<BLock> al(&self->lock);

    if (oldset != NULL) {
	self->sigBlocked.copyOut(oldset, sigsetsize);
    }

    rc = 0;
    if (set != NULL) {
	switch (how) {
	case SIG_BLOCK:
	    self->sigBlocked.orIn(set);
	    break;
	case SIG_UNBLOCK:
	    self->sigBlocked.nandIn(set);
	    break;
	case SIG_SETMASK:
	    self->sigBlocked.copyIn(set);
	    break;
	default:
	    rc = _SERROR(2321, 0, EINVAL);
	    break;
	}
	self->sigBlocked.clear(SIGKILL);
	self->sigBlocked.clear(SIGSTOP);

	locked_deliverSignals(self);
    }

    return rc;
}

/* virtual */ SysStatus
ProcessLinuxClient::sigsuspend(const sigset_t* mask)
{
    ThreadClone *self = GetSelf();

    AutoEnter ae(&requests);
    AutoLock<BLock> al(&self->lock);

    self->sigSuspended = 1;
    self->sigSuspendSaveBlocked = self->sigBlocked;
    self->sigBlocked.copyIn(mask);

    locked_deliverSignals(self);
    return 0;
}

/* virtual */ SysStatus
ProcessLinuxClient::sigreturn(SignalUtils::SignalReturnType srType,
			      uval stkPtr)
{
    SigSet oldmask;
    VolatileState vs;
    NonvolatileState nvs;

    SignalUtils::SignalReturn(srType, stkPtr, &vs, &nvs, oldmask.bits);
    handlerFinished(oldmask);
    Scheduler::LaunchSandbox(&vs, &nvs);
    // NOTREACHED
    return 0;
}
