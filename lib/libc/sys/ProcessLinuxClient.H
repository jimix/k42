#ifndef __PROCESS_LINUX_CLIENT_H_
#define __PROCESS_LINUX_CLIENT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessLinuxClient.H,v 1.58 2005/08/11 20:20:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides server for process related Linux operations
 *                     such as the association of a Linux PID with a K42 PID
 * **************************************************************************/

#ifndef EXPORT_PROCESSLINUXCLIENT
#define EXPORT_PROCESSLINUXCLIENT
#include <sys/ProcessLinux.H>
#endif /* #ifndef EXPORT_PROCESSLINUXCLIENT */
#include <stub/StubProcessLinuxServer.H>
#include <meta/MetaProcessLinuxClient.H>
#include <scheduler/Scheduler.H>
#include <sys/ppccore.H>
#include <alloc/MemoryMgrPrimitive.H>
#include <sync/RequestCount.H>

class ProcessLinuxClient : public ProcessLinux __xbase(Obj) {
protected:
    struct ThreadClone;

    RequestCountWithStop    requests;
    StubProcessLinuxServer  stub;
    pid_t                   processPID;
    uval                    did_exec;
    ExitHook                exitHook;
    uval                    vpLimit;
    uval                    cloneVPCount;
    uval                    cloneCount;
    uval                    cloneCountMax;
    uval                    nextCloneHint;
    uval                    cloneZombieCount;
    ThreadClone           **cloneThread;
    ThreadClone            *cloneThread_0;	// 1-clone process optimization
    struct sigaction        sigAction[_NSIG];
    SigSet                  sigIgnored;

#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

    DEFINE_ACCESS_SERVER(ProcessLinuxClient,
			 MetaObj::controlAccess,MetaObj::none);

#ifndef _IN_STUBGEN
    class LinuxCredsHolder {
    public:
	struct creds_t linuxCreds;
	// eventually, stuff for caching here
	DEFINE_GLOBALPADDED_NEW(LinuxCredsHolder)
    };
#endif /* #ifndef _IN_STUBGEN */

__xprivate:
    virtual __async SysStatus _acceptSignals(ProcessLinux::SigSet set);

protected:
#ifndef _IN_STUBGEN
    static uval8 GetThreadExtID() { return CObjGlobals::ProcessLinuxIndex; }
#endif /* #ifndef _IN_STUBGEN */
    static ThreadClone *GetSelf();

    static SysStatus AddThread(uval threadUval);
    virtual SysStatus moveThread(Thread *baseThread);
    static void MoveThread(uval threadUval);

    void getHandler(__sighandler_t& handler, uval& flags,
		    sval& sig, SigSet& oldmask, uval& more);
    virtual SysStatus handlerFinished(SigSet& oldmask);
    void forceSignalSelf(sval sig);
    virtual SysStatus pushSignals(VolatileState *vsp, NonvolatileState *nvsp);
    virtual SysStatus pushTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
			       VolatileState *vsp, NonvolatileState *nvsp);
    static void DisabledPushSignals(VolatileState *vsp,
				    NonvolatileState *nvsp);
    static void DisabledPushTrap(uval trapNumber, uval trapInfo,
				 uval trapAuxInfo,
				 VolatileState *vsp, NonvolatileState *nvsp);
    void locked_deliverSignals(ThreadClone *thread);
    virtual SysStatus deliverSignals(pid_t pid);
    static SysStatus DeliverSignals(uval pidUval);
    virtual SysStatus terminateClone(Thread *baseThread);
    static void TerminateClone(uval threadUval);
    SysStatus signalClone(pid_t pid, sval sig);

#ifndef _IN_STUBGEN
    static const uval CLONE_ID_SHIFT = 20;
    static const uval CLONE_ID_MASK = 0xfff;
#endif

#ifndef _IN_STUBGEN
    class TimerEventAlarm : public TimerEvent {
    private:
      pid_t victim;
      SysTime inception, expiration;

    public:
      DEFINE_GLOBAL_NEW(TimerEventAlarm);

      virtual uval timeRemaining();
      virtual SysTime scheduleAlarm(uval seconds, pid_t caller);
      virtual SysTime cancelAlarm();
      virtual void handleEvent();

      static void Worker(uval target);
    };

    ProcessLinuxClient::TimerEventAlarm *currAlarm;
#endif

    struct CloneStartArg;
    static void LaunchClone(uval cloneArgUval);
    static SysStatus CloneStart(uval cloneArgUval);
    static void DisabledCloneStart(VolatileState *vsp, NonvolatileState *nvsp,
				   CloneStartArg *cloneArg);
    void cloneCommon(ThreadClone *&thread);

    virtual SysStatus registerCallback();

