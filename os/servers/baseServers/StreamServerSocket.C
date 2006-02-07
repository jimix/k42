/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServerSocket.C,v 1.24 2005/07/15 17:14:32 rosnbrg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/
#include <sys/sysIncs.H>
#include <io/IO.H>
#include <io/FileLinuxStream.H>
#include <meta/MetaStreamServerSocket.H>
#include "StreamServerSocket.H"
#include <sys/ProcessSet.H>
#include <stub/StubStreamServerSocket.H>
#include <stub/StubFileLinuxServer.H>
#include <scheduler/Scheduler.H>

#undef VERBOSE_UDS_SERVER
#undef OVERLY_VERBOSE_UDS_SERVER

#define MAX_SOCKET_LINEBUF 1024*128

struct StreamServerSocket::SockClientData: public StreamServer::ClientData {
public:
    DEFINE_GLOBAL_NEW(SockClientData);
    SockClientData() : ClientData(),
	theEnd(INVALID), sndBufferSize(109568), rcvBufferSize(109568)
    { 
	/* empty body */ 
    }

    enum SocketEnd { INVALID, END1, END2 };

    virtual void setAvail(GenState &ma) {
	ClientData::setAvail(ma);
    }

    virtual SysStatus signalDataAvailable(GenState avail) {
	if (avail.state ^ available.state) {
	    return StreamServer::ClientData::signalDataAvailable(avail);
	}
	return 0;
    }

    void setEnd(SocketEnd end) {
	theEnd = end;
    }

    SocketEnd getEnd() const {
	tassertWrn(theEnd!=INVALID, "SockClientData::getEnd is INVALID\n");
	return theEnd;
    }

    SysStatus setSndBufferSize(sval32 size) {
	if (size < MAX_SOCKET_LINEBUF) {
	    sndBufferSize = size; 
	} else {
	    sndBufferSize = MAX_SOCKET_LINEBUF;
	}
	return 0;
    }
    
    SysStatus setRcvBufferSize(sval32 size) {
	if (size < MAX_SOCKET_LINEBUF) {
	    rcvBufferSize = size;
	} else {
	    rcvBufferSize = MAX_SOCKET_LINEBUF;
	}
	return 0;
    }

    sval32 getSndBufferSize() const {
	return sndBufferSize;
    }

    sval32 getRcvBufferSize() const {
	return rcvBufferSize;
    }

private:
    SocketEnd theEnd;
    sval32 sndBufferSize; // Not actually used, but we remember this to keep
    sval32 rcvBufferSize;  // applications happy
};


/* static */ StreamServerSocket::SockClientData * 
StreamServerSocket::sclnt(XHandle xhandle)
{
    StreamServerSocket::SockClientData* retvalue;
    retvalue = (StreamServerSocket::SockClientData *)
	(XHandleTrans::GetClientData(xhandle));
    return (retvalue);
}

inline /* virtual */ void
StreamServerSocket::calcAvailable(GenState& avail,
				  StreamServer::ClientData *client)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    if (traceThisSocket)
	 err_printf("%s: this %p  client %p\n", __PRETTY_FUNCTION__, this,
		    client);
    #endif // OVERLY_VERBOSE_UDS_SERVER

    uval state = 0;

    SockClientData* sc = (SockClientData*)(client);

    if (socketIsConnected) {
	passertMsg(sc->getEnd()!=SockClientData::INVALID, "Passed unitialised "
	       "client data\n");

	IORingBuffer &rbRead = sc->getEnd()==SockClientData::END1 ? rb1 : rb2;
	IORingBuffer &rbWrite = sc->getEnd()==SockClientData::END1 ? rb2 : rb1;

	#ifdef OVERLY_VERBOSE_UDS_SERVER
	if (traceThisSocket) {
	    err_printf("%s: rbRead is %d\n", __PRETTY_FUNCTION__,
		sc->getEnd()==SockClientData::END1 ? 1 : 2);
	    err_printf("read write sev  %ld %ld %ld\n",
	    rbRead.bytesAvail(), rbWrite.spaceAvail(), useCount);
	}
	#endif // OVERLY_VERBOSE_UDS_SERVER

	if (rbRead.bytesAvail()) {
	    state |= FileLinux::READ_AVAIL;
	}

	if ((!rbRead.bytesAvail()) && (useCount < 2)) {
 	    state |= FileLinux::ENDOFFILE;
	}

	if (rbWrite.spaceAvail() && (useCount > 1)) {
	    state |= FileLinux::WRITE_AVAIL;
	}

    } else {
	if (!connectedSocketList.isEmpty()) {
	    // there are socket clients waiting to connect
	    state |= FileLinux::READ_AVAIL;
	}
    }

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s returning 0x%lx\n", __func__, state);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    avail.state = state;
}

