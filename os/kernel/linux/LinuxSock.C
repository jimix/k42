/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001, 2002, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LinuxSock.C,v 1.21 2005/07/15 17:14:30 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a socket interface using Linux sockets.
 ****************************************************************************/
#define __KERNEL__
#include "kernIncs.H"

#define eieio __k42_eieio
#include <misc/hardware.H>
#undef eieio
extern "C" {
#include <linux/linkage.h>
#define ffs __lk_ffs
#include <asm/bitops.h>
#undef ffs
#include <linux/socketlinux.h>
#include <linux/sockios.h>
#include <linux/thread_info.h>
#include <asm/current.h>
}
#undef __KERNEL__

#include <net/if.h>
#include <cobj/CObjRootSingleRep.H>
#include <cobj/CObjRoot.H>
#include "LinuxSock.H"
#include <sync/BlockedThreadQueues.H>
#include <scheduler/Scheduler.H>
#include <io/FileLinux.H>
#include <lk/LinuxEnv.H>

#include <io/StreamServer.H>
#include <cobj/XHandleTrans.H>
#include <scheduler/Scheduler.H>
#include <trace/traceLinux.h>
#include <mem/Pin.H>
#include <io/FileLinuxStream.H>
// The LinuxSock object provides interfaces for two distinct types of
// PPC interfaces -- SocketServer and PacketServer.  We can't
// have it inherit from both of them, so we will use a template to
// create the Meta and X objects necessary for a LinuxSock object to
// take on either personality.

// Here we instantiate the templated objects that provide out Meta and
// X object implementation. Note that the actual creation of these
// objects is triggered by the init() calls in LinuxSock::ClassInit()

//Must define INSTNAME to properly instantiate the static functions

#define INSTNAME LinuxSock
#include <meta/TplMetaPacketServer.H>
#include <xobj/TplXPacketServer.H>
#include <tmpl/TplXPacketServer.I>
#include <meta/TplMetaSocketServer.H>
#include <xobj/TplXSocketServer.H>
#include <tmpl/TplXSocketServer.I>


typedef TplMetaPacketServer<LinuxSock> MetaUDPServer;
typedef TplMetaSocketServer<LinuxSock> MetaTCPServer;

// This is a limit on how many iovecs and PinnedMappings we allocate
// using alloca
const uval PAGE_DESCS_ON_STACK = 16;

void
LinuxSock::ClassInit()
{
    MetaPacketServer::init();
    MetaSocketServer::init();
    MetaUDPServer::init();
    MetaTCPServer::init();
}


void
LinuxSock::updateStatus()
{
    _ASSERT_HELD(_lock);
    SocketLinux_poll(dataSocket, &status);
}

uval
LinuxSock::trylock()
{
    return _lock.tryAcquire();
}

/*virtual*/ void
LinuxSock::lock()
{
    _lock.acquire();
}

/*virtual*/ void
LinuxSock::unlock()
{
  restart:
    if (refreshNeeded) {
	refreshNeeded = 0;
	updateStatus();
	locked_signalDataAvailable();
    }

    /*
     * Release the lock.  Then check refreshNeeded again.  We have to
     * synchronize the memory accesses to the lock and refreshNeeded locations.
     * The normal lock synchronization operations are not sufficient because
     * refreshNeeded is modified outside the protection of the lock.
     */
    _lock.release();
    SyncAfterAcquire();
    if (refreshNeeded) {
	_lock.acquire();
	goto restart;
    }
}

/*virtual*/ SysStatus
LinuxSock::async_signalDataAvailable()
{
    TraceOSLinuxDataReady((uval)dataSocket);

    /*
     * Set refreshNeeded.  Then try to get the lock.  We have to synchronize
     * the memory accesses to the lock and refreshNeeded locations.
     */
    refreshNeeded = 1;
    SyncBeforeRelease();
    if (trylock()) {
	// We got the lock.  Unlock will do the refresh.
	unlock();
    } else {
	// Someone else has the lock.  They'll do the refresh.
    }

    return 0;
}

