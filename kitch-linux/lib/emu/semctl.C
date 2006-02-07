/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: semctl.C,v 1.17 2005/05/02 18:21:17 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"

#define semctl __k42_linux_semctl
#include "stub/StubSysVSemaphores.H"
#include <sys/sem.h>

#include "ipc.H"

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
/* union semun is defined by including <sys/sem.h> */
/* arg for semctl system calls. */
#else
union semun {
	int val;			/* value for SETVAL */
	struct semid_ds *buf;		/* buffer for IPC_STAT & IPC_SET */
	unsigned short *array;		/* array for GETALL & SETALL */
	struct seminfo *__buf;		/* buffer for IPC_INFO */
	void *__pad;
};
#endif

int
semctl(int semid, int semnum, int cmd, ...)
{
#undef semctl
    int ret = 0;
    SysStatus rc = 0;
    union semun arg;
    va_list ap;


    SYSCALL_ENTER();

    // get the optional variables
    va_start (ap, cmd);
    arg = va_arg(ap, union semun);
    va_end(ap);

    switch (cmd) {
    case IPC_STAT|IPC_64:
    case IPC_SET|IPC_64:
	/* the current version of glibc makes two types of calls for
	   three commands: SEM_STAT, IPC_STAT and IPC_SET.  For these
	   we want to accept only those meant for the 64 bit linux
	   interface (otherwise presumably bad things happen when
	   we copy in and out from the parameter argument.  For
	   the other commands glibc makes only one type of call
	   without the IPC_64 flag */
	rc = StubSysVSemaphores::SemControlIPC(semid, cmd&~IPC_64, arg.buf);
	break;
    case IPC_RMID:
        rc = StubSysVSemaphores::SemControlRMID(semid);
        break;
    case SEM_STAT|IPC_64:
	rc = StubSysVSemaphores::SemControlInfo(semid, cmd&~IPC_64, arg.__buf);
	break;
    case IPC_INFO:
    case SEM_INFO:
	rc = StubSysVSemaphores::SemControlInfo(semid, cmd, arg.__buf);
	break;

    case SETVAL:
	rc = StubSysVSemaphores::SemControlSetVal(semid, semnum, arg.val);
	break;

    case GETVAL:
    case GETPID:
    case GETZCNT:
    case GETNCNT:
        rc = StubSysVSemaphores::SemControlGetVal(semid, cmd, semnum);
	ret = _SGETUVAL(rc);
        break;

    case GETALL:
    case SETALL:
        struct semid_ds buf;
        rc = StubSysVSemaphores::SemControlIPC(semid, IPC_STAT, &buf);
        if (_FAILURE(rc)) {
            err_printf("Error with %d on IPC_STAT\n", cmd);
            break;
        }

        {
          uval cnt = buf.sem_nsems;
          rc = StubSysVSemaphores::SemControlArray(semid, cmd, cnt, arg.array);
        }
	break;

    default:
	rc = _SERROR(2569, 0, EINVAL);
	break;
    }

    if (_FAILURE(rc)) {
        tassertWrn(0, "semctl() barfed rc is 0x%lx\n", rc);
	ret = -_SGENCD(rc);
    }

    SYSCALL_EXIT();
    return ret;
}

