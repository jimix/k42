/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: shmdt.C,v 1.7 2004/06/14 20:32:56 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"

#define shmdt __k42_linux_shmdt
#include <sys/shm.h>

int
shmdt(const void *shmaddr)
{
#undef shmdt
    int ret = 0;
    SysStatus rc;

    SYSCALL_ENTER();

    // Register with the fork handler
    rc = DREFGOBJ(TheProcessRef)->regionDestroy((uval)shmaddr);

    if (_FAILURE(rc)) {
	ret = -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return ret;
}