extern "C" void
SocketLinuxNotify(void *oref)
{
    if (oref) {
	DREF((LinuxSockRef) oref)->async_signalDataAvailable();
    }
}

/* virtual */ void
LinuxSock::calcAvailable(GenState& avail, StreamServer::ClientData* client)
{
    uval state;

    _ASSERT_HELD(_lock);

    if (status & FileLinux::INVALID) {
	LinuxEnv le; //Linux environment object
	updateStatus();
    }

    state = status;

    if (state & FileLinux::POLLHUP) {
	state |= FileLinux::ENDOFFILE;
    }

    if (! (state & FileLinux::ENDOFFILE)) {
	if (state & (FileLinux::POLLOUT |
		     FileLinux::POLLWRNORM |
		     FileLinux::POLLWRBAND) ) {
	    state |= FileLinux::WRITE_AVAIL;
	}
	if (state & (FileLinux::POLLIN |
		     FileLinux::POLLRDNORM |
		     FileLinux::POLLRDBAND)) {
	    state |= FileLinux::READ_AVAIL;
	}
    }

    avail.state = state;
}

/* virtual */ SysStatus
LinuxSock::giveAccessSetClientData(ObjectHandle &oh,
				   ProcessID toProcID,
				   AccessRights match,
				   AccessRights nomatch,
				   TypeID type)
{
    ClientData *cd = new ClientData();
    SysStatus rc = giveAccessInternal(oh, toProcID, match, nomatch,
				      type, (uval)cd);

    _IF_FAILURE_RET(rc);

    BaseProcessRef bpref;
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(toProcID,bpref);
    _IF_FAILURE_RET(rc);

    cd->pref = (ProcessRef)bpref;
 retry:
    XHandle ignore;  //XHandle to remote end will always be in
		     //cd->memEventHandler

    rc = MemTrans::GetMemTrans(cd->mtr, ignore, toProcID, 1234);

    // if the MemTrans already exists...
    if (_SUCCESS(rc)) {
	cd->memEventHandler = (IOMemEvents*)DREF(cd->mtr)->getMTEvents();
	return 0;
    }

    if (_SGENCD(rc)!=ENOENT) {
	return rc;
    }

    // The SMT object doesn't exist yet
    cd->memEventHandler = new StreamServer::IOMemEvents;
    rc = MemTrans::Create(cd->mtr, toProcID, 64 * PAGE_SIZE,
			  ignore, cd->memEventHandler, 1234);

    if (_FAILURE(rc) && _SGENCD(rc)==EALREADY) {
	//oops ... somebody just created it
	cd->mtr = NULL;
	delete cd->memEventHandler;
	goto retry;
    }

    return 0;
}

/* static */ SysStatus
LinuxSock::Create(ObjectHandle &oh, uval &clientType,
		  uval domain, uval type, uval protocol,
		  ProcessID pid)
{

//    clientType = TransStreamServer::TRANS_PPC;
    clientType = TransStreamServer::TRANS_VIRT;
    LinuxSock *nSck = new LinuxSock();
    tassert(nSck, err_printf("alloc of LinuxSock failed.\n"));

    nSck->init();

    LinuxSockRef oref = (LinuxSockRef)CObjRootSingleRep::Create(nSck);
    {
	LinuxEnv le; //Linux environment object
	sval rv = SocketLinux_internalCreate(&nSck->dataSocket,
					     domain, type, protocol, oref);
	if (rv < 0) {

	    DREF(oref)->destroy();
	    return _SERROR(1298, 0, -rv);
	}
    }

    SysStatus rc = 0;
    switch (type) {
	case SOCK_STREAM:
	    rc = DREF(oref)->giveAccessByServer(oh, pid,
						MetaTCPServer::typeID());
	    break;
	case SOCK_RAW:
	case SOCK_DGRAM:
	    rc = DREF(oref)->giveAccessByServer(oh, pid,
						MetaUDPServer::typeID());
	    break;
    }
    return rc;
}

/* virtual */ SysStatus
LinuxSock::detach()
{
    tassert(0, err_printf("NYI, need reference count\n"));
    return 0;
}

