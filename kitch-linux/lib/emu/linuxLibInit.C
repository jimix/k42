/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: linuxLibInit.C,v 1.52 2004/07/08 17:15:28 gktse Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for user-level initialization
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <trace/traceUser.h>
#include <sys/ProcessSetUser.H>
#include <usr/ProgExec.H>
#include "linuxEmul.H"
#include "FD.H"
#include <stdlib.h>
#include <elf.h>
#include <asm/unistd.h>   //Syscall numbers
#include <trace/traceUser.h>

extern "C" int __k42_linux_badsyscall();
int __k42_linux_badsyscall()
{
    tassertMsg(0, "System call not implemented!\n");
    return 0;
}

extern "C" {

    extern void __k42_linux_exit (int status);
    extern void *__libc_stack_end;
    extern int _dl_starting_up;
    extern void __pthread_initialize_minimal(void) __attribute__((weak));
// FIXME: Get rid of this when the call below is removed.
    extern void __pthread_initialize(void) __attribute__((weak));

}

struct init_args {
    uval argc;
    char* argv[];
};

extern SysStatus initEntryPoints();
SysStatus
LinuxPreExec()
{
    // We've got a valid executable to use.
    // point of no return, nobody can pick up return code
    SysStatus rc = DREFGOBJ(TheProcessLinuxRef)->internalExec();
    tassertMsg(_SUCCESS(rc), "ProcessLinux internalExec(): %lx\n",rc);
    _IF_FAILURE_RET(rc);

    rc = DREFGOBJ(TheProcessRef)->preExec();
    tassertMsg(_SUCCESS(rc), "RegionForkManager::preExec(): %lx\n",rc);

    _IF_FAILURE_RET(rc);

    extern void resetBrk();
    resetBrk();

    _FD::FileTable->closeOnExec();
    return rc;
}

SysStatus
LinuxLibInit()
{
    initEntryPoints();

    LinuxFileInit(ProgExec::ExecXferInfo->iofmSize,
		  ProgExec::ExecXferInfo->iofmBufPtr);
    DREFGOBJ(TheProcessLinuxRef)->registerExitHook(__k42_linux_fileFini);

    TraceOSUserEnterProc();

    return 0;
}


