/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lchown.C,v 1.11 2004/06/14 20:32:54 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  change ownership of a file, DOES NOT follow sym links.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include "linuxEmul.H"

#define lchown __k42_linux_lchown
#include <sys/types.h>
#include <unistd.h>


int
lchown(const char *path, uid_t owner, gid_t groupvoid)
{
#undef lchown
    SYSCALL_ENTER();
    // FIXME: we need a chown that doesn't follow the link!!
    SysStatus rc = FileLinux::Chown(path, owner, groupvoid);

    if (_FAILURE(rc)) {
	SYSCALL_EXIT();
	return -_SGENCD(rc);
    } else {
	SYSCALL_EXIT();
	return (rc);
    }
}

