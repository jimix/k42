/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPA.C,v 1.75 2005/01/10 15:29:07 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that obtains data via the thinwire
 * file system.
 * **************************************************************************/

#include "kernIncs.H"
#include <scheduler/Scheduler.H>
#include <cobj/CObjRootSingleRep.H>
#include <cobj/XHandleTrans.H>
#include "mem/FRPA.H"
#include "mem/FCMFile.H"
#include "mem/PageAllocatorKern.H"
#include "mem/FR.H"
#include "mem/RegionFSComm.H"
#include "meta/MetaRegionFSComm.H"
#include "proc/Process.H"
#include <stub/StubPAPageServer.H>
#include <sys/ProcessSet.H>

#if 0
static inline void
resourcePrint(const char *s)
{
    uval pinavail, avail;
    DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(pinavail);
    DREFGOBJ(ThePageAllocatorRef)->getMemoryFree(avail);
    err_printf("Avail Pinned = 0x%lx, Paged = 0x%lx: %s\n",
	       pinavail >> 12, avail >> 12, s);
}
#endif /* #if 0 */

/* virtual */ SysStatusUval
FRPA::startFillPage(uval physAddr, uval objOffset)
{
    if (objOffset >= filelen) {
	return PAGE_NOT_FOUND;
    }
    FetchAndAddSignedVolatile(&outstanding, 1);

    SysStatus rc;
    uval addr;
    
    rc = convertAddressReadFrom(physAddr, addr); /* almost nop for FRPA;
						  * conversion for FRVA */
    tassert(_SUCCESS(rc), err_printf("convertAddress failed\n"));
	
    if (kptref) {
	rc = DREF(kptref)->startFillPage(fileToken, addr, objOffset);
    } else {
	/* we don't have a transport setup, so we will use the async
	 * interfaces */

	/* not having kptref means that the old _Create itfc has been used,
	 * let's check that fileToken has not been given */
	tassertMsg(fileToken == 0, "how come?");

	uval interval = 20000;		// do this in cycles, not time

	rc = stubFile->_startFillPage(addr, objOffset);

	while (!_SUCCESS(rc) && (_SGENCD(rc) == EBUSY) &&
	       interval < 200000000000UL) {
	    Scheduler::DelayUntil(interval, TimerEvent::relative);
	    interval = interval*10;		// back off to avoid livelock
	    rc = stubFile->_startFillPage(addr, objOffset);
	}
	
	passertMsg(_SUCCESS(rc), "timed out trying to send to file system\n");
    }

    passertMsg((rc == 0 || rc == PAGE_NOT_FOUND),
	       "assuming getting back 0 or PAGE_NOT_FOUND from FS\n");
    
    return rc;
}

SysStatus
FRPA::locked_getFCM(FCMRef &r)
{
    _ASSERT_HELD(lock);

    if (beingDestroyed) {
	r = FCMRef(TheBPRef);
	return _SDELETED(2197);
    }

    if (fcmRef != 0) {
	r = fcmRef;
	return 0;
    }

    // okay, have to allocate a new fcm
    SysStatus rc = FCMFile::CreateDefault(fcmRef, (FRRef)getRef(), 1);

    tassertWrn(_SUCCESS(rc), "allocation of fcm failed\n");
    r = fcmRef;
    return rc;
}

/* virtual */ SysStatus
FRPA::startPutPage(uval physAddr, uval objOffset, IORestartRequests *rr)
{
    uval size, addr;
    SysStatus rc;

    if (objOffset >= filelen) {
	// the page may be dirty, but it's not part of the file
	// anymore so we don't do anything
#ifdef DEBUGGING_PAST_ENDFILE
	tassertMsg(0, "FIXME: ignoring page past eof: objOffset=%ld, "
		   "filelen=%ld\n", objOffset, filelen);
#endif /* #ifdef DEBUGGING_PAST_ENDFILE */
	// tell the fcm this IO is complete
	DREF(fcmRef)->ioComplete(objOffset, 0);
	return 0;
    }
    size = PAGE_SIZE;
    if (size > (filelen-objOffset)) {
	size = filelen-objOffset;
    }

    FetchAndAddSignedVolatile(&outstanding, 1);

    // FIXME, pass in blocking info here...
    rc = convertAddressWriteTo(physAddr, addr, rr);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);

    if (kptref) {
	if (rr == NULL) { // not restarting requests
	    rc = DREF(kptref)->startWrite(fileToken, addr, objOffset, size);
	    tassertMsg(_SUCCESS(rc), "woops\n");
	} else {
	    rc = DREF(kptref)->tryStartWrite(fileToken, addr, objOffset,
					     size, rr);
	    if (_FAILURE(rc)) {
		passertMsg((_SCLSCD(rc) == FR::WOULDBLOCK), "woops\n");
		// release the page
		releaseAddress(addr);
		
		// decrement outstanding count
		FetchAndAddSignedVolatile(&outstanding, -1);
	    }
	}
    } else {
	rc = stubFile->_startWrite(addr, objOffset, size);

	uval interval = 20000;
	while (_FAILURE(rc) && (_SGENCD(rc) == EBUSY) &&
	       (interval < 200000000000UL)) {
	    Scheduler::DelayUntil(interval, TimerEvent::relative);
	    rc = stubFile->_startWrite(addr, objOffset, size);
	    interval = interval * 10;
	}
	passertMsg(_SUCCESS(rc), "timed out trying to send to file system\n");
    }
	
    return rc;
}

