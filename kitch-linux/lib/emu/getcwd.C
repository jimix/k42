/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: getcwd.C,v 1.20 2004/06/14 20:32:53 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Get current working directory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <unistd.h>

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

extern "C" int __k42_linux_getcwd(char *buf, uval buflen);

int
__k42_linux_getcwd(char *buf, uval buflen)
{
    SYSCALL_ENTER();

#undef getcwd	// Must not interfere with the method below

    SysStatus rc;
    char pathBuf[PATH_MAX+1];
    PathNameDynamic<AllocGlobal> *cwd;
    uval cwdlen;

    cwd = (PathNameDynamic<AllocGlobal>*) pathBuf;

    rc = FileLinux::Getcwd(cwd, sizeof(pathBuf));
    if (_FAILURE(rc)) goto failure;

    cwdlen = _SGETUVAL(rc);

    rc = cwd->getUPath(cwdlen, buf, buflen-1);
    if (_FAILURE(rc)) goto failure;

    SYSCALL_EXIT();
    // Must return size of string returned, including trailing \0
    return strnlen(buf,buflen-1)+1;

failure:
    SYSCALL_EXIT();
    return -_SGENCD(rc);
}
