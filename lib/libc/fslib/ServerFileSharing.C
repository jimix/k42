/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileSharing.C,v 1.10 2004/11/01 19:39:18 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of methods of ServerFile related to
 *                     keeping track of shared/non-shared state
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "ServerFile.H"

void
ServerFile::BlockedQueue::wakeAll()
{
    void *curr = NULL;
    ThreadID tid;
    while ((curr = list.next(curr, tid)) != NULL) {
	Scheduler::Unblock(tid);
    }
}

SysStatus
ServerFile::locked_changeState(EventType event, XHandle xhandle,
			       uval req, uval openForWrite)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_changeState");

    uval ut = uval(~0);
    switch (useType) {
    case FileLinux::LAZY_INIT:
	ut = locked_changeFromUninitialized(event);
	break;
    case FileLinux::NON_SHARED:
	ut = locked_changeFromNonShared(event, xhandle, openForWrite);
	break;
    case FileLinux::SHARED:
	ut = locked_changeFromShared(event);
	break;
    case FileLinux::TRANSITION:
	ut = locked_changeFromTransition(event, xhandle, req);
	break;
    case FileLinux::FIXED_SHARED:
	ut = locked_changeFromFixedShared(event);
	break;
    default:
	passertMsg(0, "unexpected useType %ld\n", useType);
    }

    tassertMsg((ut == FileLinux::LAZY_INIT ||
		ut == FileLinux::NON_SHARED || ut == FileLinux::SHARED ||
		ut == FileLinux::FIXED_SHARED || ut == FileLinux::TRANSITION),
	       "ut is %ld\n", ut);

#ifdef GATHERING_STATS
    if (ut == FileLinux::NON_SHARED) {
	fileInfo->incStat(FSStats::CLIENT_BECAME_NON_SHARED);
    } else if (ut == FileLinux::SHARED) {
	fileInfo->incStat(FSStats::CLIENT_BECAME_SHARED);
    }
#endif //#ifdef GATHERING_STATS

#ifndef LAZY_SHARING_SETUP
    passertMsg(event == DETACH || ut != FileLinux::LAZY_INIT, "?");
#endif // #ifndef LAZY_SHARING_SETUP

    return ut;
}

uval
ServerFile::locked_changeFromUninitialized(EventType event)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_changeFromUninitialized");

    switch (event) {
#ifdef LAZY_SHARING_SETUP
    case GETSTATUS:
	return FileLinux::LAZY_INIT;
    case OPEN:
	return FileLinux::LAZY_INIT;
    case REG_CB:
	return FileLinux::NON_SHARED;
#else
    case GETSTATUS:
    case OPEN:
    case REG_CB:
    case LAZYREOPEN:
	return FileLinux::NON_SHARED;
    case DUP:
	passertMsg(0, "impossible?");
#endif // #ifdef LAZY_SHARING_SETUP
    case ACK_CB:
	// impossible
	passertMsg(0, "shouldn't occur\n");
	break;
    case DETACH:
	return FileLinux::LAZY_INIT;
    default:
	passertMsg(0, "unexpected event %ld\n", (uval) event);
    }
    return 0;
}

uval
ServerFile::locked_changeFromNonShared(EventType event, XHandle xhandle,
				       uval openForWrite)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_changeFromNonShared");

    switch (event) {
    case GETSTATUS:
	return locked_inspectClients(FileLinux::CALLBACK_REQUEST_INFO);
    case OPEN:
#ifndef LAZY_SHARING_SETUP
    case LAZYREOPEN:
    case DUP:
#endif // #ifdef LAZY_SHARING_SETUP
	return locked_inspectClients(FileLinux::CALLBACK_REQUEST_SWITCH,
				     openForWrite);
    case REG_CB:
#ifdef LAZY_SHARING_SETUP
	return locked_inspectClients(FileLinux::CALLBACK_REQUEST_SWITCH);
#else
	tassertMsg(Clnt(xhandle)->useType = FileLinux::NON_SHARED, "?");
	return FileLinux::NON_SHARED;
#endif // #ifdef LAZY_SHARING_SETUP
    case ACK_CB:
	// impossible
	passertMsg(0, "shouldn't occur\n");
	break;
    case DETACH:
	Clnt(xhandle)->useType = FileLinux::INVALID_UT;
	return locked_determineState();
    default:
	passertMsg(0, "unexpected event %ld\n", (uval) event);
    }

    return 0;
}

