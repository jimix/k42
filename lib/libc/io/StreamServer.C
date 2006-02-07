/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: StreamServer.C,v 1.44 2005/07/15 17:14:25 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provide infrastructure for "stream" servers.
 ***************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubStreamServer.H>
#include "StreamServer.H"
#include <stub/StubLogin.H>
#include <io/FileLinux.H>
#include <cobj/XHandleTrans.H>
#include <trace/traceIO.h>
#include <sys/ProcessSet.H>

/*virtual*/ void
StreamServer::IOMemEvents::nowDead(MemTransRef mtr)
{
    //If MemTrans is going away -> no IO objects left
    //--> nobody is referring to us -> go bye bye
    delete this;
}

StreamServer::ClientData::ClientData() :
    stub(StubObj::UNINITIALIZED), blocked(~0ULL)
{
    ici.bytesLeft = 0;
    ici.ioLen = 0;
    ici.bufLen = 0;
    // start out with invalid to avoid race condition until
    // client has initialized object for upcall
    available.state = FileLinux::INVALID;
    mtr = NULL;
    ObjectHandle oh;
    oh.initWithCommID((uval)-1,(uval)-1);
    stub.setOH(oh);
}

void
StreamServer::ClientData::setAvail(GenState &ma)
{
    ma.makeNewer(available);
    available.setIfNewer(ma);
    TraceOSIOSetAvail(stub.getOH().xhandle(), ma.fullVal);

    silence = 8;
}

SysStatus
StreamServer::ClientData::signalDataAvailable(GenState avail)
{
    SysStatus rc = 0;

    // only inform client if he has been asking, i.e., state not invalid
    if (available.state & FileLinux::INVALID) {
	return 0;
    }

    if (~available.state & avail.state) {
	if (silence == 1) {
	    // Last async notification --> convert to invalid
	    avail.state = FileLinux::INVALID;
	}

	// Use a temporary to set the counter and communicate the
	// state to the client, but only save the counter and state
	// in "available" on success --- otherwise when we retry we'd
	// fail the test above.
	avail.makeNewer(available);
	rc = stub._signalDataAvailable(avail);
	// if didn't succeed, should retry
	if (_SUCCESS(rc)) {
	    available.setIfNewer(avail);
	    TraceOSIOSetAvail(stub.getOH().xhandle(), available.fullVal);
	    silence--;
	} else {
	    // Other side hasn't registered with us,
	    // so don't report the failure
	    if (stub.getOH().commID() == SysTypes::COMMID_NULL) {
		return 0;
	    }
	}
    }
    return rc;
}

// A cludge to type cheat XBaseObj clientData to ClientData
/* static */
StreamServer::ClientData* StreamServer::clnt(XHandle xhandle) {
    StreamServer::ClientData* retvalue;
    retvalue = (StreamServer::ClientData *)
	(XHandleTrans::GetClientData(xhandle));
    return (retvalue);
}

/* static */ void
StreamServer::BeingFreed(XHandle xhandle) {
    StreamServer::ClientData *clientData = clnt(xhandle);
    delete clientData;
}

/*virtual*/ SysStatus
StreamServer::exportedXObjectListEmpty()
{
    return destroy();
}

/* virtual */ SysStatus
StreamServer::handleXObjFree(XHandle xhandle)
{
    ClientData* cd = clnt(xhandle);

    if (cd->mtr) {
	DREF(cd->mtr)->detach();
	cd->mtr = NULL;
    }
    XHandleTrans::SetBeingFreed(xhandle, BeingFreed);

    return 0;
}

/* virtual */ SysStatus
StreamServer::locked_signalDataAvailable()
{
    XHandle xhandle;

    uval retries = 0;
 retry:
    // now traverse list of clients and tell them
    if (_FAILURE(lockIfNotClosingExportedXObjectList())) return 0;
    xhandle = getHeadExportedXObjectList();
    while (xhandle != XHANDLE_NONE) {
	GenState available;
	calcAvailable(available, clnt(xhandle));
	TraceOSIOSendAsync(xhandle, available.fullVal);
	SysStatus rc = clnt(xhandle)->signalDataAvailable(available);
	//tassert(_SUCCESS(rc), err_printf("woops\n"));
	if (!_SUCCESS(rc) && _SGENCD(rc) == EBUSY) {
	    // FIXME: wrong solution since we are dlaying in kernel waiting
	    // for user resources; must have retry that doesn't use
	    // kernel resources while waiting
	    unlockExportedXObjectList();
	    //err_printf("FIXME: StreamServerPipe: client busy; retry: %ld\n",
	    //       retries);
	    tassertWrn(((retries%100)!=5),
		       "stalled on notification to %ld, retries %ld\n",
		       XHandleTrans::GetOwnerProcessID(xhandle), retries);
	    unlock();
	    Scheduler::DelayMicrosecs(1000);
	    lock();
	    retries++;
	    goto retry;
	} else if (!_SUCCESS(rc)) {
	    if (_FAILURE(DREFGOBJ(TheXHandleTransRef)->
					xhandleValidate(xhandle)) ||
		    !clnt(xhandle)->isValidOH()) {
		xhandle = getNextExportedXObjectList(xhandle);
		continue;
	    }
	    err_printf("%s Unexpected error code ", __func__);
	    _SERROR_EPRINT(rc);
	}
	xhandle = getNextExportedXObjectList(xhandle);
    }

    unlockExportedXObjectList();

    return 0;
}

