/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: chdir.C,v 1.14 2005/05/17 20:09:21 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: chdir
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define chdir __k42_linux_chdir
#include <unistd.h>

int
chdir(const char *path)
{
    SYSCALL_ENTER();

#undef chdir  // Must not interfere with the method below

    SysStatus rc = FileLinux::Chdir(path);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return (-_SGENCD(rc));
    }

    SYSCALL_EXIT();
    return (0);
}