public:
    DEFINE_GLOBALPADDED_NEW(ProcessLinuxClient);

    ProcessLinuxClient() : stub(StubObj::UNINITIALIZED) { /* empty body */ }


    static void SyscallDeactivate();
    static void SyscallActivate();
    static void SyscallBlock();
    static uval SyscallSignalsPending();

    static void SandboxUpcall(VolatileState *vsp, NonvolatileState *nvsp);

    static void SandboxTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
			    VolatileState *vsp, NonvolatileState *nvsp);

    static Thread *AllocThread(MemoryMgrPrimitive *memory);

    static SysStatus ClassInit(ObjectHandle oh);

    /*
     * Change the limit on the number of VPs that will be created for clones.
     * The default limit is the number of physical processors.
     */
    virtual SysStatus setVPLimit(uval limit);

    virtual SysStatus destroyOSData(uval data);

    /*
     * make a new linux process id, and associate it with the
     * already created k42 process.  Usually used by fork()
     */
    virtual SysStatus preFork(ObjectHandle childProcessOH,
			      ObjectHandle& childProcessLinuxOH,
			      pid_t& childPID);

    virtual SysStatus postFork(ObjectHandle processLinuxOH,
			       pid_t pid, Thread *forkThread);

    virtual SysStatus internalExec(); // fast in-process exec

    virtual SysStatus registerExitHook(ExitHook hook);

    /*
     * change the associated k42 process of this linux process
     * to the current process.  Normally used after exec by the
     * new exec'd process.
     */
    virtual SysStatus createExecProcess(
	__in ObjectHandle execProcessOH,
	ObjectHandle& execProcessLinuxOH);

    /*
     * wait for a linux process to terminate.
     * waitfor passes in the normal waitpid parameter,
     * returns the result of waitpid
     */
    virtual SysStatus waitpid(
	pid_t& waitfor,
	sval& status,
	uval options);

    static SysStatus WaitPIDInternal(pid_t& pid, sval& status, uval options);

    virtual SysStatus becomeInit();
    virtual SysStatus becomeLinuxProcess();

    /*
     * exit tells the linux process system that the linux process
     * has terminated with status
     * we send the encoded value - so we can assert we were stopped
     * by a signal if need be
     */
    virtual SysStatus exit(sval status);

    virtual SysStatus getpid(pid_t& pid);

    virtual SysStatus getppid(pid_t& pid);

    virtual SysStatus getpgid(pid_t& pid);

    virtual SysStatus kill(pid_t pid, sval sig);

    virtual SysStatus setsid();

    virtual SysStatus setpgid(pid_t pid, pid_t pgid);

    virtual SysStatus set_uids_gids(
	set_uids_gids_type type,
	uid_t uid, uid_t euid, uid_t suid, uid_t fsuid,
	gid_t gid, gid_t egid, gid_t sgid, gid_t fsgid);

    virtual SysStatus insecure_setuidgid(uid_t euid, gid_t egid);

    virtual SysStatus clone(pid_t& pid, int flags, void *child_stack,
			    void *parent_tid, void *tls, void *child_tid);
    virtual SysStatus cloneNative(pid_t& pid,
				  Scheduler::ThreadFunction fct, uval fctArg);

    virtual SysStatus getResourceUsage(
	pid_t about, struct BaseProcess::ResourceUsage& resourceUsage);

    virtual SysStatus alarm(pid_t about, uval seconds);

    // about 0 -> self
    virtual SysStatus getInfoLinuxPid(
	pid_t about, struct ProcessLinux::LinuxInfo& linuxInfo);

    virtual SysStatus getInfoNativePid(
	ProcessID k42_pid, struct ProcessLinux::LinuxInfo& linuxInfo);

    /*
     * we provide the get/release info pointer interface
     * for efficient use in servers.
     * the idea is that ProcessLinux can cache the info for many processes
     * and avoid copies by passing a pointer.
     * but we need to use count the cached data structure.
     * all this only works when passing a K42 PID, NOT a linux PID.
     * This is because suid/sgid exec can change the creds of a linux pid,
     * but it gets a new K42 PID at the same time!
     * Normal clients will use getInfo above.
     */
    virtual SysStatus getCredsPointerNativePid(
	ProcessID k42_pid, ProcessLinux::creds_t*& linuxCredsPtr);

    virtual SysStatus releaseCredsPointer(
	ProcessLinux::creds_t* linuxCredsPtr);

    virtual SysStatus setTimeOfDay(uval sec, uval usec);

    /*
     * job control methods used by privileged tty servers
     */
    virtual SysStatus addTTY(uval ttyNum, uval ttyData);

    virtual SysStatus removeTTY(uval ttyToken);

    virtual SysStatus setCtrlTTY(uval ttyNum, ProcessID processID);

    virtual SysStatus sigaction(int sig, const struct sigaction* act,
				struct sigaction* oldact, uval sigsetsize);

    virtual SysStatus sigprocmask(int how, const sigset_t* set,
				  sigset_t* oldset, uval sigsetsize);

    virtual SysStatus sigsuspend(const sigset_t* mask);

    virtual SysStatus sigreturn(SignalUtils::SignalReturnType srType,
				uval stkPtr);
};
#endif /* #ifndef __PROCESS_LINUX_CLIENT_H_ */
