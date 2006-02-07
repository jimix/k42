/******************************************************************************
 *
 *                      Kitchawan: bilge facilities
 *
 *                            IBM
 *                        Copyright 1999
 *                      All rights reserved.
 *
 * $Id: IOSocketNetBSD.C,v 1.6 2000/05/04 20:06:03 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a socket interface using NetBSD sockets.
 ****************************************************************************/
 
#include "kernIncs.H"
#include <cobj/CObjRoot.H>
#include <bilge/ThinIP.H>
#include <bilge/IOSocketNetBSD.H>
#include <sync/BlockedThreadQueues.H>
#include <scheduler/Scheduler.H>
#include <stub/StubBilgeInfo.H>

extern "C" {
#define LIBKERN_INLINE
//#define _KERNEL    
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socketvar.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/filio.h>

/* From netinet/in.h which defines a C++ incompatible structure */

/*
 * Internet address (a structure for historical reasons)
 */
    struct in_addr {
	u_int32_t s_addr;
    };

/*
 * Socket address, internet style.
 */
    struct sockaddr_in {
        u_int8_t  sin_len;
        u_int8_t  sin_family;
        u_int16_t sin_port;
        struct    in_addr sin_addr;
        int8_t    sin_zero[8];
    };
}

extern uval inet_addr(const char *);

uval IOSocketNetBSD::socketCount=0;

SysStatus
IOSocketNetBSD::init()
{
    SysStatus rc;
    uval lSocketCount;

    while (1) {
	lSocketCount = socketCount + 1;
	
	if (lSocketCount == MAX_SOCKETS) {
	    return _SERROR(1289, 0, EMFILE);
	} else {
	    rc = CompareAndSwapUvalSynced(socketCount, lSocketCount, &socketCount);
	    if(_SUCCESS(rc)) {
		break;
	    }
	}
    }
    
    lock.init();

    dataIsAvailable=0;
    threadsBlocked=0;

    return 0;
}

void
IOSocketNetBSD::end()
{
    FetchAndAddSvalSynced(&(sval)socketCount, -1);
}

/* static */ void
IOSocketNetBSD::Upcall(struct socket *so, char *arg, sval32 waitf)
{
    IOSocketRef &tref = (IOSocketRef)arg;
    
    DREF(tref)->signalDataAvailable();
}

inline SysStatusUval
IOSocketNetBSD::soReceive(char *buf, uval len)
{
    tassert(lock.isLocked(), err_printf("lock should be held\n"));
     
    uval errno;
    struct iovec iov;
    struct uio uio;

    iov.iov_base = (char*)buf;
    iov.iov_len = len;
    
    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_segflg = UIO_USERSPACE;
    uio.uio_rw = UIO_READ;
    uio.uio_procp = (struct proc *)(-1); // some functions check p!=0
    uio.uio_offset = 0;			 // XXX 
    uio.uio_resid = len;

    errno = soreceive(dataSocket, 0, &uio, 0, 0, 0);
    if (errno) {
	return _SERROR(1293, 0, errno);
    }

    if(dataSocket->so_rcv.sb_cc) {
	dataIsAvailable = 1;
    } else {
	dataIsAvailable = 0;
    }

    return len - uio.uio_resid;
}

inline SysStatusUval
IOSocketNetBSD::soSend(const char *buf, uval len, const char *addr)
{
    tassert(lock.isLocked(), err_printf("lock should be held\n"));
     
    uval errno;
    struct sockaddr_in sin;
    struct mbuf *m=0;
    struct iovec iov;
    struct uio uio;

    if(addr) {
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr("9.2.133.34");
	sin.sin_port = htons(6000);
	
	m = m_get(M_WAIT, MT_SONAME);
	bcopy(&sin, mtod(m, char *), sizeof(sin));
	m->m_len = sizeof(sin);
    }
    
    iov.iov_base = (char*)buf;
    iov.iov_len = len;

    uio.uio_iov = &iov;
    uio.uio_iovcnt = 1;
    uio.uio_segflg = UIO_USERSPACE;
    uio.uio_rw = UIO_WRITE;
    uio.uio_procp = (struct proc *)(-1); // some functions check p!=0
    uio.uio_offset = 0;			 // XXX 
    uio.uio_resid = len;

    errno = sosend(dataSocket, m, &uio, 0, 0, 0);
    if (errno) {
	return _SERROR(1294, 0, errno);
    }

    return len - uio.uio_resid;
}

