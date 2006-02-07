/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: semget.C,v 1.9 2004/10/01 18:33:22 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#define semget __k42_linux_semget
#include "stub/StubSysVSemaphores.H"
#include <sys/sem.h>
#include <sys/stat.h>

/*
 * yes I know this size is a signed int, but it's how the interface is
 * defined
 */

int
semget(key_t key, int num, int semflg)
{
#undef semget

    int ret;
    SysStatus rc;

    SYSCALL_ENTER();

#if 0
    if (!(semflg & (S_IWUSR | S_IRUSR))) {
        tassertWrn(0,
             "Semaphore perms are neither read or write.\n");
        semflg |= S_IWUSR;
    }
#endif

    rc = StubSysVSemaphores::SemGet(key, num, semflg);
    if (_FAILURE(rc)) {
	ret = -_SGENCD(rc);
    } else {
	ret = (int)_SGETUVAL(rc);
    }

    SYSCALL_EXIT();
    return ret;
}
