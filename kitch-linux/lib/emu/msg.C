/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: msg.C,v 1.3 2004/06/16 19:46:43 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sync/BlockedThreadQueues.H>
#include "linuxEmul.H"
#include "stub/StubSysVMessages.H"
#include "io/SysVMessagesClient.H"
#include <sys/ipc.h>
#include <sys/msg.h>
extern "C" int
__k42_linux_msgsnd(int msqid, struct msgbuf *msgp, int msgsz, int msgflg);

int
__k42_linux_msgsnd(int msqid, struct msgbuf *msgp, int msgsz, int msgflg)
{
    int ret;
    SysStatus rc;
    ObjectHandle oh;
    void *blockKey;
    BlockedThreadQueues::Element qe;

    SYSCALL_ENTER();

    if (msgflg&IPC_NOWAIT) {
	oh.init();
    } else {
	SysVMessagesClient::GetCallBackOH(oh, blockKey);
	DREFGOBJ(TheBlockedThreadQueuesRef)->
	    addCurThreadToQueue(&qe, blockKey);
    }
	
    while (1) {
	rc = StubSysVMessages::Msgsnd(msqid, msgp->mtype, msgp->mtext,
				      msgsz, msgflg, oh);
	if (_FAILURE(rc)) {
	    ret = -_SGENCD(rc);
	} else {
	    ret = (int)_SGETUVAL(rc);
	}
	if ((ret == -EAGAIN) & !(msgflg&IPC_NOWAIT)) {
	    SYSCALL_BLOCK();
	    if (SYSCALL_SIGNALS_PENDING()) {
		// need to do removeCur.. on the way out, so don't just return
		ret = -EINTR;
		break;
	    }
	} else break;
    }

    if(!(msgflg&IPC_NOWAIT)) {
	DREFGOBJ(TheBlockedThreadQueuesRef)->
	    removeCurThreadFromQueue(&qe, blockKey);
    }
       
    SYSCALL_EXIT();
    return ret;
}

extern "C" int
__k42_linux_msgrcv(int msqid, struct msgbuf *msgp,
                   int msgsz, sval msgtyp, int msgflg);

int
__k42_linux_msgrcv(int msqid, struct msgbuf *msgp,
                   int msgsz, sval msgtyp, int msgflg)
{
    int ret;
    SysStatus rc;
    ObjectHandle oh;
    void *blockKey;
    BlockedThreadQueues::Element qe;

    SYSCALL_ENTER();

    if (msgflg&IPC_NOWAIT) {
	oh.init();
    } else {
	SysVMessagesClient::GetCallBackOH(oh, blockKey);
	DREFGOBJ(TheBlockedThreadQueuesRef)->
	    addCurThreadToQueue(&qe, blockKey);
    }
	
    while (1) {
	rc = StubSysVMessages::Msgrcv(msqid, msgtyp, msgp->mtext,
				      msgsz, msgflg, oh);

	if (_FAILURE(rc)) {
	    ret = -_SGENCD(rc);
	} else {
	    ret = (int)_SGETUVAL(rc);
	}
	if ((ret == -ENOMSG) & !(msgflg&IPC_NOWAIT)) {
	    SYSCALL_BLOCK();
	    if (SYSCALL_SIGNALS_PENDING()) {
		// need to do removeCur.. on the way out, so don't just return
		ret = -EINTR;
		break;
	    }
	} else break;
    }

    if(!(msgflg&IPC_NOWAIT)) {
	DREFGOBJ(TheBlockedThreadQueuesRef)->
	    removeCurThreadFromQueue(&qe, blockKey);
    }

    SYSCALL_EXIT();
    return ret;
}


extern "C" int
__k42_linux_msgget(key_t key, int msgflg);

int
__k42_linux_msgget(key_t key, int msgflg)
{
    int ret;
    SysStatus rc;

    SYSCALL_ENTER();

    rc = StubSysVMessages::Msgget(key, msgflg);
    if (_FAILURE(rc)) {
	ret = -_SGENCD(rc);
    } else {
	ret = (int)_SGETUVAL(rc);
    }

    SYSCALL_EXIT();
    return ret;
}


extern "C" int
__k42_linux_msgctl(int msqid, int cmd, struct msqid_ds *buf);

int
__k42_linux_msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
    int ret;
    SysStatus rc;
    struct msqid_ds dummy;

    if(!buf) buf = &dummy;		// ppc always copies the contents
    
    SYSCALL_ENTER();

    rc = StubSysVMessages::Msgctl(msqid, cmd, buf);
    if (_FAILURE(rc)) {
	ret = -_SGENCD(rc);
    } else {
	ret = (int)_SGETUVAL(rc);
    }

    SYSCALL_EXIT();
    return ret;
}

