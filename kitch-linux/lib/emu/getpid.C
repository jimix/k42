/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getpid.C,v 1.11 2004/06/14 20:32:54 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: the various get calls
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */

#define getpid __k42_linux_getpid
#define getppid __k42_linux_getppid
#define getpgid __k42_linux_getpgid
#define getpgrp __k42_linux_getpgrp
#define setsid __k42_linux_setsid
#define setpgid __k42_linux_setpgid
#include <unistd.h>
#undef getpid
#undef getppid
#undef getpgid
#undef getpgrp
#undef setsid
#undef setpgid

#include <sys/ProcessLinux.H>

pid_t
__k42_linux_getpid(void)
{
    pid_t pid;
    SYSCALL_ENTER();
    DREFGOBJ(TheProcessLinuxRef)->getpid(pid);
    SYSCALL_EXIT();
    return pid;
}

pid_t
__k42_linux_getppid(void)
{
    pid_t pid;
    SYSCALL_ENTER();
    DREFGOBJ(TheProcessLinuxRef)->getppid(pid);
    SYSCALL_EXIT();
    return pid;
}

pid_t
__k42_linux_getpgid(pid_t pid)
{
    SysStatus rc;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getpgid(pid);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }
    return pid;
}


pid_t
__k42_linux_getpgrp(void)
{
    SysStatus rc;
    pid_t pid;
    pid = 0;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->getpgid(pid);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }

    return pid;
}

int
__k42_linux_setsid(void)
{
    SysStatus rc;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->setsid();
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }
    return 0;
}

int
__k42_linux_setpgid(pid_t pid, pid_t pgid)
{
    SysStatus rc;
    SYSCALL_ENTER();
    rc = DREFGOBJ(TheProcessLinuxRef)->setpgid(pid, pgid);
    SYSCALL_EXIT();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }
    return 0;
}