inline SysStatus
IOSocketNetBSD::soAccept(IOSocketRef &oref, struct socket *&clientSocket)
{
    tassert(lock.isLocked(), err_printf("lock should be held\n"));
    
    struct mbuf *m;
    sval s;
    
    s = splsoftnet();
    
    if ((dataSocket->so_options & SO_ACCEPTCONN) == 0) {
	splx(s);
	return _SERROR(1295, 0, EINVAL);
    }
    
    if ((dataSocket->so_state & SS_NBIO) && dataSocket->so_qlen == 0) {
	splx(s);
	return _SERROR(1296, 0, EWOULDBLOCK);
    }

    if (dataSocket->so_state & SS_CANTRCVMORE) {
	splx(s);
	return _SERROR(1297, 0, ECONNABORTED);
    }

    clientSocket = dataSocket->so_q.tqh_first;
    
    if (soqremque(clientSocket, 1) == 0) {
	panic("accept");
    }

    m = m_get(M_WAIT, MT_SONAME);
    (void) soaccept(clientSocket, m);
    m_freem(m);

    splx(s);
      
    return 0;
}

inline void
IOSocketNetBSD::addClient(ObjectHandle oh) 
{
    StubIOSocketClient *sobj;

    sobj = new StubIOSocketClient(StubObj::UNINITIALIZED);
    sobj->setOH(oh);
    clientList.add(sobj);
}

inline /* static */ SysStatus
IOSocketNetBSD::InternalCreate(IOSocketRef &oref, IOSocketNetBSD *&nSck, uval type)
{
    SysStatus rc;
    uval errno;
    
    nSck = new IOSocketNetBSD();
    tassert(nSck, err_printf("alloc of IOSocketNetBSD failed.\n"));
    
    rc = nSck->init();
    if(_FAILURE(rc)) {
	delete nSck;
	return rc;
    }

    errno = socreate(AF_INET, &nSck->dataSocket, type, 0);
    if (errno) {
	nSck->end();
	delete nSck;
	return _SERROR(1298, 0, errno);
    }
    
    oref = (IOSocketRef)CObjRootSingleRep::Create(nSck);

    nSck->dataSocket->so_state |= SS_NBIO;
    nSck->dataSocket->so_upcall = Upcall;
    nSck->dataSocket->so_upcallarg = (caddr_t)oref;
    
    return 0;
}

/* virtual */ SysStatus
IOSocketNetBSD::destroy()
{
    SysStatus rc;
   
    // remove all ObjRefs to this object
    rc = closeExportedXObj();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if(_FAILURE(rc)) {
	return _SCLSCD(rc) == 1 ? 0: rc;
    }
    
    rc = close();
       
    end();
    
    destroyUnchecked();
	
    return rc;
}

/* virtual */ SysStatus
IOSocketNetBSD::close()
{
    AutoLock<LockType> al(&lock);
    uval errno;
    
    errno = soclose(dataSocket);
    if (errno) {
	return _SERROR(1291, 0, errno);
    }

    return 0;
}

/* virtual */ SysStatus
IOSocketNetBSD::bind(uval port) 
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    uval errno;
    struct sockaddr_in sin;
    struct mbuf *m;
    char buf[32];

    StubBilgeInfo::_GetThinEnvVar("K42_IP_ADDRESS", buf);
    sin.sin_addr.s_addr = inet_addr(buf);
    sin.sin_family = AF_INET;   
    sin.sin_port = htons((uval16)port);

    m = m_get(M_WAIT, MT_SONAME);
    bcopy(&sin, mtod(m, char *), sizeof(sin));
    m->m_len = sizeof(sin);

    errno = sobind(dataSocket, m);
    m_freem(m);
    if (errno) {
	return _SERROR(1300, 0, errno);
    }
    
    return 0;
}

