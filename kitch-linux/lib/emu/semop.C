/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: semop.C,v 1.10 2005/05/02 20:10:15 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
extern "C"{
#include <sys/sem.h>
}
#include "stub/StubSysVSemaphores.H"

extern "C" int
__k42_linux_semop(int semid, struct sembuf *sops,  unsigned int nsops);

int
__k42_linux_semop(int semid, struct sembuf *sops,  unsigned int nsops)
{
    #if 0
    err_printf("%s pid:0x%lx: %d %d %d %x\n", __func__,
		DREFGOBJ(TheProcessRef)->getPID(),
		semid, sops->sem_num,
		sops->sem_op, sops->sem_flg);
    #endif

    int ret = 0;
    SysStatus rc;

    SYSCALL_ENTER();

    rc = StubSysVSemaphores::SemOperation(semid, sops, nsops);

    if (_FAILURE(rc)) {
	//err_printf("%s: ", __func__); _SERROR_EPRINT(rc);
	ret = -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return ret;
}