/* virtual */ SysStatus
StreamServer::_registerCallback(__in ObjectHandle callback,
				__XHANDLE xhandle)
{
    clnt(xhandle)->setOH(callback);
    return 0;
}

/* virtual */ SysStatusUval
StreamServer::_getAvailability(__out GenState &avail, __XHANDLE xhandle)
{
    lock();
    calcAvailable(avail, clnt(xhandle));

    clnt(xhandle)->setAvail(avail);
    unlock();
    return avail.state;
}

/* virtual */ SysStatus
StreamServer::_flush()
{
    return flush();
}

/* virtual */ SysStatus
StreamServer::flush()
{
    return 0;
};

/* virtual */ SysStatus
StreamServer::destroy()
{
    SysStatus rc;

    // remove all ObjRefs to this object
    rc = closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if (_FAILURE(rc)) {
	err_printf("In StreamServer::destroy(): already being destroyed\n");
	return _SCLSCD(rc) == 1 ? 0 : rc;
    }

    destroyUnchecked();
    return 0;
}

/* virtual */ SysStatus
StreamServer::giveAccessSetClientData(ObjectHandle &oh,
				      ProcessID toProcID,
				      AccessRights match,
				      AccessRights nomatch,
				      TypeID type)
{
    SysStatus retvalue;
    ClientData *clientData = new ClientData();
    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
			      type, (uval)clientData);
    return (retvalue);
}

/* virtual */ SysStatusUval
StreamServer::_ioctl(uval request, uval &size, char* arg, __XHANDLE xhandle)
{
    return _SERROR(2036, 0, EOPNOTSUPP);
}

