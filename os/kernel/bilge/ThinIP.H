#ifndef __THIN_IP_H_
#define __THIN_IP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ThinIP.H,v 1.87 2005/02/09 18:45:42 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A cut-through file system/IP server.
 * Currently, not much in the way of state that necessitates an object
 * implementaitoin.  However, eventually we want to have a queue here,
 * with all the blocking operations blocked up until we do a select
 * and find data is available on the requested stream.  Note,
 * exceptions should be disabled whenever doing I/O
 *
 * Note, all internal methods are now virtual to allow the gizmo version
 * of this file to be used in the same by clients (using the static methods)
 * while providing its own implementation.  The gizmo is directly inherited
 * from this class to minimize changes to other code, and since it shares
 * a similar implementation using the same class data anyway.
 * **************************************************************************/

#include <io/Socket.H>
#include <io/StreamServer.H>
#include <sys/IOChan.H>

class ThinIP {
protected:

    // just a single instance of this class
    static ThinIP *obj;

    // sockets we know about
    enum {MAX_SOCKETS=256};
    StreamServerRef sockets[MAX_SOCKETS];

    ThreadID daemon;			// polls for data for reads
    static void BeAsynchronous(uval arg);
    typedef BLock LockType;
    LockType      lock;

    ThinIP()
    {
      /* empty body */
    }
    IOChan* select_chan;
    IOChan* cut_chan;
    DEFINE_GLOBALPADDED_NEW(ThinIP);

    virtual void init(IOChan* ipcut, IOChan* ipsel);

    sval locked_blockedRead(void *p, int length);

    sval locked_blockedWrite(const void *p, int length);

    virtual SysStatus socket(sval32 &sock, StreamServerRef ref, uval type);

    virtual SysStatus close(sval32 sock);

    virtual SysStatus bind(sval32 sock, void* addr, uval addrLen);

    virtual SysStatus listen(sval32 sock, sval32 backlog);

    virtual SysStatus accept(sval32 sock, sval32 &clientSock,
			     StreamServerRef ref, uval &available);

    virtual SysStatusUval read(sval32 sock, char *buf, uval32 nbytes,
			       uval &available);

    virtual SysStatusUval write(sval32 sock, const char *buf, uval32 nbytes);

    virtual SysStatusUval sendto(sval32 sock, const char *buf, uval32 nbytes,
				 void* addr, uval addrLen);

    virtual SysStatusUval recvfrom(sval32 sock, char *buf, uval32 nbytes,
				   uval &available, void* addr, uval addrLen);

    virtual SysStatusUval connect(sval32 sock, void* addr, uval addrLen);

    virtual SysStatus getThinEnvVar(const char *envVarName, char *envVarValue);

    virtual SysStatus getThinTimeOfDay(struct timeval& tv);

    virtual SysStatus getKParms(void **data);

    virtual void doPolling();

public:
    // class error conditions
    enum {
	WOULD_BLOCK=56
    };

    static void ClassInit(VPNum vp, IOChan* ipcut, IOChan* ipsel);

    static SysStatus Socket(sval32 &sock, StreamServerRef ref, uval type) {
	return obj->socket(sock, ref, type);
    }

    static SysStatus Close(sval32 sock) {
	return obj->close(sock);
    }

    static SysStatus Bind(sval32 sock, void* addr, uval addrLen) {
	return obj->bind(sock, addr, addrLen);
    }

    static SysStatus Listen(sval32 sock, sval32 backlog) {
	return obj->listen(sock, backlog);
    }

    static SysStatus Accept(sval32 sock, sval32 &clientSock,
			    StreamServerRef ref,
			    uval &available) {
	return obj->accept(sock, clientSock, ref, available);
    }

    static SysStatusUval Read(sval32 sock, char *buf, uval32 nbytes,
			      uval &available) {
	return obj->read(sock, buf, nbytes, available);
    }

    static SysStatusUval Write(sval32 sock, const char *buf, uval32 nbytes) {
	return obj->write(sock, buf, nbytes);
    }

    static SysStatusUval Sendto(sval32 sock, const char *buf, uval32 nbytes,
			 void* addr, uval addrLen) {
	return obj->sendto(sock, buf, nbytes, addr, addrLen);
    }

    static SysStatusUval Recvfrom(sval32 sock, char *buf, uval32 nbytes,
				   uval &available, void* addr, uval addrLen) {
	return obj->recvfrom(sock, buf, nbytes, available, addr, addrLen);
    }

    static SysStatus Connect(sval32 sock, void* addr, uval addrLen) {
	return obj->connect(sock, addr, addrLen);
    }

    static SysStatus GetThinTimeOfDay(struct timeval& tv) {
	return obj->getThinTimeOfDay(tv);
    }

    static SysStatus GetKParms(void **data) {
	return obj->getKParms(data);
    }

    static void DoPolling() {
	obj->doPolling();
    }
};

#endif /* #ifndef __THIN_IP_H_ */
