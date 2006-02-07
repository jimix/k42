/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ProcessLinuxServer.C,v 1.79 2005/08/11 20:20:50 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "ProcessLinuxServer.H"
#include "PrivilegedServiceWrapper.H"
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaProcessServer.H>
#include <stub/StubProcessServer.H>
#include <stub/StubProcessLinuxClient.H>
#include <stub/StubKBootParms.H>
#include <trace/traceProc.h>
#include <sys/KernelInfo.H>
#include <sys/ProcessWrapper.H>
#define MAXTTYNUM 1024
#ifdef PROCESSLINUXSERVER_SANITY
uval sanity_calls=0, sanity_stop=0;
#endif /* #ifdef PROCESSLINUXSERVER_SANITY */

char ProcessLinuxServer::_LinuxVersion[_UTSNAME_VERSION_LENGTH];

SysStatus
ProcessLinuxServer::ClassInit()
{
    ProcessLinuxServer *ptr = new ProcessLinuxServer;
    ptr->init();
    CObjRootSingleRep::Create(ptr, (RepRef)GOBJ(TheProcessLinuxRef));
    MetaProcessLinuxServer::init();

    char buf[80];  // 80 is what _GetThinEnvVar returns
    if (_SUCCESS(StubKBootParms::_GetParameterValue("K42_LINUX_RELEASE",
						    buf, 80))) {
	if (buf[0] == '\0') {
	    strcpy(_LinuxVersion, "2.6.3");
	} else {
	    buf[80-1] = '\0';
	    strncpy(_LinuxVersion, buf, _UTSNAME_VERSION_LENGTH);
	    _LinuxVersion[_UTSNAME_VERSION_LENGTH-1] = '\0';
	}
    } else {
	strcpy(_LinuxVersion, "2.6.3");
    }

    return 0;
}

void
ProcessLinuxServer::init()
{
    task_struct **p;
    pidhash= (task_struct**)allocGlobalPadded(
	sizeof(task_struct*)*INITIAL_HASH_SIZE);
    pidhash_sz = INITIAL_HASH_SIZE;
    for (p = pidhash;p<(pidhash+INITIAL_HASH_SIZE);p++) {
	*p = 0;
    }
    last_pid = min_pid;
    saved_init_task = 0;		// MAA debug
    ttyStuff = (TTYStuff*)allocGlobalPadded(
	sizeof(TTYStuff)*MAXTTYNUM);
    uval i;
    for (i=0;i<MAXTTYNUM;i++)
	ttyStuff[i].init(0);
    markCount = 0;

    /*
     * We don't have a linux pid ourselves, but the kernel has to have
     * something for us, so use 0.
     */
    setPIDInKernel(GOBJ(TheProcessRef), 0);
}

/*
 * find a free pid.  Its not really "allocated" till its hashed
 * by the caller.
 */
SysStatus
ProcessLinuxServer::locked_allocate_new_pid(pid_t& new_pid)
{
    uval nopids=0;

    new_pid = last_pid+1;
    while (1) {
	if (uval(new_pid) >= uval(max_pid)) {
	    if (nopids) return _SERROR(1610, 0, EAGAIN);
	    nopids++;
	    new_pid = min_pid;
	}
	if (!locked_find_task_by_pid(new_pid, 1 /*zombieOK*/)) break;
	new_pid++;
    }
    last_pid = new_pid;
    return 0;
}

SysStatus
ProcessLinuxServer::locked_validateRightsAndGetType(ObjectHandle newProcessOH,
						    TypeID &procType)
{
    SysStatus rc;
    AccessRights match, nomatch, required;

    if (newProcessOH.commID() != SysTypes::KERNEL_COMMID) {
	return _SERROR(2371, 0, EINVAL);
    }

    StubObj tmpStub(StubObj::UNINITIALIZED);
    tmpStub.setOH(newProcessOH);
    rc = tmpStub._getInfo(procType, match, nomatch);
    passertMsg(_SUCCESS(rc), "getInfo failed.\n");
    _IF_FAILURE_RET(rc);

    if (!StubProcessServer::isBaseOf(procType)) {
	return _SERROR(2377, 0, EINVAL);
    }

    required = MetaProcessServer::destroy;
    if ((match & required) != required) {
	return _SERROR(2372, 0, EINVAL);
    }

    return 0;
}

void
ProcessLinuxServer::setPIDInKernel(BaseProcessRef procRef, pid_t pid)
{
    // FIXME:  We should find a way to avoid making this call to the kernel
    //         in most cases.  One possibility:  arrange things so that the
    //         linux pid for a process is usually the same as the k42 pid.
    //         Then make the kernel call only when the pids don't match.
    SysStatus rc;
    ObjectHandle procOH;
    PrivilegedServiceRef privServRef;

    (void) ((ProcessWrapper *)DREF(procRef))->getProcessHandle(procOH);

    privServRef = PrivilegedServiceWrapper::ThePrivilegedServiceRef();
    rc = DREF(privServRef)->setProcessOSData(procOH.xhandle(), uval(pid));
    tassertMsg(_SUCCESS(rc), "Could not set pid in kernel.\n");
}