StreamServerSocket::StreamServerSocket(uval d, uval t)
    : socketDomain(d), socketType(t), 
	readByteCount1(0), readByteCount2(0), writeByteCount1(0),
	writeByteCount2(0), socketIsBound(0), socketIsListening(0),
	socketIsConnected(0), listenBacklog(0), useCount(0),
	traceThisSocket(0)
{
    _lock.init();

    rb1.init(MAX_SOCKET_LINEBUF);
    rb2.init(MAX_SOCKET_LINEBUF);

    fdTransferList1.reinit();
    fdTransferList2.reinit();

    connectedSocketList.reinit();
}

StreamServerSocket::~StreamServerSocket()
{
    rb1.destroy();
    rb2.destroy();


    // TODO: remove items in fdTransferList1 and fdTransferList2
    // as alloced fd data needs to be freed

    // todo remote items in connected list
}

SysStatusUval
StreamServerSocket::locked_readInternal(IORingBuffer &rb, struct iovec *vec, 
					uval len, uval *readByteCount)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    uval stlen = 0;
    uval bytes;
    uval i = 0;

    while (i < len) {
	bytes = rb.getData((char*)vec[i].iov_base,vec[i].iov_len);
	if (bytes==0) {
	    return _SRETUVAL(stlen);
	}
	stlen += bytes;
	*readByteCount += bytes;
	++i;
    }
    return _SRETUVAL(stlen);
}

SysStatusUval
StreamServerSocket::locked_writeInternal(IORingBuffer &rb, struct iovec *vec, 
					 uval len, uval *writeByteCount)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s: this %p\n", __PRETTY_FUNCTION__, this);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    uval stlen = 0;
    uval bytes;
    uval i = 0;

    while (i < len) {
	bytes = rb.putData((char*)vec[i].iov_base,vec[i].iov_len);
	if (bytes==0) {
	    return _SRETUVAL(stlen);
	}
	stlen += bytes;
	*writeByteCount += bytes;
	++i;
    }
    return _SRETUVAL(stlen);
}

/* virtual */ SysStatusUval
StreamServerSocket::recvfrom(struct iovec *vec, uval veclen, uval flags,
			     char *addr, uval &addrLen, GenState &moreAvail, 
			     void *controlData, uval &controlDataLen,
			     __XHANDLE xhandle)
{
    SysStatusUval rc;

    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER
    passertMsg(sclnt(xhandle)->getEnd() != SockClientData::INVALID, 
	    "Passed unitialised client data\n");
    
    IORingBuffer &rb = sclnt(xhandle)->getEnd() == SockClientData::END1 
	? rb1 : rb2;
    uval &readByteCount = sclnt(xhandle)->getEnd() == SockClientData::END1 
	? readByteCount1 : readByteCount2;

    addrLen = 0;

    lock();

    ListSimple<AncillaryData *, AllocGlobal> &fdTransferList
	= sclnt(xhandle)->getEnd()==SockClientData::END1 ? fdTransferList1
	: fdTransferList2;
    AncillaryData *ad;
    char *fdData = NULL;
    uval fdDataLen = 0;
    if (fdTransferList.next(0, ad)) {
	// Have ancillary data in the list
	// See if its time to give it to the client
	if (readByteCount >= ad->index) {
	    fdData = ad->fdData;
	    fdDataLen = ad->fdDataSize;
	    freeGlobal(ad, sizeof(AncillaryData));
	    fdTransferList.removeHead(ad);
	}
    }

    rc = locked_readInternal(rb, vec, veclen, &readByteCount);

    if (_SUCCESS(rc)) {
	calcAvailable(moreAvail, clnt(xhandle));
	clnt(xhandle)->setAvail(moreAvail);
	locked_signalDataAvailable();
    }

    if (fdData) {
	locked_getFDFromMessage(xhandle, fdData,
				(char *)controlData, controlDataLen);
	freeGlobal(fdData, fdDataLen);
    } else {
	controlDataLen = 0;
    }

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("return from read is %ld, readByte is %ld, controlDataLen %ld\n", 
	       rc, readByteCount, controlDataLen);
    #endif // OVERLY_VERBOSE_UDS_SERVER

    unlock();
    return rc;
}

