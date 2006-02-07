/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: pipe.C,v 1.9 2004/07/01 21:14:21 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Create a pipe.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


int
__k42_linux_pipe (int fd[2])
{
#if 0
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, ENOSYS));
#else
    SYSCALL_ENTER();

    SysStatus rc;
    FileLinuxRef newFileRefR, newFileRefW;

    rc = FileLinux::Pipe(newFileRefR, newFileRefW);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    fd[0] = _FD::AllocFD(newFileRefR);
    fd[1] = _FD::AllocFD(newFileRefW);

    SYSCALL_EXIT();
    return  _SRETUVAL(rc);
#endif
}
