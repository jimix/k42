#ifndef __PROCESS_LINUX_H_
#define __PROCESS_LINUX_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessLinux.H,v 1.53 2005/08/11 20:20:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides server for process related Linux operations
 *                     such as the association of a Linux PID with a K42 PID
 * **************************************************************************/

// FIXME GCC3 -JX. should not need this.
#if __GNUC__ >= 3
#ifndef __USE_MISC
#define __USE_MISC		// need this to get SA_ONESHOT
#endif
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <signal.h>
#include <scheduler/Thread.H>
#include <bits/posix1_lim.h>

#include <scheduler/Scheduler.H>
#include <emu/SignalUtils.H>

class ProcessLinux : public Obj {
public:

    /*
     * Glibc, built for Linux, defines a 1024-bit sigset_t, even though _NSIG
     * is 64 and Linux supports just 64 signals.  We define our own SigSet
     * class and provide methods for interfacing with glibc sigset_t's.
     * As with sigset_t, everything is 1-based.
     *
     * rt_sigprocmask allows size to be passed in, and we must respect that.
     */
#if _NSIG != 65
#error  Multi-word SigSet implementation now needed.
#endif
    class SigSet {
    public:
	uval64 bits;

	void copyOut(sigset_t* _set, uval size) {
	    memcpy(&_set->__val[0], &bits, MIN(size, sizeof(bits)));
	    if (size > sizeof(bits)) {
		memset(&_set->__val[1], 0ULL, size - sizeof(bits));
	    }
	}

	void copyIn(const sigset_t *_set) {
	    bits = _set->__val[0];
	}

	void orIn(const sigset_t *_set) {
	    bits |= _set->__val[0];
	}

	void nandIn(const sigset_t *_set) {
	    bits &= ~_set->__val[0];
	}

	void orInSigSet(const SigSet *_set) {
	    bits |= _set->bits;
	}

	uval isSet(const uval sig) {
	    return (bits & (1ull << (sig-1)));
	}

	void set(const uval sig) {
	    tassertMsg(sig,
		       "attempt to set signal 0 - signals are one based\n");
	    bits |= (1ull << (sig-1));
	}

	void clear(const uval sig) {
	    bits &= ~(1ull << (sig-1));
	}

	void empty() {
	    bits = 0ull;
	}

	uval any() {
	    return bits;
	}

	uval anyNonblocked(const SigSet& mask) {
	    return (bits & ~mask.bits);
	}

	void clearNonblockedAndIgnored(const SigSet& mask,
				       const SigSet& ignored) {
	    bits &= (mask.bits | ~ignored.bits);
	}

	uval getAndClearNextNonblocked(const SigSet& mask) {
	    uval sig;
	    uval64 v;
	    v = bits & ~mask.bits;
	    if (v) {
		sig = 1;		// one based counting
		if ((v & 0xffffffffUL) == 0) { sig += 32; v >>= 32; }
		if ((v & 0x0000ffffUL) == 0) { sig += 16; v >>= 16; }
		if ((v & 0x000000ffUL) == 0) { sig +=  8; v >>=  8; }
		if ((v & 0x0000000fUL) == 0) { sig +=  4; v >>=  4; }
		if ((v & 0x00000003UL) == 0) { sig +=  2; v >>=  2; }
		if ((v & 0x00000001UL) == 0) { sig +=  1; }
		bits &= ~(1ull << (sig-1));
		return sig;
	    }
	    return 0;			// nothing found
	}
    };

//define creds inside ProcessLinux, but use seperate file
//for convenience.
#include <sys/LinuxCreds.H>

    struct LinuxInfo {
	// field names follow linux task_struct in sched.h
	pid_t pid;			// this process
	pid_t pgrp;			// process group leaded
	pid_t tty_old_pgrp;
	pid_t session;			// session group leader
	pid_t tgid;			// terminal group
	pid_t ppid;			// parent (linux p->p_pptr->pid)
	uval  tty;			// controlling tty or null
	struct creds_t creds;		// credentials

    };

#define NOTLINUX 1

    /*
     * Change the limit on the number of VPs that will be created for clones.
     * The default limit is the number of physical processors.
     */
    virtual SysStatus setVPLimit(uval limit)=0;

    virtual SysStatus destroyOSData(uval data)=0;

    /*
     * make a new linux process id, and associate it with the
     * already created k42 process.  Usually used by fork()
     */
    virtual SysStatus preFork(ObjectHandle childProcessOH,
			      ObjectHandle& childProcessLinuxOH,
			      pid_t& childPID)=0;

    virtual SysStatus postFork(ObjectHandle processLinuxOH,
			       pid_t pid, Thread *forkThread)=0;

    virtual SysStatus internalExec()=0; // fast in-process exec

    typedef void (*ExitHook)(void);
    virtual SysStatus registerExitHook(ExitHook hook)=0;