SysStatus
ProcessLinuxServer::locked_convertToLinuxProc(BaseProcessRef processRef,
					      task_struct *parent_task,
					      task_struct *&child_task)
{
    SysStatus rc;

    if (!(parent_task->k42process)) {
	/* should we use init or return error here? */
	parent_task = locked_find_task_by_pid(1);
	passertMsg(parent_task, "init (pid 1) is gone\n");
    }

    child_task = new task_struct;
    tassertMsg(child_task != NULL, "Could not allocate a new task_struct.\n");

    rc = locked_allocate_new_pid(child_task->pid);
    _IF_FAILURE_RET(rc);

    child_task->p_pgrp = parent_task->p_pgrp;
    locked_pgid_ref(parent_task->p_pgrp, 1);
    child_task->p_session = parent_task->p_session;
    locked_pgid_ref(parent_task->p_session, 1);
    child_task->creds = parent_task->creds;

    locked_hash_pid(child_task);

    //FIXME for now I'm doing the p_optr and p_ysptr that linux keeps, but
    //      its not clear that these are needed.
    child_task->p_opptr=child_task->p_pptr=parent_task;
    child_task->p_ysptr=0;
    if (parent_task->p_cptr) {
	parent_task->p_cptr->p_ysptr = child_task;
	child_task->p_osptr = parent_task->p_cptr;
    }
    parent_task->p_cptr=child_task;

    child_task->k42process = processRef;
    child_task->k42PID = _SGETPID(DREF(processRef)->getPID());

    DREF(processRef)->setOSData(uval(child_task->pid));
    setPIDInKernel(processRef, child_task->pid);

    //maa
    //err_printf("parent %d new child %d\n", parent_task->pid, child_task->pid);
    TraceOSProcLinuxFork(child_task->pid, child_task->k42PID,
	       parent_task->pid, parent_task->k42PID);

    return 0;
}

SysStatus
ProcessLinuxServer::locked_create(ObjectHandle processOH,
				  task_struct *parent_task,
				  ObjectHandle& processLinuxOH,
				  pid_t& linuxPID)
{
    SysStatus rc;
    TypeID procType;
    BaseProcessRef processRef;
    ProcessID pid;	// k42 of course
    task_struct *child_task;

    /*
     * Verify that processOH is a valid kernel ProcessServer object
     * handle made for us with the necessary access rights.
     */
    rc = locked_validateRightsAndGetType(processOH, procType);
    if (_FAILURE(rc)) return rc;

    /*
     * Construct a registered ProcessRef around this object handle.
     */
    rc = ProcessWrapper::Create(processRef, procType, processOH);
    if (_FAILURE(rc)) return rc;

    pid = _SGETPID(DREF(processRef)->getPID());

    if (!parent_task) {
	//give child a useable oh but with no linux task
	giveAccessByServer(processLinuxOH, pid);
	linuxPID = pid_t(-1);
	return NOTLINUX;
    }

    rc = locked_convertToLinuxProc(processRef, parent_task, child_task);
    if (_FAILURE(rc)) {
	(void) DREF(processRef)->destroy();
	return rc;
    }

    /*
     * we do not use the normal giveAccessSetClientData approach
     * since we don't want user calles to give access to create
     * a linux task.  Pass along the same access rights that our caller has.
     */
    giveAccessInternal(processLinuxOH,
		       pid, MetaObj::controlAccess, MetaObj::none,
		       0, (uval)child_task);
    child_task->callerXHandle = processLinuxOH.xhandle();
    linuxPID = child_task->pid;

    return 0;
}

/*virtual*/ SysStatus
ProcessLinuxServer::createInternal(ObjectHandle& processLinuxOH,
				   ObjectHandle processOH)
{
    AutoLock<BLock> as(&taskLock);

    pid_t dummyPID;

    return locked_create(processOH, NULL, processLinuxOH, dummyPID);
}

/*static*/ SysStatusProcessID
ProcessLinuxServer::_GetPID()
{
    return _SGETPID(DREFGOBJ(TheProcessRef)->getPID());
}

/*static*/ SysStatus
ProcessLinuxServer::_Create(ObjectHandle& processLinuxOH,
			    ObjectHandle processOH)
{
    return ((ProcessLinuxServer *)DREFGOBJ(TheProcessLinuxRef))->
				    createInternal(processLinuxOH, processOH);
}

SysStatus
ProcessLinuxServer::_createChild(ObjectHandle childProcessOH,
				 ObjectHandle& childProcessLinuxOH,
				 pid_t& childLinuxPID,
				 __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);

    task_struct *parent_task;
    parent_task = (task_struct *) XHandleTrans::GetClientData(xhandle);

    return locked_create(childProcessOH, parent_task,
			 childProcessLinuxOH, childLinuxPID);
}

/*
 * process died.  if it didn't call exit, we deal with
 * death here.
 */
SysStatus
ProcessLinuxServer::handleXObjFree(XHandle xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *clientData;
    clientData = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    if (clientData) {
	tassertMsg(clientData->k42process, "task has no K42 process.\n");
	//N.B. we don't free the task - that happens in waitpid
	clientData->status = SIGKILL;
	locked_processChildDeath(clientData);
    }
    return 0;
}


/*
 * pass the linux task_struct from the current k42 process to the
 * new one which represents the exec image
 */
SysStatus
ProcessLinuxServer::_createExecProcess(ObjectHandle execProcessOH,
				       ObjectHandle& execProcessLinuxOH,
				       __XHANDLE xhandle)
{
    SysStatus rc;
    TypeID procType;
    ProcessID matchPID, execProcessID;	// k42 of course
    BaseProcessRef execProcessRef;
    task_struct *exec_task;
    AccessRights match, nomatch;

    AutoLock<BLock> as(&taskLock);

    /*
     * Verify that execProcessOH is a valid kernel ProcessServer object
     * handle made for us with the necessary access rights.
     */
    rc = locked_validateRightsAndGetType(execProcessOH, procType);
    if (_FAILURE(rc)) return rc;

    /*
     * Construct a registered ProcessRef around this object handle.
     */
    rc = ProcessWrapper::Create(execProcessRef, procType, execProcessOH);
    if (_FAILURE(rc)) return rc;

    execProcessID = _SGETPID(DREF(execProcessRef)->getPID());

    execProcessLinuxOH.init();

    // if this is not a linux task, we will just propogate the
    // null client data to the new process
    // N.B. exec_task may be null
    exec_task = (task_struct*)(XHandleTrans::GetClientData(xhandle));

    // take task_struct away from old (calling) process
    XHandleTrans::SetClientData(xhandle, 0);

    XHandleTrans::GetRights(xhandle, matchPID, match, nomatch);
    giveAccessInternal(execProcessLinuxOH, execProcessID, match, nomatch,
		       0, (uval)exec_task);

    if (!exec_task) {
	return NOTLINUX;
    }

    tassert(exec_task->k42process,
	    err_printf("zombie %ld trying to exec\n",(sval)exec_task->pid));

    // old k42 process is no longer a linux process
    DREF(exec_task->k42process)->setOSData(0);
    setPIDInKernel(exec_task->k42process, 0);

    // clear call back pointer to old k42 process
    // the new k42 process will register its call back later

    exec_task->callbackOH.init();

    exec_task->k42process = execProcessRef;
    exec_task->k42PID = execProcessID;
    exec_task->callerXHandle = execProcessLinuxOH.xhandle();

    DREF(execProcessRef)->setOSData(uval(exec_task->pid));
    setPIDInKernel(execProcessRef, exec_task->pid);

    /*
     *kludge in posix spec - once you exec, no one else can do
     *setpgid to you. inorder for both the fork parent and child
     *to "see" a new process group for the child, both must attempt
     *to set it!  If the child gets to exec before the parent gets
     *to set it, must prevent the set.  You can't make this stuff up!
     */
    exec_task->did_exec = 1;

    //maa
    //err_printf("linux process %d execd, is child of %d\n",
    //	       exec_task->pid, exec_task->p_pptr->pid);
    TraceOSProcLinuxExec(exec_task->pid,
	       execProcessID, matchPID);
    return 0;
}

