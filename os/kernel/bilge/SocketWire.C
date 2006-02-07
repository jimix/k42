/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SocketWire.C,v 1.112 2005/07/15 17:14:27 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a socket interface using thinwire. Read
 * and write requests that come in from user level are only handled if
 * there is no blocking.  Internal requests (in this address space)
 * are handled by blocking the requesting thread.
 *
 * As temporary cludge, not yet tying into user level IO derivatoin tree.
 *
 * Note, for now using BlockThreadQueues, for locality should really put queue
 * in object itself, write a generic implementation of this.
 * **************************************************************************/

#ifdef FIXME_BUT_STOP_COMPAINING
#ifdef DWARF_HACK
#warning /u/kitchawa/knightly/build/install/include/misc/HashSimple.H:232: Internal compiler error in dwarf2out_finish, at dwarf2out.c:10054
#endif // DWARF_HACK
#endif // FIXME_BUT_STOP_COMPAINING

#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include <bilge/ThinIP.H>
#include <bilge/SocketWire.H>
#include <sync/BlockedThreadQueues.H>
#include <scheduler/Scheduler.H>
#include <trace/traceIO.h>
#include <io/FileLinux.H>
#include <io/Socket.H>
#include <meta/MetaPacketServer.H>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <mem/Pin.H>
#include <io/FileLinuxStream.H>

#define INSTNAME SocketWire
#include <tmpl/TplXSocketServer.I>
#include <tmpl/TplXPacketServer.I>



/* static */ SysStatus
SocketWire::Create(ObjectHandle &oh, uval &clientType,
		   uval domain, uval type, uval protocol,
		   ProcessID pid)
{
    if (domain!=AF_INET) {
	return _SERROR(2034, 0, EAFNOSUPPORT);
    }

    clientType = TransStreamServer::TRANS_PPC;
//    clientType = TransIOServer::TRANS_VIRT;
    SocketWire *nSck = new SocketWire();
    tassert(nSck, err_printf("alloc of LinuxTCP failed.\n"));

    nSck->init();

    SocketWireRef oref = (SocketWireRef)CObjRootSingleRep::Create(nSck);

    SysStatus rc = ThinIP::Socket(nSck->dataSocket,
				  (StreamServerRef)oref, type);
    if (_FAILURE(rc)) {
	DREF(oref)->destroy();
	return _SERROR(2772, 0, ENETUNREACH);
    }
    switch (type) {
	case SOCK_STREAM:
	    rc = DREF(oref)->giveAccessByServer(oh, pid,
						MetaSocketServer::typeID());
	    break;
	case SOCK_RAW:
	case SOCK_DGRAM:
	    rc = DREF(oref)->giveAccessByServer(oh, pid,
						MetaPacketServer::typeID());
	    break;
    }
    return rc;
}

/* virtual */ SysStatus
SocketWire::giveAccessSetClientData(ObjectHandle &oh,
				    ProcessID toProcID,
				    AccessRights match,
				    AccessRights nomatch,
				    TypeID type)
{
    SysStatus rc = SocketServer::giveAccessSetClientData(oh, toProcID,
							 match,
							 nomatch, type);

    _IF_FAILURE_RET(rc);


    ClientData *cd = clnt(oh.xhandle());

 retry:
    rc = MemTrans::GetMemTrans(cd->mtr, cd->smtXH, toProcID, 1234);

    // if the MemTrans already exists...
    if (_SUCCESS(rc)) {
	cd->memEventHandler = (IOMemEvents*)DREF(cd->mtr)->getMTEvents();
	return 0;
    }

    if (_SGENCD(rc)!=ENOENT) {
	return rc;
    }

    // The SMT object doesn't exists
    cd->memEventHandler = new StreamServer::IOMemEvents;
    rc = MemTrans::Create(cd->mtr, toProcID, 64 * PAGE_SIZE,
			  cd->smtXH, cd->memEventHandler, 1234);

    if (_FAILURE(rc) && _SGENCD(rc)==EALREADY) {
	//oops ... somebody just created it
	cd->mtr = NULL;
	delete cd->memEventHandler;
	goto retry;
    }

    cd->smtXH = oh.xhandle();

    return 0;
}


/* virtual */ SysStatus
SocketWire::detach()
{
    tassert(0, err_printf("detach called, must do reference count\n"));
    return 0;
}

/* virtual */ SysStatus
SocketWire::destroy()
{
    SysStatus rc;

    // remove all ObjRefs to this object
    rc = closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if (_FAILURE(rc)) {
	return _SCLSCD(rc) == 1 ? 0: rc;
    }

    ThinIP::Close(dataSocket);
    destroyUnchecked();

    return rc;
}

