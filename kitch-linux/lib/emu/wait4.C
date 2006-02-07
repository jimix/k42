/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: wait4.C,v 1.18 2004/07/11 21:59:17 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating wait4()
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/BaseProcess.H>
#include <sys/ProcessLinux.H>
#include <sync/BlockedThreadQueues.H>
#include "linuxEmul.H"

#include <sys/types.h>
#include <sys/resource.h>
#include <stdlib.h>
#define wait4 __k42_linux_wait4
#define waitpid __k42_linux_waitpid
#include <sys/wait.h>
#undef waitpid

pid_t
wait4(pid_t pid, void *stat_loc, int options, struct rusage *rusage)
{
#undef wait4
    int *status = (int *)stat_loc;
    sval statback;
    SysStatus rc;

    SYSCALL_ENTER();


    if (options&WNOHANG) {
	rc = DREFGOBJ(TheProcessLinuxRef)->waitpid(pid, statback, options);
    } else {
	// we must deal with blocking here, rather than in ProcessLinuxClient,
	// because we must use SYSCALL_BLOCK()
	BlockedThreadQueues::Element qe;
	pid_t pid_in = pid;

	DREFGOBJ(TheBlockedThreadQueuesRef)->
		addCurThreadToQueue(&qe, GOBJ(TheProcessLinuxRef));
	while (1) {
	    rc = DREFGOBJ(TheProcessLinuxRef)->waitpid(pid, statback, options);
	    if (_FAILURE(rc)||pid) break;
	    pid = pid_in;	// restore for next call
	    SYSCALL_BLOCK();


	    if (SYSCALL_SIGNALS_PENDING()) {
	    // Check for success before checking for EINTR
		rc = DREFGOBJ(TheProcessLinuxRef)->waitpid(pid,
							   statback, options);
		if (_SUCCESS(rc) && pid) break;
		// need to do removeCur.. on the way out, so don't just return
		rc = _SERROR(2570, 0, EINTR); // see _set_errno below
		pid = (pid_t)-1;
		break;
	    }
	}
	DREFGOBJ(TheBlockedThreadQueuesRef)->
		removeCurThreadFromQueue(&qe, GOBJ(TheProcessLinuxRef));
    }
    if (_FAILURE(rc)) {
	TraceOSUserSyscallInfo(
		     (uval64)Scheduler::GetCurThreadPtr(), rc, options, NULL);
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    if (status) {
	*status = (int)statback;
    }

    TraceOSUserSyscallInfo(
		 (uval64)Scheduler::GetCurThreadPtr(), pid, options, NULL);

    SYSCALL_EXIT();

    return pid;
}

#if not_yet
    if (pid < -1) {
	// wait for any child process whose process group ID is equal
	// to the absolute value of pid.
    } else if (pid == -1) {
	// wait for any child process.
    } else if (pid == 0) {
	// wait for any child process whose process group ID is equal
	// to that of the calling process.
    } else { // pid > 0
	// wait for the child whose process ID is equal to the value of pid.
    }
#endif

extern "C" pid_t
__k42_linux_waitpid(pid_t pid, int *stat_loc, int options);

pid_t
__k42_linux_waitpid(pid_t pid, int *stat_loc, int options)
{
    return __k42_linux_wait4(pid, stat_loc, options, NULL);
}

void put_rusage(struct rusage32* to32, struct rusage* from64);

extern "C" pid_t
__k42_linux_wait4_32(pid_t pid, void *stat_loc, int options,
                     struct rusage32 *usage32)
{
    struct rusage usage;
    SysStatus rc;
    rc = __k42_linux_wait4(pid, stat_loc, options, usage32?&usage:0);
    if(usage32) put_rusage(usage32, &usage);
    return rc;
}
