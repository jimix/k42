/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVMessages.C,v 1.4 2004/09/14 21:16:34 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Sys V Messages
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>

#include <sync/atomic.h>
#include "meta/MetaSysVMessages.H"
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sync/Sem.H>
#include "SysVMessages.H"
#include "stub/StubSysVMessagesClient.H"

/* static */ SysVMessages *SysVMessages::obj;

/* static */ void
SysVMessages::ClassInit()
{
    MetaSysVMessages::init();
    obj = new SysVMessages();
    obj->init();
}


void
SysVMessages::init(void)
{
    uval i;
    /* unallocated queue anchors have -1 for msqid */
    for (i=0;i<numberOfQueues;message_array[i++].msqid = -1);
    seq = 0;
}

sval
SysVMessages::create(key_t key)
{
    uval i;
    for (i=0;i<numberOfQueues;i++) {
	if ( message_array[i].msqid == -1 ) {
	    message_array[i].key = key;
	    message_array[i].msqid = seq + i;
	    seq += 1<<15;		// linux
	    message_array[i].msgqueue = 0;
	    message_array[i].msqid_ds.msg_qnum = 0;
	    message_array[i].msqid_ds.msg_qbytes = MSGMNB;
	    message_array[i].msqid_ds.__msg_cbytes = 0;
	    message_array[i].waitlist.init();
	    return message_array[i].msqid;
	}
    }
    return -1;			// no free queue
}

sval
SysVMessages::find(key_t key)
{
    uval i;
    for (i=0;i<numberOfQueues;i++) {
	if (message_array[i].key == key &&
	   message_array[i].msqid != -1)
	    return message_array[i].msqid;
    }
    return -1;
}

uval
SysVMessages::index(sval msqid)
{
    uval i;
    i = msqid & ((1<<15)-1);		// linux
    if ((i < numberOfQueues) &&
	(message_array[i].msqid == msqid)) return i;
    return uval(-1);
}

void
SysVMessages::remove(uval i)
{
    messagebuf *p, *q, *tail;
    tail = p = message_array[i].msgqueue;
    if (p) {
	do {
	    q = p->next;
	    p->remove();
	    p = q;
	} while (p != tail);
    }
    message_array[i].msqid = -1;
    message_array[i].waitlist.reinit();
}

/*static*/ SysStatusUval
SysVMessages::msgget(__in key_t key, __in uval msgflag, __CALLER_PID pid)
{
    AutoLock<BLock> al(&obj->lock);
    sval msqid;
    if (key == IPC_PRIVATE)
	return _SRETUVAL(create(key));
    if ((msqid = find(key)) != -1) {
	if ((msgflag & (IPC_CREAT|IPC_EXCL)) == (IPC_CREAT|IPC_EXCL))
	    return _SERROR(2748, 0, EEXIST);
	return _SRETUVAL(msqid);
    }
    if (msgflag & IPC_CREAT)
	return _SRETUVAL(create(key));
    return _SERROR(2749, 0, ENOENT);
}

/*static*/ SysStatusUval
SysVMessages::msgctl(__in sval msqid, __in uval cmd,
		     __inout struct msqid_ds *buf,
		     __CALLER_PID pid)
{
    AutoLock<BLock> al(&obj->lock);
    uval i;
    i = index(msqid);
    if (i == uval(-1)) {
	return _SERROR(2750, 0, EINVAL);
    }
    switch (cmd) {
    case IPC_STAT:
	*buf = message_array[i].msqid_ds;
	return 0;
    case IPC_SET:
	message_array[i].msqid_ds.msg_perm.uid = buf->msg_perm.uid;
	message_array[i].msqid_ds.msg_perm.gid = buf->msg_perm.gid;
	message_array[i].msqid_ds.msg_perm.mode &= 0777;
	message_array[i].msqid_ds.msg_perm.mode |=
	    (buf->msg_perm.mode & 0777);
	message_array[i].msqid_ds.msg_qbytes = buf->msg_qbytes;
	return 0;
    case IPC_RMID:
	message_array[i].waitlist.notify();
	remove(i);			// reset queue to unused state
	return 0;
    default:
	return _SERROR(2751, 0, EINVAL);
    }
}