/* virtual */ SysStatus
IOSocketNetBSD::listen(sval backlog) 
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    uval errno;

    errno = solisten(dataSocket, backlog);
    if (errno) {
	return _SERROR(1301, 0, errno);
    }

    return 0;
}

inline SysStatus 
IOSocketNetBSD::internalAccept(IOSocketRef &oref, IOSocketNetBSD *&nSck)
{    
    SysStatus rc;
    
    tassert(lock.isLocked(), err_printf("lock should be held\n"));

    nSck = new IOSocketNetBSD();
    tassert(nSck, err_printf("alloc of IOSocketNetBSD failed.\n"));
    
    rc = nSck->init();
    if(_FAILURE(rc)) {
	delete nSck;
	return rc;
    }
    
    rc = soAccept(oref, nSck->dataSocket);
    if(_FAILURE(rc)) {
	nSck->end();
	delete nSck;
	return rc;
    }

    oref = (IOSocketRef)CObjRootSingleRep::Create(nSck);

    nSck->dataSocket->so_state |= SS_NBIO;
    nSck->dataSocket->so_upcall = Upcall;
    nSck->dataSocket->so_upcallarg = (caddr_t)oref;    
    
    return 0;
}

/* virtual */ SysStatus
IOSocketNetBSD::accept(IOSocketRef &ref)
{
    SysStatus rc;
    BlockedThreadQueues::Element qe;
    IOSocketNetBSD *tmp;
    lock.acquire();

    while (!dataIsAvailable) {
	threadsBlocked = 1;
	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(
	    &qe, (void *)this);
	lock.release();
	Scheduler::Block();
	lock.acquire();
	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
	    &qe, (void *)this);
    }
    
    rc = internalAccept(ref, tmp);
    lock.release();
    tassert( _SUCCESS(rc), err_printf("FIXME: write cleanup code\n"));
    return rc;
};

/* virtual */ SysStatusUval 
IOSocketNetBSD::read(char *buf, uval len)
{
    BlockedThreadQueues::Element qe;
    SysStatusUval rc;

    lock.acquire();
    while (!dataIsAvailable) {
	threadsBlocked = 1;
	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(
	    &qe, (void *)this);
	lock.release();
	Scheduler::Block();
	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
	    &qe, (void *)this);
	lock.acquire();
    }
    
    rc = soReceive(buf, len);
    
    lock.release();

    return rc;
}

/* virtual */ SysStatusUval 
IOSocketNetBSD::write(const char *buf, uval len)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    
    return soSend(buf, len, 0);
}

/* virtual */ SysStatusUval
IOSocketNetBSD::recvfrom(char *buf, uval len)
{
    return read(buf, len);
}

/* virtual */ SysStatusUval
IOSocketNetBSD::sendto(const char *buf, uval len, const char *addr)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    return soSend(buf, len, addr);
}

/* virtual */ SysStatus
IOSocketNetBSD::signalDataAvailable()
{
    void *iter;
    StubIOSocketClient *client;
    
    //AutoLock<LockType> al(&lock); // locks now, unlocks on return

    dataIsAvailable = 1;
    if (threadsBlocked) {
	DREFGOBJ(TheBlockedThreadQueuesRef)->wakeupAll((void *)this);
    }
    threadsBlocked = 0;

    // now traverse list of clients and tell them
    iter = NULL;
    while((iter=clientList.next(iter, client))) {
	client->signalDataAvailable();
    }
    
    return 0;
}

/* virtual */ SysStatus
IOSocketNetBSD::_destroy()
{
    return destroy();
}

/* virtual */ SysStatus
IOSocketNetBSD::_bind(__in uval port)
{
    return bind(port);
}

/* virtual */ SysStatus
IOSocketNetBSD::_listen(__in sval backlog)
{
    return listen(backlog);
}