/* virtual */ SysStatusUval
StreamServerSocket::sendto(struct iovec* vec, uval veclen, uval flags,
			   const char *addr, uval addrLen, GenState &moreAvail, 
			   void *controlData, uval controlDataLen,
			   __XHANDLE xhandle)
{
    lock();
    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    if (traceThisSocket)
	 err_printf("%s: rb is %d\n", __PRETTY_FUNCTION__,
		sclnt(xhandle)->getEnd()==SockClientData::END1 ? 2 : 1);
    #endif // OVERLY_VERBOSE_UDS_SERVER

    if (controlDataLen) {
	locked_addFDToMessage(xhandle, (char *)controlData, controlDataLen);
    }

    passertMsg(sclnt(xhandle)->getEnd() != SockClientData::INVALID, 
	       "Passed unitialised client data\n");
    IORingBuffer &rb = sclnt(xhandle)->getEnd()==SockClientData::END1 
			? rb2 : rb1;

    SysStatusUval rc=0;

    addrLen = 0;

    uval &writeByteCount = sclnt(xhandle)->getEnd() == SockClientData::END1 
	? writeByteCount1 : writeByteCount2;

    if (useCount < 2 ) {
 	unlock();
 	return _SERROR(2883, 0, EPIPE);
    }

    rc = locked_writeInternal(rb, vec, veclen, &writeByteCount);
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("return from send is %ld, writeByte is %ld\n", 
	       rc, writeByteCount);
    #endif // OVERLY_VERBOSE_UDS_SERVER

    if (_SUCCESS(rc)) {
	calcAvailable(moreAvail, clnt(xhandle));
	clnt(xhandle)->setAvail(moreAvail);
	locked_signalDataAvailable();
    }
    unlock();
    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::giveAccessSetClientData(ObjectHandle &oh,
					    ProcessID toProcID,
					    AccessRights match,
					    AccessRights nomatch,
					    TypeID type)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s: this %p  handle<CommID 0x%lx  xhandle 0x%lx> "
	       " to procID 0x%lx\n", 
	      __func__, this, 
	      oh.commID(), oh.xhandle(), toProcID);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    SysStatus retvalue;

    lock();
    useCount++;

    SockClientData *clientData = new SockClientData();

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s: %p to procID 0x%lx returning %p\n", 
	      __func__, this, toProcID, clientData);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
				  type, (uval)clientData);
    unlock();
    return (retvalue);
}

void
StreamServerSocket::ClassInit()
{
    MetaStreamServerSocket::init();
}

/* static */ SysStatus
StreamServerSocket::Create(StreamServerSocketRef &ref,
			   uval domain /* = AF_UNIX */,
			   uval type /* = SOCK_STREAM */,
			   uval traceFlag /* = 0 */)
{
    #ifdef VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
    #endif // VERBOSE_UDS_SERVER
    StreamServerSocket *newSocket = new StreamServerSocket(domain, type);
    newSocket->traceThisSocket = traceFlag;

    ref = (StreamServerSocketRef)CObjRootSingleRep::Create(newSocket);
    tassert((ref != 0), err_printf("woops\n"));

    return 0;
}


