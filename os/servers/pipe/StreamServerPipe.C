/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServerPipe.C,v 1.46 2005/07/15 17:14:33 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Exports a pipe interface.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <io/IO.H>
#include <cobj/CObjRoot.H>
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include <stub/StubStreamServer.H>
#include <meta/MetaStreamServer.H>
#include <meta/MetaStreamServerPipe.H>
#include "StreamServerPipe.H"
#include <sys/ProcessSet.H>
#include <unistd.h>
#include <sync/Sem.H>

struct StreamServerPipe::PipeClientData: public StreamServer::ClientData {
    uval isWriter;		// client purpose: 0=reader, 1=writer
public:
    DEFINE_GLOBAL_NEW(PipeClientData);
    PipeClientData():ClientData(),isWriter(0) { /* empty body */ }
    virtual void setAvail(GenState &ma) {
	if ( isWriter )
	    ma.state &= ~FileLinux::READ_AVAIL;
	else
	    ma.state &= ~FileLinux::WRITE_AVAIL;
	ClientData::setAvail(ma);
    }

    virtual SysStatus signalDataAvailable(GenState avail) {
	if (isWriter) {
	    avail.state &= ~FileLinux::READ_AVAIL;
	} else {
	    avail.state &= ~FileLinux::WRITE_AVAIL;
	}
	if (avail.state ^ available.state) {
	    return StreamServer::ClientData::signalDataAvailable(avail);
	}
	return 0;
    }
    virtual void setWriter(uval writer) {isWriter = writer;}
    uval getWriter() {return isWriter;}

};

inline bool
StreamServerPipe::writersExist() {
    return wcount>0;
}

inline bool
StreamServerPipe::readersExist() {  // Assumes lock is held
    return rcount>0;
}

inline /* virtual */ void
StreamServerPipe::calcAvailable(GenState& avail,
				StreamServer::ClientData *client) {
    uval state = 0;
    PipeClientData* pc = (PipeClientData*)client;

    // Am a reader and bytes available to read
    if (rb.bytesAvail() && (!pc || !pc->getWriter())) {
	state |= FileLinux::READ_AVAIL;
    }
    // Am a reader and no writers exist
    if (!writersExist() && (!pc || !pc->getWriter()) &&!neverHadWriter) {
	state |= FileLinux::ENDOFFILE;
    }

    // Am a writer and readers exist and space available
    if (rb.spaceAvail() && (!pc || pc->getWriter()) && readersExist()) {
	state |= FileLinux::WRITE_AVAIL;
    }

    avail.state = state;
}

// A cludge to type cheat XBaseObj clientData to ClientData
inline
StreamServerPipe::PipeClientData* StreamServerPipe::pclnt(XHandle xhandle) {
    StreamServerPipe::PipeClientData* retvalue;
    retvalue = (StreamServerPipe::PipeClientData *)
	(XHandleTrans::GetClientData(xhandle));
    return (retvalue);
}

/* virtual */ SysStatus
StreamServerPipe::handleXObjFree(XHandle xhandle)
{
    SysStatus rc;

    tassert(wcount >= 0 && rcount>=0, err_printf("reference count woops\n"));
    rc = StreamServer::handleXObjFree(xhandle);

    _IF_FAILURE_RET(rc);

    lock();
    // export list locked, so decrement reference count
    // but do not signal (walking export list)
    if (pclnt(xhandle)->getWriter()) {
	--wcount;
//	err_printf("wcount decreased to %ld\n", wcount);
    } else {
	--rcount;
//	err_printf("rcount decreased to %ld\n", rcount);
    }

    if (wcount == 0 || rcount == 0) {
	    // walks exported xobj list so list must not be locked
	// but object must be locked
	locked_signalDataAvailable();
    }

    // Hack to compensate that this object should be destroyed
    // when there is neither a reader or writer, but this isn't done yet
    if (wcount==0 && rcount==0) {
	neverHadWriter = true;
    }

    unlock();
    return 0;
}

/* static */ SysStatus
StreamServerPipe::Create(IORef &ref)
{
    // create the new pipe
    StreamServerPipe *nPipe = new StreamServerPipe();

    nPipe->init();

    ref = (IORef)CObjRootSingleRep::Create(nPipe);
    tassert((ref != 0), err_printf("woops\n"));

    return 0;
}

/* virtual */ SysStatus
StreamServerPipe::flush()
{
    return 0;
};

/* virtual */ SysStatus
StreamServerPipe::destroy()
{
    SysStatus rc;

    // remove all ObjRefs to this object
    rc = closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if (_FAILURE(rc)) {
	err_printf("StreamServerPipe::already being destroyed\n");
	return _SCLSCD(rc) == 1 ? 0 : rc;
    }
    destroyUnchecked();
    return 0;
}

/* virtual */ SysStatus
StreamServerPipe::detach()
{
    lock();

    // XXX - are there additional references that should have a refcount?
    if (isEmptyExportedXObjectList()) {
	unlock();
	err_printf("\t xlist empty: destroying pipe\n");
	return destroy();
    } else {
	unlock();
	err_printf("\t xlist not empty\n");
	return 0;
    }
}