/* virtual */ SysStatus
IOSocketNetBSD::_accept(__out ObjectHandle &oh,
			__out uval &moreAvail, __CALLER_PID processID)
{
    SysStatus rc;
    IOSocketRef sref;
    IOSocketNetBSD *nSck;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    if (!dataIsAvailable) {
	Scheduler::Yield();		// FIXME: For spinning app need to 
					// progress
	return _SERROR(1302,IOSocketServer::WOULD_BLOCK,EWOULDBLOCK);
    }
    
    rc = internalAccept(sref, nSck);
    if (_FAILURE(rc)) {
	return rc;
    }

    // FIXME: figure out a way to combine xobject holder list with
    // the stubobject holder indicated above
    rc = DREF(sref)->giveAccess(oh, processID);
    if (_FAILURE(rc)) { 
	tassert(0, err_printf("FIXME: cleanup\n"));
	return rc;
    }

    return 0;
}


/* virtual */ SysStatusUval
IOSocketNetBSD::_readBlockInServer(__outbuf(__rc:len) char *buf, __in uval len) 
{
    BlockedThreadQueues::Element qe;
    SysStatusUval rc;

    lock.acquire();
    while (!dataIsAvailable) {
	threadsBlocked = 1;
	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(
	    &qe, (void *)this);
	lock.release();
	Scheduler::Block();
	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
	    &qe, (void *)this);
	lock.acquire();
    }
    rc = soReceive(buf, len);
    
    lock.release();
    return rc;
}

/* virtual */ SysStatus
IOSocketNetBSD::_acceptBlockInServer(__out ObjectHandle &oh, 
				     __CALLER_PID processID)
{
    BlockedThreadQueues::Element qe;
    SysStatus rc;
    IOSocketRef sref;
    IOSocketNetBSD *nSck;

    lock.acquire();
    while (!dataIsAvailable) {
	threadsBlocked = 1;
	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(
	    &qe, (void *)this);
	lock.release();
	Scheduler::Block();
	lock.acquire();
	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(
	    &qe, (void *)this);
    }

    rc = internalAccept(sref, nSck);
    if (_FAILURE(rc)) {
	lock.release();
	return rc;
    }

    // FIXME: figure out a way to combine xobject holder list with
    // the stubobject holder indicated above
    rc = DREF(sref)->giveAccess(oh, processID);
    if (_FAILURE(rc)) { 
	tassert(0, err_printf("FIXME: cleanup\n"));
	lock.release();
	return rc;
    }

    lock.release();
    return 0;
}

/* virtual */ SysStatusUval
IOSocketNetBSD::_read(__outbuf(__rc:len) char *buf, __in uval len,
		    __out uval &moreAvail) 
{  
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    SysStatusUval rc;
    
    if (!dataIsAvailable) {
	Scheduler::Yield();		// FIXME: For spinning app need to 
					// progress
	return _SERROR(1303,IOSocketServer::WOULD_BLOCK,EWOULDBLOCK);
    }
    
    rc = soReceive(buf, len);
    moreAvail = dataIsAvailable;

    return rc;
}

/* virtual */ SysStatusUval
IOSocketNetBSD::_write(__inbuf(len) const char *buf, __in uval len)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    return soSend(buf, len, 0);
}

/* virtual */ SysStatusUval
IOSocketNetBSD::_recvfrom(__outbuf(__rc:len) char *buf, __in uval len,
			  __out uval &moreAvail) 
{
    return _read(buf, len, moreAvail);
}

/* virtual */ SysStatusUval
IOSocketNetBSD::_sendto(__inbuf(len) const char *buf, __in uval len,
			__inbuf(80) const char *addr)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    return soSend(buf, len, addr);
}

/* virtual */ SysStatusUval
IOSocketNetBSD::readBlockInServer(char *, uval)
{
    tassert(0, err_printf("should never be called from user level\n"));
    return 0;
}

/* virtual */ SysStatus
IOSocketNetBSD::acceptBlockInServer(IOSocketRef &)
{
    tassert(0, err_printf("should never be called from user level\n"));
    return 0;
}

void
IOSocketNetBSD::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaIOSocketServer::init();
}

/* virtual */ SysStatus
IOSocketNetBSD::_registerCallback(
    __in ObjectHandle callback, __CALLER_PID processID)
{
    // error if callback is not in caller - would be denial of service
    if (processID != callback.pid()) {
	return _SERROR(1304, 0, EINVAL);
    }
    addClient(callback);
    return 0;
}

