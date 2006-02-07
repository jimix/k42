/***************************************************************************** *
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxPacket.C,v 1.54 2005/05/09 21:10:42 butrico Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxPacket.H"
#include <cobj/CObjRootSingleRep.H>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "PacketServer.H"
#include <stub/StubPacketServer.H>
#include <meta/MetaPacketServer.H>
#include <FileLinuxPacketUnix.H>
extern "C" {
#include <linux/types.h>
#include <netinet/ip.h>
#include <linux/if_tunnel.h>
}
#include <stub/StubIPSock.H>
#include <stub/StubStreamServerSocket.H>


/* static */ SysStatus
FileLinuxPacket::Create(FileLinuxRef &newSocket, sval domain, sval type,
		        sval protocol)
{
    ObjectHandle stubOH;
    uval clientType;
    SysStatus rc = 0;
    switch (domain) {
      case AF_NETLINK:
      case AF_INET:
	rc = StubIPSock::_Create(stubOH, clientType,
				 domain, type, protocol);
	break;
      case AF_UNIX:
	rc = StubStreamServerSocket::_CreateSocket(stubOH, clientType, domain,
				                   type, protocol, /*trace */0);
	break;
    default:
	tassertWrn(0, "unsupported socket domain %ld\n", domain);
	rc = _SERROR(2667, 0, EAFNOSUPPORT);
	break;
    }

    if (_SUCCESS(rc)) {
	rc = FileLinuxPacket::CreateInternal(newSocket, stubOH, clientType,
					     O_RDWR, domain, type, protocol);
    }
    return rc;
}


/* static */ SysStatus
FileLinuxPacket::SocketPair(FileLinuxRef &newSocket1,
			    FileLinuxRef &newSocket2,
			    uval domain, uval type, uval protocol)
{

#if 1
    return _SERROR(2766, 0, EAFNOSUPPORT);
#else
    ObjectHandle stubOH1, stubOH2;
    uval clientType;
    SysStatus rc;
    if (domain != AF_UNIX) {
	return _SERROR(2766, 0, EAFNOSUPPORT);
    }

    rc = StubUNIXSock::_CreatePair(stubOH1, stubOH2, clientType,
			       domain, type, protocol);

    if (_SUCCESS(rc)) {
	rc = FileLinuxPacket::CreateInternal(
	    newSocket1, stubOH1, clientType, O_RDWR, domain, type, protocol);
	if (_FAILURE(rc)) return rc;
	rc = FileLinuxPacket::CreateInternal(
	    newSocket2, stubOH2, clientType, O_RDWR, domain, type, protocol);
    }
    return rc;
#endif
}

/* virtual */ SysStatus
FileLinuxPacket::dup(FileLinuxRef& newSocket)
{
    SysStatus rc;
    ObjectHandle copyOH;
    rc = giveAccessByClient(copyOH, DREFGOBJ(TheProcessRef)->getPID());
    if (_FAILURE(rc)) return rc;

    rc = CreateInternal(newSocket, copyOH, stubHolder->transType, openFlags,
			sDomain, sType, sProtocol);
    return rc;
}

/*static*/ SysStatus
FileLinuxPacket::CreateInternal(FileLinuxRef& newSocket, ObjectHandle stubOH,
				uval clientType, uval oflags, sval domain,
			        sval type, sval protocol)
{
    SysStatus rc;
    FileLinuxPacket *newp = NULL;

    ObjectHandle dummy;

    switch (type) {
    case SOCK_DGRAM:
    case SOCK_RAW:
	switch (domain) {
	case AF_NETLINK:
	case AF_INET:
	    newp = new FileLinuxPacket(domain, type, protocol);
	    break;
	case AF_UNIX:
	    newp = (FileLinuxPacket*) new FileLinuxPacketUnix(domain, type,
                                                        protocol);
	    break;
	default:
	    tassertMsg(0, "fixme.  Add the domain (%ld) here as well.\n",
	                 domain);
	    break;
	}
	break;
    default:
	tassertMsg(0, "fixme.  Add the type (%ld) here as well.\n", type);
	break;
    }

    // using dummy tells init not to make a stubHolder
    dummy.init();
    newp->init(dummy, NULL);

    newp->local = newp->remote = 0;
    newp->localLen = newp->remoteLen = 0;

    newp->sps.setOH(stubOH);

    switch (clientType) {
    case TransStreamServer::TRANS_VIRT:
	newp->stubHolder = new TransVirtStream(stubOH);
	break;
    case TransStreamServer::TRANS_PPC:
	newp->stubHolder = new TransPPCStream(stubOH);
	break;
    default:
	err_printf("Unknown socket client type: %ld\n", clientType);
    }

    newSocket = (FileLinuxRef)newp->getRef();

    // register object for callback by server
    rc = newp->registerCallback();
    tassert(_SUCCESS(rc), err_printf("do cleanup code\n"));
    return 0;
}