/* virtual */ SysStatus
LinuxSock::destroy()
{
    SysStatus rc;
    if (!dataSocket) {
	// remove all ObjRefs to this object
	rc = closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) {
	    return _SCLSCD(rc) == 1 ? 0: rc;
	}
	destroyUnchecked();
	return 0;
    }

    {
	LinuxEnv le; //Linux environment object

	SocketLinux_destroy(dataSocket, 1);

	// remove all ObjRefs to this object
	rc = closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) {
	    return _SCLSCD(rc) == 1 ? 0: rc;
	}

	SocketLinux_destroy(dataSocket, 2);
    }
    lock();
    TraceOSLinuxClose((uval)dataSocket);

    destroyUnchecked();
    return rc;

}

/* virtual */
SysStatus LinuxSock::_getname(__in uval peer,
			      __outbuf(addrLen:addrLen) char* buf,
			      __inout uval &addrLen)
{
    sval rv;
    int len = (int)addrLen;
    LinuxEnv le; //Linux environment object
    lock();
    rv = SocketLinux_getname(peer, dataSocket, buf, &len);
    unlock();
    addrLen = len;
    return rv;
}


/* virtual */ SysStatus
LinuxSock::_setsockopt(__in uval level, __in uval optname,
		       __inbuf(optlen) char *optval,
		       __in uval optlen, __XHANDLE xhandle)
{
    sval rv;
    LinuxEnv le; //Linux environment object
    lock();
    rv = SocketLinux_setsockopt(dataSocket, level, optname, optval, optlen);
    unlock();
    if (rv < 0) {
	err_printf("setsockopt failed: %ld\n",rv);
        return _SERROR(1927, 0, -rv);
    }
    return 0;
}

/* virtual */ SysStatus
LinuxSock::_getsockopt(__in uval level, __in uval optname,
                       __outbuf(optlen:optlen) char *optval,
                       __inout uval *optlen, __XHANDLE xhandle)
{
    sval rv;
    LinuxEnv le; //Linux environment object

    lock();
    int i = (int) *optlen;
    rv = SocketLinux_getsockopt(dataSocket, level, optname, optval, &i);
    *optlen = i;
    unlock();

    if (rv < 0) {
	err_printf("getsockopt failed: %ld\n",rv);
        return _SERROR(2908, 0, -rv);
    }
    return 0;
}

/* virtual */ SysStatus
LinuxSock::_bind(__inoutbuf(addrLen:addrLen:addrLen) char *addr,
		 uval& addrLen, __XHANDLE xhandle)
{
    sval rv;
    LinuxEnv le; //Linux environment object
    lock();
    rv = SocketLinux_bind(dataSocket, addr, addrLen);
    unlock();
    if (rv < 0) {
	return _SERROR(2536, 0, -rv);
    }

    return 0;
}

/* virtual */ SysStatusUval
LinuxSock::_recvfromVirt(__inbuf(len) struct iovec* vec, __in uval veclen,
			 __in uval flags,
			 __outbuf(addrLen:addrLen) char *addr,
			 __inout uval &addrLen,
			 __inout GenState &moreAvail,
			 __XHANDLE xhandle)
{
    ClientData *cd = (ClientData*)clnt(xhandle);
    uval numPages = pageSpan(vec, veclen);
    struct iovec *localVec;
    PinnedMapping* pages;
    uval realLength = 0;
    char *controlData = NULL;
    uval controlDataLen = 0;

    if (numPages > PAGE_DESCS_ON_STACK) {
	localVec = (iovec*) allocLocalStrict(sizeof(struct iovec)*numPages);
	pages = (PinnedMapping*)allocLocalStrict(sizeof(PinnedMapping)*numPages);
    } else {
	localVec = (iovec*)  alloca(sizeof(struct iovec)*numPages);
	pages = (PinnedMapping*)alloca(sizeof(PinnedMapping)*numPages);
    }

    memset(pages, 0, sizeof(PinnedMapping)*numPages);

    SysStatusUval rc = PinnedMapping::pinToIOVec(cd->pref, vec, veclen, 1,
						 pages, localVec);
    if (_FAILURE(rc)) goto abort;

    realLength = _SGETUVAL(rc);

    rc = recvfrom(localVec, realLength, flags, addr, addrLen, moreAvail,
		  controlData, controlDataLen, xhandle);

    for (uval i = 0; i< realLength; ++i) {
	pages[i].unpin();
    }
  abort:
    if (numPages > PAGE_DESCS_ON_STACK) {
	freeLocalStrict(localVec, sizeof(struct iovec)*numPages);
	freeLocalStrict(pages, sizeof(PinnedMapping)*numPages);
    }
    return rc;
}

