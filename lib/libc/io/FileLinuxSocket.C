/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxSocket.C,v 1.100 2005/08/17 13:53:38 butrico Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileLinuxSocket.H"
#include <cobj/CObjRootSingleRep.H>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "SocketServer.H"
#include "PacketServer.H"
#include "FileLinuxPacket.H"
#include "FileLinuxSocketUnix.H"
#include "FileLinuxSocketInet.H"

extern "C" {
#include <linux/types.h>
#include <netinet/ip.h>
#include <linux/if_tunnel.h>
}

/*static*/ SysStatus
FileLinux::Socket(FileLinuxRef& newSocket,
		  sval domain, sval type, sval protocol)
{
    switch (type) {
	case SOCK_STREAM:
	    return FileLinuxSocket::Create(newSocket, domain, type, protocol);
	    break;
	case SOCK_RAW:
	case SOCK_DGRAM:
	    return FileLinuxPacket::Create(newSocket, domain, type, protocol);
	    break;
	default:
	    passertWrn(0, "unsupported socket type %ld\n", type);
	    return _SERROR(1398, 0, EAFNOSUPPORT);
    }
    return 0;
}

/*static*/ SysStatus
FileLinux::SocketPair(FileLinuxRef& newSocket1,
		      FileLinuxRef& newSocket2,
		      sval domain, sval type, sval protocol)
{
    switch (type) {
	case SOCK_STREAM:
	    // fix me
	    return FileLinuxStream::SocketPair(newSocket1, newSocket2,
					       domain, type, protocol);
	    break;
	case SOCK_RAW:
	case SOCK_DGRAM:
	    return FileLinuxPacket::SocketPair(newSocket1, newSocket2,
					   domain, type, protocol);
	    break;
	default:
	    passertWrn(0, "unsupported socket type %ld\n", type);
	    return _SERROR(2887, 0, EAFNOSUPPORT);
    }
    return 0;
}

/* static */ SysStatus
FileLinuxSocket::Create(FileLinuxRef &newSocket, sval domain,
			sval type, sval protocol)
{
    SysStatus rc;

    switch (domain) {
      case AF_NETLINK:
      case AF_INET:
	rc = FileLinuxSocketInet::Create(newSocket, domain, type, protocol);
	break;
      case AF_UNIX:
	rc = FileLinuxSocketUnix::Create(newSocket, domain, type, protocol);
	break;
    default:
	passertWrn(0, "unsupported socket domain %ld\n", domain);
	rc = _SERROR(2666, 0, EAFNOSUPPORT);
    }

    return rc;
}

/* virtual */ SysStatus
FileLinuxSocket::dup(FileLinuxRef& newSocket)
{
    if (traceThisSocket) err_printf("%s: pid=0x%lx  %p\n",
		__PRETTY_FUNCTION__, DREFGOBJ(TheProcessRef)->getPID(), this);
    SysStatus rc;
    ObjectHandle copyOH;
    rc = giveAccessByClient(copyOH, DREFGOBJ(TheProcessRef)->getPID());
    if (_FAILURE(rc)) return rc;

    rc = CreateInternal(newSocket, copyOH, stubHolder->transType, openFlags,
		        sDomain, sType, sProtocol,
			local, localLen, remote, remoteLen);

    return rc;
}

