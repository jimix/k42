/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: readlink.C,v 1.14 2005/08/02 17:59:43 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: readlink - read value of a symbolic link
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <io/FileLinux.H>

#define readlink __k42_linux_readlink
#include <unistd.h>

// ugly hack to make /proc/self/exe work for now. J9 needs this
char __readlink_proc_self_exe_hack[128];

int
readlink(const char *path, char *buf, size_t bufsize)
{
#undef readlink
    SysStatus rc;
    SYSCALL_ENTER();
    
    if (bufsize > 3000) {
	//tassertWrn(0, "trimming buffer to readlink from %ld to 3000\n", 
	//	   bufsize);
	bufsize = 3000;
    }

    // FIXME, FIXME, FIXME!!! Once we have a real /proc file system, this goes
    // away. For now this is just a hack to let J9 run.
    if (!strcmp(path, "/proc/self/exe")) {
	strncpy(buf, __readlink_proc_self_exe_hack, bufsize);
	return strlen(__readlink_proc_self_exe_hack);
    }

    rc = FileLinux::Readlink(path, buf, bufsize);
    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    }
    SYSCALL_EXIT();
    return _SGETUVAL(rc);
}