/*static*/ SysStatusUval
SysVMessages::msgsnd(__in sval msqid, __in sval mtype,
		     __inbuf(msgsize) char* mtext, __in uval msgsize,
		     __in uval msgflg, ObjectHandle oh, __CALLER_PID pid)
{
    AutoLock<BLock> al(&obj->lock);
    sval i;
    messagebuf *p;
    i = index(msqid);
    if (i == -1) {
	return _SERROR(2752, 0, EINVAL);
    }
    p = (messagebuf*)allocGlobalPadded(sizeof(messagebuf)+msgsize-1);
    p->mtype = mtype;
    p->mlength = msgsize;
    memcpy((void*)&(p->mtext), (void*)mtext, msgsize);
    if (message_array[i].msgqueue) {
	p->next = message_array[i].msgqueue->next;
	message_array[i].msgqueue->next = p;
    } else {
	p->next = p;
    }
    message_array[i].msgqueue = p;
    message_array[i].msqid_ds.msg_qnum++;
    message_array[i].msqid_ds.__msg_cbytes += msgsize;
    message_array[i].waitlist.notify();
    return 0;
}

/*static*/ SysStatusUval
SysVMessages::msgrcv(__in sval msqid, __inout sval& mtype,
		     __outbuf(__rc:msgsize) char* mtext, __in uval msgsize,
		     __in uval msgflg, __in ObjectHandle oh,
		     __CALLER_PID pid)
{
    AutoLock<BLock> al(&obj->lock);
    sval i;
    messagebuf *p, *prev, *tail;
    i = index(msqid);

    if (i == -1) {
	return _SERROR(2753, 0, EINVAL);
    }
    // find the message using the crazy rules
    prev = 0;
    tail = message_array[i].msgqueue;
    if (tail) {
	p = tail->next;
	prev = tail;
	if (mtype > 0) {
	    while (1) {
		if (msgflg & MSG_EXCEPT) {
		    if (mtype != p->mtype) break;
		} else {
		    if (mtype == p->mtype) break;
		}
		prev = p;
		p = p->next;
		if (prev == tail) {
		    // wrapped
		    p = 0;		// nothing found
		    break;
		}
	    }
	} else if (mtype < 0) {
	    sval maxtype = (-mtype)+1;
	    messagebuf *minp = 0, *minprev = 0;
	    do {
		if (p->mtype < maxtype) {
		    minp = p;
		    minprev = prev;
		    maxtype = p->mtype;
		}
		prev = p;
		p = p->next;
	    } while (prev != tail);
	    p = minp;
	    prev = minprev;
	}
	// mtype == 0 falls through with p pointing to first entry if any
	// if nothing is found, p is set to 0
	// go to is called for, but it would be wrong :-)
	if (p) {
	    // N.B. mtype == 0 falls through
	    uval sendsize;
	    sendsize = p->mlength;
	    if (sendsize > msgsize ) {
		if (msgflg & MSG_NOERROR) {
		    sendsize = msgsize;
		} else {
		    return _SERROR(2755, 0, E2BIG);
		}
	    }
	    memcpy((void*)mtext, (void*)(p->mtext), sendsize);
	    mtype = p->mtype;
	    prev->next = p->next;
	    if (prev == p) {
		// no more messages
		message_array[i].msgqueue = 0;
	    } else if (tail == p) {
		// removed the tail, new tail is prev
		message_array[i].msgqueue = prev;
	    }
	    p->remove();
	    return sendsize;
	}
    }
    // If IPC_NOWAIT is not asserted, the client will block
    // oh is invalid if the client isn't interested in blocking
    message_array[i].waitlist.add(oh);
    return _SERROR(2754, 0, ENOMSG);
}

void
SysVMessages::waitlist::notify()
{
    uval i,j;
    ObjectHandle* ohp;
    StubSysVMessagesClient stub(StubObj::UNINITIALIZED);
    SysStatus rc;
    ohp = (size == INIT_SIZE)?ohlist:ohbiglist;
    j=0;
    for (i=0;i<nextslot;i++) {
	stub.setOH(ohp[i]);
	rc = stub._notify();
	tassertMsg(_SUCCESS(rc), "upcall failed %ld\n", rc);
	if (_FAILURE(rc)) {
	    ohp[j++] = ohp[i];
	}
    }
    nextslot = j;
}
