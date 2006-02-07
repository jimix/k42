#ifndef __PROCESS_LINUX_SERVER_H_
#define __PROCESS_LINUX_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessLinuxServer.H,v 1.56 2005/08/11 20:20:50 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides server for process related Linux operations
 *                     such as the association of a Linux PID with a K42 PID
 *                     There is only one server object for this class
 *                     Each linux process is attached through its own
 *                     XObject and distinguished using XObject client data
 * **************************************************************************/

#ifndef EXPORT_PROCESSLINUXSERVER
#define EXPORT_PROCESSLINUXSERVER
#include <sys/ProcessLinux.H>
#endif /* #ifndef EXPORT_PROCESSLINUXSERVER */

#include <cobj/XHandleTrans.H>
#include <meta/MetaProcessLinuxServer.H>
#include <sys/ProcessSet.H>
#include <misc/ListSimple.H>
#include <cobj/TypeMgr.H>
#include <scheduler/Scheduler.H>
#ifdef PROCESSLINUXSERVER_SANITY
extern     uval sanity_calls, sanity_stop;
#endif /* #ifdef PROCESSLINUXSERVER_SANITY */

/* copied from sys/utsname.h and sys/utsname.h */
#define _UTSNAME_LENGTH 65
#ifndef _UTSNAME_VERSION_LENGTH
# define _UTSNAME_VERSION_LENGTH _UTSNAME_LENGTH
#endif


struct siginfo;

