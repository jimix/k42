/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: shmctl.C,v 1.9 2004/10/04 17:45:19 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"

#define shmctl __k42_linux_shmctl
#include "stub/StubSysVSharedMem.H"
#include <sys/shm.h>

#include "ipc.H"

int
shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
#undef shmctl
    int ret = 0;
    SysStatus rc;

    SYSCALL_ENTER();

    /* deal with old and new flavors of system calls.  Old flavors
       come with small buffers and should be rejected */
    if (cmd & IPC_64) {
    	cmd &= ~IPC_64;

	switch(cmd) {
	case IPC_STAT:
        case IPC_SET:
	    rc =  StubSysVSharedMem::ShmControl(shmid, cmd, *buf);
	    if (_FAILURE(rc)) {
	        ret = -_SGENCD(rc);
	    }
            break;

        case IPC_RMID:
        case SHM_LOCK:
        case SHM_UNLOCK:
            ret = -EINVAL;
	    goto out;
	    if (buf == NULL) {
		// punt
		// FIXME:  a null buf parameter is only acceptable for
		// some commands.
		struct shmid_ds tmp;
		rc =  StubSysVSharedMem::ShmControl(shmid, cmd, tmp);
	    } else {
		rc =  StubSysVSharedMem::ShmControl(shmid, cmd, *buf);
	    }
	    if (_FAILURE(rc)) {
	        ret = -_SGENCD(rc);
	    }
            break;
        default:
            ret = -EINVAL;
            break;
	}
	
    } else {	
	// it's the old flavor which we do not handle
	ret = -EINVAL;
    }

out:
    SYSCALL_EXIT();
    return ret;
}