/* virtual */ SysStatus
StreamServer::_lazyGiveAccess(__XHANDLE xhandle,
			      __in sval file, __in uval type,
			      __in sval closeChain,
			      __inbuf(dataLen) char *data,
			      __in uval dataLen)
{
    BaseProcessRef pref;
    SysStatus rc;
    AccessRights match, nomatch;
    ProcessID dummy;
    ObjectHandle oh;
    ProcessID procID;

    // go a giveacessfromserver on object to kernel, passing same rights
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    rc = giveAccessByServer(oh, _KERNEL_PID, match, nomatch);
    if (_FAILURE(rc)) {
	return rc;
    }

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


/* virtual */ SysStatusUval
StreamServer::_sendto(__inbuf(len) char *buf, __in uval len,
		      __in uval flags,
		      __inbuf(addrLen) const char *addr, __in uval addrLen,
		      __in uval bytesLeft,
		      __out GenState &moreAvail, 
		      __inbuf(controlDataLen) const char *controlData,
		      __in uval controlDataLen,
		      __XHANDLE xhandle)
{
    struct iovec vec[2];
    uval lastVec = 0;
    SysStatusUval rc = len;
    ClientData * cd = (ClientData*)clnt(xhandle);
    ClientData::IOCompInfo * ici = cd->getICI();
    if (!ici || bytesLeft == len) {
	if (ici && ici->ioLen) {
	    tassertMsg(bytesLeft + ici->ioLen == ici->bufLen,
		       "Buffer size mismatch: %lx %x %x\n",
		       bytesLeft, ici->ioLen, ici->bufLen);
	    vec[0].iov_base = (void*)ici->buf;
	    vec[0].iov_len = ici->ioLen;
	    ++lastVec;
	}
	vec[lastVec].iov_base = (char*)buf;
	vec[lastVec].iov_len = len;

	/*
	 * control data is passed in on the last sendto of the total 
	 * message, so we can pass it right through to the internal
	 * object here
	 */
	rc = sendto(vec, lastVec+1, flags, addr, addrLen, moreAvail, 
		    (void *)controlData, controlDataLen, xhandle);

	if (ici && ici->ioLen) {
	    freeLocalStrict(ici->buf, ici->bufLen);
	    ici->buf = NULL;
	    ici->bytesLeft = ici->ioLen = 0;
	}
    } else {
	tassertMsg(bytesLeft>len,"bytesLeft small: %lx %lx\n", bytesLeft, len);
	if (ici->ioLen == 0) {
	    tassertMsg(ici->bytesLeft == 0,
		       "Another PPC IO operation in progress? %p\n",cd);
	    ici->buf = (char*)allocLocalStrict(bytesLeft);
	    ici->bufLen = bytesLeft;
	    ici->bytesLeft = ici->ioLen = 0;
	}

	tassertMsg(bytesLeft + ici->ioLen == ici->bufLen,
		   "Buffer size mismatch: %lx %x %x\n",
		   bytesLeft, ici->ioLen, ici->bufLen);

	memcpy(ici->buf + ici->ioLen, buf, len);
	ici->ioLen += len;
    }

    return rc;
}

/* virtual */ SysStatusUval
StreamServer::_recvfrom(
    __outbuf(__rc:len) char *buf, __in uval len,
    __in uval flags,
    __outbuf(addrLen:addrLen) char *addr,
    __inout uval &addrLen, __inout uval &bytesLeft,
    __out GenState &moreAvail, 
    __outbuf(controlDataLen:controlDataLen) char *controlData,
    __inout uval &controlDataLen, 
    __XHANDLE xhandle)
{
    SysStatusUval rc = 0;
    ClientData * cd = (ClientData*)clnt(xhandle);
    ClientData::IOCompInfo * ici = cd->getICI();

    // If bytesLeft, then this is a continuation of an earlier recvfrom
    if (ici && ici->bytesLeft > 0) {
	len = MIN(MIN(len, uval(ici->bytesLeft)),PPCPAGE_LENGTH_MAX-1024);
	memcpy(buf, ici->buf + ici->ioLen - ici->bytesLeft, len);
	addrLen = 0;
	ici->bytesLeft -= len;
	bytesLeft = ici->bytesLeft;
	if (ici->bytesLeft == 0) {
	    freeLocalStrict(ici->buf, ici->bufLen);
	    ici->buf = NULL;
	    ici->ioLen = 0;
	}
	controlDataLen = 0;
	rc = _SRETUVAL(len);
    } else {
	//Do a recvfrom operation from scratch
	struct iovec vec[2];
	uval vecs = 1;
	uval totalLen = bytesLeft;
	uval len1;
	// First vec is all that can be returned immediately
	vec[0].iov_base = buf;
	len1 = vec[0].iov_len  = MIN(len, PPCPAGE_LENGTH_MAX-1024);

	// Second vec is what can be returned only on a continuation operation
	// bytesLeft as an input tells us how many bytes are expected
	// for the entire set of calls

	// This test identifies if this will be a multi-ppc transfer
	if (ici && vec[0].iov_len < totalLen) {
	    ici->bufLen = vec[1].iov_len = totalLen - vec[0].iov_len;
	    ici->buf = (char*)allocLocalStrict(vec[1].iov_len);
	    vec[1].iov_base = ici->buf;
	    ++vecs;
	}
	bytesLeft = 0;

	//recvfrom corrupts "vec"
	rc = recvfrom(vec, vecs, flags, addr, addrLen, moreAvail, 
		      controlData, controlDataLen, xhandle);

	if (_FAILURE(rc)) {
	    if (vecs > 1 && ici) {
		freeLocalStrict(ici->buf, ici->bufLen);
		ici->buf = NULL;
		ici->ioLen = 0;
	    }
	} else {
	    tassertMsg((controlDataLen != 128), "probably not initilized\n");
	    //Can we return everything now, or will there be bytesLeft?
	    if (_SGETUVAL(rc) <= len1) {
		if (vecs>1 && ici) {
		    freeLocalStrict(ici->buf, ici->bufLen);
		    ici->buf = NULL;
		    ici->ioLen = 0;
		}
	    } else if (ici) {
		// bytesLeft>0 indicates more data to recv
		ici->bytesLeft = _SGETUVAL(rc) - len1;
		bytesLeft = _SGETUVAL(rc) - len1;
		ici->ioLen = _SGETUVAL(rc) - len1;
		rc = _SRETUVAL(len1);
	    }
	}
    }

    return rc;

}

/* virtual */ SysStatus
StreamServer::_getStatus(struct stat &status)
{
    status.st_dev	= 0;	// FIXME unix major
    status.st_ino	= 0;
    status.st_mode	= S_IFCHR+0666;
    status.st_nlink	= 1;
    status.st_uid	= 0;
    status.st_gid	= 0;
    status.st_rdev	= 1;	// FIXME      minor I think
    status.st_size	= 0;
    status.st_blksize	= 0x1000;
    status.st_blocks	= 0;	// FIXME
    status.st_atime	= 0;	// FIXME
    status.st_ctime	= 0;	// FIXME
    status.st_mtime	= 0;	// FIXME
    return 0;
}

/* virtual */ SysStatus 
StreamServer::signalDataAvailable() 
{
	lock();
	SysStatus rc=locked_signalDataAvailable();
	unlock();
	return rc;
    }