SysStatus
ProcessLinuxServer::_exit(__in sval status, __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *current;
    current = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    //If no client data, k42 process termination got here first
    //This is ok - a race between killing a process and it exiting
    if (current) {
	//N.B. we don't free the task - that happens in waitpid
	current->status = status;
	locked_processChildDeath(current);
    }
    locked_sanity();
    return 0;
}

void
ProcessLinuxServer::testKludge(task_struct* it)
{
    // for now tolerate the mem leak of task
    // and hash
    //FIXME MAA
    // when this is removed, remove partner kludge in _removeTTY
    init();
}

/*
 * child is dead.  signal its parent, move its children to init.
 * it is now a zombie - still on the parent's list until waitpid
 * reaps it
 */
void
ProcessLinuxServer::locked_processChildDeath(task_struct *child_task)
{
    TraceOSProcLinuxKill(child_task->pid, child_task->k42PID);

    tassert(child_task->k42process,
	    err_printf("trying to kill dead process %ld\n",
		       uval(child_task->pid)));

    task_struct *parent_task,*init_task;
    // take link away from terminating process
    child_task->k42process = 0;
    // preserve k42PID for use in diagnosing zombies
    XHandleTrans::SetClientData(child_task->callerXHandle, 0);

    //N.B. we do NOT make a remove access on the callbackOH
    //     since the client is going to terminate anyway.
    child_task->callbackOH.init();
    parent_task = child_task->p_pptr;

    //maa
    //err_printf("linux process %d killed, was child of %d\n",
    //           child_task->pid, parent_task->pid);

    tassert(parent_task, err_printf("killing orphan linux task\n"));

    tassert(child_task->pid != 1, err_printf("killing linux init\n"));

    /*
     * we always upcall on child death.  we could try to improve
     * the case where a real SIGCHLD will also be delievered, but
     * the logic is complicated and we judge not worth bothering with
     * (Its made more complicated because SIGCHLD could be blocked
     */
    locked_signal(parent_task, SIGCHLD);

    //Remove from process group and session
    locked_pgid_ref(child_task->p_pgrp, -1);
    locked_pgid_ref(child_task->p_session, -1);

    //Move children to init
    if (child_task->p_cptr) {
	//maa
	//err_printf("moving children starting with %d to init\n",
	//	   child_task->p_cptr->pid);

	task_struct* temp_task;
	uval need_sigchld = 0;
	init_task = locked_find_task_by_pid(1);
	//find end of init chain
	//N.B. init_task must have children if there is a task to kill
	tassert(init_task->p_cptr,err_printf("init has no children\n"));
	for (temp_task = init_task->p_cptr;
	    temp_task->p_osptr; temp_task = temp_task->p_osptr);
	temp_task->p_osptr = child_task->p_cptr;
	child_task->p_cptr = 0;		// just in case
	//now fix parent pointers of moved children
	//temp_task starts out pointing to last task that was
	//on init's chain.  Skip forward one and process the rest
	for (temp_task=temp_task->p_osptr;temp_task;
	    temp_task=temp_task->p_osptr) {
	    temp_task->p_opptr = temp_task->p_pptr;
	    temp_task->p_pptr = init_task;
	    //if we move a zombie, we need to tell new parent
	    if (!temp_task->k42process) need_sigchld = 1;
	}
	if (need_sigchld) {
	    locked_signal(init_task, SIGCHLD);
	}
    }
    locked_sanity();
}

void
ProcessLinuxServer::locked_signal(task_struct* target_task, uval sig)
{
    _ASSERT_HELD(taskLock);
    tassertMsg((sig != SIGKILL) && (sig != SIGSTOP),
	       "Sending a KILL or STOP signal to task!\n");

    if (sig == 0) {
	// Sig 0 is used to cause a resend of any pending signals.  If
	// there aren't any signals pending we can return immediately.
	if (!target_task->pendingSignals.any()) {
	    return;
	}
    } else {
	target_task->pendingSignals.set(sig);
    }

    StubProcessLinuxClient stub(StubObj::UNINITIALIZED);
    if (target_task->callbackOH.valid()) {
	stub.setOH(target_task->callbackOH);
	//FIXME what if rejected
	SysStatus rc;
	rc = stub._acceptSignals(target_task->pendingSignals);
	if (_SUCCESS(rc)) {
	    target_task->pendingSignals.empty();
	}
    }
}

/*
 * used when the kernel launches a process itself.  It will never
 * be a linux process, since the kernel is not a linux process.
 */
