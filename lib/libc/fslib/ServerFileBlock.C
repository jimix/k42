/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlock.C,v 1.53 2005/01/10 15:27:22 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Common implementation of block file system,
 * maintains relationship with paging object (FR) in memory manager.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "ServerFileBlock.H"
#include <trace/traceFS.h>

/***************** General idea of using stubFR *******************
 * - lock stubDetachLock is used when creating and deleting stubFR
 * - for I/O operations it is safe to use stubFR without locking (it's
 *   not going away when there are outstanding I/O operations)
 * - for operations that can tolerate the disappearance of stubFr, we
 *   copy stubFR into a stubTmp, then test if stubTmp is NULL. The
 *   return code for the stub invocation may be checked to detect that
 *   stubFr is gone.
 */

// returns 1 if detach succeeded, 0 otherwise
template<class STUBFR>
uval
ServerFileBlock<STUBFR>::detachFR()
{
    SFTRACE("detachFR");

    SysStatus rc;
    /* destroy can't be invoked locked, since destroy may block for
     * i/o. stubDetachLock prevents more than one detach
     * from messing with stub at the same time
     * we must aquire it before the regular lock, since
     * while holding it, we do a destroyIfNotInuse which may
     * block in the FR waiting for the file system to complete some IO
     * and completing IO needs the regular lock.
     */
    stubDetachLock.acquire();
    if (stubFR == NULL) {
	stubDetachLock.release();
	return 1;
    } else {
	XHandle xh;
	rc = stubFR->stub._destroyIfNotInUse(xh);
	tassertMsg(_SUCCESS(rc)||_ISDELETED(rc),
		   "not dealing with error here yet\n");
	// non zero returns happen if either destroy can't be done
	// or has been done.  Remember, we don't hold the lock!
	if (_SGETUVAL(rc) != 0) {
	    stubDetachLock.release();
	    return 0;
	} else {
	    /* We'll invoke releaseAccess(), which triggers a method that
	     * if stubFR is available, invokes _removed. Since we're in
	     * the middle of doing detachFR, we don't want to invoke stubFR's
	     * _removed
	     */
	    StubFRHolder *stubTmp = stubFR;
	    stubFR = NULL;
	    if (stubTmp) {
		rc = releaseAccess(xh);
		tassert(_SUCCESS(rc), err_printf("remove access failed\n"));
		delete stubTmp;
	    }
	    stubDetachLock.release();
	    return 1;
	}
    }
}

/* destroy() is only invoked in situations where there are no more clients
 * attached (therefore not even a FR) and no more can be attached (file has
 * been either deleted or has became invalid/stale).
 */
template<class STUBFR>
/* virtual*/ SysStatus
ServerFileBlock<STUBFR>::destroy()
{
    SFTRACE("ServerFileBlock::destroy");

    /* the object will be locked to make check if we have to
     * tell the File System object to take it from the freeSFList
     */
    lock.acquire();
    if (entToFree.timeFreed != 0) {
	SysStatus rc;
	rc = fileInfo->unFreeServerFile(&entToFree);
	tassertMsg(_SUCCESS(rc), "always returns 0\n");
    }
    lock.release();

    /* for debugging, let's check if the assumption that this method
     * is invoked at most once for each object */
    uval oldDestroy = FetchAndAddSignedVolatile(&doingDestroy, 1);
#ifdef DILMA_DEBUG_DESTROY
    tassertMsg(oldDestroy==0, "ServerFileBlock::destroy(): Assumption of "
	       "destroy called no more than once doesn't hold!\n");
#else
    tassertWrn(oldDestroy==0, "ServerFileBlock::destroy(): Assumption of "
	       "destroy called no more than once doesn't hold!\n");
    if (oldDestroy != 0) {
	return 0;
    }
#endif

    // another assumption is that we don't have a FR attached
    passertMsg(stubFR == NULL, "Assumption about no FR attached isn't true\n");

    // checking if there are no clients as we assume
    passertMsg(isEmptyExportedXObjectList(),
	       "Assumption about no clients is false\n");

    if (fileInfo != NULL) {
	fileInfo->destroy();
	fileInfo = NULL;
    }

    {   // remove all ObjRefs to this object
	SysStatus rc = closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc) == 1?0:rc;
    }

    if (parent) {
	delete parent;
    }

#ifdef HACK_FOR_FR_FILENAMES
    if (nameAtCreation != NULL) {
	AllocGlobalPadded::free(nameAtCreation, strlen(nameAtCreation)+1);
    }
#endif //#ifdef HACK_FOR_FR_FILENAMES

    // schedule the object for deletion
    destroyUnchecked();

    return 0;
}

/* This method is invoked by the FR to inform us that the FR is not
 * being used now.  We try to detach it.
 */
