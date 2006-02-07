/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: exit.C,v 1.12 2004/06/14 20:32:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: exit(2) system call
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <sys/ProcessLinux.H>
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called exit and not __k42_linux_exit).
 */
#define exit __k42_linux_exit

#include <stdlib.h>

#include "FD.H"

void
exit(int status)
{
#undef exit
    SYSCALL_ENTER();

    DREFGOBJ(TheProcessLinuxRef)->exit(__W_EXITCODE(status,0));
    tassert(0,err_printf("Returned from %s?\n", __PRETTY_FUNCTION__));
    while (1);				// get rid of noreturn warning
}

// Must return ENOSYS
extern "C" int __k42_linux_exit_group(int status);
int
__k42_linux_exit_group(int status)
{
    return -ENOSYS;
}

