#ifndef __FILE_LINUX_PACKET_H_
#define __FILE_LINUX_PACKET_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxPacket.H,v 1.24 2005/05/09 21:02:21 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality for
 *                     datagram sockets.
 * **************************************************************************/

#include <io/FileLinuxStream.H>
#include <io/PacketServer.H>
#include <stub/StubPacketServer.H>
#include <meta/MetaPacketServer.H>
#include <io/FileLinuxStream.H>
#include <sys/stat.h>
#include <fcntl.h>
#include <io/Socket.H>
#include <sys/uio.h>

class FileLinuxPacket : public FileLinuxStream {
protected:
    DEFINE_GLOBAL_NEW(FileLinuxPacket);
    // structure for holding stubobject
    StubPacketServer sps;
    sval sDomain;
    sval sType;
    sval sProtocol;

    FileLinuxPacket(sval d, sval t, sval p):sps(StubObj::UNINITIALIZED),
	sDomain(d), sType(t), sProtocol(p) {};

    char *local;  //Local socket address
    char *remote; //Remote socket address
    uval32 localLen;
    uval32 remoteLen;

    static SysStatus CreateInternal(FileLinuxRef& newSocket,
				    ObjectHandle stubOH,
				    uval clientType, uval oflags, sval domain,
				    sval type, sval protocol);

public:
    static SysStatus Create(FileLinuxRef &newSocket, sval domain,
			    sval type, sval protocol);

    static SysStatus SocketPair(FileLinuxRef &newSocket1,
				FileLinuxRef &newSocket2,
				uval domain, uval type, uval protocol);

    virtual SysStatus destroy();

    virtual SysStatus getStatus(FileLinux::Stat *status);

    /*
     * For a file that can be memory mapped, returns object handle to
     * FR needed for creating specialized mappings.
     */

    /*
     * socket functions - these will return error for non-socket files
     */
    virtual SysStatus bind(const char* addr, uval addrLen);

    virtual SysStatus connect(const char* addr, uval addrLen,
			      GenState &moreAvail, ThreadWait **tw);

    // this end of socket
    virtual SysStatus getsocketname(char* addr, uval &addrLen);

    // other end of socket
    virtual SysStatus getpeername(char* addr, uval &addrLen);

    virtual SysStatus setsockopt(uval level, uval optname,
				 const void *optval, uval optlen);
    virtual SysStatus getsockopt(sval level, sval optname,
				 const void *optval, uval *optlen);

    static SysStatus Socket(
	FileLinuxRef& newSocket, sval domain, sval type, sval protocol);

    virtual SysStatus dup(FileLinuxRef& newFile);

    virtual SysStatus ioctl(uval request, va_list args);
};

#endif /* #ifndef __FILE_LINUX_PACKET_H_ */
