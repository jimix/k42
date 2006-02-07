/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: shmget.C,v 1.8 2004/06/14 20:32:56 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <stub/StubSysVSharedMem.H>
#define shmget __k42_linux_shmget
#include <sys/shm.h>
#undef shmget

/*
 * yes I know this size is a signed int, but it's how the interface is
 * defined
 */
extern "C" int
__k42_linux_shmget(key_t key, int size, int shmflg);

int
__k42_linux_shmget(key_t key, int size, int shmflg)
{
    int ret;
    SysStatus rc;

    SYSCALL_ENTER();

    // Round up
    size = PAGE_ROUND_UP(size);

    if (key == IPC_PRIVATE) {
	// This should eb done with an addapter pbject, so I'm not
	// going to do it at all yet.
	// ignore and fall thru
    } /* else  */ {
	rc = StubSysVSharedMem::ShmGet(key, size, shmflg);
	if (_FAILURE(rc)) {
	    ret = -_SGENCD(rc);
	} else {
	    ret = (int)_SGETUVAL(rc);
	}
    }
#if 0					// maa debugging
    err_printf("shmget pid:%lx %x %x %x ret= %x\n",
	       DREFGOBJ(TheProcessRef)->getPID(),
	       key, size, shmflg, ret);
#endif
    SYSCALL_EXIT();
    return ret;
}
