/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: sync.C,v 1.2 2004/06/14 20:32:57 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
/*
 * This is a kludge to get the correct prototype from the unix
 * include file, and mangle it for K42 (as an extern C).  This will go away
 * when we are a true glibc target (i.e., our implementation of fork
 * is called fork and not __k42_linux_fork).
 */
#define sync __k42_linux_sync
#include <unistd.h>
#undef sync // Must not interfere with the method below

#include "FD.H"
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>

#define sync __k42_linux_sync
void
sync()
{
    SysStatus rc;

    SYSCALL_ENTER();

#undef sync	// Must not interfere with the method below

    rc = FileLinux::Sync();

    tassertWrn(_SUCCESS(rc), "sync sys call failed with rc 0x%lx\n", rc);

    SYSCALL_EXIT();
}