/* virtual */ SysStatusUval
LinuxSock::_sendtoVirt(__inbuf(len) struct iovec* vec, __in uval veclen,
		       __in uval flags,
		       __inbuf(addrLen) const char *addr,
		       __in uval addrLen,
		       __inout GenState &moreAvail,
		       __XHANDLE xhandle)
{
    ClientData *cd = (ClientData*)clnt(xhandle);
    volatile uval numPages = pageSpan(vec, veclen);
    struct iovec *localVec;
    PinnedMapping* pages;
    uval realLength = 0;
    char *controlData = NULL;
    uval controlDataLen = 0;

    if (numPages > PAGE_DESCS_ON_STACK) {
	localVec = (iovec*) allocLocalStrict(sizeof(struct iovec)*numPages);
	pages = (PinnedMapping*)allocLocalStrict(sizeof(PinnedMapping)*numPages);
    } else {
	localVec = (iovec*)  alloca(sizeof(struct iovec)*numPages);
	pages = (PinnedMapping*)alloca(sizeof(PinnedMapping)*numPages);
    }

    memset(pages, 0, sizeof(PinnedMapping)*numPages);

    SysStatusUval rc = PinnedMapping::pinToIOVec(cd->pref, vec, veclen, 0,
						 pages, localVec);

    if (_FAILURE(rc)) goto abort;

    realLength = _SGETUVAL(rc);

    rc = sendto(localVec, realLength, flags, addr, addrLen, moreAvail,
		controlData, controlDataLen, xhandle);

    for (uval i = 0; i< realLength; ++i) {
	pages[i].unpin();
    }
  abort:
    if (numPages > PAGE_DESCS_ON_STACK) {
	freeLocalStrict(localVec, sizeof(struct iovec)*numPages);
	freeLocalStrict(pages, sizeof(PinnedMapping)*numPages);
    }

    return rc;
}

/* virtual */ SysStatusUval
LinuxSock::_readSMT(uval &offset, uval len,
		    GenState &moreAvail,
		    __XHANDLE xhandle)
{
    ClientData *cd = (ClientData*)clnt(xhandle);
    uval addr;
    uval size = len;
    SysStatus rc=0;
    char *controlData = NULL;
    uval controlDataLen = 0;

    tassertMsg(cd->mtr, "No SMT defined\n");

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
    rc = recvfrom(&vec, 1, 0, NULL, addrLen, moreAvail, controlData,
		  controlDataLen, xhandle);

    offset = DREF(cd->mtr)->localOffset(addr);
    if (_FAILURE(rc) || _SGETUVAL(rc)==0) {
	DREF(cd->mtr)->freePage(offset);
    }

    return rc;
}

/* virtual */ SysStatusUval
LinuxSock::_writeSMT(uval offset, uval len,
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
	rc = _SERROR(2448, 0, EINVAL);
	breakpoint();
    } else {
	char *controlData = NULL;
	uval controlDataLen = 0;
	rc = sendto(&vec, 1, 0, NULL, 0, moreAvail, controlData,
		    controlDataLen, xhandle);
    }

    rc2 = DREF(cd->mtr)->insertToRing(cd->memEventHandler->other, offset);
    // Remember the offset for freeing later
    if (_FAILURE(rc2)) {
	cd->blocked = offset;
    }
    return rc;
}