/* virtual */ SysStatus
SocketWire::_bind(__inoutbuf(addrLen:addrLen:addrLen) char* addr,
		  __inout uval &addrLen, __XHANDLE xhandle)
{
    TraceOSIOWireBind((uval)this);
    SysStatus rc;
    if (addrLen< sizeof(struct sockaddr_in)) {
	return _SERROR(2566, 0,EINVAL);
    }

    lock();
    rc = ThinIP::Bind(dataSocket, (char*) addr, addrLen);
    unlock();
    return rc;
}

/* virtual */ SysStatus
SocketWire::_setsockopt(__in uval level, __in uval optname,
			__inbuf(optlen) char *optval,
			__in uval optlen, __XHANDLE xhandle)
{
    // NEED TO TRANSLATE TO NATIVE OPTIONS, SO IGNORING FOR NOW
    return 0;
}


/* virtual */ void
SocketWire::calcAvailable(GenState& avail, StreamServer::ClientData* client)
{
    if (!dataIsAvailable) ThinIP::DoPolling();
    if (dataIsAvailable) {
	avail.state = FileLinux::READ_AVAIL | FileLinux::WRITE_AVAIL;
    } else {
	avail.state = FileLinux::WRITE_AVAIL;
    }
}


/* virtual */ SysStatusUval
SocketWire::recvfrom(struct iovec *vec, uval veclen, uval flags,
		     char *addr, uval &addrLen, GenState &moreAvail, 
		     void *controlData, uval &controlDataLen,
		     __XHANDLE xhandle)
{
    SysStatusUval rc;
    uval len = vecLength(vec,veclen);

    TraceOSIOWireRecvFrom((uval)this);

    controlDataLen = 0; /* setting to zero, since no control data */

    lock();
    // performance hack, poll unlocked to give us a chance
    if (!dataIsAvailable) {
	ThinIP::DoPolling();
    }

    if (!dataIsAvailable) {
	moreAvail.state = FileLinux::WRITE_AVAIL;
	clnt(xhandle)->setAvail(moreAvail);
	unlock();
	return 0;
    }

    char *buf = (char*)allocPinnedGlobalPadded(len);
    rc = ThinIP::Recvfrom(dataSocket, buf, len, dataIsAvailable,
			  addr, addrLen);

    moreAvail.state = FileLinux::WRITE_AVAIL;
    //Return 0 bytes read + correct moreAvail on this error
    if (_FAILURE(rc) && (_SGENCD(rc) == EWOULDBLOCK)) {
	rc = 0;
    } else if (_SUCCESS(rc)) {
	if (dataIsAvailable) {
	    if (_SGETUVAL(rc) == 0) {
		err_printf("Warning: trapping EOF condition in SocketWire\n");
		moreAvail.state |= FileLinux::ENDOFFILE;
	    } else {
		moreAvail.state |= FileLinux::READ_AVAIL;
	    }
	}
    }
    if (_SUCCESS(rc)) {
	clnt(xhandle)->setAvail(moreAvail);
    }

    memcpy_toiovec(vec, buf, veclen, len);
    freePinnedGlobalPadded(buf,len);
    unlock();
    return rc;
}


/* virtual */ SysStatusUval
SocketWire::sendto(struct iovec *vec, uval veclen, uval flags,
		   const char *addr, uval addrLen, GenState &moreAvail, 
		   void *controlData, uval controlDataLen,
		   __XHANDLE xhandle)
{
    SysStatusUval rc;

    TraceOSIOWireSendTo((uval)this);

    tassertMsg((controlDataLen == 0), "oops\n");

    lock();
    uval len = vecLength(vec,veclen);
    char *buf = (char*)allocPinnedGlobalPadded(len);
    memcpy_fromiovec(buf, vec, veclen, len);
    rc = ThinIP::Sendto(dataSocket, buf, len, (char*)addr, addrLen);

    if (_SUCCESS(rc)) {
	if (dataIsAvailable) {
	    moreAvail.state = FileLinux::READ_AVAIL | FileLinux::WRITE_AVAIL;
	} else {
	    moreAvail.state = FileLinux::WRITE_AVAIL;
	}

	clnt(xhandle)->setAvail(moreAvail);
    }

    freePinnedGlobalPadded(buf,len);
    TraceOSIOWireSendToDone((uval)this);
    unlock();
    return rc;
}