uval
ServerFile::locked_inspectClients(FileLinux::RequestType req,
				  uval openForWrite)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_inspectClients");

    passertMsg(useType == FileLinux::NON_SHARED, "useType %ld", useType);

    SysStatus rc;
    uval foundNonShared = 0; // for development assertions only, not needed
    uval ut;
    XHandle xh;
    ClientData *cl = NULL;
    ut = FileLinux::NON_SHARED;
    rc = lockIfNotClosingExportedXObjectList();
    tassert(_SUCCESS(rc),
	    err_printf("closing xobject list on open, possible?\n"));
    xh = getHeadExportedXObjectList();
    while (xh != XHANDLE_NONE) {
	// see if client, i.e., if FR, may not have any open
	if (XHandleTrans::GetTypeID(xh) == MetaFileLinuxServer::typeID()
	    && XHandleTrans::GetOwnerProcessID(xh) != _KERNEL_PID) {
	    cl = Clnt(xh);
	    // found a file client
	    if (cl->isSharingOffset == 1) {
		// from dup
		passertMsg((cl->useType == FileLinux::NON_SHARED ||
			    cl->useType == FileLinux::LAZY_INIT ||
			    cl->useType == FileLinux::SHARED ||
			    cl->useType == FileLinux::INVALID_UT),
			   "unexpected cl->useType %ld\n",
			   (uval) cl->useType);
		passertMsg(cl->useType!= FileLinux::SHARED, "wrong");
		if (cl->useType == FileLinux::NON_SHARED) {
#ifdef LAZY_SHARING_SETUP
		    SysStatus rctmp = locked_contactClient(xh, cl, req);
		    if (_SUCCESS(rctmp)) {
			foundNonShared = 1;
			ut = cl->useType = FileLinux::TRANSITION;
		    } else {
			cl->useType = FileLinux::INVALID_UT;
		    }
#else
		    if (cl->useTypeCallBackStub.getOH().valid()) {
			SysStatus rctmp = locked_contactClient(xh, cl, req);
			if (_SUCCESS(rctmp)) {
			    foundNonShared = 1;
			    ut = cl->useType = FileLinux::TRANSITION;
			} else {
			    cl->useType = FileLinux::INVALID_UT;
			}
		    } else {
			// ignore this client for now; when it finally register
			// itself we tell it what the current state is
			cl->useType = FileLinux::INVALID_UT;
		    }
#endif // #ifdef LAZY_SHARING_SETUP
		} else if (cl->useType == FileLinux::SHARED) {
		    // there shouldn't be any other NON_SHARED in the list
		    foundNonShared = 0;
		    ut = FileLinux::SHARED;
		    break;
		} else {
		    tassertMsg(cl->useType == FileLinux::LAZY_INIT ||
			       cl->useType == FileLinux::INVALID_UT,
			       "cl->useType is %ld\n", (uval) cl->useType);
		}
	    } else {
		passertMsg((cl->useType == FileLinux::NON_SHARED
			    || cl->useType == FileLinux::SHARED
			    || cl->useType == FileLinux::LAZY_INIT
			    || cl->useType == FileLinux::INVALID_UT),
			   "unexpected cl->useType %ld\n",
			   (uval) cl->useType);
		passertMsg(cl->useType != FileLinux::SHARED, "wrong");
		if (cl->useType == FileLinux::SHARED) {
		    // there shouldn't be any other NON_SHARED in the list
		    foundNonShared = 0;
		    ut = FileLinux::SHARED;
		    break;
		} else if (cl->useType == FileLinux::NON_SHARED) {
#ifdef LAZY_SHARING_SETUP
		    passertMsg(cl->useTypeCallBackStub.getOH().valid(), "?");
		    if (openForWrite == 1 ||
			(O_ACCMODE & cl->flags) != O_RDONLY) {
			SysStatus rctmp = locked_contactClient(xh, cl, req);
			if (_SUCCESS(rctmp)) {
			    foundNonShared = 1;
			    ut = cl->useType = FileLinux::TRANSITION;
			} else {
			    cl->useType = FileLinux::INVALID_UT;
			}
		    }
#else
		    if (cl->useTypeCallBackStub.getOH().valid()) {
			if (openForWrite == 1 ||
			    (O_ACCMODE & cl->flags) != O_RDONLY) {
			    SysStatus rctmp = locked_contactClient(xh, cl,
								   req);
			    if (_SUCCESS(rctmp)) {
				foundNonShared = 1;
				ut = cl->useType = FileLinux::TRANSITION;
			    } else {
				cl->useType = FileLinux::INVALID_UT;
			    }
			}
		    } else {
			// ignore this client for now; when it finally register
			// itself we tell it what the current state is
			cl->useType = FileLinux::INVALID_UT;
		    }
#endif // #ifdef LAZY_SHARING_SETUP
		}
	    }
	}
	xh = getNextExportedXObjectList(xh);
    }

    unlockExportedXObjectList();

    return ut;
}