/* virtual */ SysStatusUval
LinuxSock::_recvfromSMT(__out uval &offset,
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
    char *controlData = NULL;
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
LinuxSock::_sendtoSMT(__in uval offset,
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
	rc = _SERROR(2447, 0, EINVAL);
    } else {
	char *controlData = NULL;
	uval controlDataLen = 0;
	rc = sendto(&vec, 1, 0, addr, addrLen, moreAvail,
		    controlData, controlDataLen, xhandle);
    }

    rc2 = DREF(cd->mtr)->insertToRing(cd->memEventHandler->other, offset);

    // Remember the offset for freeing later
    if (_FAILURE(rc2)) {
	cd->blocked = offset;
    }
    return rc;
}

/* virtual */ SysStatus
LinuxSock::_connect(__inbuf(addrLen) const char *addr, __in uval addrLen,
		      __out GenState &moreAvail, __XHANDLE xhandle)
{

    sval rv;
    LinuxEnv le; //Linux environment object

    lock();

    rv = SocketLinux_connect(dataSocket, addr, addrLen);

    if ((rv < 0) && (rv != -EINPROGRESS)) {
	unlock();
        return _SERROR(2537, 0, -rv);
    }

    updateStatus();
    calcAvailable(moreAvail);
    clnt(xhandle)->setAvail(moreAvail);

    unlock();

    return 0;
}

/* virtual */ SysStatusUval
LinuxSock::recvfrom(struct iovec *vec, uval veclen, uval flags,
		    char *addr, uval &addrLen, GenState &moreAvail,
		    void *controlData, uval &controlDataLen,
		    __XHANDLE xhandle)
{
    (void)xhandle;			// unused

    SysStatusUval rc;
    sval ret;

    controlDataLen = 0; /* setting to zero, since no control data */

    LinuxEnv le; //Linux environment object

    lock();

    if (!addrLen) addr = NULL;

    struct msghdr msg = { addr, addrLen, vec, veclen, NULL, 0 };

    tassertWrn(flags == 0 , "Non-zero flags arg to recvfrom(): %lx\n",flags);
    ret = SocketLinux_soReceive(dataSocket, &msg, flags, NULL);

  recover:
    if (addrLen) addrLen = msg.msg_namelen;

    TraceOSLinuxRecv((uval)dataSocket, ret, status);

    if (ret < 0 && ret!=-EWOULDBLOCK) {
	unlock();
        return _SERROR(2535, 0, -ret);
    }

    updateStatus();
    calcAvailable(moreAvail);

    if ((ret == 0) && (moreAvail.state & FileLinux::READ_AVAIL)) {
	/*
	 * We got no data but we think data is available.  It's probably
	 * because the other end of the socket shut down, in which case we
	 * need to assert end-of-file.  But to be sure it's not just a
	 * race, we repeat the receive while we still have the lock.
	 */
	ret = SocketLinux_soReceive(dataSocket, &msg, flags, NULL);
	if (ret != 0) {
	    /*
	     * This time we got something, so it was just a race.  Back up
	     * and deal with what we got.
	     */
	    goto recover;
	}

	/*
	 * There's still no data, so deassert READ_AVAIL and assert ENDOFFILE.
	 */
	moreAvail.state &= ~(FileLinux::POLLIN |
			     FileLinux::POLLRDNORM |
			     FileLinux::POLLRDBAND |
			     FileLinux::READ_AVAIL);
	moreAvail.state |= FileLinux::ENDOFFILE;
    }

    clnt(xhandle)->setAvail(moreAvail);

    unlock();

    //Return code of 0--> no data read instead of EOF
    if (ret==-EWOULDBLOCK) {
	ret = 0;
    }
    rc = _SRETUVAL(ret);

    return rc;
}