SysStatus
ProcessLinuxServer::createExecProcess(ObjectHandle execProcessOH,
				      ObjectHandle& execProcessLinuxOH)
{
    // make a xhandle with no linux process associated
    // we've been passed on ObjectHandle to the ProcessObject
    // of the new process.  We need its pid.
    ObjRef ref;
    TypeID type;
    XHandleTrans::XHToInternal(execProcessOH.xhandle(), 0, 0,
			       ref, type);
    giveAccessByServer(execProcessLinuxOH,
	       _SGETPID(DREF((BaseProcessRef)ref)->getPID()));
    return 0;
}

/*
 * we don't do any authentication checking.  We assume that during startup
 * a process will become init before any user processes can be running.
 * once a process has become init, we don't let anyone else become
 * init
 */
SysStatus
ProcessLinuxServer::_becomeInit(__CALLER_PID caller,__XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *current;
    BaseProcessRef callerProcessRef;
    SysStatus rc;
    rc = DREFGOBJ(TheProcessSetRef)->
	getRefFromPID(caller,  callerProcessRef);
    if (_FAILURE(rc)) return rc;
    current = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    // can't become init if allready a Linux task
    if (current) return _SERROR(1623, 0, EINVAL);
    // if someone is already init can't become init
    if (locked_find_task_by_pid(1)) return _SERROR(1624, 0, EINVAL);
    current = new task_struct;
    saved_init_task = current;	// MAA for debug
    current->pid = 1;
    current->p_pgrp = current->p_session = current;
    current->pgid_use = 2;
    current->p_opptr=current->p_pptr=current;
    current->creds.rootPrivs();
    locked_hash_pid(current);
    current->k42process = callerProcessRef;
    current->k42PID = _SGETPID(DREF(current->k42process)->getPID());
    DREF(callerProcessRef)->setOSData(uval(current->pid));
    setPIDInKernel(callerProcessRef, current->pid);
    XHandleTrans::SetClientData(xhandle, (uval)current);
    current->callerXHandle = xhandle;
    return 0;
}

SysStatus
ProcessLinuxServer::_becomeLinuxProcess(__CALLER_PID caller,__XHANDLE xhandle)
{
    SysStatus rc;
    BaseProcessRef processRef;
    task_struct *parent_task, *current;

    AutoLock<BLock> as(&taskLock);

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(caller, processRef);
    _IF_FAILURE_RET(rc);

    current = (task_struct *) XHandleTrans::GetClientData(xhandle);
    if (current != NULL) {
	return _SERROR(2391, 0, EINVAL); // already a linux process
    }

    parent_task = locked_find_task_by_pid(1);
    passertMsg(parent_task != NULL, "init (pid 1) doesn't exist.\n");

    rc = locked_convertToLinuxProc(processRef, parent_task, current);

    XHandleTrans::SetClientData(xhandle, uval(current));
    current->callerXHandle = xhandle;

    return 0;
}

SysStatus
ProcessLinuxServer::_registerCallback(ObjectHandle callbackOH,
				      __XHANDLE xhandle)
{
    taskLock.acquire();
    task_struct *current;
    if (XHandleTrans::GetOwnerProcessID(xhandle) !=
       SysTypes::PID_FROM_COMMID(callbackOH.commID())) {
 	taskLock.release();
	return _SERROR(1632, 0, EACCES);
    }
    current = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    if (!current) {
	taskLock.release();
	return _SERROR(1631, 0, ESRCH);
    }
    current->callbackOH.initWithOH(callbackOH);
    // if any signals came in the window before the new process
    // could register, send them now.
    locked_signal(current, 0);
    taskLock.release();
    return 0;
}

SysStatus
ProcessLinuxServer::_waitpid(__inout pid_t& waitfor,
			     __out sval& status, __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    locked_sanity();
    uval foundChild;
    task_struct *current, *child_task, **prev;
    current = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    if (!current) return _SERROR(1660, 0, ESRCH);
    prev = &(current->p_cptr);
    foundChild = 0;
    while ((child_task=*prev)) {
	   // waitfor > 0 waits for that process
	if (((waitfor>0) && (child_task->pid != waitfor)) ||
	   // waitfor == 0 waits for any process in callers process group
	   ((waitfor==0) && (child_task->p_pgrp != current->p_pgrp)) ||
	   // waitfor < -1 waits for callers in process group -waitfor
	   ((waitfor < -1) && (child_task->p_pgrp->pid != -waitfor))) {
	    //maa
	    //err_printf("waitpid %ld skipping task %ld\n",
	    //	       (sval)waitfor, (sval)(child_task->pid));

	    // skip - child_task does not match criteria
	} else {
	    foundChild = 1;
	    if (!child_task->k42process) {
		//Found a child zombie
		//Remove from sibling list of parent
		*prev = child_task->p_osptr;
		status = child_task->status;
		waitfor = child_task->pid;
		//Mark child as reaped
		child_task->p_opptr = child_task->p_pptr;// debug info
		child_task->p_pptr = 0;
		//If the child's pid isn't still naming an active
		//process group or session, free it
		if (0==child_task->pgid_use) {
		    locked_unhash_pid(child_task);
		    delete child_task;
		}
		locked_sanity();
		return 0;
	    }
	}
	prev = &(child_task->p_osptr);
    }
    waitfor = 0;
    locked_sanity();
    if (foundChild) {
	return 0;
    } else {
	return _SERROR(1661, 0, ECHILD);
    }
}

SysStatus
ProcessLinuxServer::getResourceUsage(
    pid_t about, struct BaseProcess::ResourceUsage& resourceUsage)
{
    if (about) {
      return _getResourceUsage(about, resourceUsage, XHandle(0));
    }
    else {
      return _SERROR(1728, 0, ESRCH);
    }
}