template<class STUBFR>
/* virtual */ SysStatus
ServerFileBlock<STUBFR>::frIsNotInUse()
{
    SFTRACE("frIsNotInUse");

    // for invalid (stale) files or deleted files we should detach FR
    if (fileInfo == NULL || removalOnLastClose == 1) {
	// detach the FR
	SysStatus rc = detachFR();
	/* FIXME: need to deal with detachFR failure (read/write is not
	 * properly dealing with stale file) */
	tassertMsg(_SUCCESS(rc), "detachFR failed\n");
    } else {
#ifdef DILMA_HAS_THIS_WORKING
	/* if there are no clients besides an FR, we make it a candidate
	 * for future destruction */
	lock.acquire();
	if (entToFree.timeFreed == 0 && locked_isThereNonFRClient() == 0) {
	    // add itself to FileSytem::FreeList
	    fileInfo->freeServerFile(&entToFree);
	}
	lock.release();
#endif //#ifdef DILMA_HAS_THIS_WORKING
    }

    return 0;
}

template<class STUBFR>
/* virtual */ SysStatus
ServerFileBlock<STUBFR>::_discardCachedPages()
{
    SFTRACE("_discardCachedPages");

    // we can tolerate stubFR failure if it disappears
    StubFRHolder *stubTmp = stubFR;
    if (stubTmp != NULL) {
	return stubTmp->stub._discardCachedPages();
    }
    return 0;
}

// same as ServerFile, but setfilelength in fr
template<class STUBFR>
/* virtual*/ SysStatus
ServerFileBlock<STUBFR>::ftruncate(off_t len)
{
    SysStatus rc;
    AutoLock<LockType> al(getLockPtr()); // locks now, unlocks on return

    SFTRACE("ftruncate");

    // FIXME dilma: check credentials

    // if file length reduced, tell file system, otherwise, delay until sync
    // FIXME: notice that if we want to support truncating for making
    // the file bigger (Linux implements that), we can't bypass the file
    // system
    if (len < (off_t)fileLength) {
	rc = fileInfo->ftruncate(len);
    } else {
	rc = 0;
    }

    // modify cached length and tell FR about new length
    if (_SUCCESS(rc)) {
	rc = locked_setFileLength(len);
    }

    return rc;
}

template<class STUBFR>
uval
ServerFileBlock<STUBFR>::locked_isThereNonFRClient()
{
    _ASSERT_HELD_PTR(getLockPtr());

    SFTRACE("locked_isThereNonFRClient");

    SysStatus rc;
    XHandle xh;
    uval ret = 0;

    rc = lockIfNotClosingExportedXObjectList();
    if (_FAILURE(rc)) {
	/* This scenario is possible when a handleXObjFree message gets
	 * to the object when it's already in the middle of the destruction.
	 * Notice that this is a valid situation, since there may be a delay
	 * between the XObjectList becoming empty (which may trigger ServerFile
	 * destruction) and invocations of handleXObjFree for the released
	 * clients */
	tassertMsg(doingDestroy == 1, "it should be!\n");
	return 0;
    }

    xh = getHeadExportedXObjectList();

#define DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR
#ifdef DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR
    // The ServerFile should have only one FR client
    uval nbFR = 0;
#endif /* DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR */

    while (xh != XHANDLE_NONE) {
	// see if client is non FR (i.e., if it corresponds to a
	// FileLinuxClient or kernel process for lazyGiveAccess)
	if (XHandleTrans::GetTypeID(xh) == MetaFileLinuxServer::typeID()) {
	    ret = 1;
	    break;			// found a non-FR client
	}

#ifdef DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR
	nbFR++;
#endif /* DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR */

	xh = getNextExportedXObjectList(xh);
    }

    unlockExportedXObjectList();

#ifdef DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR
    if (ret == 0) { // only external (FileLinuxFile) or lazyGiveAccess clients
	tassertMsg(nbFR <= 1, "There are multiple FR clients\n");
    }
#endif /* DILMA_TESTING_ASSUMPTION_ONLY_ONE_FR */

    return ret;
}