SysStatus
ServerFile::locked_contactClient(XHandle xh, ClientData *cl,
				 FileLinux::RequestType req)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_contactClient");

    // Besides the object lock, this is invoked with the XObjectList locked

    SysStatus rc;

#ifdef TRACE_HOT_SWAP

#ifdef HACK_FOR_FR_FILENAMES
    char *fileName = nameAtCreation;
#else
    char *fileName = "UNKNOWN";
#endif // #ifdef HACK_FOR_FR_FILENAMES

    char* strreq[3] = {"INFO", "SWITCH", "INVALID"};
    // gathering information about situations where we hot swap
    TraceOSFSNonSharedToShared((uval) this, fileName,
		   strreq[req]);
#endif // #ifdef TRACE_HOT_SWAP

    // make sure we have a call back object for this client
    tassertMsg(cl->useTypeCallBackStub.getOH().xhandle() != 0, "?");

    rc = cl->useTypeCallBackStub._callBack((uval) req);
    if (_SUCCESS(rc)) {
	waitingForClients.add(xh);
    } else {
	tassertWrn(0, "Failure in useTypeCallBackStub._callBack"
		   " rc = (%ld,%ld,%ld)\n",
		   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
    }

    return rc;
}

uval
ServerFile::locked_changeFromShared(EventType event)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_changeFromShared");

    switch (event) {
    case OPEN:
    case GETSTATUS:
	return FileLinux::SHARED;
    case REG_CB:
#ifndef LAZY_SHARING_SETUP
	passertMsg(0, "niy");
#else
	return FileLinux::SHARED;
#endif // #ifndef LAZY_SHARING_SETUP
#ifndef LAZY_SHARING_SETUP
    case LAZYREOPEN:
    case DUP:
#endif // #ifdef LAZY_SHARING_SETUP
	return FileLinux::SHARED;
    case ACK_CB:
	passertMsg(0, "shouldn't occur\n");
	break;
    case DETACH:
	// am i last?
	if (locked_isThereExternalClient() == 0) {
	    return FileLinux::LAZY_INIT;
	} else {
	    return FileLinux::SHARED;
	}
    default:
	passertMsg(0, "unexpected event %ld\n", (uval) event);
    }
    return 0;
}

uval
ServerFile::locked_changeFromTransition(EventType event, XHandle xhandle,
					uval req)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_changeFromTransition");

    switch (event) {
#ifndef LAZY_SHARING_SETUP
    case LAZYREOPEN:
    case DUP:
#endif // #ifdef LAZY_SHARING_SETUP
    case OPEN: {
	uval ut = locked_findClient();
#ifdef LAZY_SHARING_SETUP
	if (ut == FileLinux::NON_SHARED) {
	    return FileLinux::LAZY_INIT;
	} else {
	    return ut;
	}
#else
	return ut;
#endif // #ifdef LAZY_SHARING_SETUP
    }
    case GETSTATUS:
    case REG_CB:
#ifndef LAZY_SHARING_SETUP
	passertMsg(0, "NIY");
#endif // #ifdef LAZY_SHARING_SETUP
	// if client still around, queue myself on it else wake up everybody
	return locked_findClient();
    case ACK_CB:
	// if I'm the guy people are waiting for, make thing progress
	return locked_ackFromClient(xhandle, req);
    case DETACH:
	return locked_ackFromClient(xhandle, req);
    default:
	passertMsg(0, "unexpected event %ld\n", (uval) event);
    }
    return 0;
}