/* virtual */ SysStatusUval
SocketWire::_recvfromVirt(__inbuf(len) struct iovec* vec, __in uval veclen,
			  __in uval flags,
			  __outbuf(addrLen:addrLen) char *addr,
			  __inout uval &addrLen,
			  __inout GenState &moreAvail,
			  __XHANDLE xhandle)
{
    uval numPages = pageSpan(vec, veclen);
    struct iovec *localVec = (iovec*)alloca(sizeof(struct iovec)*numPages);

    PinnedMapping* pages =
	(PinnedMapping*)alloca(sizeof(PinnedMapping)*numPages);
    memset(pages, 0, sizeof(PinnedMapping)*numPages);

    BaseProcessRef bpref;
    ProcessID pid = XHandleTrans::GetOwnerProcessID(xhandle);
    SysStatusUval rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, bpref);
    _IF_FAILURE_RET(rc);

    rc = PinnedMapping::pinToIOVec((ProcessRef)bpref, vec, veclen, 1,
				   pages, localVec);
    _IF_FAILURE_RET_VERBOSE(rc);

    uval realLength = _SGETUVAL(rc);

    void *controlData = NULL;
    uval controlDataLen = 0;
    rc = recvfrom(localVec, realLength, flags, addr, addrLen, moreAvail, 
		  controlData, controlDataLen, xhandle);
    tassertMsg( (controlDataLen == 0), "oops\n");
    for (uval i = 0; i< realLength; ++i) {
	pages[i].unpin();
    }
    return rc;
}

/* virtual */ SysStatusUval
SocketWire::_sendtoVirt(__inbuf(len) struct iovec* vec, __in uval veclen,
			__in uval flags,
			__inbuf(addrLen) const char *addr,
			__in uval addrLen,
			__inout GenState &moreAvail,
			__XHANDLE xhandle)
{
    volatile uval numPages = pageSpan(vec, veclen);
    struct iovec *localVec = (iovec*)alloca(sizeof(struct iovec)*numPages);

    PinnedMapping* pages =
	(PinnedMapping*)alloca(sizeof(PinnedMapping)*numPages);
    memset(pages, 0, sizeof(PinnedMapping)*numPages);


    BaseProcessRef bpref;
    ProcessID pid = XHandleTrans::GetOwnerProcessID(xhandle);
    SysStatusUval rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, bpref);
    _IF_FAILURE_RET(rc);

    rc = PinnedMapping::pinToIOVec((ProcessRef)bpref,
				   vec, veclen, 0,
				   pages, localVec);

    _IF_FAILURE_RET_VERBOSE(rc);

    uval realLength = _SGETUVAL(rc);

    void *controlData = NULL;
    uval controlDataLen = 0;
    rc = sendto(localVec, realLength, flags, addr, addrLen, moreAvail, 
		controlData, controlDataLen, xhandle);

    for (uval i = 0; i< realLength; ++i) {
	pages[i].unpin();
    }
    return rc;
}

/* virtual */ SysStatusUval
SocketWire::_readSMT(uval &offset, uval len, GenState &moreAvail,
		     __XHANDLE xhandle)
{
    ClientData *cd = (ClientData*)clnt(xhandle);
    uval addr;
    uval size = len;
    tassertMsg(cd->mtr, "No SMT defined\n");
    SysStatus rc=0;
    offset = ~0ULL;

    rc = DREF(cd->mtr)->allocPagesLocal(addr, len);
    if (_FAILURE(rc) && _SCLSCD(rc)==MemTrans::MemTransErr) {
	rc = cd->memEventHandler->pokeRing(cd->mtr,cd->memEventHandler->other);
	if (_SUCCESS(rc)) {
	    rc = DREF(cd->mtr)->allocPagesLocal(addr, len);
	}
    }

    _IF_FAILURE_RET(rc);

    if (len<size) {
	size = len;
    }

    struct iovec vec;
    vec.iov_base = (void*)addr;
    vec.iov_len = size;
    uval addrLen = 0;
    void *controlData = NULL;
    uval controlDataLen = 0;
    rc = recvfrom(&vec, 1, 0, NULL, addrLen, moreAvail, controlData, 
		  controlDataLen, xhandle);

    offset = DREF(cd->mtr)->localOffset(addr);
    if (_FAILURE(rc) || _SGETUVAL(rc)==0) {
	DREF(cd->mtr)->freePage(offset);
    }

    return rc;
}