SysStatusUval
StreamServerPipe::locked_readInternal(struct iovec *vec, uval len)
{
    uval stlen = 0;
    uval bytes;
    uval i = 0;
    while (i < len) {
	bytes = rb.getData((char*)vec[i].iov_base,vec[i].iov_len);
	if (bytes==0) {
	    return _SRETUVAL(stlen);
	}
	stlen += bytes;
	++i;
    }
    return _SRETUVAL(stlen);
}

SysStatusUval
StreamServerPipe::locked_writeInternal(struct iovec *vec, uval len)
{
    uval stlen = 0;
    uval bytes;
    uval i = 0;
    while (i < len) {
	bytes = rb.putData((char*)vec[i].iov_base,vec[i].iov_len);
	if (bytes==0) {
	    return _SRETUVAL(stlen);
	}
	stlen += bytes;
	++i;
    }
    return _SRETUVAL(stlen);
}

/* static */ SysStatus
StreamServerPipe::_Create(__out ObjectHandle &ohr, __out ObjectHandle &ohw,
			  __CALLER_PID caller)
{
    SysStatus rc;
    IORef sref;

    rc = StreamServerPipe::Create(sref);
    if (_FAILURE(rc)) return rc;

    rc = DREF(sref)->giveAccessByServer(ohr, caller,
					MetaObj::read|MetaObj::controlAccess,
					MetaObj::none);
    if (_FAILURE(rc)) return rc;

    rc = DREF(sref)->giveAccessByServer(ohw, caller,
					MetaObj::write|MetaObj::controlAccess,
					MetaObj::none);

    return rc;
}

/* static */ SysStatus
StreamServerPipe::_Create(__out ObjectHandle &oh, __CALLER_PID caller)
{
    SysStatus rc;
    IORef sref;

    rc = StreamServerPipe::Create(sref);
    if (_FAILURE(rc)) return rc;

    rc = DREF(sref)->giveAccessByServer(oh, caller,
					MetaObj::controlAccess,
					MetaObj::none);
    if (_FAILURE(rc)) return rc;

    return rc;
}

/* virtual */ SysStatusUval
StreamServerPipe::recvfrom(struct iovec *vec, uval veclen, uval flags,
			   char *addr, uval &addrLen, GenState &moreAvail, 
			   void *controlData, uval &controlDataLen,
			   __XHANDLE xhandle)
{
    SysStatusUval rc;
    addrLen = 0;

    controlDataLen = 0; /* setting to zero, since no control data */

    lock();
    rc = locked_readInternal(vec, veclen);


    if (_SUCCESS(rc)) {
	calcAvailable(moreAvail);
	clnt(xhandle)->setAvail(moreAvail);
	locked_signalDataAvailable();
    }
    unlock();
    return rc;
}

/* virtual */ SysStatusUval
StreamServerPipe::sendto(struct iovec* vec, uval veclen, uval flags,
			 const char *addr, uval addrLen, GenState &moreAvail, 
			 void *controlData, uval controlDataLen,
			 __XHANDLE xhandle)
{
    SysStatusUval rc=0;

     tassertMsg((controlDataLen == 0), "oops\n");
     addrLen = 0;
    lock();
    if ( !readersExist() ) {
	unlock();
	return _SERROR(1882, 0, EPIPE);
    }

    rc = locked_writeInternal(vec, veclen);

    if (_SUCCESS(rc)) {
	calcAvailable(moreAvail, clnt(xhandle));
	clnt(xhandle)->setAvail(moreAvail);
	locked_signalDataAvailable();
    }
    unlock();
    return rc;
}

/* virtual */ SysStatus
StreamServerPipe::giveAccessSetClientData(ObjectHandle &oh,
					  ProcessID toProcID,
					  AccessRights match,
					  AccessRights nomatch,
					  TypeID type)
{
    SysStatus retvalue;
    lock();
    PipeClientData *clientData = new PipeClientData();
//     err_printf("In giveAccessSetClientData match 0x%lx\n", (uval) match);
    if (match & MetaObj::write) {
	clientData->setWriter(1);
	++wcount;
	neverHadWriter = false;
// 	err_printf("wcount increased to %ld\n", wcount);
    } else if (match & MetaObj::read) {
	clientData->setWriter(0);
	++rcount;
	if (rcount==1) {
	    // Signal that writers can now write data
	    locked_signalDataAvailable();
	}
// 	err_printf("rcount increased to %ld\n", rcount);
    }

    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
			      type, (uval)clientData);
    unlock();
    return (retvalue);
}

/* static */ void
StreamServerPipe::ClassInit()
{
    MetaStreamServerPipe::init();

#if 0
    while (1) {
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
#endif /* #if 0 */
    return;
}


SysStatus
StreamServerPipe::_createReader(__out ObjectHandle &oh,
				__in ProcessID caller)
{
    SysStatus rc;
    rc = giveAccessByServer(oh, caller,  MetaObj::read|MetaObj::controlAccess,
			    MetaObj::none);
    return rc;
}

SysStatus
StreamServerPipe::_createWriter(__out ObjectHandle &oh, 
				__in ProcessID caller)
{
    SysStatus rc;
    rc = giveAccessByServer(oh, caller,  MetaObj::write|MetaObj::controlAccess,
			    MetaObj::none);
    return rc;
}