SysStatus
ProcessLinuxServer::alarm(pid_t about, uval seconds)
{
    /* We implement this dummy method because ProcessLinux must
       declare alarm as a pure virtual function in order for the emu
       code to know that it can invoke it on the ref it has.  If you
       have a better idea, please fix this.  */
    passertMsg(0, "This should never be called!\n");

    /* We don't bother to generate a new error number since this code
       only exists to make the compiler happy.  */
    return _SERROR(1728, 0, ESRCH);
}

SysStatus
ProcessLinuxServer::getInfoLinuxPid(
    pid_t about, struct ProcessLinux::LinuxInfo& linuxInfo)
{
    if (about) {
      return _getInfoLinuxPid(about, linuxInfo, XHandle(0));
    }
    else {
      return _SERROR(1728, 0, ESRCH);
    }
}

SysStatus
ProcessLinuxServer::getInfoNativePid(
    ProcessID k42_pid, struct ProcessLinux::LinuxInfo& linuxInfo)
{
    if (k42_pid) {
      return _getInfoNativePid(k42_pid, linuxInfo);
    }
    else {
      return _SERROR(2068, 0, ESRCH);
    }
}

SysStatus
ProcessLinuxServer::_getInfoNativePid(__in ProcessID about,
		   __out struct ProcessLinux::LinuxInfo& linuxInfo)
{
    AutoLock<BLock> as(&taskLock);
    SysStatus rc;
    task_struct* current;
    rc = locked_nativePIDToTask(about, current);
    if (_FAILURE(rc)) return rc;
    return locked_getInfo(current, linuxInfo);
}

SysStatus
ProcessLinuxServer::_getResourceUsage(__in pid_t about,
		      __out struct BaseProcess::ResourceUsage& resourceUsage,
		      __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *current;

    if (about) {
	current = locked_find_task_by_pid(about);
    } else {
	current = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    }

    /* Return error if we are not a Linux process.  */
    if (!current) return _SERROR(1666, 0, ESRCH);
 
    /* The clustered object dereference gets us a ProcessWrapper object.  */
    return DREF(current->k42process)->getRUsage(resourceUsage);
}

SysStatus
ProcessLinuxServer::_getInfoLinuxPid(__in pid_t about,
			     __out struct LinuxInfo& linuxInfo,
			     __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *current;
    if (about) {
	current = locked_find_task_by_pid(about);
    } else {
	current = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    }
    //not a linux process
    if (!current) return _SERROR(1666, 0, ESRCH);
    return locked_getInfo(current, linuxInfo);
}

SysStatus
ProcessLinuxServer::locked_getInfo(task_struct *current,
				   struct LinuxInfo& linuxInfo)
{
    linuxInfo.pid = current->pid;
    linuxInfo.ppid = current->p_pptr->pid;
    linuxInfo.pgrp = current->p_pgrp->pid;
    linuxInfo.session = current->p_session->pid;
    if (current->p_session->controlling_tty_token != uval(-1)) {
	linuxInfo.tty = ttyStuff[current->p_session->controlling_tty_token]
	    .ttyData;
    } else {
	linuxInfo.tty = 0;
    }
    linuxInfo.creds = current->creds;
    return 0;
}

/*
 * FIXME MAA need credentials checks.
 * Special treatment of SIGCONT - it can be sent to any
 * process in same session regardless of creds
 */
SysStatus
ProcessLinuxServer::_kill(pid_t pid, sval sig, __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *target_task, *calling_task;
    uval doing_pg = 0;
    // this nonsense is to get error returns POSIX right
    // different return if we don't find any processes
    // than if we find one we don't have permissions to kill
    uval found_killee;
    uval found_killable_killee;
    calling_task = (task_struct*)(XHandleTrans::GetClientData(xhandle));

    if (sig<0 || sig>=_NSIG) {
	return _SERROR(2323, 0, EINVAL);
    }

    // because of a race, calling task is no longer a linux task
    if (!calling_task || !calling_task->k42process)
	return _SERROR(1681, 0, EINVAL);

    if (pid==-1) {
	return _SERROR(1679, 0, EINVAL);
    } else if (pid==0) {
	doing_pg = 1;
	target_task = calling_task->p_pgrp;
    } else if (pid<0) {
	doing_pg = 1;
	target_task = locked_find_task_by_pid(-pid, 1/*zombieOK*/);
	// verify its a process group leader
	if (!target_task || !target_task->pgid_use) {
	    return _SERROR(1680, 0, ESRCH);
	}
    } else {
	target_task = locked_find_task_by_pid(pid);
	if (!target_task)
	{
	    return _SERROR(1670, 0, ESRCH);
	}
    }

    // At this point we know we have permissions to kill
    if (sig == 0) {
        return 0;
    }

    locked_kill(target_task, sig, doing_pg, found_killee, found_killable_killee,
		calling_task);

    if (!found_killee) {
	return _SERROR(1682, 0, ESRCH);
    } else if (!found_killable_killee) {
	return _SERROR(1683, 0, EPERM);
    } else {
	return 0;
    }
}

SysStatus
ProcessLinuxServer::kill(pid_t pid, sval sig)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *target_task;
    uval doing_pg = 0;
    // this nonsense is to get error returns POSIX right
    // different return if we don't find any processes
    // than if we find one we don't have permissions to kill
    uval found_killee;
    uval found_killable_killee;

    if (sig<1 || sig>=_NSIG || pid == -1 || pid == 0) {
	return _SERROR(2399, 0, EINVAL);
    }

    if (pid<0) {
	doing_pg = 1;
	target_task = locked_find_task_by_pid(-pid, 1/*zombieOK*/);
	// verify its a process group leader
	if (!target_task || !target_task->pgid_use) {
	    return _SERROR(2400, 0, ESRCH);
	}
    } else {
	target_task = locked_find_task_by_pid(pid);
	if (!target_task)
	{
	    return _SERROR(2401, 0, ESRCH);
	}
    }

    locked_kill(target_task, sig, doing_pg, found_killee, found_killable_killee,
		0);

    if (!found_killee) {
	return _SERROR(2402, 0, ESRCH);
    } else if (!found_killable_killee) {
	return _SERROR(2403, 0, EPERM);
    } else {
	return 0;
    }
}