template<class STUBFR>
/* virtual */ void
ServerFileBlock<STUBFR>::frInteractionForDeletion()
{
    SFTRACE("frInteractionForDeletion");

    uval done = 0;
    SysStatus rc = 0;
    // let's make sure it hear us (triggers frIsNotInUse),
    // otherwise the temporary file we created will
    // be around until the "daemon pager" decides
    // to trigger frIsNotInUse
    do {
	StubFRHolder *stubTmp = stubFR;
	if (stubTmp != NULL) {
	    rc = stubTmp->stub._removed();
	    if (_SUCCESS(rc)) {
		//err_printf("Told stub to go away\n");
		done = _SGETUVAL(rc);
	    } else {
		// stubFR call failed (object probably disappeared). NIY
		tassertMsg(0, "not dealing with this yet\n");
	    }
	} else {
	    done = 1;
	}
#ifdef DILMA_FRINTERACTION
	// FIXME: maybe we should sleep or something for a while,
	// releasing the lock
	tassertMsg(done == 1 || _FAILURE(rc),
		   "_removed didn't do its work. The retry NIY (see FIXME)\n");
#else
	if (done == 0 && _SUCCESS(rc)) {
	    // should retry, for now let's give up
	    tassertWrn(0, "_removed didn't do its work. The retry NIY (see FIXME), ignoring for now\n");
	    break;
	}
#endif // #ifdef DILMA_DEBUG
    } while (stubFR != NULL && _SUCCESS(rc) && done == 0);
}

/* runFreeProtocol() detects if the file is free for destruction.
 * It returns 1 if file can be destroyed, 0 otherwise.
 *
 * It works in fases: (1) check if there are external clients,
 * (2) interacts with the FR (if there is one) to see if the FR
 * can be detached; (3) interacts with DirLinuxFS parents and (4) interacts
 * with MultiLink manager. When a phase fails (e.g., DirLinuxFS
 * tries to detach this file, but then the file has new clients
 * so it doesn't go ahead), it simply gives up (returning 0).
 * If the phase succeeds, it will invoke runFreeProtocol recursively
 * with the next phase. This means that previous phases are
 * rechecked (with lock held), and things have changed (checks for
 * each phase that have succeeded in the past now fail), it
 * will give up.
 */
template<class STUBFR>
/* virtual */ SysStatusUval
ServerFileBlock<STUBFR>::runFreeProtocol(uval nextPhase)
{
    tassertMsg((nextPhase >= 1 && nextPhase <= 4), "invalid nextPhase value\n");

    lock.acquire();
    SFTRACE("runFreeProtocol");

    if (locked_isThereNonFRClient()) {
	// can't free this file now
	lock.release();
	return 0;
    }

    // check if there is a FR client
    StubFRHolder *stubTmp = stubFR;
    if (stubTmp) {
	if (nextPhase >= 2) {
	    // we have tried detaching FR before, if FR is back we should give up
	    lock.release();
	    return 0;
	}
	// try to detach FR
	lock.release();
	uval ret = detachFR();
	if (ret == 1) {
	    // succeeded in detaching FR
	    tassertMsg(stubFR == NULL, "something's wrong\n");
	    return runFreeProtocol(2);
	} else {
	    // couldn't detach FR
	    return 0;
	}
    }

    // interact with DirLinuxFS parents
    ParentSet *pset = parent->getParentSet();
    DirLinuxFSRef p;
    if (pset->next(NULL, p) != NULL) {
	// there are parents
	if (nextPhase >= 3) {
	    /* we have tried detaching parents before, but they're here, so
	     * give up */
	    lock.release();
	    return 0;
	}
	lock.release();
	SysStatusUval rc = tryDetachParents();
	// FIXME: deal with failure here
	tassertMsg(_SUCCESS(rc), "need to deal with this failure\n");
	if (_SGETUVAL(rc) == 1) {
	    // detached successfully
#ifdef DILMA_DEBUG
	    pset = parent->getParentSet();
	    void *curr = pset->next(NULL, p);
	    tassertMsg(curr == NULL, "parent set is not empty?!\n");
#endif
	    return runFreeProtocol(3);
	} else { // detach parents failed
	    return 0;
	}
    }

    if (nextPhase != 4) {
	// everything done but interaction with MultiLinkMgr
	lock.release();
	FileLinux::Stat status;
	SysStatusUval rc;
	rc = fileInfo->getStatus(&status);
	// FIXME: have to deal with possible stale failure
	tassertMsg(_SUCCESS(rc), "wops");
	rc = fileInfo->detachMultiLink(getRef(), status.st_ino);
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) == ENOENT) {
		rc = fileInfo->getStatus(&status);
		// FIXME: have to deal with possible stale failure
		tassertMsg(_SUCCESS(rc), "wops");
		/* after we released the lock, element was taken out from the
		 * list; ok to run protocol from the begining
		 */
		tassertMsg(status.st_nlink == 1, "file with multiple links is "
			   "not in MultiLinkMgr's list?!\n");
		return runFreeProtocol(4);
	    } else {
		tassertMsg(0, "look at failure\n");
		return 0;
	    }
	} else if (_SGETUVAL(rc) == 1) {
	    return runFreeProtocol(4);
	} else {
	    return 0;
	}
    }

    lock.release();

    return 0;
}

