/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: open.C,v 1.22 2004/08/18 13:51:26 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: open and possibly create a file or device
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
__k42_linux_open (const char *filename, int oflag, mode_t mode)
{
    SYSCALL_ENTER();

    TraceOSUserSyscallInfo((uval64)Scheduler::GetCurThreadPtr(),
		 oflag,mode,filename);

    int newfd;
    SysStatus rc;

    FileLinuxRef flr;
    rc = FileLinux::Create(flr, filename, oflag, mode);

    #undef TRACE_PROCFS
    #ifdef TRACE_PROCFS
    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	tassertWrn(strncmp(filename, "/proc", 5) != 0,
		   "Trying to open /proc (%s) rc=%lx\n", filename, rc);
    }
    #endif

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }

    newfd = _FD::AllocFD(flr);
    if (newfd == -1) {
	rc = DREF(flr)->detach();
	tassert(_SUCCESS(rc),
		err_printf("detach() of new FileLinuxRef failed\n"));
	newfd = -EMFILE;
    }
    SYSCALL_EXIT();
    return (newfd);
}

extern "C" int __k42_linux_creat (const char *filename, mode_t mode);
int
__k42_linux_creat (const char *filename, mode_t mode)
{
    return __k42_linux_open(filename, O_CREAT|O_WRONLY|O_TRUNC, mode);
}