void
ProcessLinuxServer::locked_pgid_ref(task_struct* linux_task, sval updown)
{
    _ASSERT_HELD(taskLock);
    linux_task->pgid_use += updown;
    tassert((sval)(linux_task->pgid_use) >=0,
	    err_printf("ProcessLinuxServer oops\n"));
    if ((0==linux_task->pgid_use) &&
       (0==linux_task->p_pptr)) {
	// task has been reaped but was left around
	// as a place holder for the process group name
	locked_unhash_pid(linux_task);
	delete linux_task;
    }
}

/*setsid() creates a new session if the calling process is not a
 *process group leader.  The calling process is the leader of the new
 *session, the process group leader of the new process group, and has
 *no controlling tty.  The process group ID and session ID of the
 *calling process are set to the PID of the calling process.  The
 *calling process will be the only process in this new process group
 *and in this new session.
 */

SysStatus
ProcessLinuxServer::_setsid(__XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct* current;
    current = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    // is this process already a session or process group leader?
    if (current->pgid_use)
	return _SERROR(1672, 0, EPERM);
    locked_pgid_ref(current->p_pgrp, -1);
    locked_pgid_ref(current->p_session, -1);
    current->p_pgrp = current->p_session = current;
    current->pgid_use = 2;
    current->controlling_tty_token = uval(-1);
    return 0;
}
/*setpgid sets the process group ID of the process specified by pid
 *to pgid.  If pid is zero, the process ID of the current process is
 *used.  If pgid is zero, the process ID of the process specified by
 *pid is used.
 *
 */

SysStatus
ProcessLinuxServer::_setpgid(__in pid_t pid, __in pid_t pgid, uval did_exec,
			     __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *current, *calling_task, *pgid_task;
    calling_task = (task_struct*)(XHandleTrans::GetClientData(xhandle));

    /*
     * fast exec does not need to call ProcessLinuxServer, so we
     * don't learn that the process did an exec.  We don't need
     * to know unless a setpgid from another process tries to change
     * its pgid AFTER it has changed it itself.  So its good enough
     * to learn that a process has exec'd when it calls setpgid.
     */
    if (did_exec) calling_task->did_exec = 1;

    if (pid) {
	current = locked_find_task_by_pid(pid);
    } else {
	current = calling_task;
    }

    if (!current) return _SERROR(2747, 0, ESRCH);

    if (pgid) {
	// is it legal to use a process that's already terminated as a
	// process group leader - I think so!
	pgid_task = locked_find_task_by_pid(pgid, 1 /*zombieOK*/);
    } else {
	pgid_task = current;
    }

    if (!pgid_task) return _SERROR(1673, 0, EPERM);

#ifdef MAA
    err_printf("setpgid %lx %lx\n",
	       uval(current->pid), uval(pgid_task->pid));
#endif /* #ifdef MAA */
    //error checks
    //child did an exec
    if ((calling_task!=current)&& current->did_exec)
	return _SERROR(1674, 0, EACCES);

    if (
	// trying to change session leader
	(current->p_session == current) ||
	//task to be changed must be in callers session
	(calling_task->p_session != current->p_session) ||
	//pgid must be same as task being set or a valid
	//process group leader in the same session
	((current != pgid_task) &&
	 ((0==pgid_task->pgid_use) ||
	  (pgid_task->p_session != current->p_session))))
	return _SERROR(1675, 0, EPERM);

    //task to be changed caller or child of caller?
    if ((calling_task != current) && (calling_task != current->p_pptr))
	return _SERROR(1676, 0, ESRCH);

    locked_pgid_ref(current->p_pgrp, -1);
    current->p_pgrp = pgid_task;
    locked_pgid_ref(pgid_task, 1);
    return 0;
}

SysStatus
ProcessLinuxServer::_set_uids_gids(
    __in uval type,
    __in uid_t uid, __in uid_t euid, __in uid_t suid, __in uid_t fsuid,
    __in uid_t gid, __in uid_t egid, __in uid_t sgid, __in uid_t fsgid,
    __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    uval errno_value;
    task_struct *calling_task;
    calling_task = (task_struct*)(XHandleTrans::GetClientData(xhandle));

    if (!calling_task) return _SERROR(1726, 0, ESRCH);

    switch (type) {
    case SETUID:
	errno_value = calling_task->creds.sys_setuid(uid);
	break;
    case SETREUID:
	errno_value = calling_task->creds.sys_setreuid(uid, euid);
	break;
    case SETEUID:
      // cast -1 to avoid compiler warning
	errno_value = calling_task->creds.sys_setreuid((uid_t)(-1), euid);
	break;
    case SETRESUID:
	errno_value = calling_task->creds.sys_setresuid(uid, euid, suid);
	break;
    case SETFSUID:
	errno_value = calling_task->creds.sys_setfsuid(fsuid);
	break;
    case SETGID:
	errno_value = calling_task->creds.sys_setgid(gid);
	break;
    case SETREGID:
	errno_value = calling_task->creds.sys_setregid(gid, egid);
	break;
    case SETEGID:
      // cast -1 to avoid compiler warning
	errno_value = calling_task->creds.sys_setregid((gid_t)(-1), egid);
	break;
    case SETRESGID:
	errno_value = calling_task->creds.sys_setresgid(gid, egid, sgid);
	break;
    case SETFSGID:
	errno_value = calling_task->creds.sys_setfsgid(fsgid);
	break;
    default:
	errno_value = EINVAL;
    }
    // if we get a unix errno_value from creds, package it up as a SERROR
    return errno_value?_SERROR(1725, 0, errno_value):0;
}