/*
 * for sockets, stat returns a canned and useless result.
 * every system seems different.  linux, for example, seems
 * to return times, AIX doesn't
 */
/*virtual*/ SysStatus
FileLinuxPacket::getStatus(FileLinux::Stat *status)
{
    status->st_dev	= 0;
    status->st_ino	= 0;
    status->st_mode	= S_IFSOCK|S_IRWXU|S_IRWXG|S_IRWXO;
    status->st_nlink	= 1;
    status->st_uid	= 0;	// FIXME
    status->st_gid	= 0;	// FIXME
    status->st_rdev	= 0;	// FIXME
    status->st_size	= 0;
    status->st_blksize	= 0x1000; // 4k
    status->st_blocks	= 0;
    status->st_atime	= 0;	// FIXME
    status->st_mtime	= 0;	// FIXME
    status->st_ctime	= 0;	// FIXME
    return 0;
}

/* virtual */ SysStatus
FileLinuxPacket::setsockopt(uval level, uval optname, const void *optval,
			    uval optlen)
{
    SysStatus retvalue;

    switch (optname) {
    default:	// option we never heard from
	tassertWrn(0, "unknown socket option level=%ld option=%ld",
			level, optname);
	break;

    case SO_DEBUG:	// option we pass through
    case SO_REUSEADDR:
    case SO_TYPE:
    case SO_ERROR:
    case SO_DONTROUTE:
    case SO_BROADCAST:
    case SO_SNDBUF:
    case SO_RCVBUF:
    case SO_KEEPALIVE:
    case SO_OOBINLINE:
    case SO_NO_CHECK:
    case SO_PRIORITY:
    case SO_LINGER:
    case SO_BSDCOMPAT:
    case SO_RCVLOWAT:
    case SO_SNDLOWAT:
    case SO_PASSCRED:
    case SO_PEERCRED:
    case SO_SECURITY_AUTHENTICATION:
    case SO_SECURITY_ENCRYPTION_TRANSPORT:
    case SO_SECURITY_ENCRYPTION_NETWORK:
    case SO_BINDTODEVICE:
    case SO_ATTACH_FILTER:
    case SO_DETACH_FILTER:
    case SO_PEERNAME:
    case SO_TIMESTAMP:
    case SO_ACCEPTCONN:
    //case SO_PEERSEC:
	break;

    case SO_RCVTIMEO:	// options we don't implement
    case SO_SNDTIMEO:
	tassertWrn(0, "Socket option (%ld) not supported (level %ld).\n",
				optname, level);
        return _SERROR(0, 0, EOPNOTSUPP);
	break;
    }

    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    retvalue = sps._setsockopt(level, optname, (char *)optval, optlen);
    return (retvalue);
}

/*virtual*/ SysStatus
FileLinuxPacket::bind(const char* addr, uval addrLen)
{
    SysStatus rc;
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    uval len = addrLen;
    char *buf = (char*)allocGlobal(len);
    memcpy(buf, addr, addrLen);
    rc = sps._bind(buf, addrLen);
    if (_SUCCESS(rc)) {
	if (local) {
	    freeGlobal(local, localLen);
	}
	local = buf;
	localLen = addrLen;
    } else {
	freeGlobal(buf, len);
    }
    return rc;

}