/* virtual */ SysStatusUval
SocketWire::_writeSMT(uval offset, uval len,
		      GenState &moreAvail, __XHANDLE xhandle)
{
    SysStatusUval rc;
    SysStatus rc2;
    ClientData *cd = (ClientData*)clnt(xhandle);
    tassertMsg(cd->mtr, "No SMT defined\n");

    // Handle back-pressure: see if we can free the last offset we
    // had to deal with
    if (cd->blocked != NOPRESSURE) {
	rc = DREF(cd->mtr)->insertToRing(cd->memEventHandler->other,
					 cd->blocked);
	if (_FAILURE(rc)) {
	    return rc;
	}
	cd->blocked = NOPRESSURE;
    }

    const char *srcAddr = (const char*)
	DREF(cd->mtr)->remotePtr(offset, cd->memEventHandler->other);

    struct iovec vec;
    vec.iov_base = (void*)srcAddr;
    vec.iov_len = len;
    if (!srcAddr) {
	rc = _SERROR(2021, 0, EINVAL);
	breakpoint();
    } else {
	void *controlData = NULL;
	uval controlDataLen = 0;
	rc = sendto(&vec, 1, 0, NULL, 0, moreAvail, controlData, controlDataLen,
		    xhandle);
    }


    rc2 = DREF(cd->mtr)->insertToRing(cd->memEventHandler->other, offset);
    // Remember the offset for freeing later
    if (_FAILURE(rc2)) {
	cd->blocked = offset;
    }
    return rc;
}


/* virtual */ SysStatusUval
SocketWire::_recvfromSMT(__out uval &offset,
			 __in uval len,
			 __outbuf(addrLen:addrLen) char* addr,
			 __inout uval &addrLen,
			 __out GenState &moreAvail,
			 __XHANDLE xhandle)
{
    ClientData *cd = (ClientData*)clnt(xhandle);

    uval ptr;
    uval size = len;
    offset = ~0ULL;
    tassertMsg(cd->mtr, "No SMT defined\n");
    SysStatus rc = DREF(cd->mtr)->allocPagesLocal(ptr, len);
    if (_FAILURE(rc) && _SCLSCD(rc)==MemTrans::MemTransErr) {
	rc = cd->memEventHandler->pokeRing(cd->mtr,cd->memEventHandler->other);
	if (_SUCCESS(rc)) {
	    rc = DREF(cd->mtr)->allocPagesLocal(ptr, len);
	}
    }

    _IF_FAILURE_RET(rc);

    if (len<size) {
	size = len;
    }

    struct iovec vec;
    vec.iov_base = (void*)ptr;
    vec.iov_len = size;
    void *controlData = NULL;
    uval controlDataLen = 0;
    rc = recvfrom(&vec, 1, 0, addr, addrLen, moreAvail, controlData, 
		  controlDataLen, xhandle);

    if (_FAILURE(rc) || _SGETUVAL(rc)==0) {
	DREF(cd->mtr)->freePage(ptr);
    } else {
	offset = DREF(cd->mtr)->localOffset(ptr);
    }

    return rc;
}


/* virtual */ SysStatusUval
SocketWire::_sendtoSMT(__in uval offset,
		       __in uval len,
		       __inbuf(addrLen) const char *addr,
		       __in uval addrLen,
		       __out GenState &moreAvail,
		       __XHANDLE xhandle)
{
    SysStatusUval rc;
    SysStatus rc2;
    ClientData *cd = (ClientData*)clnt(xhandle);
    tassertMsg(cd->mtr, "No SMT defined\n");

    // Handle back-pressure: see if we can free the last offset we
    // had to deal with
    if (cd->blocked != NOPRESSURE) {
	rc = DREF(cd->mtr)->insertToRing(cd->memEventHandler->other,
					 cd->blocked);
	if (_FAILURE(rc)) {
	    return rc;
	}
	cd->blocked = NOPRESSURE;
    }

    struct iovec vec;
    vec.iov_base =DREF(cd->mtr)->remotePtr(offset, cd->memEventHandler->other);
    vec.iov_len = len;
    if (!vec.iov_base) {
	rc = _SERROR(2020, 0, EINVAL);
    } else {
	void *controlData = NULL;
	uval controlDataLen = 0;
	rc = sendto(&vec, 1, 0, addr, addrLen, moreAvail, controlData, 
		    controlDataLen, xhandle);
    }

    rc2 = DREF(cd->mtr)->insertToRing(cd->memEventHandler->other, offset);

    // Remember the offset for freeing later
    if (_FAILURE(rc2)) {
	cd->blocked = offset;
    }
    return rc;
}


