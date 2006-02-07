/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linuxSpawn.C,v 1.32 2004/07/08 17:15:28 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: emulate a for followed by an exec
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"
#include "FD.H"

SysStatus
spawn_common(ProgExec::ArgDesc *args, int wait);

int
__k42_linux_spawn (const char *filename,
                   char *const argv[], char *const envp[],
                   int wait)
{
    SYSCALL_ENTER();
    SysStatus rc;
    ProgExec::ArgDesc* args;
    rc = ProgExec::ArgDesc::Create(filename, argv, envp, args);

    if(_SUCCESS(rc)) {
	rc = spawn_common(args, wait);
	//Normally does not return
    } else {
	rc =  -_SGENCD(rc);
    }
    args->destroy();
    SYSCALL_EXIT();
    return rc;
}

SysStatus
__k42_linux_spawn_32 (const char *filename,
                      char *const argv[], char *const envp[],
                      int wait)
{
    SYSCALL_ENTER();
    SysStatus rc;
    ProgExec::ArgDesc* args;
    rc = ProgExec::ArgDesc::Create32(filename, argv, envp, args);

    if(_SUCCESS(rc)) {
	rc = spawn_common(args, wait);
	//Normally does not return
    } else {
	rc =  -_SGENCD(rc);
    }
    args->destroy();
    SYSCALL_EXIT();
    return rc;
}

SysStatus
spawn_common(ProgExec::ArgDesc *args, int wait)
{
    SysStatus rc;
    FileLinux::Stat statBuf;

    TraceOSUserSyscallInfo((uval64)Scheduler::GetCurThreadPtr(),
		 wait,0,args->getFileName());

    rc = FileLinux::GetStatus(args->getFileName(), &statBuf);
    
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }

    if (! S_ISREG(statBuf.st_mode)) {
	return -ENOEXEC;
    }

    // FIXME check permissions better than this
    if (! (statBuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))) {
	return -EACCES;
    }


    // We could keep a few generation counts (of fd.allocs/frees) to
    // decide whether to reuse memory or even contents.

    rc = _FD::ExecAll();
    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }

    if (_SUCCESS(rc)) rc = runExecutable(args, wait);

    if (_FAILURE(rc)) {
	return -_SGENCD(rc);
    }
    return 0;
}