/*
 * fcm has no users
 * if fcm has no frames and fr has no users, tell the file system
 * (if the file is removed, we are more agressive, telling the
 * file system even if there are still frames)
 */
/* virtual */ SysStatus
FRPA::fcmNotInUse()
{
    // pre check prevents trouble when locked_destroy calls the FCM
    if (beingDestroyed) return _SDELETED(2198);

    lock.acquire();

    if (beingDestroyed) {
	lock.release();
	return _SDELETED(2199);
    }

    if (locked_notInUse() &&
	(removed || fcmRef == 0 || DREF(fcmRef)->isEmpty())) {
	//note this is an async upcall, so the fact that we hold the
	//lock is not a problem
	DREF(kptref)->frIsNotInUse(fileToken);
    }

    lock.release();

    return 0;
}

/*
 * We have to intercept the giveAccess process to synchronize it
 * with destruction, because the FR may destroy itself while the
 * file system is trying to do a new open.
 * If that happens, the file system will make a new FR and start
 * again.
 */
/* virtual */ SysStatus
FRPA::giveAccessSetClientData(ObjectHandle &oh, ProcessID toProcID,
    AccessRights match, AccessRights nomatch, TypeID type)
{
    tassertMsg(!removed, "new access to removed FR\n");
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    SysStatus rc;
    if (beingDestroyed) {
	return _SDELETED(1997);
    }
    ohCount++;

    rc = giveAccessInternal(
	oh, toProcID,
	match, nomatch,
	//(MetaObj::controlAccess|MetaObj::attach) & match, MetaObj::none,
	type, 0);
    return (rc);
}

/*
 * respond to all but the fs object handle going away.
 * because of the fs object handle, we can't use exportedXObjectListEmpty
 *
 * once we make the transition to no object handles, no fcm users, we can
 * destroy the object.  But note that a silly user could have an attach
 * request in flight, say in the FR XObject, which will arrive later. So
 * we check being destroyed on the attach path and reject the call.
 */
/* virtual */ SysStatus
FRPA::handleXObjFree(XHandle xhandle)
{
    // first check guarde against callbacks from the locked_destroy
    // loop that frees object handles - there is no race here so
    // we can check before locking
    if (beingDestroyed) return _SDELETED(2200);
    lock.acquire();
    // second check guards against stale calls after destroy
    if (beingDestroyed) {
	lock.release();
	return _SDELETED(2201);
    }
    if (XHandleTrans::GetClientData(xhandle)) {
	tassert(0,
		err_printf("File System access lost while FR still active\n"));
	// file system object handle has gone away
	// blow everything else away
	// set removed so we don't try any more IO
	//FIXME once more complex async IO is in force, make sure
	//we stop it on this path
	removed = 1;
	locked_destroy();
	lock.release();
	return 0;
    }
    ohCount--;
    if (locked_notInUse()) {
	DREF(kptref)->frIsNotInUse(fileToken);
    }
    // if the fcm is still in use or is still not empty, we wait for
    // it to report a transition to trigger destroy
    lock.release();
    return 0;
}

//we don't need to lock on calls like this. the worst that can happen
//is that we get a deleted object return code if the FCM is already
//gone - in which case it has been synchronized.
SysStatus
FRPA::_fsync()
{
    //load it first since we don't lock here
    FCMRef tmpfcm = fcmRef;
    if (tmpfcm) {
	SysStatus rc;
	// the call MUST be made not holding the lock
	rc = DREF(tmpfcm)->fsync(1 /*force*/);
	return (_FAILURE(rc)&&_ISDELETED(rc))?0:rc;
    }
    return 0;
}