    /*
     * transfer the ownership of a linux process from the current
     * k42 process to a new k42 process.  Used by exec.
     * Makes a new object handle for the new process, and remove
     * the current process object handles task reference.
     */
    virtual SysStatus createExecProcess(
	__in ObjectHandle execProcessOH,
	ObjectHandle& execProcessLinuxOH)=0;

    /*
     * wait for a linux process to terminate.
     * waitfor passes in the normal waitpid parameter,
     * returns the result of waitpid
     */
    virtual SysStatus waitpid(
	pid_t& waitfor,
	sval& status,
	uval options)=0;

    /*
     * Turn the calling k42 process into linux process 1.
     */
    virtual SysStatus becomeInit()=0;

    /*
     * Turn the calling k42 process into a vanilla linux process.
     */
    virtual SysStatus becomeLinuxProcess()=0;

    /*
     * exit tells the linux process system that the linux process
     * has terminated with status
     */
    virtual SysStatus exit(sval status)=0;

    virtual SysStatus getpid(pid_t& pid)=0;

    virtual SysStatus getppid(pid_t& pid)=0;

    virtual SysStatus getpgid(pid_t& pid)=0;

    virtual SysStatus kill(pid_t pid, sval sig)=0;

    virtual SysStatus setsid()=0;

    virtual SysStatus setpgid(pid_t pid, pid_t pgid)=0;

    enum set_uids_gids_type{SETUID, SETREUID, SETEUID, SETRESUID, SETFSUID,
	  SETGID, SETREGID, SETEGID, SETRESGID, SETFSGID};

    virtual SysStatus set_uids_gids(
	set_uids_gids_type type,
	uid_t uid, uid_t euid, uid_t suid, uid_t fsuid,
	gid_t gid, gid_t egid, gid_t sgid, gid_t fsgid)=0;

    virtual SysStatus insecure_setuidgid(uid_t euid, gid_t egid)=0;

    virtual SysStatus clone(pid_t& pid, int flags, void *child_stack,
			    void *parent_tid, void *tls, void *child_tid)=0;
    virtual SysStatus cloneNative(pid_t& pid,
				  Scheduler::ThreadFunction fct, uval fctArg)=0;

    virtual SysStatus getResourceUsage(
	pid_t about, struct BaseProcess::ResourceUsage& resourceUsage)=0;

    virtual SysStatus alarm(pid_t about, uval seconds)=0;

    // about 0 -> self
    virtual SysStatus getInfoLinuxPid(
	pid_t about, struct ProcessLinux::LinuxInfo& linuxInfo)=0;

    virtual SysStatus getInfoNativePid(
	ProcessID k42_pid, struct ProcessLinux::LinuxInfo& linuxInfo)=0;

    /*
     * we provide the get/release creds pointer interface
     * for efficient use in servers.
     * the idea is that ProcessLinux can cache the info for many processes
     * and avoid copies by passing a pointer.
     * but we need to use count the cached data structure.
     * all this only works when passing a K42 PID, NOT a linux PID.
     * This is because suid/sgid exec can change the creds of a linux pid,
     * but it gets a new K42 PID at the same time!
     * Normal clients will use getInfo above.
     * N.B. we only cache creds - process group info can change anytime
     */
    virtual SysStatus getCredsPointerNativePid(
	ProcessID k42_pid, ProcessLinux::creds_t*& linuxCredsPtr)=0;

    virtual SysStatus releaseCredsPointer(
	ProcessLinux::creds_t* linuxCredsPtr)=0;

    virtual SysStatus setTimeOfDay(uval sec, uval usec)=0;

    /*
     * job control methods used by privileged tty servers
     */
    virtual SysStatus addTTY(uval ttyNum, uval ttyData)=0;

    virtual SysStatus removeTTY(uval ttyToken)=0;

    virtual SysStatus setCtrlTTY(uval ttyNum, ProcessID processID)=0;

    enum {SIG_DEFAULT, SIG_IGNORE, SIG_ACTION, SIG_VERIFY, SIG_ACTION_RETURN};

    virtual SysStatus sigaction(int signum, const struct sigaction* act,
				struct sigaction* oldact, uval sigsetsize)=0;

    virtual SysStatus sigprocmask(int how,
				  const sigset_t* set,
				  sigset_t* oldset,
				  uval sigsetsize)=0;

    virtual SysStatus sigsuspend(const sigset_t* mask)=0;

    virtual SysStatus sigreturn(SignalUtils::SignalReturnType srType,
				uval stkPtr)=0;

    virtual SysStatus moveThread(Thread *baseThread)=0;
    virtual SysStatus handlerFinished(SigSet& oldmask)=0;
    virtual SysStatus pushSignals(VolatileState *vsp, NonvolatileState *nvsp)=0;
    virtual SysStatus pushTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
			       VolatileState *vsp, NonvolatileState *nvsp)=0;
    virtual SysStatus deliverSignals(pid_t pid)=0;
    virtual SysStatus terminateClone(Thread *baseThread)=0;
};
#endif /* #ifndef __PROCESS_LINUX_H_ */
