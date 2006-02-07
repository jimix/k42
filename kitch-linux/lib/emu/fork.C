/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fork.C,v 1.15 2005/08/11 20:20:42 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: fork(2) system call
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <sys/ProcessLinux.H>
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define fork __k42_linux_fork

#include <unistd.h>

#include <usr/ProgExec.H>
#include "FD.H"

pid_t
fork(void)
{
#undef fork
    SysStatus rc;
    pid_t childLinuxPID;

    SYSCALL_ENTER();

    rc = _FD::ForkAll();

    rc = ProgExec::ForkProcess(childLinuxPID);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    SYSCALL_EXIT();

    return childLinuxPID;
}