/* virtual */ SysStatus
StreamServerSocket::_lazyGiveAccess(__XHANDLE xhandle,
			      __in sval file, __in uval type,
			      __in sval closeChain,
			      __inbuf(dataLen) char *data,
			      __in uval dataLen)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    BaseProcessRef pref;
    SysStatus rc;
    AccessRights match, nomatch;
    ProcessID dummy;
    ObjectHandle oh;
    ProcessID procID;

    // go a giveacessfromserver on object to kernel, passing same rights
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    rc = giveAccessByServer(oh, _KERNEL_PID, match, nomatch);
    _IF_FAILURE_RET(rc);

    // Set the client data
    sclnt(oh._xhandle)->setEnd(sclnt(xhandle)->getEnd());

    // get process from xhandle
    procID = XHandleTrans::GetOwnerProcessID(xhandle);
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(procID, pref);
    if (_FAILURE(rc)) {
	DREFGOBJ(TheXHandleTransRef)->free(xhandle);
	return rc;
    }

    // make a call on process object to pass along new object handle and
    //    data
    rc = DREF(pref)->lazyGiveAccess(file, type, oh, closeChain,
				    match, nomatch, data, dataLen);
    tassertMsg(_SUCCESS(rc), "?");

    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::_lazyReOpen(__out ObjectHandle &oh,
				__in ProcessID toProcID,
				__in AccessRights match,
				__in AccessRights nomatch,
				__XHANDLE xhandle)
{
    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER
    SysStatus rc;
    rc =  giveAccessByServer(oh, toProcID, match, nomatch);

    // Set the client data
    sclnt(oh._xhandle)->setEnd(sclnt(xhandle)->getEnd());
    
    return rc;
}


/* static */ SysStatus
StreamServerSocket::_CreateSocketPair(__out ObjectHandle &socket1,
				      __out ObjectHandle &socket2,
				      __CALLER_PID caller)
{
    #ifdef VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
    #endif // VERBOSE_UDS_SERVER
    SysStatus rc;
    StreamServerSocketRef sref;

    // Create the socket pair
    rc = StreamServerSocket::Create(sref);
    if (_FAILURE(rc)) return rc;

    rc = DREF(sref)->giveAccessByServer(socket1, caller,
					MetaObj::read|MetaObj::write|
					MetaObj::controlAccess,
					MetaObj::none);
    if (_FAILURE(rc)) return rc;
    sclnt(socket1._xhandle)->setEnd(SockClientData::END1);

    rc = DREF(sref)->giveAccessByServer(socket2, caller,
					MetaObj::read|MetaObj::write|
					MetaObj::controlAccess,
					MetaObj::none);
    sclnt(socket2._xhandle)->setEnd(SockClientData::END2);

    DREF(sref)->setSocketIsConnected(1);
    return rc;
}

void my_secret_check_handle(ObjectHandle oh) {
    ObjRef cRef; 
    TypeID t;
    SysStatus rc;
    rc = XHandleTrans::XHToInternal(oh.xhandle(), 0, 0, cRef, t);
    tassertRC(rc, "help");
}

