/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mkdir.C,v 1.13 2004/06/14 20:32:55 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: mkdir() - create a directory
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define mkdir __k42_linux_mkdir
int
mkdir(const char *pathname, mode_t mode)
{
#undef mkdir
    SYSCALL_ENTER();
    SysStatus rc = FileLinux::Mkdir(pathname, mode);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (0);
    }
}