/* virtual */ SysStatus
SocketWire::_connect(__inbuf(addrLen) const char* addr, __in uval addrLen,
			__out GenState &moreAvail, __XHANDLE xhandle)
{
    TraceOSIOWireConnect((uval)this);

    if (addrLen< sizeof(struct sockaddr_in)) {
	return _SERROR(1834, 0,EINVAL);
    }

    lock();
    SysStatus rc = ThinIP::Connect(dataSocket, (char*)addr, addrLen);

    if ( _SUCCESS(rc) ) {
	if (dataIsAvailable) {
	    moreAvail.state = FileLinux::READ_AVAIL | FileLinux::WRITE_AVAIL;
	} else {
	    moreAvail.state = FileLinux::WRITE_AVAIL;
	}
    } else {
	moreAvail.state = FileLinux::INVALID;
    }

    clnt(xhandle)->setAvail(moreAvail);
    unlock();
    return rc;
}

/* virtual */ SysStatusUval
SocketWire::_ioctl(uval request, uval &size, char* arg, __XHANDLE xhandle)
{
    switch (request) {
    case SIOCGIFCONF:
    {
	struct ifconf *ifc;
	ifc = (struct ifconf*) arg;

	if (size < sizeof(struct ifreq)) {
	    return _SERROR(2703, 0, EINVAL);
	}
	size = sizeof(struct ifreq);

	// FIXME: How to get this info? just return enuff to make to
	// RPC routines happy. We will tell it about the loopback interface
	struct ifreq *ifr = (struct ifreq*)arg;
	strcpy(ifr->ifr_name, "lo0");
	ifr->ifr_addr.sa_family = AF_INET;

	return 0;
    }

    case SIOCGIFFLAGS:
    {
	struct ifreq *ifr;
	ifr = (struct ifreq *)arg;
	// FIXME: How to get this info? just return enuff to make to
	// RPC routines happy. We will tell it about the loopback interface
	if (strcmp(ifr->ifr_name, "lo0") == 0) {
	    ifr->ifr_flags = IFF_UP;
	} else {
	    return _SERROR(1718, 0, EINVAL);
	}
	return 0;
    }
    }
    return StreamServer::_ioctl(request, size, arg, xhandle);
}

void
SocketWire::ClassInit()
{
    MetaSocketServer::init();
}



/* virtual */ SysStatus
SocketWire::_listen(__in sval backlog)
{
    TraceOSIOWireListen((uval)this);
    lock();
    SysStatus rc = ThinIP::Listen(dataSocket, backlog);
    unlock();
    return rc;
}


/* virtual */ SysStatus
SocketWire::_accept(__out ObjectHandle &oh,
		    __inoutbuf(len:len:len) char* addr,
		    __inout uval& len,
		    __out GenState &moreAvail,
		    __XHANDLE xhandle)
{
    SysStatus rc=0;
    SocketWireRef sref;
    SocketWire *nSck;

    moreAvail.state = 0;

    TraceOSIOWireAccept((uval)this);

    lock();

    // performance hack, poll unlocked to give us a chance
    if (!dataIsAvailable) {
	ThinIP::DoPolling();
    }


    if (!dataIsAvailable) {
	Scheduler::Yield();		// FIXME: For spinning app need to
					// progress
	moreAvail.state = 0;
	clnt(xhandle)->setAvail(moreAvail);
	unlock();
	// Return success, invalid OH
	oh.init();
	return 0;
    }


    nSck = new SocketWire();

    // lock new socket so can't get call back until initialization complete
    nSck->init();

    sref = (SocketWireRef)CObjRootSingleRep::Create(nSck);

    rc = ThinIP::Accept(dataSocket, nSck->dataSocket, (StreamServerRef)sref,
			dataIsAvailable);
    tassert(_SUCCESS(rc), err_printf("woops write cleanup code for accept\n"));


    if (dataIsAvailable) {
	moreAvail.state = FileLinux::READ_AVAIL;
    }
    clnt(xhandle)->setAvail(moreAvail);

    if (_FAILURE(rc)) {
	unlock();
	return rc;
    }

    rc = DREF(sref)->giveAccessByServer(oh,
				    XHandleTrans::GetOwnerProcessID(xhandle),
				    MetaSocketServer::typeID());
    if (_FAILURE(rc)) {
	tassert(0, err_printf("FIXME: cleanup\n"));
	unlock();
	return rc;
    }
    unlock();
    return 0;
}

/* virtual */ SysStatus
SocketWire::_getname(__in uval peer,
		     __outbuf(addrLen:addrLen) char *buf,
		     __inout uval &addrLen)
{
    // Not getsockname or getpeername in ThinIP
    return _SERROR(2410, 0, EOPNOTSUPP);
}