/* static */ SysStatus
StreamServerSocket::_CreateSocket(__out ObjectHandle &socket,
				  __out uval &clientType,
                                  __in sval domain, __in sval type,
                                  __in sval protocol, __in uval traceFlag,
				  __CALLER_PID caller)
{
    #ifdef VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
    #endif // VERBOSE_UDS_SERVER
    SysStatus rc;
    StreamServerSocketRef sref;

    /*
     * the class TransPPCStream can be used by a server.  The client
     * instatiate one of these to coordinate with us.
     */
    clientType = TransStreamServer::TRANS_PPC;

    // Create the socket
    rc = StreamServerSocket::Create(sref, domain, type, traceFlag);
    if (_FAILURE(rc)) return rc;

    rc = DREF(sref)->giveAccessByServer(socket, caller,
					MetaObj::read|MetaObj::write|
					MetaObj::controlAccess,
					MetaObj::none);
    if (_FAILURE(rc)) return rc;
    sclnt(socket._xhandle)->setEnd(SockClientData::END1);
    my_secret_check_handle(socket);

    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::_dup(__out ObjectHandle &oh, __in ProcessID toProcID,
			 __XHANDLE xhandle)
{
    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER
    SysStatus rc;
    rc =  _giveAccess(oh, toProcID, xhandle);

    // Set the client data
    passertMsg(sclnt(xhandle)->getEnd()!=SockClientData::INVALID,
	       "Tried to copy invalid data\n");
    sclnt(oh._xhandle)->setEnd(sclnt(xhandle)->getEnd());
    

    return rc;
}

/* virtual */ SysStatus 
StreamServerSocket::_setsockopt(__in uval level, __in uval optname,
				__inbuf(optlen) char *optval,
				__in uval optlen, __XHANDLE xhandle) 
{
    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER
    SysStatus rc = 0;

    if (level != SOL_SOCKET) return _SERROR(2904, 0, EINVAL);

    switch (optname) {
    case SO_SNDBUF:
	#ifdef OVERLY_VERBOSE_UDS_SERVER
	err_printf("StreamServerSocket::_setsockoptions SO_SNDBUF\n");
	#endif // OVERLY_VERBOSE_UDS_SERVER
	if (optlen!=sizeof(sval32)) {
	    rc = -2;
	} else {
	    sclnt(xhandle)->setSndBufferSize(*((sval32 *)optval));
	}
	break;
	
    case SO_RCVBUF:
	#ifdef OVERLY_VERBOSE_UDS_SERVER
	err_printf("StreamServerSocket::_setsockoptions SO_RCVBUF\n");
	#endif // OVERLY_VERBOSE_UDS_SERVER
	if (optlen!=sizeof(sval32)) {
	    rc = -2;
	} else {
	    sclnt(xhandle)->setRcvBufferSize(*((sval32 *)optval));
	}
	break;

    default:
	tassertWrn(0, "Unsupported option for setsockopt %li\n", optname);
	rc = -1;
    }

    return rc;
}

/* virtual */ SysStatus 
StreamServerSocket::_getsockopt(__in uval level, __in uval optname,
				__outbuf(optlen:optlen) char *optval,
				__inout uval *optlen, __XHANDLE xhandle)
{
    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER
    SysStatus rc = 0;

    if (level != SOL_SOCKET) return _SERROR(2917, 0, EINVAL);

    switch (optname) {
    case SO_SNDBUF:
	*optlen = sizeof(sval32);
	*(sval32*)optval = sclnt(xhandle)->getSndBufferSize();
	break;
	
    case SO_RCVBUF:
	*optlen = sizeof(sval32);
	*(sval32*)optval = sclnt(xhandle)->getRcvBufferSize();
	break;

    default:
	tassertWrn(0, "Unsupported option for getsockoptions %li\n", optname);
	rc = _SERROR(2903, 0, ENOPROTOOPT);
    }

    return rc;
}

SysStatus
StreamServerSocket::locked_addFDToMessage(__XHANDLE xhandle,
					  __inbuf(dataLen) char *data,
					  __in uval dataLen)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    char *dataCopy;
    dataCopy = (char *)allocGlobal(dataLen);
    memcpy(dataCopy, data, dataLen);

    AncillaryData *ad;
    if (sclnt(xhandle)->getEnd()==SockClientData::END1) {
	ad = (AncillaryData *)allocGlobal(sizeof(AncillaryData));
	ad->index = writeByteCount1;
	ad->fdData = dataCopy;
	ad->fdDataSize = dataLen;
	fdTransferList2.addToEndOfList(ad);
    } else {
	ad = (AncillaryData *)allocGlobal(sizeof(AncillaryData));
	ad->index = writeByteCount2;
	ad->fdData = dataCopy;
	ad->fdDataSize = dataLen;
	fdTransferList1.addToEndOfList(ad);
    }

    return 0;
}

SysStatus
StreamServerSocket::_giveAccess(__out ObjectHandle &oh,
				__in ProcessID toProcID,
				__XHANDLE xhandle) __xa(controlAccess)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    SysStatus rc;
    rc = Obj::_giveAccess(oh, toProcID, xhandle);
    passertMsg(sclnt(xhandle)->getEnd()!=SockClientData::INVALID,
	       "Tried to copy invalid data\n");
    sclnt(oh._xhandle)->setEnd(sclnt(xhandle)->getEnd());

    return rc;
}

SysStatus
StreamServerSocket::_giveAccess(__out ObjectHandle &oh,
				__in ProcessID toProcID,
				__in AccessRights match,
				__in AccessRights nomatch,
				__XHANDLE xhandle) __xa(controlAccess)
{
    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    SysStatus rc;
    rc = Obj::_giveAccess(oh, toProcID, match, nomatch, xhandle);

    passertMsg(sclnt(xhandle)->getEnd()!=SockClientData::INVALID,
	       "Tried to copy invalid data\n");
    sclnt(oh._xhandle)->setEnd(sclnt(xhandle)->getEnd());
    return rc;
}