class ProcessLinuxServer : public ProcessLinux
    __xbase(Obj)
    __xadef(tty)
{
protected:
    static char _LinuxVersion[_UTSNAME_VERSION_LENGTH];

    struct task_struct;

    virtual void init();

    SysStatus locked_getInfo(task_struct *linux_task,
			     struct LinuxInfo& linuxInfo);

//stubgen has trouble with several things below - just hide it all
#ifndef _IN_STUBGEN
    DEFINE_ACCESS_SERVER(ProcessLinuxServer,
			 MetaObj::controlAccess, MetaObj::none);
    DEFINE_GLOBALPADDED_NEW(ProcessLinuxServer);


    /*
     * cribbed from linux sched.h
     */
    struct task_struct {
	BaseProcessRef   k42process;
	ProcessID        k42PID;	// for debugging only
	ObjectHandle     callbackOH;	// object handle for linux process
	XHandle          callerXHandle;	// xhandle we gave to linux process
	uval             did_exec:8;	// used by job control
	pid_t            pid;
	task_struct*     p_pgrp;	// process group leader
	pid_t            tty_old_pgrp;
	task_struct*     p_session;	// session leader
	sval             status;	// exit status
	uval             pgid_use;	// uses of this pid as a pgid or sid
					// including self references
	SigSet           pendingSignals;// signals that haven't been delivered

	SysStatus killTask(siginfo* si, void *arg);
	SysStatus passSignal(siginfo* si,void *arg);


	/*
	 * pointers to (original) parent process, youngest child,
	 * younger sibling, older sibling, respectively.
	 * (p->father can be replaced with p->p_pptr->pid)
	 */
	task_struct      *p_opptr, *p_pptr, *p_cptr, *p_ysptr, *p_osptr;
	task_struct      *pidhash_next, **pidhash_pprev;

	struct creds_t   creds;

	uval controlling_tty_token;

	uval markCount;			// used in tree walks

	DEFINE_GLOBALPADDED_NEW(task_struct);
	task_struct() {
	    did_exec = 0; p_pgrp = 0; tty_old_pgrp = -1; p_session = 0;
	    pgid_use = 0;
	    p_opptr=p_pptr=p_cptr=p_ysptr=p_osptr=0;
	    k42process = 0;
	    k42PID = 0;
	    callbackOH.init();
	    status = sval(-1);		// not set
	    markCount = 0;
	    controlling_tty_token = uval(-1); // no controlling tty
	    pendingSignals.empty();
	}
	siginfo* pullSignal();
    };


    /*
     * follows linux, but set up for extensible hashing
     * by using variables
     */
    BLock taskLock;			// single lock for now
					// may need to be finer
    task_struct **pidhash;
    uval pidhash_sz;			// num. elemenct must be power of 2
    pid_t last_pid;		// last pid value allocated

    enum {min_pid=300, max_pid=0xffff8000, INITIAL_HASH_SIZE=256};

    uval pid_hashfn(uval x) {
	return ( ((x>>8) ^ x) & (pidhash_sz-1) ); }

    /*
     * task list iterator.  call with 0 to start.
     * returns next on list. call with last seen to get next
     */
    task_struct* locked_next_task(task_struct* prev_task)
    {
	_ASSERT_HELD(taskLock);
	uval hash_index;
	if (prev_task && prev_task->pidhash_next)
	    return prev_task->pidhash_next;
	else if (prev_task)
	    hash_index = pid_hashfn(prev_task->pid) + 1;
	else hash_index = 0;
	while (hash_index < pidhash_sz) {
	    if (pidhash[hash_index]) return pidhash[hash_index];
	    hash_index++;
	}
	return 0;
    }

    /*
     * insert a task into the hash table
     */
    void locked_hash_pid(task_struct *p)
    {
	_ASSERT_HELD(taskLock);
	tassert(!(p->pid==1 && p!=saved_init_task),err_printf("hash\n"));
	task_struct **htable = &pidhash[pid_hashfn(p->pid)];

	if ((p->pidhash_next = *htable) != NULL)
		(*htable)->pidhash_pprev = &p->pidhash_next;
	*htable = p;
	p->pidhash_pprev = htable;
    }

    /*
     * remove a task from the hash table
     */
    void locked_unhash_pid(struct task_struct *p)
    {
	_ASSERT_HELD(taskLock);
	tassert(p->pid != 1,
		err_printf("ProcessLinuxServer - unhash init!\n"));
	if (p->pidhash_next)
		p->pidhash_next->pidhash_pprev = p->pidhash_pprev;
	*p->pidhash_pprev = p->pidhash_next;
    }

    task_struct *locked_find_task_by_pid(pid_t pid, uval zombieOK = 0)
    {
	_ASSERT_HELD(taskLock);
	task_struct *p, **htable = &pidhash[pid_hashfn(pid)];

	for (p = *htable; p && p->pid != pid; p = p->pidhash_next);

	// unless zombieOK is set, only return task_struct if its really
	// a process
	return zombieOK?p:(p?(p->k42process?p:0):0);
    }

    void locked_signal(task_struct* target_task, uval sig);

    // count a pgid ref up or down, free on down if last use
    void locked_pgid_ref(task_struct* linux_task, sval updown);


    task_struct* saved_init_task;	// for debug only

    void locked_sanity()
    {
#ifdef PROCESSLINUXSERVER_SANITY
	_ASSERT_HELD(taskLock);
	task_struct *p, *c;
	if (++sanity_calls == sanity_stop) breakpoint();
	//find init
	p = locked_find_task_by_pid(1);
	tassert(p == saved_init_task, err_printf("sanity\n"));
	c = p->p_cptr;
	while (1) {
	    tassert(locked_find_task_by_pid(p->pid) == p,
		    err_printf("sanity\n"));
	    tassert(!(p->pid==1 && p!=saved_init_task),err_printf("hash\n"));
	    if (c) {
		tassert(p == c->p_pptr, err_printf("sanity\n"));
		p = c;
		c = p->p_cptr;
	    } else if ((c=p->p_osptr)) {
		tassert(p->p_pptr == c->p_pptr, err_printf("sanity\n"));
		p = c;
		c = p->p_cptr;
	    } else {
		p = p->p_pptr;
		c = 0;
		if (p == saved_init_task) break;
	    }
	}
#endif /* #ifdef PROCESSLINUXSERVER_SANITY */
    }

    /*
     * finds a free pid.
     * N.B. pid must be added to hash before caller
     * releases lock.
     */
    SysStatus locked_allocate_new_pid(pid_t& new_pid);

    void locked_processChildDeath(task_struct *child_task);

    uval locked_killtree(
	task_struct* root_task, task_struct* pgrp_task, uval signum,
	uval& found_killee, uval& found_killable_killee,
	task_struct* calling_task,
	ListSimple<BaseProcessRef, AllocLocalStrict>& killList);

    void locked_kill(
	task_struct* pgrp_task, uval sig, uval doing_pg,
	uval& found_killee, uval& found_killable_killee,
	task_struct* calling_task);

    SysStatus locked_validateRightsAndGetType(
	ObjectHandle newProcessOH,
	TypeID &procType);

    void setPIDInKernel(BaseProcessRef procRef, pid_t pid);

    SysStatus locked_convertToLinuxProc(
	BaseProcessRef processRef,
	task_struct *parent_task,
	task_struct * &child_task);

    SysStatus locked_create(
	ObjectHandle processOH,
	task_struct *parent_task,
	ObjectHandle& processLinuxOH,
	pid_t& linuxPID);

    SysStatus locked_nativePIDToTask(
	ProcessID nativePID, task_struct*& linux_task) {
	BaseProcessRef spref;
	uval data;
	pid_t linux_pid;
	SysStatus rc;

	rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(
	    nativePID, spref);
	if (_FAILURE(rc)) return rc;
	// can't fail, but may return garbage if never set
	// but if garbage, lookup or test below will catch it
	DREF(spref)->getOSData(data);
	linux_pid = pid_t(data);
	linux_task = locked_find_task_by_pid(linux_pid);
	if (!linux_task || (linux_task->k42process != spref)) {
	    return _SERROR(1735, 0, ESRCH);
	}
	return 0;
    }

    SysStatus locked_setControllingTTY(uval ttyToken, ProcessID processID) ;

    class LinuxCredsHolder {
    public:
	struct creds_t linuxCreds;
	// eventually, stuff for caching here
	DEFINE_GLOBALPADDED_NEW(LinuxCredsHolder)
    };

    struct TTYStuff {
	union {
	    task_struct* session;	// a session leader
	    uval nextFree;		// index
	};
	task_struct* foreground;	// a process group leader
	uval ttyData;			// the tty
	void init(uval data) {
	    session = foreground = 0;
	    ttyData = data;
	}
    };

    TTYStuff* ttyStuff;

    uval      markCount;		// used in mark treewalks

#endif /* #ifndef _IN_STUBGEN */

    // all virtual things must appear to stubgen
    // since Xobject is compiled _IN_STUBGEN
    virtual SysStatus handleXObjFree(XHandle xhandle);

    virtual SysStatus createInternal(ObjectHandle& processLinuxOH,
				     ObjectHandle processOH);

    virtual SysStatus printStatus();

    void testKludge(task_struct* it);

public:
    static SysStatus ClassInit();

    /*
     * Change the limit on the number of VPs that will be created for clones.
     * The default limit is the number of physical processors.
     */
    virtual SysStatus setVPLimit(uval limit) {
	return _SERROR(2779, 0, EINVAL);
    }

    virtual SysStatus destroyOSData(uval data) {
	return _SERROR(2389, 0, EINVAL);
    }

    /*
     * make a new linux process id, and associate it with the
     * already created k42 process.  Usually used by fork()
     */
    virtual SysStatus preFork(ObjectHandle childProcessOH,
			      ObjectHandle& childProcessLinuxOH,
			      pid_t& childPID) {
	return _SERROR(1626, 0, EINVAL);
    }

    virtual SysStatus postFork(ObjectHandle processLinuxOH,
			       pid_t pid, Thread *forkThread) {
	return _SERROR(1627, 0, EINVAL);
    }

    virtual SysStatus internalExec() {
	return _SERROR(2362, 0, EINVAL);
    }

    virtual SysStatus registerExitHook(ExitHook hook) {
	return _SERROR(2337, 0, EINVAL);
    }


    /*
     * change the associated k42 process of this linux process
     * to the current process.  Normally used after exec by the
     * new exec'd process.
     */
    virtual SysStatus createExecProcess(
	ObjectHandle execProcessOH,
	ObjectHandle& execProcessLinuxOH);

    /*
     * wait for a linux process to terminate.
     * waitfor passes in the normal waitpid parameter,
     * returns the result of waitpid
     */
    virtual SysStatus waitpid(
	pid_t& waitfor,
	sval& status,
	uval options)  {
	return _SERROR(1607, 0, EINVAL); }

    virtual SysStatus becomeInit() {
	return _SERROR(1608, 0, EINVAL); }

    virtual SysStatus becomeLinuxProcess() {
	return _SERROR(2392, 0, EINVAL); }


    /*
     * exit tells the linux process system that the linux process
     * has terminated with status
     */
    virtual SysStatus exit(sval status)  {
	return _SERROR(1609, 0, EINVAL); }

    virtual SysStatus getpid(pid_t& pid)  {
	return _SERROR(1663, 0, EINVAL); }

    virtual SysStatus getppid(pid_t& pid)  {
	return _SERROR(1664, 0, EINVAL); }

    virtual SysStatus getpgid(pid_t& pid)  {
	return _SERROR(1665, 0, EINVAL); }

    virtual SysStatus kill(pid_t pid, sval sig);

    virtual SysStatus setsid() {
    	return _SERROR(1677, 0, EINVAL); }

    virtual SysStatus setpgid(pid_t pid, pid_t pgid) {
    	return _SERROR(1678, 0, EINVAL); }

    virtual SysStatus set_uids_gids(
	set_uids_gids_type type,
	uid_t uid, uid_t euid, uid_t suid, uid_t fsuid,
	gid_t gid, gid_t egid, gid_t sgid, gid_t fsgid) {
	return _SERROR(1724, 0, EINVAL);
    }

    virtual SysStatus insecure_setuidgid(uid_t euid, gid_t egid) {
	return _SERROR(2842, 0, EINVAL);
    }

    virtual SysStatus clone(pid_t& pid, int flags, void *child_stack,
			    void *parent_tid, void *tls, void *child_tid) {
	return _SERROR(2317, 0, EINVAL);
    }

    virtual SysStatus cloneNative(pid_t& pid,
				  Scheduler::ThreadFunction fct, uval fctArg) {
	return _SERROR(2810, 0, EINVAL);
    }

    virtual SysStatus setTimeOfDay(uval sec, uval usec);

    // about 0 -> self - not supported in kernel
    virtual SysStatus getResourceUsage(
	pid_t about, struct BaseProcess::ResourceUsage& resourceUsage);

    virtual SysStatus alarm(pid_t about, uval seconds);

    // about 0 -> self - not supported in kernel
    virtual SysStatus getInfoLinuxPid(
	pid_t about, struct ProcessLinux::LinuxInfo& linuxInfo);

    virtual SysStatus getInfoNativePid(
	ProcessID k42_pid, struct ProcessLinux::LinuxInfo& linuxInfo);

    virtual SysStatus getCredsPointerNativePid(
	ProcessID k42_pid, ProcessLinux::creds_t*& linuxCredsPtr);

    virtual SysStatus releaseCredsPointer(
	ProcessLinux::creds_t* linuxCredsPtr);

__xprotected:

    static SysStatusProcessID _GetPID();

    static SysStatus _Create(
	__out ObjectHandle& processLinuxOH,
	__in ObjectHandle processOH);

    static SysStatus _getLinuxVersion(
	__outbuf(__rc:buflen) char * buf, __in uval buflen);

    virtual SysStatus _createChild(
	__in ObjectHandle childProcessOH,
	__out ObjectHandle& childProcessLinuxOH,
	__out pid_t& childLinuxPID,
	__XHANDLE xhandle);

    virtual SysStatus _createExecProcess(
	__in ObjectHandle execProcessOH,
	__out ObjectHandle& execProcessLinuxOH,
	__XHANDLE xhandle);

    virtual SysStatus _registerCallback(ObjectHandle callbackOH,
					__XHANDLE xhandle);

    virtual SysStatus _becomeInit(__CALLER_PID caller, __XHANDLE xhandle);

    virtual SysStatus _becomeLinuxProcess(__CALLER_PID caller,
					  __XHANDLE xhandle);

    virtual SysStatus _exit(__in sval status, __XHANDLE xhandle);

    virtual SysStatus _waitpid(__inout pid_t& waitfor,
			       __out sval& status, __XHANDLE xhandle);

    virtual SysStatus _getResourceUsage(__in pid_t about,
                       __out struct BaseProcess::ResourceUsage& resourceUsage,
		       __XHANDLE xhandle);

    virtual SysStatus _getInfoLinuxPid(__in pid_t about,
			       __out struct ProcessLinux::LinuxInfo& linuxInfo,
			       __XHANDLE xhandle);

    virtual SysStatus _getInfoNativePid(__in ProcessID about,
			       __out struct ProcessLinux::LinuxInfo& linuxInfo);

    virtual SysStatus _kill(__in pid_t pid, __in sval sig, __XHANDLE xhandle);

    virtual SysStatus _setsid(__XHANDLE xhandle);

    virtual SysStatus _setpgid(__in pid_t pid, __in pid_t pgid,
			       __in uval did_exec, __XHANDLE xhandle);

    virtual SysStatus _set_uids_gids(
	__in uval type,
	__in uid_t uid, __in uid_t euid, __in uid_t suid, __in uid_t fsuid,
	__in uid_t gid, __in uid_t egid, __in uid_t sgid, __in uid_t fsgid,
	__XHANDLE xhandle);

    virtual SysStatus _insecure_setuidgid(__in uid_t euid, __in gid_t egid,
	__XHANDLE xhandle);

    virtual SysStatus _setTimeOfDay(__in uval sec, __in uval usec,
				    __XHANDLE xhandle);

    /*
     * job control methods used by privileged tty servers
     */
    virtual SysStatus _addTTY(__in uval ttyNum,
			      __in uval ttyData) /*__xa(tty)*/;

    virtual SysStatus _removeTTY(uval ttyNum) /*__xa(tty)*/;

    virtual SysStatus _setCtrlTTY(uval ttyNum,
				  ProcessID processID) /*__xa(tty)*/;

    static SysStatus _PrintStatus();
public:
    /*
     * job control methods used by privileged tty servers
     */
    virtual SysStatus addTTY(uval ttyNum, uval ttyData) {
	return _addTTY(ttyNum, ttyData);
    }

    virtual SysStatus removeTTY(uval ttyToken) {
	return _removeTTY(ttyToken);
    }

    virtual SysStatus setCtrlTTY(uval ttyNum, ProcessID processID) {
	return _setCtrlTTY(ttyNum, processID);
    }

    virtual SysStatus sigaction(int signum, const struct sigaction* act,
				struct sigaction* oldact, uval sigsetsize) {
#ifndef _IN_STUBGEN
	passert(0, err_printf("sigaction called in kernel\n"));
#endif /* #ifndef _IN_STUBGEN */
	return 0;			// keep gcc happy, not reached
    }

    virtual SysStatus sigprocmask(int how,
				  const sigset_t* set,
				  sigset_t* oldset, uval sigsetsize) {
#ifndef _IN_STUBGEN
	passert(0, err_printf("sigprocmask called in kernel\n"));
#endif /* #ifndef _IN_STUBGEN */
	return 0;			// keep gcc happy, not reached
    }

    virtual SysStatus sigsuspend(const sigset_t* mask) {
#ifndef _IN_STUBGEN
	passert(0, err_printf("sigsuspend called in kernel\n"));
#endif /* #ifndef _IN_STUBGEN */
	return 0;			// keep gcc happy, not reached
    }

    virtual SysStatus sigreturn(SignalUtils::SignalReturnType srType,
				uval stkPtr) {
#ifndef _IN_STUBGEN
	passert(0, err_printf("sigreturn called in kernel\n"));
#endif /* #ifndef _IN_STUBGEN */
	return 0;			// keep gcc happy, not reached
    }

protected:
    virtual SysStatus moveThread(Thread *baseThread) {
	/*
	 * The IO/CPU migration mechanism invokes this method to get a
	 * thread moved from one domain to the other.  Here in the server
	 * itself we don't support IO/CPU migration (at least not yet), so
	 * just put the thread back in the ready queue.
	 */
	Scheduler::ResumeThread(baseThread);
	return 0;
    }

    virtual SysStatus handlerFinished(SigSet& oldmask) {
	return _SERROR(2383, 0, EINVAL);
    }

    virtual SysStatus pushSignals(VolatileState *vsp, NonvolatileState *nvsp) {
	return _SERROR(2830, 0, EINVAL);
    }

    virtual SysStatus pushTrap(uval trapNumber, uval trapInfo, uval trapAuxInfo,
			       VolatileState *vsp, NonvolatileState *nvsp) {
	return _SERROR(2829, 0, EINVAL);
    }

    virtual SysStatus deliverSignals(pid_t pid) {
	return _SERROR(2384, 0, EINVAL);
    }

    virtual SysStatus terminateClone(Thread *baseThread) {
	return _SERROR(2385, 0, EINVAL);
    }

};
#endif /* #ifndef __PROCESS_LINUX_SERVER_H_ */
