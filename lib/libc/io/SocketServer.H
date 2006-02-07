#ifndef __SOCKET_SERVER_H_
#define __SOCKET_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SocketServer.H,v 1.41 2005/05/04 01:27:37 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines socket interface server exports to client
 * **************************************************************************/

#ifndef EXPORT_SOCKETSERVER
#define EXPORT_SOCKETSERVER
#include <io/GenState.H>
#include <sys/uio.h>
#endif /* #ifndef EXPORT_SOCKETSERVER */

#include <io/FileLinux.H>
#include <io/PacketServer.H>

class SocketServer : public PacketServer {
public:
    DEFINE_GLOBAL_NEW(SocketServer);
__xprivate:
    virtual SysStatus _connect(__inbuf(len) const char *addr,
			       __in uval len, __out GenState& moreAvail,
			       __XHANDLE xhandle) = 0;

    virtual SysStatus _bind(__inoutbuf(addrLen:addrLen:addrLen) char *addr,
			    __inout uval &addrLen, __XHANDLE xhandle) = 0;


    virtual SysStatus _listen(__in sval backlog)=0;
    virtual SysStatus _accept(__out ObjectHandle &oh,
			      __inoutbuf(len:len:len) char* addr,
			      __inout uval& len,
			      __out GenState &moreAvail,
			      __XHANDLE xhandle)=0;

    virtual SysStatus _getname(__in uval peer,
			       __outbuf(addrLen:addrLen) char *buf,
			       __inout uval &addrLen) {
	return _SERROR(2409, 0, EOPNOTSUPP);
    }

    virtual SysStatusUval _sendtoVirt(__inbuf(len) struct iovec* vec,
				      __in uval len,
				      __in uval flags,
				      __inbuf(addrLen) const char *addr,
				      __in uval addrLen,
				      __inout GenState &moreAvail,
				      __XHANDLE xhandle) __xa(write) {
	return _SERROR(2085, 0, EOPNOTSUPP);
    }

    virtual SysStatusUval _recvfromVirt(__inbuf(len) struct iovec* vec,
					__in uval len,
					__in uval flags,
					__outbuf(addrLen:addrLen) char *addr,
					__inout uval &addrLen,
					__inout GenState &moreAvail,
					__XHANDLE xhandle) {
	return _SERROR(2086, 0, EOPNOTSUPP);
    }

__xpublic:
    virtual SysStatus _socketTrace(__in uval onOrOff) {
	return _SERROR(0, 0, EOPNOTSUPP);
    }
    virtual SysStatus _shutdown(__in uval how) {
	return _SERROR(0, 0, EOPNOTSUPP);
    }
};

#endif /* #ifndef __SOCKET_SERVER_H_ */
