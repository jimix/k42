#ifndef __SysVMessages_H_
#define __SysVMessages_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVMessages.H,v 1.1 2003/11/12 02:28:05 marc Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Sys V Shared Memory
 ****************************************************************************/

#include <misc/HashSimple.H>

#ifndef EXPORT_SYSVMESSAGES
#define EXPORT_SYSVMESSAGES
#include <sys/ipc.h>
#include <sys/msg.h>
#endif // #ifndef EXPORT_SYSVMESSAGES

// the System V messages server object
class SysVMessages : private Obj {
    enum {
	numberOfQueues = 16,
	MSGMNB = 16384 /*linux kernel constant*/
    };
    BLock lock;
    static SysVMessages *obj;
    uval seq;
#ifndef _IN_STUBGEN
    struct waitlist {
	enum {INIT_SIZE=4};
	uval nextslot;
	uval size;
	union {
	    ObjectHandle ohlist[INIT_SIZE];
	    ObjectHandle* ohbiglist;
	};
	void init() {
	    nextslot = 0; size = INIT_SIZE;
	}
	void reinit() {
	    nextslot = 0;
	}
	void add(ObjectHandle oh) {
	    uval i;
	    ObjectHandle* ohp;
	    if(oh.invalid()) return;
	    ohp = (size == INIT_SIZE)?ohlist:ohbiglist;
	    for(i=0; i< nextslot; i++) {
		if(ohp[i].isSameOH(oh)) return;
	    }
	    if(nextslot == size) {
		// enlarge list - sigh
		uval newsize = (size == INIT_SIZE)?16:size *2;
		ObjectHandle *newohp;
		newohp = (ObjectHandle*)
		    allocGlobalPadded(sizeof(ObjectHandle)*newsize);
		passertMsg(newohp, "Out of memory\n");
		for(i=0;i<size;i++) {
		    newohp[i] = ohp[i];
		}
		if (size != INIT_SIZE) {
		    freeGlobalPadded(ohp, sizeof(ObjectHandle) * size);
		}
		size = newsize;
		ohbiglist = newohp;
	    }
	    ohp[nextslot++] = oh;
	    return;
	}	    
	void notify();
    };
    struct messagebuf {
	messagebuf *next;
	sval mtype;
	uval32 mlength;
	char mtext[1];
	void remove() {
	    freeGlobalPadded(this, sizeof(messagebuf) + mlength -1);
	}
    };
    struct {
	int key;
	int msqid;
	messagebuf* msgqueue;		// ring queue tail
	struct waitlist waitlist;
	struct msqid_ds msqid_ds;
    } message_array[numberOfQueues];
    
    void init();
    DEFINE_GLOBALPADDED_NEW(SysVMessages);
#endif
protected:
    sval create(key_t key);
    sval find(key_t key);
    uval index(sval msqid);
    void remove(uval i);
    SysStatusUval msgget(__in key_t key,
			 __in uval msgflag,
			 __CALLER_PID pid);
    
    SysStatusUval msgctl(__in sval msqid, __in uval cmd,
			 __inout struct msqid_ds *buf,
			 __CALLER_PID pid);

    SysStatusUval msgsnd(__in sval msqid,
			 __in sval mtype,
			 __inbuf(msgsize) char* mtext,
			 __in uval msgsize,
			 __in uval msgflg,
			 __in ObjectHandle oh,
			 __CALLER_PID pid);

    SysStatusUval msgrcv(__in sval msqid,
			 __inout sval& mtype,
			 __outbuf(__rc:msgsize) char* mtext,
			 __in uval msgsize,
			 __in uval msgflg,
			 __in ObjectHandle oh,
			 __CALLER_PID pid);
    
public:
    static void ClassInit();

__xprotected:
    static SysStatusUval Msgget(__in key_t key,
				__in uval msgflag,
				__CALLER_PID pid) {
	return obj->msgget(key, msgflag, pid);}
    
    static SysStatusUval Msgctl(__in sval msqid, __in uval cmd,
				__inout struct msqid_ds *buf,
				__CALLER_PID pid){
	return obj->msgctl(msqid, cmd, buf, pid);}

    static SysStatusUval Msgsnd(__in sval msqid,
				__in sval mtype,
				__inbuf(msgsize) char* mtext,
				__in uval msgsize,
				__in uval msgflg,
				__in ObjectHandle oh,
				__CALLER_PID pid){
	return obj->msgsnd(msqid, mtype, mtext, msgsize, msgflg, oh, pid);}

    static SysStatusUval Msgrcv(__in sval msqid,
				__inout sval& mtype,
				__outbuf(__rc:msgsize) char* mtext,
				__in uval msgsize,
				__in uval msgflg,
				__in ObjectHandle oh,
				__CALLER_PID pid){
	return obj->msgrcv(msqid, mtype, mtext, msgsize, msgflg, oh, pid);}
};

#endif /* #ifndef __SysVMessages_H_ */