SysStatus
StreamServerSocket::locked_getFDFromMessage(__XHANDLE xhandle, 
					    char *sourceData,
					    char *data,
					    uval &buflen)

{
#ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s\n", __PRETTY_FUNCTION__);
#endif // OVERLY_VERBOSE_UDS_SERVER

    SysStatus rc;
    passertMsg(sourceData!=NULL, "sourceData should not be NULL");

    FileLinuxStream::FDTransferData transferData(sourceData);

    uval numEntries = transferData.getNumEntries();

    FileLinuxStream::FDTransferData::EntryLayout *el;
    TypeID clientType;
    
    for (uval i=0; i<numEntries; i++) {
	ObjectHandle newOh;
	
	el = transferData.getEntry(i);
	clientType = el->dataForType;
	switch (clientType) {
	case FileLinux_FILE:
	case FileLinux_FILEFIXEDSHARED:
	case FileLinux_DIR:
	case FileLinux_CHR_NULL:
	case FileLinux_CHR_ZERO:
	case FileLinux_CHR_TTY:
	case k42makedev(UNIX98_PTY_MASTER_MAJOR,0) ...
	    k42makedev(UNIX98_PTY_MASTER_MAJOR + UNIX98_PTY_MAJOR_COUNT-1,255):
	case k42makedev(UNIX98_PTY_SLAVE_MAJOR,0) ...
	    k42makedev(UNIX98_PTY_SLAVE_MAJOR + UNIX98_PTY_MAJOR_COUNT-1,255):
	case k42makedev(TTYAUX_MAJOR,2):
	case FileLinux_VIRT_FILE:
		{
#ifdef OVERLY_VERBOSE_UDS_SERVER
		    err_printf("Have FD data (file)\n");
#endif // OVERLY_VERBOSE_UDS_SERVER
		    StubFileLinuxServer stub(StubBaseObj::UNINITIALIZED);
		    stub.setOH(el->oh);
		    rc = stub._giveAccess(newOh, 
					  XHandleTrans::GetOwnerProcessID(xhandle));
		    passertMsg(_SUCCESS(rc), 
			       "Could not give access to receiving client\n");
		    rc = stub._releaseAccess();
		}
	break;
	    
	case FileLinux_STREAM:
	    {
#ifdef OVERLY_VERBOSE_UDS_SERVER
		err_printf("Have FD data (stream)\n");
#endif // OVERLY_VERBOSE_UDS_SERVER
		StubStreamServerSocket stub(StubBaseObj::UNINITIALIZED);
		stub.setOH(el->oh);
		rc = stub._giveAccess(newOh, 
				      XHandleTrans::GetOwnerProcessID(xhandle));
		passertMsg(_SUCCESS(rc), 
			   "Could not give access to receiving client\n");
		rc = stub._releaseAccess();
	    }
	    break;

	default:
	    tassertMsg(0, 
		       "Have unknown type (%li) to create on other "
		       "end of socket\n", clientType);
	}
	el->oh.initWithOH(newOh);
    }


    char *fdData;
    uval fdDataSize;
    transferData.getDataBlock(&fdData, &fdDataSize);
    
    passertMsg(fdDataSize<=buflen, 
	       "Not enough room to put fdData in\n");
    memcpy(data, fdData, fdDataSize);

    buflen = fdDataSize;
    //FIXME: set buflen to actual size of data returned

    return 0;
}

/* virtual */ SysStatus
StreamServerSocket::_bind()
{
    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER

    lock();
    tassertMsg(!socketIsBound,"socket already bound\n");
    socketIsBound = 1;
    unlock();
    return 0;
}

/**
 * Give access to the server object for a bound socket to an
 * arbitrary client.
 */