#define localaddr "127.0.0.1"
/*virtual*/ SysStatus
FileLinuxPacket::connect(const char* addr, uval addrLen,
			 GenState &moreAvail, ThreadWait **tw)
{
    AutoLock<LockType> al(&objLock);

    if (remote) {
	freeGlobal(remote, remoteLen);
    }
    remote = (char*)allocGlobal(addrLen);
    remoteLen = addrLen;
    memcpy(remote, addr, addrLen);

    if (local) {
	freeGlobal(local, localLen);
    }
    localLen = sizeof(localaddr) + 1;
    local = (char*)allocGlobal(localLen);
    memcpy(local, localaddr, localLen);

    moreAvail = available;
    return 0;
}

// this end of socket
/*virtual*/ SysStatus
FileLinuxPacket::getsocketname(char* addr, uval &addrLen)
{
    AutoLock<LockType> al(&objLock);
    tassert(local, err_printf("local sockaddr is not available\n"));
    if (addrLen < localLen) {
	return _SERROR(2044, 0, EINVAL);
    }

    if (local) {
	addrLen = localLen;
	memcpy(addr, local, addrLen);
	return 0;
    }
    return _SERROR(2045, 0, ENOTSOCK);
}

// other end of socket
/*virtual*/ SysStatus
FileLinuxPacket::getpeername(char* addr, uval &addrLen)
{
    AutoLock<LockType> al(&objLock);
    if (addrLen < remoteLen) {
	return _SERROR(2040, 0, EINVAL);
    }

    if (remote) {
	addrLen = remoteLen;
	memcpy(addr, remote, addrLen);
	return 0;
    }
    return _SERROR(1830, 0, ENOTCONN);
}

/* virtual */ SysStatusUval
FileLinuxPacket::destroy()
{
    lock();
    if (remote) {
	freeGlobal(remote,remoteLen);
	remote = NULL;
	remoteLen = 0;
    }
    if (local) {
	freeGlobal(local,localLen);
	local = NULL;
	localLen = 0;
    }
    unLock();
    return FileLinuxStream::destroy();		// no special state
}

/* virtual */ SysStatus
FileLinuxPacket::ioctl(uval request, va_list args)
{
    SysStatus rc = 0;		// Success

    uval size = 0 ;
    switch (request) {
	case SIOCGIFCONF:{
	    struct ifconf *ifc = va_arg(args, struct ifconf*);
	    size = ifc->ifc_len;
	    if (size > FileLinux::SMTCutOff) {
		size = FileLinux::SMTCutOff;
	    }

	    rc = sps._ioctl(request, size, ifc->ifc_buf);
	    tassertMsg(_SUCCESS(rc),"SIOCGIFCONF should succeed\n");
	    ifc->ifc_len = size;
	    return rc;
	    break;
	}
	case SIOCGIFTXQLEN:
	case SIOCGIFFLAGS:
	case SIOCSIFFLAGS:
	case SIOCGIFMETRIC:
	case SIOCSIFMETRIC:
	case SIOCGIFMEM:
	case SIOCSIFMEM:
	case SIOCGIFMTU:
	case SIOCSIFMTU:
	case SIOCSIFLINK:
	case SIOCGIFHWADDR:
	case SIOCSIFHWADDR:
	case SIOCSIFMAP:
	case SIOCGIFMAP:
	case SIOCSIFSLAVE:
	case SIOCGIFSLAVE:
	case SIOCGIFINDEX:
	case SIOCGIFNAME:
	case SIOCGIFCOUNT:
	case SIOCSIFHWBROADCAST:
	    size = sizeof(struct ifreq);
	    break;
	case SIOCGETTUNNEL:
	case SIOCADDTUNNEL:
	case SIOCDELTUNNEL:
	case SIOCCHGTUNNEL:
	    size = sizeof(struct ip_tunnel_parm);
	default:
	    rc = FileLinuxStream::ioctl(request,args);
	    return rc;
    }

    char* x = va_arg(args,char*);
    rc = sps._ioctl(request, size, x);
    return rc;
}

/* virtual */ SysStatus
FileLinuxPacket::getsockopt(sval level, sval optname, const void *optval,
			    uval *optlen)
{
    SysStatus rc;

    rc =sps._getsockopt(level, optname, (char *)optval, optlen);
    return rc;
}