SysStatus
ProcessLinuxServer::_insecure_setuidgid(__in uid_t euid, __in gid_t egid,
	__XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    uval errno_value=0;
    task_struct *calling_task;
    calling_task = (task_struct*)(XHandleTrans::GetClientData(xhandle));

    if (!calling_task) return _SERROR(1726, 0, ESRCH);
    if ((uid_t)-1 != euid)
	errno_value = calling_task->creds.insecure_sys_setreuid(euid);
    if ((gid_t)-1 != egid)
	errno_value = calling_task->creds.insecure_sys_setregid(egid);
    return errno_value?_SERROR(1725, 0, errno_value):0;
}

SysStatus
ProcessLinuxServer::getCredsPointerNativePid(
    ProcessID k42_pid, ProcessLinux::creds_t*& linuxCredsPtr)
{
    task_struct *linux_task;
    BaseProcessRef spref;
    uval data;
    pid_t linux_pid;
    SysStatus rc;

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(
	k42_pid, spref);
    if (_FAILURE(rc)) return rc;
    // can't fail, but may return garbage if never set
    // but if garbage, lookup or test below will catch it
    DREF(spref)->getOSData(data);
    linux_pid = pid_t(data);
    LinuxCredsHolder *linuxCredsHolder;
    linuxCredsHolder = new LinuxCredsHolder;
    if (!linuxCredsHolder) return _SERROR(2559, 0, ENOMEM);
    taskLock.acquire();			// minimize lock hold time
    linux_task = locked_find_task_by_pid(linux_pid);
    if (!linux_task || (linux_task->k42process != spref)) {
	taskLock.release();
	delete linuxCredsHolder;
	return _SERROR(2558, 0, ESRCH);
    }
    linuxCredsHolder->linuxCreds = linux_task->creds;
    taskLock.release();			// have to copy holding lock
    linuxCredsPtr = &(linuxCredsHolder->linuxCreds);
    return 0;
}

SysStatus
ProcessLinuxServer::releaseCredsPointer(
    ProcessLinux::creds_t* linuxCredsPtr)
{
    LinuxCredsHolder *linuxCredsHolder;
    linuxCredsHolder = (LinuxCredsHolder*) linuxCredsPtr;
    delete linuxCredsHolder;
    return 0;
}

SysStatus
ProcessLinuxServer::setTimeOfDay(uval sec, uval usec)
{
    PrivilegedServiceRef privServRef;

    privServRef = PrivilegedServiceWrapper::ThePrivilegedServiceRef();
    return DREF(privServRef)->setTimeOfDay(sec, usec);
}

SysStatus
ProcessLinuxServer::_setTimeOfDay(__in uval sec, __in uval usec,
				  __XHANDLE xhandle)
{
    AutoLock<BLock> as(&taskLock);
    task_struct *linux_task;
    linux_task = (task_struct*)(XHandleTrans::GetClientData(xhandle));
    if (!linux_task) {
	return _SERROR(1740, 0, ESRCH);
    }

    if (!linux_task->creds.capable(creds_t::CAP_SYS_TIME)) {
	return _SERROR(1743, 0, EPERM);
    }

    return setTimeOfDay(sec, usec);
}

/*
 * job control methods used by privileged tty servers
 */
/* virtual */ SysStatus
ProcessLinuxServer::_addTTY(uval ttyNum, uval ttyData) __xa(tty)
{
    AutoLock<BLock> as(&taskLock);
    tassertMsg(ttyNum < MAXTTYNUM,"Bad ttyNum: %ld\n",ttyNum);
    tassertMsg(ttyStuff[ttyNum].ttyData==0 || ttyData==0
	       || ttyData == ttyStuff[ttyNum].ttyData,
	       "Data mismatch: %lx %lx\n", ttyData, ttyStuff[ttyNum].ttyData);
    ttyStuff[ttyNum].ttyData = ttyData;
    return 0;
}

/* virtual */ SysStatus
ProcessLinuxServer::_removeTTY(uval ttyNum) __xa(tty)
{
    AutoLock<BLock> as(&taskLock);
    //FIXME MAA
    //kludge to skip tty freed by testKludge when init terminated
    //tests to see if it looks like its on the free list
//    if (ttyStuff[ttyToken].nextFree &&
//       ttyStuff[ttyToken].nextFree<ttyStuffSize) return 0;
    if (ttyStuff[ttyNum].session) {
	locked_pgid_ref(ttyStuff[ttyNum].session, -1);
	ttyStuff[ttyNum].session->controlling_tty_token = uval(-1);
	ttyStuff[ttyNum].session = 0;
    }
    if (ttyStuff[ttyNum].foreground) {
	locked_pgid_ref(ttyStuff[ttyNum].foreground, -1);
    }
    ttyStuff[ttyNum].init(0);
//    ReleaseAccess(ttyStuff[ttyToken].controllingTTY);
    return 0;
}

/*
 * on open of a tty, it may become the controlling tty of the
 * openning process
 * return 1 if that happens
 */
/* virtual */ SysStatus
ProcessLinuxServer::_setCtrlTTY(uval ttyToken, ProcessID processID) __xa(tty)
{
    AutoLock<BLock> as(&taskLock);
    return locked_setControllingTTY(ttyToken, processID);
}

SysStatus
ProcessLinuxServer::locked_setControllingTTY(
    uval ttyToken, ProcessID processID)
{
    SysStatus rc;
    task_struct* linux_task;
    rc = locked_nativePIDToTask(processID, linux_task);
    if (_FAILURE(rc)) return rc;
    // assume only called if noctty is NOT set
    // are we session leader?
    if ((linux_task->p_session == linux_task) &&
	// to we have a controlling tty
	(linux_task->controlling_tty_token == uval(-1)) &&
	// is this tty controlling
	(ttyStuff[ttyToken].session == 0)) {
	ttyStuff[ttyToken].session = linux_task;
	tassert(linux_task->p_pgrp == linux_task,
		err_printf("Session leader not also process group leader\n"));
	ttyStuff[ttyToken].foreground = linux_task;
	locked_pgid_ref(ttyStuff[ttyToken].foreground, 2);
	linux_task->controlling_tty_token = ttyToken;
	return 1;
    }
    return 0;
}

