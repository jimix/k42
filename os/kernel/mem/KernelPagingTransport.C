/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelPagingTransport.C,v 1.9 2005/08/30 19:09:53 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: kernel paging transport object, which talks to a
 *                     paging transport in the file system, providing
 *                     flow control
 * **************************************************************************/

#include "kernIncs.H"
#include "KernelPagingTransport.H"
#include "StubFileHolder.H"
#include "FR.H"
#include "FRPA.H"
#include "FRVA.H"
#include "FRPANonPageable.H"
#include "FRPANonPageableRamOnly.H"
#include "mem/IORestartRequests.H"

uval ok_kpsfp=0;
uval ok_kpsfpc=0;
uval ok_kpswrite=0;
uval ok_kpcomp=0;
uval ok_kptry=0;
uval ok_kptryfail=0;
uval ok_kptrysucc=0;


/* virtual */ SysStatus
KernelPagingTransport::startFillPage(uval fileToken, uval addr, uval objOffset)
{
    tassertMsg(pidx_ptr != NULL, "?");
    tassertMsg(*pidx_ptr < NUM_ENTRIES, "?");
    SysStatus rc;

    Request req = {START_FILL, fileToken, addr, objOffset, 0};

    AutoLock<BLock> al(&lock); // locks now, unlocks on return

    ok_kpsfp++;
    rc = locked_putRequest((uval*) &req, 1 /* highPriority */);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    ok_kpsfpc++;
    return rc;
}

/* virtual */ SysStatus
KernelPagingTransport::startWrite(uval fileToken, uval addr, uval objOffset,
				  uval sz)
{
    tassertMsg(pidx_ptr != NULL, "?");
    tassertMsg(*pidx_ptr < NUM_ENTRIES, "?");
    SysStatus rc;

    Request req = {START_WRITE, fileToken, addr, objOffset, sz};

    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    ok_kpswrite++;
    rc = locked_putRequest((uval*) &req, 0 /* not highPriority */);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    return rc;
}

/* virtual */ SysStatus
KernelPagingTransport::frIsNotInUse(uval fileToken)
{
    tassertMsg(pidx_ptr != NULL, "?");
    tassertMsg(*pidx_ptr < NUM_ENTRIES, "?");
    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    return stubTrans->_frIsNotInUse(fileToken);
}

/* virtual */ SysStatus
KernelPagingTransport::_createFRPA(__out ObjectHandle &oh,
				   __CALLER_PID processID,
				   __in ObjectHandle file,
				   __in uval len,
				   __in uval filetoken,
				   __inbuf(namelen) char *name,
				   __in uval namelen) __xa(none)
{
    return FRPA::Create(oh, processID, file, len, filetoken,
			name, namelen,
			(KernelPagingTransportRef)getRef());
}

/* virtual */ SysStatus
KernelPagingTransport::_createFRVA(__out ObjectHandle &oh,
				   __CALLER_PID processID,
				   __in uval transferAddr,
				   __in ObjectHandle file,
				   __in uval len,
				   __in uval filetoken,
				   __inbuf(namelen) char *name,
				   __in uval namelen) __xa(none) {
    return FRVA::Create(oh, processID, transferAddr, file, len, filetoken,
			name, namelen, (KernelPagingTransportRef) getRef());
}

/* virtual */ SysStatus
KernelPagingTransport::_createFRPANonPageable(
    __out ObjectHandle &oh,
    __CALLER_PID processID,
    __in ObjectHandle file,
    __in uval len,
    __in uval filetoken,
    __inbuf(namelen) char *name,
    __in uval namelen) __xa(none)
{
    return FRPANonPageable::Create(oh, processID, file, len, filetoken,
				   name, namelen,
				   (KernelPagingTransportRef)getRef());
}

/* virtual */ SysStatus
KernelPagingTransport::_createFRPANonPageableRamOnly(
    __out ObjectHandle &oh,
    __CALLER_PID processID,
    __in ObjectHandle file,
    __in uval len,
    __in uval filetoken,
    __inbuf(namelen) char *name,
    __in uval namelen) __xa(none)
{
    return FRPANonPageableRamOnly::Create(oh, processID, file, len, filetoken,
					  name, namelen,
					  (KernelPagingTransportRef)getRef());
}

/* virtual */ SysStatus
KernelPagingTransport::kickConsumer()
{
    return stubTrans->_startIO();
}


/* virtual */ SysStatus
KernelPagingTransport::ioComplete()
{
    SysStatus rc;
    IORestartRequests *copyrr;

    lock.acquire();
    // FIXME: what is this - 15 ? should it be a fraction of numEntries
    if (outstanding < (numEntries - 15)) {
	copyrr = restartRequests;
	restartRequests = 0;
	ok_kpcomp++;
	lock.release();
	if (copyrr != NULL) {
	    IORestartRequests::NotifyAll(copyrr);
	}
	lock.acquire();
    }
    rc = locked_requestCompleted();
    lock.release();
    return rc;
}

/* virtual */ SysStatus
KernelPagingTransport::tryStartWrite(uval fileToken, uval addr, uval objOffset,
				     uval sz, IORestartRequests *rr)
{
    tassertMsg(pidx_ptr != NULL, "?");
    tassertMsg(*pidx_ptr < NUM_ENTRIES, "?");
    SysStatus rc;

    Request req = {START_WRITE, fileToken, addr, objOffset,
		   sz};

    AutoLock<BLock> al(&lock); // locks now, unlocks on return
    ok_kptry++;
    rc = locked_tryPutRequest((uval*) &req, 0 /* not highPriority */);
 
    if (_FAILURE(rc)) { // let's queue this request so we can call back later
	ok_kptryfail++;
	tassertMsg(_SGENCD(rc) == EBUSY, "?");
	tassertMsg(rr, "woopsn\n");
	rr->enqueue(restartRequests);
	return _SERROR(2860, FR::WOULDBLOCK, EBUSY);
    }
    ok_kptrysucc++;
    return rc;
}