/* virtual */  SysStatus
FRPA::_destroyIfNotInUse(XHandle &xh) __xa(fileSystemAccess)
{
    AutoLock<LockType> al(&lock);	// locks now, unlocks on return

    // not sure this can happen but ...
    if (beingDestroyed) return _SDELETED(2202);

    xh = stubFile->getXHandle();

    // if no users then destroy - checked under lock so story cant change
    // out from under us.  If an attachRegion call is in flight
    // it will be rejected because beingDestroyed is set by locked_destroy
    if (locked_notInUse()) {
	return locked_destroy();
    } else {
	return 1;			// cant destroy right now
    }
}

/* virtual */ SysStatusUval
FRPA::_removed() __xa(fileSystemAccess)
{
    AutoLock<LockType> al(&lock);	// locks now, unlocks on return
    // we don't even turn on removed if beingDestroyed since
    // it should not make any difference - but we need to design
    // destroy continuations carefully to get this right.
    // I'm assuming beingDestroyed does not come on until FCM has no frames.
    if (beingDestroyed) return _SDELETED(2203);

    removed = 1;
    if (locked_notInUse()) {
	//note this is an async upcall, so the fact that we hold the
	//lock is not a problem
	DREF(kptref)->frIsNotInUse(fileToken);
	return 1;
    } else {
	return 0;
    }
}

/*
 * For now, destruction only under the control of ServerFileBlockNFS,
 * eventually the kernel will have to have some way to manage how
 * many FRs there are in the system, and destroy those of a
 * non-responsive file system.
 */
SysStatus
FRPA::locked_destroy()
{
    _ASSERT_HELD(lock);

    // FIXME: take a look at synchronization

    //err_printf("FRPA::locked_destroy() : %lx\n",getRef());

    if (beingDestroyed) return _SDELETED(2204);

    beingDestroyed = 1;

    //err_printf("Before fcmRef->fsync and destroy(), outstanding=%ld\n",
    //           outstanding);

    // first flush back dirty pages and destroy FCM
    // these might result in callbacks, and there may be I/Os pending
    // checks on beingDestroyed avoid deadlock in those cases

    if (fcmRef) {
	if (!removed) DREF(fcmRef)->fsync(1 /*force*/);
	DREF(fcmRef)->destroy();
    }

    tassert(outstanding == 0, err_printf("oops: outstanding is %ld\n",
					 outstanding));

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	if (_FAILURE(rc)) return rc;
    }

    delete stubFile;
    stubFile = 0;

    // schedule the object for deletion
    destroyUnchecked();

    return (0);
}

SysStatus
FRPA::init(ObjectHandle fileOH, uval len, uval token, char *name, uval namelen,
	   KernelPagingTransportRef ref)
{
    stubFile = new StubFileHolderImp<StubPAPageServer>(fileOH);
    kptref = ref;
    filelen = len;
    fileToken = token;
    outstanding = 0;
    ohCount = 0;
    FRCommon::init();

    removed = 0;

#ifdef HACK_FOR_FR_FILENAMES
    (void) initFileName(name, namelen);
#endif //#ifdef HACK_FOR_FR_FILENAMES

    return (0);
}

/* virtual */ SysStatus
FRPA::_ioComplete(__in uval addr, __in uval fileOffset, __in SysStatus rc)
{
    FetchAndAddSignedVolatile(&outstanding, -1);
    if (kptref) {
	SysStatus rrc = DREF(kptref)->ioComplete();
	tassertMsg(_SUCCESS(rrc), "?");
    }
    return DREF(fcmRef)->ioComplete(fileOffset, rc);
}

/* static */ SysStatus
FRPA::Create(ObjectHandle &oh, uval processID,
	     ObjectHandle file, uval len, uval fileToken,
	     char *name, uval namelen,
	     KernelPagingTransportRef kptref)
{
    SysStatus rc;
    FRPARef frref;
    ProcessRef pref;
    FRPA *fr;

    // get process ref for calling file system
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(processID,
						   (BaseProcessRef&)pref);
    tassertMsg(_SUCCESS(rc), "calling process can't go away??\n");

    fr = new FRPA;
    tassertMsg( (fr!=NULL), "alloc should never fail\n");

    rc = fr->init(file, len, fileToken, name, namelen, kptref);
    tassertMsg( _SUCCESS(rc), "woops\n");

    frref = (FRPARef)CObjRootSingleRep::Create(fr);

    // call giveAccessInternal here to provide fileSystemAccess to
    // the file systems OH
    // note that we set the client data to 1 to mark this as the fr
    // oh so we know if the fs goes away
    rc = DREF(frref)->giveAccessInternal(
	oh, processID,
	MetaFR::fileSystemAccess|MetaObj::controlAccess|MetaObj::attach,
	MetaObj::none, 0, 1);

    if (_FAILURE(rc)) return rc;

    return 0;
}

