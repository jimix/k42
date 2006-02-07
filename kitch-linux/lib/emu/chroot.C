/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: chroot.C,v 1.10 2005/03/04 00:43:57 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Change Root directory for this process
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"

#define chroot __k42_linux_chroot
#include <unistd.h>

int
__k42_linux_chroot (const char *path)
{
    SYSCALL_ENTER();
    SYSCALL_EXIT();
    return 0;
    /*
    return (__k42_linux_emulNoSupport (__PRETTY_FUNCTION__, -1));
    */
}