/* virtual */ SysStatusUval
LinuxSock::sendto(struct iovec* vec, uval veclen, uval flags,
		  const char *addr, uval addrLen, GenState &moreAvail,
		  void *controlData, uval controlDataLen,
		  __XHANDLE xhandle)
{
    ClientData * cd = (ClientData*)clnt(xhandle);
    sval ret;
    SysStatusUval rc;

    tassertMsg((controlDataLen == 0), "oops\n");

    LinuxEnv le; //Linux environment object

    lock();

    if (addrLen == 0) addr = NULL;

    struct msghdr msg = { (void*)addr, addrLen, vec, veclen, NULL, 0, 0 };
    ret = SocketLinux_soSend(dataSocket, &msg, NULL);

    TraceOSLinuxSend((uval)dataSocket, ret, status);

    //Return code of 0--> no data read instead of EOF
    if (ret==-EWOULDBLOCK) {
	ret = 0;
    } else if (ret < 0) {
	unlock();
	return _SERROR(1293, 0, -ret);
    }
    rc = _SRETUVAL(ret);

    updateStatus();
    calcAvailable(moreAvail);
    cd->setAvail(moreAvail);

    unlock();

    return rc;
}

/* static */ SysStatus
LinuxSock::_Create(__out ObjectHandle &oh, __out uval &clientType,
		   __in uval domain,
		   __in uval type, __in uval protocol,
		   __CALLER_PID processID)
{
    return _SERROR(2656, 0, EOPNOTSUPP);
}


/* virtual */ SysStatusUval
LinuxSock::_ioctl(uval request, uval &size, char* args, __XHANDLE xhandle)
{
    SysStatusUval rc =0;
    int ret = 0;
    LinuxEnv le; //Linux environment object
    lock();
    switch (request) {
	case SIOCGIFCONF: {
	    struct ifconf ifc;
	    ifc.ifc_len = size;
	    ifc.ifc_buf = args;
	    ret = SocketLinux_ioctl(dataSocket, request, sizeof(ifc), &ifc);
	    size = ifc.ifc_len;
	    tassertMsg(ret>=0,"SIOCGIFCONF should succeed\n");
	    break;
	}
	default:
	    ret = SocketLinux_ioctl(dataSocket, request, size, args);
    }
    unlock();
    if (ret < 0) {
	err_printf("ioctl failure: %d\n",ret);
	rc = _SERROR(2050, 0, -ret);
    } else {
	rc = _SRETUVAL(ret);
    }
    return rc;
}


/* virtual */ SysStatus
LinuxSock::_accept(__out ObjectHandle &oh,
		   __inoutbuf(len:len:len) char* addr,
		   __inout uval& len,
		   __out GenState &moreAvail,
		   __XHANDLE xhandle)
{
    SysStatus rc;
    LinuxSockRef sref;
    LinuxSock *nSck;

    nSck = new LinuxSock();
    tassert(nSck, err_printf("alloc of LinuxSock failed.\n"));

    nSck->init();

    sref = (LinuxSockRef)CObjRootSingleRep::Create(nSck);

    sval rv=0;


    LinuxEnv le; //Linux environment object

    lock();

    rv = SocketLinux_soAccept(dataSocket, &nSck->dataSocket,
			      sref, addr, &len);

    if ((rv < 0) && (rv != -EWOULDBLOCK)) {
	LinuxEnvSuspend();
	DREF(sref)->destroy();
	LinuxEnvResume();
	unlock();
	return _SERROR(1294, 0, -rv);
    }
	
    LinuxEnvSuspend();
    if (rv == -EWOULDBLOCK) {
	DREF(sref)->destroy();
	oh.init();	// return success but invalid oh
    } else {
	rc = DREF(sref)->giveAccessByServer(oh,
			    XHandleTrans::GetOwnerProcessID(xhandle),
			    MetaTCPServer::typeID());
	passertMsg(_SUCCESS(rc), "FIXME: cleanup and recover.\n");
	TraceOSLinuxAccept((uval) dataSocket,
			   xhandle, (uval) nSck->dataSocket);
    }
    LinuxEnvResume();

    updateStatus();
    calcAvailable(moreAvail);
    clnt(xhandle)->setAvail(moreAvail);

    unlock();
    return 0;
}

/* virtual */ SysStatus
LinuxSock::_listen(__in sval backlog)
{
    sval rv;
    LinuxEnv le; //Linux environment object
    lock();
    rv = SocketLinux_listen(dataSocket, backlog);
    unlock();
    if (rv < 0) {
        return _SERROR(1301, 0, -rv);
    }

    return 0;
}