/* static */ void
FRPA::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaFRPA::init();
}

/* virtual */ SysStatus
FRPA::_explicitFsync()
{
    FCMRef fcmRefTmp;
    fcmRefTmp = fcmRef;			// careful because locks not held
    if (fcmRefTmp) {
	return DREF(fcmRefTmp)->fsync(1 /*force*/);
    } else {
	// FIXME dilma: is is possible that fcmRef is 0 here?
	tassert(0, err_printf("investigate if we should invoke getFCM\n"));
	return 0;
    }
}

/*
 * discard all frames - (e.g., used when NFS goes stale)
 * note that we must call the FCM without the lock, since
 * (we always call the FCM without the lock.  The fcm makes upcalls
 * to fcmNotInUse which acquires the lock).
 */
/* virtual */ SysStatus
FRPA::discardCachedPages()
{
    lock.acquire();
    FCMRef f;
    SysStatus rc;
    rc = locked_getFCM(f);
    lock.release();
    if (_FAILURE(rc)) return 0;
    (void)DREF((FCMDefaultRef)f)->discardCachedPages();
    return 0;
}

// FIXME: this 2 variables and the 2 following methods should
// go away soon. They are only on temporary use

/* static */ uval FRPA::howmany_ro[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
/* static */ uval FRPA::howmany_w[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

/* static */ SysStatus
FRPA::ReportFileSize(uval size, uval readOnly)
{
    uval *howmany = (readOnly ? howmany_ro : howmany_w);

    if (size < 0x400) howmany[0]++;
    else if (size < 0x800) howmany[1]++;
    else if (size < 0xc00) howmany[2]++;
    else if (size < 0x1000) howmany[3]++;
    else if (size < 0x2000) howmany[4]++;
    else if (size < 0x10000) howmany[5]++;
    else if (size < 0x20000) howmany[6]++;
    else if (size < 0x010000) howmany[7]++;
    else howmany[8]++;
    return 0;
}

/* static */ SysStatus
FRPA::PrintReportFileSize()
{
    err_printf("File Size statistics for read only file:\n");
    err_printf("\t< 1K: %ld\n", howmany_ro[0]);
    err_printf("\t>= 1K, < 2K: %ld\n", howmany_ro[1]);
    err_printf("\t>= 2K, < 3K: %ld\n", howmany_ro[2]);
    err_printf("\t>= 3K, < 4K: %ld\n", howmany_ro[3]);
    err_printf("\t>= 4K, < 8K: %ld\n", howmany_ro[4]);
    err_printf("\t>= 8K, < 128K: %ld\n", howmany_ro[5]);
    err_printf("\t>= 128K, < 256K: %ld\n", howmany_ro[6]);
    err_printf("\t>= 256K, < 1M: %ld\n", howmany_ro[7]);
    err_printf("\t>= 1M: %ld\n", howmany_ro[8]);

    err_printf("File Size statistics for other files:\n");
    err_printf("\t< 1K: %ld\n", howmany_w[0]);
    err_printf("\t>= 1K, < 2K: %ld\n", howmany_w[1]);
    err_printf("\t>= 2K, < 3K: %ld\n", howmany_w[2]);
    err_printf("\t>= 3K, < 4K: %ld\n", howmany_w[3]);
    err_printf("\t>= 4K, < 8K: %ld\n", howmany_w[4]);
    err_printf("\t>= 8K, < 128K: %ld\n", howmany_w[5]);
    err_printf("\t>= 128K, < 256K: %ld\n", howmany_w[6]);
    err_printf("\t>= 256K, < 1M: %ld\n", howmany_w[7]);
    err_printf("\t>= 1M: %ld\n", howmany_w[8]);
    return 0;
}

/* static */ SysStatus
FRPA:: _Create(__out ObjectHandle &oh, __CALLER_PID processID,
	       __in ObjectHandle file,
	       __in uval len,
	       __in uval filetoken,
	       __inbuf(namelen) char *name,
	       __in uval namelen)
{
    /* Most file systems don't call this method; instead they call
     * a method from KernelPagingTransport that will create the
     * proper FRA.
     * This old version is called by ServerFile objects pertaining
     * to /dev, because for them we don't create a kernel paging
     * transport object. */

    passertMsg(filetoken == 0, "who is calling this without filetoken?\n");
    return Create(oh, processID, file, len, filetoken, name, namelen, NULL);
}