uval
ProcessLinuxServer::locked_killtree(
    task_struct* root_task, task_struct* pgrp_task, uval sig,
    uval& found_killee, uval& found_killable_killee,
    task_struct* calling_task,
    ListSimple<BaseProcessRef, AllocLocalStrict>& killList)
{
    _ASSERT_HELD(taskLock);
    task_struct *t_task, *k_task;
    uval pgcount;

    t_task = root_task;
    k_task = 0;
    pgcount = 0;
    do {
	// if pgrp_task is null, just kill root_task
	if (pgrp_task && t_task->p_cptr &&
	   t_task->p_cptr->markCount != markCount) {
	    //go down tree
	    t_task = t_task->p_cptr ;
	} else {
	    k_task = t_task;
	    // choose next sibling or parent in tree to process
	    if (k_task->p_osptr) {
		t_task = k_task->p_osptr;
	    } else {
		t_task = k_task->p_pptr;
	    }
	    // skip zombies
	    // if we are killing a specific task pgrp_task is 0
	    // and we only get here once.
	    if (k_task->k42process &&
	       (pgrp_task == 0 || k_task->p_pgrp == pgrp_task)) {
		found_killee = 1;
		//FIXME creds checking for rights
		// returns scattered through this code to make it easier
		// to verify that lock is released exactly once on all paths.
		// if calling_task is null just do it
		found_killable_killee = 1;
		if (sig == SIGKILL) {
		    killList.add(k_task->k42process);
		} else if (sig == SIGSTOP) {
		    err_printf("NYI - sending SIGSTOP to process 0x%lx.\n",
			       uval(k_task->pid));
		} else {
		    locked_signal(k_task, sig);
		}
		pgcount ++;
	    }
	    // mark as processed
	    k_task->markCount = markCount;
	}
    } while (k_task != root_task);
    return pgcount;
}

/*
 * kill is tricky, particularly SIGKILL, and process groups
 * we can't destroy a k42 process while holding the linux lock.
 * because we'd deadlock in removeExportedXobject (above).
 * but we need to process the whole process group in one go.
 * so we need to make a list of processes to destroy later.
 * we can't make the list in the task_struct because once
 * we release the lock the task_struct could go away.
 *
 */
void
ProcessLinuxServer::locked_kill(
    task_struct* pgrp_task, uval sig, uval doing_pg,
    uval& found_killee, uval& found_killable_killee,
    task_struct* calling_task)

{
    _ASSERT_HELD(taskLock);
    ListSimple<BaseProcessRef, AllocLocalStrict> killList;
    uval pgcount, targetcount;

    /*
     * loop is only a loop if we are doing a process group
     * but its easier to write this way than duplicating the core
     * kill logic in the middle
     */
    found_killee = found_killable_killee = 0;

    if (doing_pg) {
	/*
	 * This code is delicate.  The kills can modify the tree
	 * we are walking by moving subtrees to become children of
	 * init.  This all works out, given the way we do the moves.
	 * None of the tasks can actually disappear since no zombies can
	 * be harvested while we are holding the lock
	 *
	 * Optimized for the normal case that all the processes in the
	 * group are children of the leader.  less one because count
	 * includes the foreground tty reference.  note that this
	 * count is high for the session leader, but the optimization
	 * won't work for the session leader anyhow
	 */
	//targetcount >= actual number of tasks in processgroup
	targetcount = pgrp_task->pgid_use-1;
	markCount++;
	pgcount = locked_killtree(
	    pgrp_task, pgrp_task, sig, found_killee, found_killable_killee,
	    calling_task, killList);
	if (targetcount != pgcount) {
	    // must look at children of init
	    task_struct *root_task, *next_task;
	    root_task = locked_find_task_by_pid(1);
	    next_task = root_task->p_cptr;
	    while (next_task) {
		root_task = next_task;
		next_task = next_task->p_osptr;
		if (root_task->markCount != markCount) {
		    locked_killtree(
			root_task, pgrp_task, sig,
			found_killee, found_killable_killee,
			calling_task, killList);
		}
	    }
	}
    } else {
	locked_killtree(
	    pgrp_task, 0, sig, found_killee, found_killable_killee,
	    calling_task, killList);
    }
    if (!killList.isEmpty()) {
	taskLock.release();
	BaseProcessRef killee;
	while (killList.removeHead(killee)) {
	    DREF(killee)->kill();
	}
	taskLock.acquire();
    }
    return;
}

/*virtual*/ SysStatus
ProcessLinuxServer::printStatus()
{
    AutoLock<BLock> as(&taskLock);
    task_struct *p, **htable;
    uval i;
    err_printf("\nLinux Status\n");
    err_printf(
	"            pid     parent   proc_grp    session k42process\n");
    for (i=0;i<pidhash_sz;i++) {
	htable = &pidhash[i];
	for (p = *htable; p; p = p->pidhash_next) {
	    err_printf("     %10ld %10ld %10ld %10ld",
		       uval(p->pid),
		       p->p_pptr?uval(p->p_pptr->pid):0,
		       p->p_pgrp?uval(p->p_pgrp->pid):0,
		       p->p_session?uval(p->p_session->pid):0);
	    if (p->k42process) {
		err_printf(" 0x%lx\n", p->k42PID);
	    } else {
		err_printf(" zombie (was 0x%lx)\n", p->k42PID);
	    }
	}
    }
    return 0;
}

/*static*/ SysStatus
ProcessLinuxServer::_PrintStatus()
{
    return ((ProcessLinuxServer *)DREFGOBJ(TheProcessLinuxRef))->printStatus();
}

/*static*/ SysStatus
ProcessLinuxServer::_getLinuxVersion(char * buf, uval buflen)
{
    uval size = strlen(_LinuxVersion) + 1;
    if (size > buflen)
	size = buflen - 1;
    memcpy(buf, _LinuxVersion, size);
    buf[size] = '\0';
    return size;
}
