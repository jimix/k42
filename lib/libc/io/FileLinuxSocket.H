#ifndef __FILE_LINUX_SOCKET_H_
#define __FILE_LINUX_SOCKET_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxSocket.H,v 1.57 2005/05/09 18:33:42 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality for
 *                     sockets.
 * **************************************************************************/

#include <io/FileLinuxStream.H>
#include <stub/StubSocketServer.H>
#include <meta/MetaSocketServer.H>

#include <sys/stat.h>
#include <fcntl.h>
#include <io/Socket.H>
#include <sys/uio.h>

class FileLinuxSocket : public FileLinuxStream {
protected:

public:
    DEFINE_GLOBAL_NEW(FileLinuxSocket);

    FileLinuxSocket(): sss(StubObj::UNINITIALIZED), traceThisSocket(0) {};
    FileLinuxSocket(sval d, sval t, sval p) : 
	sss(StubObj::UNINITIALIZED), sDomain(d), sType(t), sProtocol(p),
	traceThisSocket(0), traceThisSocket2(0), startedToConnect(0)
	{ /* empty body */ };

    StubSocketServer sss;
    sval sDomain;
    sval sType;
    sval sProtocol;
    struct sockaddr* local;
    struct sockaddr* remote;
    uval32 localLen;
    uval32 remoteLen;

    uval traceThisSocket;
    uval traceThisSocket2;	// debugging aid  to be removed

    uval startedToConnect;	// does not belong here

    static SysStatus Socket(FileLinuxRef& newSocket, sval domain, sval type,
		            sval protocol);
    static SysStatus SocketPair(FileLinuxRef &newSocket1,
				FileLinuxRef &newSocket2,
				sval domain, sval type, sval protocol);

    static SysStatus CreateInternal(FileLinuxRef& newSocket, ObjectHandle oh,
				    uval clientType, uval oflags,
				    sval domain, sval type, sval protocol,
				    struct sockaddr *localAddr, uval localLen,
				    struct sockaddr *remoteAddr,uval remoteLen,
				    uval debugFlag = 0);

    struct LazyReOpenData: public  FileLinux::LazyReOpenData {
        sval domain;
	sval type;
	sval protocol;
	struct sockaddr local;
	struct sockaddr remote;
	uval remoteLen;
	uval localLen;
    };

    static SysStatus Create(FileLinuxRef &newSocket, sval domain,
			    sval type, sval protocol);

    virtual SysStatus getStatus(FileLinux::Stat *status);

    /*
     * socket functions - these will return error for non-socket files
     */
    // unix accepts bundles a getpeername.  We don't since the
    // information is cached locally anyhow
    virtual SysStatus accept(FileLinuxRef& newSocket, ThreadWait **tw);

    virtual SysStatus bind(const char* addr, uval addrLen) = 0;

    virtual SysStatus setsockopt(uval level, uval optname,
				 const void *optval, uval optlen);
    virtual SysStatus getsockopt(uval level, uval optname,
				 const void *optval, uval *optlen);

    virtual SysStatus connect(const char* addr, uval addrLen,
			      GenState &moreAvail, ThreadWait **tw) = 0;

    virtual SysStatusUval sendmsg(struct msghdr &msg, uval flags,
				  ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval recvmsg(struct msghdr &msg, uval flags,
				  ThreadWait **tw, GenState &moreAvail);

    // this end of socket
    virtual SysStatus getsocketname(char* addr, uval &addrLen);

    // other end of socket
    virtual SysStatus getpeername(char* addr, uval &addrLen);

    virtual SysStatus listen(sval backlog);

    virtual SysStatus shutdown(uval how);

    virtual SysStatus dup(FileLinuxRef& newFile);

    virtual SysStatus ioctl(uval request, va_list args);

    virtual SysStatus lazyGiveAccess(sval fd);
};

#endif /* #ifndef __FILE_LINUX_SOCKET_H_ */