uval
ServerFile::locked_findClient()
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_findClient");

    passertMsg(waitingForClients.isEmpty() == 0, "??\n");

    SysStatus rc;
    uval ut = FileLinux::TRANSITION;

    rc = lockIfNotClosingExportedXObjectList();
    tassert(_SUCCESS(rc),
	    err_printf("closing xobject list on open, possible?\n"));
    /* FIXME: we want to check which elements of waitingForClients do not
     * belong to the XObjectList anymore; we can do better than the simple
     * approach below */
    void *curr = NULL;
    uval found;
    XHandle xh, cl;
    while ( (curr = waitingForClients.next(curr, cl)) != NULL) {
	found = 0;
	xh = getHeadExportedXObjectList();
	while (xh != XHANDLE_NONE) {
	    // are we waiting for this one ?
	    if (xh == cl) {
		found = 1;
		break;
	    }
	    xh = getNextExportedXObjectList(xh);
	}
	if (found == 0) {
	    // this particular client we were waiting for has gone away
	    waitingForClients.remove(cl);
	    // iterator may become confused after element removal
	    curr = NULL;
	} else {
	    // queueing myself on this guy, let it in the
	    // waitintForClients list
	}
    }

    if (waitingForClients.isEmpty() == 0) {
	unlockExportedXObjectList();
	ut = FileLinux::TRANSITION;
    } else { 	// all clients we're waiting for have gone away
	waitingQueue.wakeAll();
	unlockExportedXObjectList();
	ut = locked_determineState();
    }

    return ut;
}

uval
ServerFile::locked_ackFromClient(XHandle xhandle, uval req)
{
    SFTRACE("locked_ackFromClient");

    //err_printf("In locked_ackFromCLient with xhandle %ld\n", xhandle);
    tassertMsg(xhandle != 0, "invalid xhandle\n");
    if (waitingForClients.find(xhandle) == 1) {
	waitingForClients.remove(xhandle);
	if (req == FileLinux::CALLBACK_REQUEST_INFO) {
	    Clnt(xhandle)->useType = FileLinux::NON_SHARED;
	} else if (req == FileLinux::CALLBACK_REQUEST_SWITCH) {
	    // FIXME: for now assuming only switches to SHARED
	    Clnt(xhandle)->useType = FileLinux::SHARED;
	    fileInfo->incStat(FSStats::CLIENT_SWITCH);
	} else if (req == FileLinux::CALLBACK_INVALID) {
	    /* not from receiving an ack, so we don't care about this client
	     * anymore, since it shouldn't be in the XObjectList */
	    /* FIXME: we could do better (if we knew what we were waiting
	     * for), this is a safe approach for now */
	    Clnt(xhandle)->useType = FileLinux::SHARED;
	} else {
	    tassertMsg(0, "invalid req %ld\n", (uval) req);
	}
	if (waitingForClients.isEmpty()) {
	    useType = locked_determineState();
	    waitingQueue.wakeAll();
	    return useType;
	}
    }
    // FIXME dilma: should it return current useType ?
    return FileLinux::TRANSITION;
}

uval
ServerFile::locked_determineState()
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_determineState");

    SysStatus rc;
    XHandle xh;
    uval ut = FileLinux::LAZY_INIT;

    /* FIXME: this is for determining state and to make sure
     * we are in a consistent state. Once we're convinced things
     * are working, we can optimize this so that it doesn't
     * go through the whole list */
    uval counters[5] = {0, 0, 0, 0, 0};
    rc = lockIfNotClosingExportedXObjectList();
    tassert(_SUCCESS(rc),
	    err_printf("closing xobject list on open, possible?\n"));
    xh = getHeadExportedXObjectList();
    while (xh != XHANDLE_NONE) {
	if (XHandleTrans::GetTypeID(xh) == MetaFileLinuxServer::typeID()
	    && XHandleTrans::GetOwnerProcessID(xh) != _KERNEL_PID) {
	    // found a file client
	    uval tmp = Clnt(xh)->useType;
	    switch (tmp) {
	    case FileLinux::TRANSITION:
		if (waitingForClients.isEmpty()) break;
#ifdef LAZY_SHARING_SETUP
	    case FileLinux::LAZY_INIT:
#endif //#ifdef LAZY_SHARING_SETUP
	    case FileLinux::NON_SHARED:
	    case FileLinux::SHARED:
	    case FileLinux::FIXED_SHARED:
		counters[tmp]++;
		break;
#ifndef LAZY_SHARING_SETUP
	    case FileLinux::INVALID_UT:
		// Ignore
		break;
	    case FileLinux::LAZY_INIT:
		passertMsg(0, "how come?");
#endif // #ifndef LAZY_SHARING_SETUP
	    default:
		tassertMsg(0, "Unexpected useType %ld\n", tmp);
	    }
	}
	xh = getNextExportedXObjectList(xh);
    }
    unlockExportedXObjectList();

    passertMsg(counters[FileLinux::FIXED_SHARED] == 0, "should't be here\n");
    if (counters[FileLinux::TRANSITION] > 0) {
	//err_printf("determineState found TRANSITION\n");
	ut = FileLinux::TRANSITION;
    } else if (counters[FileLinux::SHARED] > 0) {
	tassertMsg(counters[FileLinux::NON_SHARED] == 0, "ops\n");
	ut = FileLinux::SHARED;
    } else if (counters[FileLinux::NON_SHARED] > 0) {
	tassertMsg(counters[FileLinux::SHARED]== 0, "ops\n");
	ut = FileLinux::NON_SHARED;
    } else {
	// returning FileLinux::LAZY_INIT
//#ifndef LAZY_SHARING_SETUP
//	passertMsg(0, "?");
//#endif // #ifndef LAZY_SHARING_SETUP
    }

    if (ut != FileLinux::TRANSITION) {
	if (waitingForClients.isEmpty() == 0) {
	    // waiting for Clients that have disappeared
	    waitingQueue.wakeAll();
	    uval dummy;
	    while (waitingForClients.removeHead(dummy));
	}
    }