/*static*/ SysStatus
FileLinuxSocket::CreateInternal(FileLinuxRef& newSocket, ObjectHandle stubOH,
				uval clientType, uval oflags,
				sval domain, sval type, sval protocol,
				struct sockaddr *localAddr, uval localAddrLen,
				struct sockaddr *remoteAddr,uval remoteAddrLen,
				uval debugFlag /* = 0 */)
{
    SysStatus rc;
    FileLinuxSocket *newp = NULL;
    switch(type) {
    case SOCK_STREAM:
	switch (domain) {
	case AF_NETLINK:
	case AF_INET:
	    newp = (FileLinuxSocket*) new FileLinuxSocketInet(domain, type, 
							protocol);
	    break;
	case AF_UNIX:
	    newp = (FileLinuxSocket*) new FileLinuxSocketUnix(domain, type, 
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

    ObjectHandle dummy;
    newp->traceThisSocket = debugFlag;

    if (newp->traceThisSocket)
	err_printf("%s: creating with oh<CommID 0x%lx  xhandle 0x%lx>\n",
			   "FileSocket::CreateInternal", 
			   stubOH.commID(), stubOH.xhandle());

    // using dummy tells init not to make a stubHolder
    dummy.init();
    newp->init(dummy, NULL);

    newp->sss.setOH(stubOH);
    switch (clientType) {
    case TransStreamServer::TRANS_VIRT:
	newp->stubHolder = new TransVirtStream(stubOH);
	break;
    case TransStreamServer::TRANS_PPC:
	newp->stubHolder = new TransPPCStream(stubOH);
	break;
    default:
	passertMsg(0, "Unknown socket client type: %ld\n", clientType);
    }

    newp->setFlags(oflags);

    newp->remoteLen = remoteAddrLen;
    if  (newp->remoteLen) {
	newp->remote = (struct sockaddr*)allocGlobal(newp->remoteLen);
	tassertMsg(newp->remote != NULL, "nomem\n");
	memcpy(newp->remote, remoteAddr, newp->remoteLen);
    } else {
	newp->remote = NULL;
    }

    newp->localLen = localAddrLen;
    if  (newp->localLen) {
	newp->local =  (struct sockaddr*)allocGlobal(newp->localLen);
	tassertMsg(newp->local != NULL, "nomem\n");
	memcpy(newp->local, localAddr, newp->localLen);
    } else {
	newp->local = NULL;
    }

    newSocket = (FileLinuxRef)newp->getRef();

    // register object for callback by server
    rc = newp->registerCallback();
    tassertRC(rc, "do cleanup code\n");

    return 0;
}

/*
 * for sockets, stat returns a canned and useless result.
 * every system seems different.  linux, for example, seems
 * to return times, AIX doesn't
 */
/*virtual*/ SysStatus
FileLinuxSocket::getStatus(FileLinux::Stat *status)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
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
FileLinuxSocket::setsockopt(uval level, uval optname, const void *optval,
			    uval optlen)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
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
	tassertMsg(0, "Socket option (%ld) not supported (level %ld).",
				optname, level);
	break;
    }

    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    retvalue = sss._setsockopt(level, optname, (char *)optval, optlen);
    return (retvalue);
}

/* virtual */ SysStatus
FileLinuxSocket::getsockopt(uval level, uval optname, const void *optval,
			    uval *optlen)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    SysStatus rc;
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    rc =sss._getsockopt(level, optname, (char *)optval, optlen);
    return rc;
}

// this end of socket
/*virtual*/ SysStatus
FileLinuxSocket::getsocketname(char* addr, uval &addrLen)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    SysStatus rc = 0;
    AutoLock<LockType> al(&objLock);

    if (local) {
	if (addrLen < localLen) {
	    return _SERROR(2041, 0, EINVAL);
	}
	addrLen = localLen;
	memcpy(addr, local, addrLen);
	return 0;
    }

    rc = sss._getname(0, addr, addrLen);
    if (_SUCCESS(rc)) {
	local = (struct sockaddr*)allocGlobal(addrLen);
	localLen = addrLen;
	memcpy(local, addr, addrLen);  // fixme must check len
    }
    return rc;
}

// other end of socket
/*virtual*/ SysStatus
FileLinuxSocket::getpeername(char* addr, uval &addrLen)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    AutoLock<LockType> al(&objLock);

    if (addrLen<remoteLen) {
	return _SERROR(2043, 0, EINVAL);
    }

    if (remote) {
	addrLen = remoteLen;
	memcpy(addr, remote, addrLen);
	return 0;
    }
    return _SERROR(2042, 0, ENOTCONN);
}

/*virtual*/ SysStatus
FileLinuxSocket::listen(sval backlog)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    AutoLock<LockType> al(&objLock); // locks now, unlocks on return
    return sss._listen(backlog);
}

