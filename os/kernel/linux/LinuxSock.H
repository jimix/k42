#ifndef __LINUX_SOCK_H_
#define __LINUX_SOCK_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxSock.H,v 1.10 2005/07/15 17:14:30 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a socket interface using the linux TCP/IP stack.
 * **************************************************************************/

#include <io/SocketServer.H>
#include <meta/MetaSocketServer.H>
#include <xobj/XSocketServer.H>
#include <meta/MetaFileLinuxClientStream.H>
#include <stub/StubFileLinuxClientStream.H>
#include <misc/ListSimple.H>
#include <io/FileLinux.H>
#include <io/StreamServer.H>



class LinuxSock;
typedef LinuxSock** LinuxSockRef;
class LinuxSock : public SocketServer {
    // Each object is associated with a linux socket object.  Both of
    // these have locks.  To avoid deadlocks, the rule is the the
    // linux object's lock must be grabbed first.  (This means, don't
    // hold the lock when calling into linux code.)  Also, the K42 lock
    // cannot be held when a LinuxEnv object goes out of scope --
    // because that may trigger a SoftIRQ to run and violate this rule.

public:
    DEFINE_ACCESS_SERVER(SocketServer,
			 MetaObj::write|MetaObj::read|MetaObj::controlAccess,
			 MetaObj::none);
    virtual void calcAvailable(GenState& avail,
			       StreamServer::ClientData* client=NULL);
protected:
    struct socket *dataSocket;

    /*
     * "status" is a cache of the bits returned from SocketLinux_poll().
     * It is always updated under the lock, and the result passed up via
     * either ClientData::setAvail() or locked_signalDataAvailable().
     * An asynchronous upcall from the Linux layer sets "refreshNeeded", but
     * does not convey the new state.  The state is retrieved under the
     * lock, immediately if the lock is available, or when the lock is
     * next released otherwise.
     */
    volatile uval refreshNeeded;
    uval status;
    void updateStatus();
public:
    virtual SysStatusUval async_signalDataAvailable();
protected:
    uval trylock();

    struct ClientData: public StreamServer::ClientData {
	ProcessRef pref;
	DEFINE_GLOBAL_NEW(ClientData);
	ClientData():pref(NULL) {};
    };
    virtual void init() {
	_lock.init();
	refreshNeeded = 0;
	status = FileLinux::INVALID;
	dataSocket = NULL;
    }
    virtual void lock();
    virtual void unlock();

public:
    DEFINE_GLOBAL_NEW(LinuxSock);
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type);
    virtual SysStatus flush() { return 0; };


    static SysStatus Create(ObjectHandle &oh, uval &clientType,
			    uval domain, uval type, uval protocol,
			    ProcessID caller);

    virtual SysStatus destroy();
    virtual SysStatus detach();

    /* exported functions */
    static SysStatus _Create(__out ObjectHandle &oh, __out uval &clientType,
			     __in uval domain, __in uval type,
			     __in uval protocol, __CALLER_PID processID);
    virtual SysStatus _accept(__out ObjectHandle &oh,
			      __inoutbuf(len:len:len) char* addr,
			      __inout uval& len,
			      __out GenState &moreAvail,
			      __XHANDLE xhandle);
    virtual SysStatus _listen(__in sval backlog);


    virtual SysStatus _setsockopt(__in uval level, __in uval optname,
				  __inbuf(optlen) char *optval,
				  __in uval optlen, __XHANDLE xhandle);
    virtual SysStatus _getsockopt(__in uval level, __in uval optname,
                                  __outbuf(optlen:optlen) char *optval,
                                  __inout uval *optlen, __XHANDLE xhandle);

    virtual SysStatus _bind(__inoutbuf(addrLen:addrLen:addrLen) char *addr,
			    __inout uval &addrLen, __XHANDLE xhandle);

    virtual SysStatusUval _sendtoVirt(__inbuf(len) struct iovec* vec,
				      __in uval len,
				      __in uval flags,
				      __inbuf(addrLen) const char *addr,
				      __in uval addrLen,
				      __inout GenState &moreAvail,
				      __XHANDLE xhandle);

    virtual SysStatusUval _recvfromVirt(__inbuf(len) struct iovec* vec,
					__in uval len,
					__in uval flags,
					__outbuf(addrLen:addrLen) char *addr,
					__inout uval &addrLen,
					__inout GenState &moreAvail,
					__XHANDLE xhandle);

    virtual SysStatusUval _readSMT(__out uval &offset,
				   __in uval len,
				   __out GenState &moreAvail,
				   __XHANDLE xhandle);

    virtual SysStatusUval _writeSMT(__in uval offset,
				    __in uval len,
				    __out GenState &moreAvail,
				    __XHANDLE xhandle);

    virtual SysStatus _connect(__inbuf(addrLen) const char *addr,
			       __in uval addrLen, __out GenState &moreAvail,
			       __XHANDLE xhandle);

    virtual SysStatusUval _recvfromSMT(__out uval &offset,
				       __in uval len,
				       __outbuf(addrLen:addrLen) char *addr,
				       __inout uval &addrLen,
				       __out GenState &moreAvail,
				       __XHANDLE xhandle) __xa(read);

    virtual SysStatusUval _sendtoSMT(__in uval offset,
				     __in uval len,
				     __inbuf(addrLen) const char *addr,
				     __in uval addrLen,
				     __out GenState &moreAvail,
				     __XHANDLE xhandle) __xa(write);

    virtual SysStatusUval _ioctl(__in uval request, __inout uval &size,
				 __inoutbuf(size:size:size) char* arg,
				 __XHANDLE xhandle);

    virtual SysStatus _getname(__in uval peer,
			       __outbuf(addrLen:addrLen) char *buf,
			       __inout uval &addrLen);

    virtual SysStatusUval sendto(struct iovec *vec, uval len,
				 uval flags, const char *_addr, uval addrLen,
				 GenState &moreAvail, 
				 void *controlData, uval controlDataLen,
				 __XHANDLE xhandle);

    virtual SysStatusUval recvfrom(struct iovec *vec, uval veclen,
				   uval flags, char *_addr, uval &addrLen,
				   GenState &moreAvail, 
				   void *controlData, uval &controlDataLen,
				   __XHANDLE xhandle);

    static void ClassInit();
};

#endif /* #ifndef __LINUX_SOCK_H_ */