#ifdef GATHERING_STATS
    if (ut == FileLinux::NON_SHARED) {
	fileInfo->incStat(FSStats::CLIENT_BECAME_NON_SHARED);
    } else if (ut == FileLinux::SHARED) {
	fileInfo->incStat(FSStats::CLIENT_BECAME_SHARED);
    }
#endif //#ifdef GATHERING_STATS

    tassertMsg(ut != FileLinux::INVALID_UT, "?");

    SFTRACE_USETYPE("locked_determineState", ut);

    return ut;
}

uval
ServerFile::locked_changeFromFixedShared(EventType event)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_changeFromFixedShared");

    switch (event) {
    case GETSTATUS:
    case OPEN:
    case DETACH:
#ifndef LAZY_SHARING_SETUP
    case LAZYREOPEN:
    case DUP:
#endif // #ifndef LAZY_SHARING_SETUP
	return FileLinux::FIXED_SHARED;
    case REG_CB:
    case ACK_CB:
	// impossible
	passertMsg(0, "shouldn't occur, event %ld\n", (uval) event);
	break;
    default:
	passertMsg(0, "unexpected event %ld\n", (uval) event);
    }
    return 0;
}

SysStatus
ServerFile::locked_waitForState()
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_waitForState");

    uval count = 0;
    uval timeout = TIMEOUT_TICKS;

    if (useType == FileLinux::TRANSITION) {
	waitingQueue.add(Scheduler::GetCurThread());

    block:
	count++;
#ifdef DILMA_DEBUG_SWITCH
	err_printf("It's going to block count %ld\n", count);
#endif // #ifdef DILMA_DEBUG_SWITCH
	lock.release();
	Scheduler::BlockWithTimeout(timeout, TimerEvent::relative);
#ifdef DILMA_DEBUG_SWITCH
	err_printf("Out of block with timeout\n");
#endif // #ifdef DILMA_DEBUG_SWITCH
	lock.acquire();
	SFTRACE("locked_waitForState'll invoke determineState");
	useType = locked_determineState();
	if (useType == FileLinux::TRANSITION) {
	    passertMsg(count < 20, "not getting out of TRANSITION state\n");
	    timeout *= 5;
	    goto block;
	} else {
	    waitingQueue.remove(Scheduler::GetCurThread());
	    SFTRACE_USETYPE("locked_waitForState leaving with", useType);
#ifdef DILMA_DEBUG_SWITCH
	    err_printf("Out of waitForState\n");
#endif // #ifdef DILMA_DEBUG_SWITCH
	}
    } else {
	SFTRACE_USETYPE("locked_waitForState", useType);
}
    return 0;
}

#if defined(DEBUG_TRACE_SERVER_FILE) || defined (DEBUG_ONE_FILE)
uval
ServerFile::locked_getNumberClients()
{
    SysStatus rc;
    XHandle xh;
    rc = lockIfNotClosingExportedXObjectList();
    tassert(_SUCCESS(rc),
	    err_printf("closing xobject list on open, possible?\n"));

    uval nb = 0;
    xh = getHeadExportedXObjectList();
    while (xh != XHANDLE_NONE) {
	// see if client, i.e., if FR, may not have any open
	if (XHandleTrans::GetTypeID(xh) == MetaFileLinuxServer::typeID()
	    && XHandleTrans::GetOwnerProcessID(xh) != _KERNEL_PID) {
	    nb++;
	}
	xh = getNextExportedXObjectList(xh);
    }

    unlockExportedXObjectList();

    return nb;
}
#endif // #if defined(DEBUG_TRACE_SERVER_FILE) || defined (DEBUG_ONE_FILE)