/*virtual*/ SysStatus
FileLinuxSocket::accept(FileLinuxRef& newSocket, ThreadWait **tw)
{
    if (traceThisSocket) err_printf("%s: pid=0x%lx  %p\n",
		__PRETTY_FUNCTION__, DREFGOBJ(TheProcessRef)->getPID(), this);
    SysStatus rc;
    ObjectHandle stubOH;
    objLock.acquire();			// grab my own lock

    GenState moreAvail;

    struct sockaddr tmp;

    //"Remote" peer name can't be used for anything,
    //use it here to pass the correct remote name to the new socket
    uval len = sizeof(struct sockaddr);

  retry:
    rc = checkAvail(tw, READ_AVAIL);

    if (unlikely(_FAILURE(rc))) {
	goto abort;
    };
    if (unlikely(_SGETUVAL(rc)==0)) {
	// Not ready;
	rc = _SERROR(2300, 0, EWOULDBLOCK);
	goto abort;
    }

    rc = sss._accept(stubOH, (char*)&tmp, len, moreAvail);

    if (likely(_SUCCESS(rc))) {
	setAvailable(moreAvail);
	if (likely(stubOH.valid())) {
	    rc = FileLinuxSocket::CreateInternal(newSocket, stubOH,
				stubHolder->transType, openFlags,
				sDomain, sType, sProtocol, 
				local, localLen, &tmp, len);
            if (traceThisSocket)
		err_printf("%s: %p  newSocket 0x%p "
			   "new oh<CommID 0x%lx  xhandle 0x%lx>\n",
			   __PRETTY_FUNCTION__, this, newSocket,
			   stubOH.commID(), stubOH.xhandle());
	} else {
	    goto retry;
	}
    }

  abort:

    objLock.release();
    return rc;
}

/* virtual */ SysStatus
FileLinuxSocket::ioctl(uval request, va_list args)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    SysStatus rc = 0;
    uval size = 0 ;
    switch (request) {
	case SIOCGIFCONF:{
	    struct ifconf *ifc = va_arg(args, struct ifconf*);
	    uval len = ifc->ifc_len;
	    if (len > FileLinux::SMTCutOff) {
		len = FileLinux::SMTCutOff;
	    }

	    rc = sss._ioctl(request, len, ifc->ifc_buf);
	    tassertMsg(_SUCCESS(rc),"SIOCGIFCONF should succeed\n");
	    ifc->ifc_len = len;
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
    rc = sss._ioctl(request, size, x);
    return rc;
}

/* virtual */ SysStatusUval
FileLinuxSocket::sendmsg(struct msghdr &msg, uval flags, ThreadWait **tw,
			 GenState &moreAvail)
{
    SysStatus rc;

    if (traceThisSocket) err_printf("%s: pid=0x%lx  %p\n",
		__PRETTY_FUNCTION__, DREFGOBJ(TheProcessRef)->getPID(), this);
    if (!remote) {
	rc =  _SERROR(2404, 0, ENOTCONN);
    } else {
	rc = FileLinuxStream::sendmsg(msg, flags, tw, moreAvail);
    }

    if (traceThisSocket && _FAILURE(rc)) {
	 err_printf("%s: %p ", __PRETTY_FUNCTION__, this);
	_SERROR_EPRINT(rc);
    }
    
    return rc;
}

/* virtual */ SysStatusUval
FileLinuxSocket::recvmsg(struct msghdr &msg, uval flags,
			 ThreadWait **tw, GenState &moreAvail)
{
    if (traceThisSocket) err_printf("%s: pid=0x%lx  %p\n",
		__PRETTY_FUNCTION__, DREFGOBJ(TheProcessRef)->getPID(), this);
    SysStatusUval rc = 0;

    if (!remote) {
	rc = _SERROR(2405, 0, ENOTCONN);
    } else {
	rc = FileLinuxStream::recvmsg(msg, flags, tw, moreAvail);
    }

    if (traceThisSocket && _FAILURE(rc)) {
	 err_printf("%s: %p ", __PRETTY_FUNCTION__, this);
	_SERROR_EPRINT(rc);
    }
    
    return rc;
}

/* virtual */ SysStatus
FileLinuxSocket::lazyGiveAccess(sval fd)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    SysStatus rc;
    LazyReOpenData data;
    data.openFlags	= openFlags;
    data.transType	= stubHolder->transType;
    data.domain		= sDomain;
    data.type		= sType;
    data.protocol	= sProtocol;
    if (remoteLen) {
	memcpy(&data.remote, remote, remoteLen);
    }
    data.remoteLen = remoteLen;
    if (localLen) {
	memcpy(&data.local, local, localLen);
    }
    data.localLen = localLen;

    // call server to transfer to my process
    rc = sss._lazyGiveAccess(fd, FileLinux_SOCKET, -1,
			     (char *)&data, sizeof(LazyReOpenData));
    tassertMsg(_SUCCESS(rc), "?");

    // Detach should destroy --- return 1
    rc = detach();
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==0, "detach failure %lx\n",rc);

    return rc;
}

/* virtual */ SysStatus
FileLinuxSocket::shutdown(uval how)
{
    if (traceThisSocket) err_printf("%s: %p\n", __PRETTY_FUNCTION__, this);
    SysStatus rc;

    rc =sss._shutdown(how);
    return rc;
}
