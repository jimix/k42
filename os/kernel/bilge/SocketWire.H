#ifndef __SOCKET_WIRE_H_
#define __SOCKET_WIRE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SocketWire.H,v 1.63 2005/07/15 17:14:28 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a socket interface using thinwire. Read
 * and write requests that come in from user level are only handled if
 * there is no blocking.  Internal requests (in this address space)
 * are handled by blocking the requesting thread.
 *
 * As temporary cludge, not yet tying into user level IO derivatoin tree.
 * **************************************************************************/

#include <meta/MetaFileLinuxClientStream.H>
#include <stub/StubFileLinuxClientStream.H>
#include <cobj/XHandleTrans.H>

#include <io/SocketServer.H>
#include <stub/StubSocketServer.H>
#include <meta/MetaSocketServer.H>

class WireTCP;
typedef WireTCP **WireTCPRef;
class WireUDP;
typedef WireUDP **WireUDPRef;
class SocketWire;
typedef SocketWire **SocketWireRef;


class SocketWire : public SocketServer {
protected:
    sval32 dataSocket;
    uval dataIsAvailable;		// data is available
    uval threadsBlocked;		// threads blocked waiting for data,
					// used for user-level debugger

    void init() { _lock.init(); dataIsAvailable=0; threadsBlocked=0; }

    virtual void calcAvailable(GenState& avail,
			       StreamServer::ClientData* client=NULL);

public:
    DEFINE_GLOBAL_NEW(SocketWire);
    DEFINE_ACCESS_SERVER(SocketServer,
			 MetaObj::write|MetaObj::read|MetaObj::controlAccess,
			 MetaObj::none);
    virtual SysStatus giveAccessSetClientData(ObjectHandle &oh,
					      ProcessID toProcID,
					      AccessRights match,
					      AccessRights nomatch,
					      TypeID type);
    virtual SysStatus locked_signalDataAvailable() {
	dataIsAvailable=1;
	return StreamServer::locked_signalDataAvailable();
    }
    virtual SysStatus flush() { return 0;};
    static SysStatus Create(ObjectHandle &oh, uval &clientType,
			    uval domain, uval type, uval protocol,
			    ProcessID caller);

    virtual SysStatus destroy();
    virtual SysStatus detach();

    static void ClassInit();


    virtual SysStatus _bind(__inoutbuf(addrLen:addrLen::addrLen) char* addr,
			    __inout uval& addrLen, __XHANDLE xhandle);

    virtual SysStatus _setsockopt(__in uval level, __in uval optname,
				  __inbuf(optlen) char *optval,
				  __in uval optlen, __XHANDLE xhandle);
    virtual SysStatus _getsockopt(__in uval level, __in uval optname,
                                  __outbuf(optlen:optlen) char *optval,
                                  __inout uval *optlen, __XHANDLE xhandle) {
	return _SERROR(2907, 0, EOPNOTSUPP);
    }

    virtual SysStatus _connect(__inbuf(addrLen) const char* addr,
			       __in uval addrLen,
			       __out GenState &moreAvail, __XHANDLE xhandle);

    virtual SysStatusUval _writeSMT(__in uval offset,
				    __in uval len, __out GenState &moreAvail,
				    __XHANDLE xhandle)__xa(write);

    virtual SysStatusUval _readSMT(__out uval &offset,
				   __in uval len,
				   __out GenState &moreAvail,
				   __XHANDLE xhandle) __xa(read);

    virtual SysStatusUval _recvfromSMT(__out uval &offset,
				       __in uval len,
				       __outbuf(addrLen:addrLen) char* addr,
				       __inout uval &addrLen,
				       __out GenState &moreAvail,
				       __XHANDLE xhandle) __xa(read);


    virtual SysStatusUval _sendtoSMT(__in uval offset,
				     __in uval len,
				     __inbuf(addrLen) const char* addr,
				     __in uval addrLen,
				     __out GenState &moreAvail,
				     __XHANDLE xhandle) __xa(write);

    virtual SysStatusUval _ioctl(__in uval request, __inout uval &size,
				 __inoutbuf(size:size:size) char* arg,
				 __XHANDLE xhandle);

    virtual SysStatusUval _sendtoVirt(__inbuf(len) struct iovec* vec,
				      __in uval len,
				      __in uval flags,
				      __inbuf(addrLen) const char *addr,
				      __in uval addrLen,
				      __inout GenState &moreAvail,
				      __XHANDLE xhandle) __xa(write);

    virtual SysStatusUval _recvfromVirt(__inbuf(len) struct iovec* vec,
					__in uval len,
					__in uval flags,
					__outbuf(addrLen:addrLen) char *addr,
					__inout uval &addrLen,
					__inout GenState &moreAvail,
					__XHANDLE xhandle);

    virtual SysStatus _getname(__in uval peer,
			       __outbuf(addrLen:addrLen) char *buf,
			       __inout uval &addrLen);

    virtual SysStatus _listen(__in sval backlog);
    virtual SysStatus _accept(__out ObjectHandle &oh,
			      __inoutbuf(len:len:len) char* addr,
			      __inout uval& len,
			      __out GenState &moreAvail,
			      __XHANDLE xhandle);

    virtual SysStatusUval sendto(struct iovec *vec, uval len, uval flags,
				 const char *_addr, uval addrLen,
				 GenState &moreAvail, 
				 void *controlData, uval controlDataLen,
				 __XHANDLE xhandle);

    virtual SysStatusUval recvfrom(struct iovec *vec, uval veclen, uval flags,
				   char *_addr, uval &addrLen,
				   GenState &moreAvail, 
				   void *controlData, uval &controlDataLen,
				   __XHANDLE xhandle);

};
#endif /* #ifndef __SOCKET_WIRE_H_ */
