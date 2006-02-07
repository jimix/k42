/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ipc.C,v 1.8 2004/10/01 18:33:22 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for ipc() syscall --- gate to real syscall impl.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"

#define semget __k42_linux_semget
#define semop  __k42_linux_semop
#define semctl  __k42_linux_semctl
#define msgsnd  __k42_linux_msgsnd
#define msgrcv  __k42_linux_msgrcv
#define msgget  __k42_linux_msgget
#define msgctl  __k42_linux_msgctl
#define shmdt  __k42_linux_shmdt
#define shmget  __k42_linux_shmget
#define shmctl  __k42_linux_shmctl

#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>

typedef uval32 u32;
typedef sval32 s32;
#include <asm/ipc.h>

#undef IPC_DEBUG

extern  int shmatByIPC(int shmid, const void *shmaddr, int shmflg,
		       void**addr);

extern "C" int 
__k42_linux_ipc (uint call, int first, int second, long third, 
                 void *ptr, long fifth);

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


//
// This code is hacked from Linux arch/ppc64/kernel/syscalls.c
//


int 
__k42_linux_ipc (uint call, int first, int second, long third, 
                 void *ptr, long fifth)
{
    
    int version, ret;
    version = call >> 16; /* hack for backward compatibility */
    call &= 0xffff;

    ret = -EINVAL;

    #ifdef IPC_DEBUG
    pid_t pid;
    DREFGOBJ(TheProcessLinuxRef)->getpid(pid);
    #endif

    switch (call) {
    case SEMOP:
	#ifdef IPC_DEBUG
	err_printf("%s: 0x%x  first/semid=%d  second/nsops=%d  ptr=%p\n",
		"SEMOP",
		pid, first, second, ptr);
	#endif
	ret = semop (first, (struct sembuf *)ptr, second);
	break;
    case SEMGET:
	#ifdef IPC_DEBUG
	err_printf("%s: 0x%x  first/key=%d  second/num=%d  "
		   "third/flags=0x%lx\n",
		"SEMGET",
		pid, first, second, third);
	#endif
	ret = semget (first, second, third);
	break;
    case SEMCTL: {
	#ifdef IPC_DEBUG
	err_printf("%s: 0x%x  first/semid=%d  second/semnum=%d  "
		   "third/cmd=%lx  ptr=%p\n",
		"SEMCTL",
		pid, first, second, third, ptr);
	#endif
	union semun fourth;

	if (!ptr)
	    break;

	fourth.__pad = *(void**)ptr;

	ret = semctl (first, second, third, fourth);
	break;
    }
    case MSGSND:
	ret = msgsnd (first, (struct msgbuf *) ptr, second, third);
	break;
    case MSGRCV:
	switch (version) {
	case 0: {
	    struct ipc_kludge tmp;

	    if (!ptr)
		break;
	    memcpy(&tmp, (struct ipc_kludge*)ptr, sizeof(tmp));

	    ret = msgrcv (first, (struct msgbuf *)(unsigned long)tmp.msgp,
			  second, tmp.msgtyp, third);
	    break;
	}
	default:
	    ret = msgrcv (first, (struct msgbuf *) ptr,
			  second, fifth, third);
	    break;
	}
	break;
    case MSGGET:
	ret = msgget ((key_t) first, second);
	break;
    case MSGCTL:
	ret = msgctl (first, second, (struct msqid_ds *) ptr);
	break;
    case SHMAT:
	switch (version) {
	default: {
	    void* raddr;

	    if(!third){
		ret = -EINVAL;
		break;
	    }

	    ret = shmatByIPC (first, (const void *) ptr, second, &raddr);
	    if (ret)
		break;
	    *(void **)third = raddr;
	    break;
	}
	case 1:	/* iBCS2 emulator entry point */
	    passertMsg(0,"Can't deal with shmat!!!!\n");
	    //if (!segment_eq(get_fs(), get_ds()))
	    //	break;
	    //ret = sys_shmat (first, (char *) ptr, second,
	    //		     (ulong *) third);
	    break;
	}
	break;
    case SHMDT: 
	ret = shmdt ((char *)ptr);
	break;
    case SHMGET:
	ret = shmget (first, second, third);
	break;
    case SHMCTL:
	#ifdef IPC_DEBUG
	err_printf("%s: 0x%x  first/shmid=%d  second/cmd=0x%x  ptr=%p\n",
		"SHMCTL",
		pid, first, second, ptr);
	#endif
	ret = shmctl (first, second, (struct shmid_ds *) ptr);
	break;
    }

    #ifdef IPC_DEBUG
    err_printf("%s %d: %x  rc=%d\n", "IPCret", call, pid, ret);
    #endif
    return ret;
}

/*
 * from linux arch/ppc64/kernel/sys_ppc32.c
 */
extern "C" sval32
__k42_linux_ipc_32 (uint call, int first, int second, long third, 
		    void *ptr, long fifth)
{
    switch (call & 0xffff) {
#if 0
    case SEMTIMEDOP:
#endif
    case SEMCTL:
	/* sign extend semid, semnum */
	first = SIGN_EXT(first);
	second = SIGN_EXT(second);
	#ifdef IPC_DEBUG
	err_printf("%s: first/semid=%d  second/semnum=%d  "
		   "third/cmd=%lx  ptr=%p\n",
		"SEMCTL",
		first, second, third, ptr);
	#endif
	union semun fourth;

	if (!ptr)
	    break;

	fourth.__pad = compat_ptr(*(compat_uptr_t*)ptr);

	return semctl (first, second, third, fourth);
	break;
    case SEMOP: 
	/* struct sembuf is the same on 32 and 64bit :)) */
	/* sign extend semid */
	first = SIGN_EXT(first);
	break;
    case SEMGET:
	/* sign extend key, nsems */
    case MSGSND:
	/* sign extend msqid */ /*SIC*/
	first = SIGN_EXT(first);
	second = SIGN_EXT(second);
	break;
    case MSGRCV:
	/* sign extend msqid, msgtyp */
	first = SIGN_EXT(first);
	fifth = SIGN_EXT(fifth);
	break;
    case MSGGET:
	/* sign extend key */
    case MSGCTL:
	/* sign extend msqid */
    case SHMAT:
	/* sign extend shmid */
	first = SIGN_EXT(first);
	break;
    case SHMDT:
	/* no conversions */
	break;
    case SHMGET:
	/* sign extend key_t */
    case SHMCTL:
	/* sign extend shmid */
	first = SIGN_EXT(first);
	break;
    default:
	break;
    }

    return __k42_linux_ipc (call, first,  second,  third,  ptr,  fifth);
}
