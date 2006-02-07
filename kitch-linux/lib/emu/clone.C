/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: clone.C,v 1.12 2004/06/28 17:01:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: the clone call
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <sys/ProcessLinux.H>
#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

int
__k42_linux_clone(int flags, void *child_stack,
		  void *parent_tid, void *tls, void *child_tid)
{
    SysStatus rc;
    pid_t childLinuxPID;

    if((NULL == child_stack)) {
	/*
	 * For reasons I will not try to understand, Linux has chosen to
	 * implement fork() as a call on sys_clone (the clone system call)
	 * with stylized arguments.  Our clone implementation does not
	 * follow Linux - we do not create a new process/address space for each
	 * clone, rather we use our user mode thread schedular to create
	 * clone threads.
	 * But fork is different - we really need a new
	 * K42 process/address space.  So we detect the idiom here and
	 * fork().
	 */
	passertMsg((CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD) == flags,
		   "Special case of clone to implement fork used unexpected"
		   " flags %x\n", flags);
	pid_t __k42_linux_fork(void);
	pid_t retval;
	retval = __k42_linux_fork();
	if(retval >= 0) {
	    *(pid_t*)child_tid = retval;
	}
	return retval;
    }

    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->clone(childLinuxPID, flags, child_stack,
					     parent_tid, tls, child_tid);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    //This prevents us from doing a "fast exec".
    useLongExec = 1;
    SYSCALL_EXIT();
    return childLinuxPID;
}

#ifdef __cplusplus
}
#endif