template<class STUBFR>
/* virtual */ SysStatus
ServerFileBlock<STUBFR>::handleXObjFree(XHandle xhandle)
{
    XHandleTrans::SetBeingFreed(xhandle, ServerFile::BeingFreed);
    lock.acquire();
    fileLockList.removeFileLock(xhandle);

    SFTRACE_USETYPE("ServerFileBlock::handleXObjFree", Clnt(xhandle)->useType);

    if (XHandleTrans::GetOwnerProcessID(xhandle) == _KERNEL_PID) {
	// nothing special to do if a kernel client (FR or client for
	// lazy reopen scheme) is going away
	lock.release();
	return 0;
    }

    useType = locked_changeState(DETACH, xhandle);

    SFDEBUG_ARGS("ServerFileBlock::handleXObjFree",
		 "has %ld clients, changed useType to %ld\n",
		 locked_getNumberClients(), useType);

    if (locked_isThereNonFRClient() == 0) {
	fileInfo->incStat(FSStats::CLOSE_SIZE, fileLength);
	if (removalOnLastClose == 1) {
	    lock.release();
	    frInteractionForDeletion();
	} else {
	    readOnly = 1;
	    lock.release();
	}
    } else {
	lock.release();
    }

    return 0;
}

template<class STUBFR>
/* virtual */ SysStatus
ServerFileBlock<STUBFR>::_getSharedBuf(__out ObjectHandle &oh,
				       __out uval &offset,
				       __inout uval &length,
				       __XHANDLE xh, __CALLER_PID pid)
{
    SysStatus rc = 0;
    uval addr;
    uval readLength = 0;
    ShMemBufRef smb;
    ClientData *cd = Clnt(xh);

    if (!cd) {
	return _SERROR(2595, 0, EINVAL);
    }

    AutoLock<LockType> al(&cd->lock); // locks now, unlocks on return

    stubDetachLock.acquire();
    if (stubFR != NULL) {
	stubDetachLock.release();
	/* FIXME: return an approriate error (instead of EINVAL and
	 * change client to understand it */
	return _SERROR(2243, 0, ENOENT);
    }
    stubDetachLock.release();

    if (!cd->smb) {
	rc = ShMemBuf::Fetch(pid, oh, smb, PAGE_SIZE);
	_IF_FAILURE_RET(rc);

	tassertMsg(cd->smb== NULL || smb == cd->smb,
		   "Got a different ShMemBuf object\n");
	cd->smb = smb;
    }

    if (!cd->smbAddr) {
	rc = DREF(smb)->shareAlloc(pid, offset, addr, length);
	_IF_FAILURE_RET(rc);
	cd->smbLen = length = _SRETUVAL(rc);
	tassertMsg(length >= fileLength, "File is too big\n");
	cd->smbAddr= addr - offset;
	cd->smbOffset = offset;
	tassertMsg(length >= fileLength, "File is too big\n");
	readLength = fileLength;
	if (readLength) {
	    rc = _read((char*)(cd->smbAddr + offset), readLength, 0, xh);
	    tassertMsg(_SUCCESS(rc), "Don't expect failure here\n");
	    if (_FAILURE(rc)) {
		DREF(smb)->releaseAccess(oh.xhandle());
		cd->smbAddr = cd->smbOffset = cd->smbLen = 0;
	    }
	}
    }
    return rc;

}

template<class STUBFR>
/* virtual */ SysStatusUval
ServerFileBlock<STUBFR>::_syncSharedBuf(__in uval newLength,
					__in uval start,
					__in uval modLen,
					__in uval release,
					__XHANDLE xh,
					__CALLER_PID pid)
{
    SysStatus rc = 0;
    ClientData *cd = Clnt(xh);
    ShMemBufRef smb;

    if (!cd || !cd->smb || !cd->smbAddr) {
	return _SERROR(2596, 0, EINVAL);
    }
//    err_printf("sync: %lx %lx %lx %lx\n",offset, newLength, start, modLen);
    AutoLock<LockType> al(&cd->lock); // locks now, unlocks on return
    tassertMsg(newLength <= cd->smbLen, "File is too big\n");
    tassertMsg(start+modLen <= newLength, "Modified range out of file\n");
    smb = cd->smb;
    if (newLength && modLen) {
	rc = _write((const char*)(cd->smbAddr+cd->smbOffset+start),
		    modLen, start, xh);
    }

    if (release) {
	DREF(smb)->unShare(cd->smbOffset, pid);
	DREF(smb)->_unShare(cd->smbOffset, XHANDLE_NONE);
	cd->smbAddr = cd->smbOffset = cd->smbLen = 0;
    }
    tassertMsg(_SUCCESS(rc) && _SGETUVAL(rc)==modLen,
	       "Need to consider failure cases here\n");

    return rc;
}

#include <stub/StubFRPA.H>
#include <stub/StubFRVA.H>
template class ServerFileBlock<StubFRPA>;
template class ServerFileBlock<StubFRVA>;