/* virtual */ SysStatus
StreamServerSocket::_getSocketObj(ObjectHandle &oh, ProcessID pid)
{
    SysStatus rc;

    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER

    // we are operating(i. e., 'this' is) on the server (bound) end of the
    // socket.
    if ((!socketIsBound)) {
	rc =  _SERROR(2885, 0, ECONNREFUSED);
    } else {
        rc = giveAccessByServer(oh, pid,
			   MetaObj::read|MetaObj::write|
			   MetaObj::controlAccess,
			   MetaObj::none);
    }

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s: returning ", __PRETTY_FUNCTION__);
    _SERROR_EPRINT(rc);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::_queueToConnect(__in ObjectHandle connectingSocketOH,
				    __XHANDLE xhandle)
{
    SysStatus rc;

    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER

    // we are operating on the server (bound) end of the socket.
    // save the connecting socket handle in the list anchored
    // in 'this' object (the server object for a bound socket).
    lock();
    if ((!socketIsBound) || (!socketIsListening)) {
	rc =  _SERROR(2891, 0, ECONNREFUSED);
    } else {
	ObjRef cRef; 
	TypeID type;
	rc = XHandleTrans::XHToInternal(
		connectingSocketOH.xhandle(), 0, 0, cRef, type);
	tassertRC(rc, "oops");
	tassertMsg(MetaStreamServerSocket::isBaseOf(type), 
	               "wrong type in connected object handle");

	// Note:  Although we are reading a file in the socket that wishes
	// connect, we have no guarantee on the read (could be changed by
	// another thread soon after since we have no lock on that object).
	uval socketConnected;
	DREF((StreamServerSocketRef)cRef)->getSocketIsConnected(socketConnected);
	if (socketConnected == 1) {
	    rc = 0;
	} else {
	    ConnectSocketData* csd;
	    csd =(ConnectSocketData*)allocGlobal(sizeof(ConnectSocketData));
	    csd->socketOH = connectingSocketOH;

            if (connectedSocketList.find(csd)) {
		rc = 0;
		delete csd;
	    } else if (listenBacklog > 0 ) {
		listenBacklog--;
		#ifdef OVERLY_VERBOSE_UDS_SERVER
		err_printf("Adding connection   xhandle 0x%lx "
		       " to socket %p (backlog %ld)\n",
		       connectingSocketOH.xhandle(), this, listenBacklog);
		#endif // OVERLY_VERBOSE_UDS_SERVER
		my_secret_check_handle(connectingSocketOH);
		connectedSocketList.addToEndOfList(csd);
		rc = 0;
	    } else {
		delete csd;
		rc = _SERROR(2893, 0, ECONNREFUSED);
	    }
        }
    }

    // Through this we expect to wake up eventual thread that might
    // be blocked in accept()
    locked_signalDataAvailable();
    unlock();

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s: returning ", __PRETTY_FUNCTION__);
    _SERROR_EPRINT(rc);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::_connect(__inbuf(addrLen) const char *addr,
			     __in uval addrLen, __out GenState& moreAvail,
			     __XHANDLE xhandle)
{
    SysStatus rc = 0;

    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER

    lock();
    calcAvailable(moreAvail, clnt(xhandle));
    clnt(xhandle)->setAvail(moreAvail);
    locked_signalDataAvailable();	// this might not be needed
    unlock();

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s: returning ", __PRETTY_FUNCTION__);
    _SERROR_EPRINT(rc);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::_listen(__in sval backlog)
{
    SysStatus rc;

    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER
    
    lock();
    if ((socketIsListening) || (!socketIsBound)) {
	rc = _SERROR(2902, 0, EINVAL);
    } else {
	socketIsListening = 1;
	listenBacklog = backlog;
	rc = 0;
    }
    unlock();

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s ", __PRETTY_FUNCTION__);
    _SERROR_EPRINT(rc);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::_accept(__out ObjectHandle &oh,
                            __inoutbuf(len:len:len) char* addr,
                            __inout uval& len,
                            __out GenState &moreAvail,
                            __XHANDLE xhandle)
{
// oh is the returned object handle of the connected socket
// addr is the remote address (in this case it is the file name
// unless someone is playing games with symlinks.  ATM it is ignored
// len is the length of the same
// more avail is a complicated way to return how many ready (connected)
// socket there are beside the one being connected.  The complication
// has to do with the fact that a semi-exact count is being passed between
// processes and more than one such may arrive, not necessatly in the
// order in which is was sent.
    SysStatus rc;
    ObjectHandle connectedOH;
    uval found = 0;

    #ifdef VERBOSE_UDS_SERVER
    if (traceThisSocket)
	err_printf("%s:this %p\n", __PRETTY_FUNCTION__, this);
    #endif // VERBOSE_UDS_SERVER

    lock();
    if ((!socketIsListening) || (!socketIsBound)) {
	rc = _SERROR(2886, 0, EINVAL);
    } else {
	if (connectedSocketList.isEmpty()) {
	    // Return success, invalid OH
            oh.init();
	} else { // someone is waiting to connect */
	    ConnectSocketData* csd;

	    connectedSocketList.removeHead(csd);
	    listenBacklog++;	
	    connectedOH = csd->socketOH;
	    found = 1;
	    #ifdef OVERLY_VERBOSE_UDS_SERVER
    	    err_printf("accepting connection <CommID 0x%lx  xhandle 0x%lx> "
		       " for socket %p\n", 
		       connectedOH.commID(), connectedOH.xhandle(), this);
	    #endif // OVERLY_VERBOSE_UDS_SERVER
	    freeGlobal(csd, sizeof(struct ConnectSocketData));
	}
	rc = 0;
	calcAvailable(moreAvail, clnt(xhandle));
	clnt(xhandle)->setAvail(moreAvail);
    }
    unlock();
 
    if (_SUCCESS(rc) && (1==found)) {
        ObjRef cRef; 
	TypeID type;
	SysStatus rcx;
	rcx = XHandleTrans::XHToInternal(connectedOH.xhandle(),
					 0, 0, cRef, type);
	if (likely(_SUCCESS(rcx))) {
	    tassertMsg(MetaStreamServerSocket::isBaseOf(type), 
	               "wrong type in connected object handle");
            DREF((StreamServerSocketRef)cRef)->giveAccessByServer(oh,
				XHandleTrans::GetOwnerProcessID(xhandle),
				MetaObj::read|MetaObj::write|
				MetaObj::controlAccess,
				MetaObj::none);
	    setEndSocketClientData(oh._xhandle);
            DREF((StreamServerSocketRef)cRef)->setSocketIsConnected(1);
            DREF((StreamServerSocketRef)cRef)->signalDataAvailable();
	    //signalDataAvailable();
	} else {
	    // in all likelyhood this socket was freed
	    // Return success, invalid OH
            oh.init();
            rc = 0;
	}
    }

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s  current backlog=%ld  oh returned ",
	        __PRETTY_FUNCTION__, listenBacklog);
    if (oh.valid()) {
	err_printf("<CommID 0x%lx  xhandle 0x%lx> ", oh.commID(), oh.xhandle());
		   
    } else {
	err_printf("INVALID ");
    }
    _SERROR_EPRINT(rc);
    #endif // OVERLY_VERBOSE_UDS_SERVER
    return rc;
}

/* virtual */ SysStatus
StreamServerSocket::setEndSocketClientData(XHandle xhandle)
{
    sclnt(xhandle)->setEnd(SockClientData::END2);
    return 0;
}

/* virtual */ SysStatus
StreamServerSocket::handleXObjFree(XHandle xhandle)
{
    SysStatus rc;

    #ifdef OVERLY_VERBOSE_UDS_SERVER
    err_printf("%s  this=%p  (objRef is %p) "
	       "for xhandle 0x%lx\n", __PRETTY_FUNCTION__, this,
               getRef(), (uval) xhandle);
    #endif // OVERLY_VERBOSE_UDS_SERVER

    rc = StreamServer::handleXObjFree(xhandle);
    _IF_FAILURE_RET(rc);

    lock();

    // the count field is incorrect for bound socket.  The fs is counted
    // as a user.  Furthermore we do not revokeAccess to the connecting
    // clients.  The count should be correct for connected sockets.
    useCount--;

    // only streams have this property
    if ((SOCK_STREAM==socketType)&&(socketIsConnected)&&(useCount<2)) {
	locked_signalDataAvailable();
    }

    unlock();

    return 0;
}

/* virtual */ SysStatus
StreamServerSocket::getType(TypeID &id) { 
    id = MetaStreamServerSocket::typeID();
    return 0;
}

/* virtual */ SysStatus
StreamServerSocket::_socketTrace(__in uval onOrOff) {
    traceThisSocket = onOrOff;
    return 0;
}

/* virtual */ SysStatus
StreamServerSocket::setSocketIsConnected(uval connected) {
   socketIsConnected = connected;
   return 0;
}

/* virtual */ SysStatus
StreamServerSocket::getSocketIsConnected(uval &connected) {
    connected = socketIsConnected;
    return 0;
}
