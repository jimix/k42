/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockNFS.C,v 1.173 2005/07/07 00:55:55 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <fcntl.h>
#include <io/FileLinux.H>
#include <fslib/DirLinuxFSVolatile.H>
#include <sys/KernelInfo.H>
#include "NFSExport.H"
#include "ServerFileBlockNFS.H"
#include "FileSystemNFS.H"
#include <scheduler/Scheduler.H>
#include <io/VAPageServer.H>
#include <meta/MetaVAPageServer.H>

#include <trace/traceFS.h>

#define INSTNAME ServerFileBlockNFS
#include <meta/TplMetaVAPageServer.H>
#include <xobj/TplXVAPageServer.H>
#include <tmpl/TplXVAPageServer.I>

typedef TplXVAPageServer<ServerFileBlockNFS> XVAPageServerSFNFS;
typedef TplMetaVAPageServer<ServerFileBlockNFS> MetaVAPageServerSFNFS;


// Definition of minimum timeout value as 3 seconds
/* static */ SysTime
ServerFileBlockNFS::TIMEOUT_MIN = Scheduler::TicksPerSecond()*3;
// Definition of maximum timeout value as 60 seconds
/* static */ SysTime
ServerFileBlockNFS::TIMEOUT_MAX = Scheduler::TicksPerSecond()*60;

/* virtual */ SysStatus
ServerFileBlockNFS::locked_createFR()
{
    SFTRACE("ServerFileBlockNFS::locked_createFR");
    _ASSERT_HELD(stubDetachLock);

    ObjectHandle oh, myOH;
    SysStatus rc;

    // create an object handle for upcall from kernel FR
    giveAccessByServer(myOH, _KERNEL_PID, MetaVAPageServerSFNFS::typeID());

    uval specialRegion = fsfNFS()->getMMSharedRegionAddr();
    tassert( (specialRegion!=0),
	     err_printf("transfer region not init"));

    char *name;
    uval namelen;
#ifdef HACK_FOR_FR_FILENAMES
    // FIXME: for some reason we get here with nameAtCreation = NULL
    if (nameAtCreation == NULL) {
	setNameAtCreation("????");
    }
    name = nameAtCreation;
    namelen = strlen(nameAtCreation);
#else
    /* FIXME: could we pass NULL through the PPC? Not time to check it now,
     * so lets fake some space */
    char foo;
    name = &foo;
    namelen = 0;
#endif // #ifdef HACK_FOR_FR_FILENAMES

    rc = stubKPT._createFRVA(oh, specialRegion, myOH, fileLength,
			     (uval) getRef(), name, namelen);
    tassert( _SUCCESS(rc), err_printf("woops\n"));

    stubFR = new StubFRHolder();
    stubFR->stub.setOH(oh);
    // err_printf("constructing StubFRHolder %p\n", this);
    return 0;
};

/* virtual */ SysStatus
ServerFileBlockNFS::startWrite(uval virtAddr, uval objOffset, uval len,
			       XHandle xhandle)
{
    SysStatus rc;

    /*
     * do not need to acquire lock here, since access read-only data, and
     * ServerFile guaranteed not to go away while FR has a request outstanding.
     * We cannot acquire lock, since the FileSystem may make a call back to us
     * to complete the request.
     */
    rc = fsfNFS()->startWrite((char *)virtAddr, len, objOffset, getRef());

#ifdef HACK_FOR_FR_FILENAMES
    tassertMsg(_SUCCESS(rc), "write failed on %p return on "
	       "I/O request <%ld %ld %ld>, file is %s\n",
	       fileInfo, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc),
	       nameAtCreation);
#else
    tassertMsg(_SUCCESS(rc), "write failed on %p return on "
	       "I/O request <%ld %ld %ld>\n",
	       fileInfo, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
#endif

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockNFS::completeWrite(uval virtAddr, uval objOffset, uval len,
				  SysStatus rc)
{
    uval finalSize = objOffset + len;

    acquireLock();
    if (finalSize > fileLength) {
	FileSystemNFS::FINF(fileInfo->getToken())->status.st_size =
	    fileLength = finalSize;
    }
    releaseLock();

    stubFR->stub._ioComplete(virtAddr, objOffset, rc);
    return 0;
}

/* virtual */ SysStatus
ServerFileBlockNFS::startFillPage(uval virtAddr, uval objOffset,
				  XHandle xhandle)
{
    SysStatus rc;

    if (KernelInfo::OnSim()) {
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("P");
	}
    }

    // make an async call to start filling the page
    rc = fsfNFS()->startRead((char *)virtAddr, PAGE_SIZE, objOffset, getRef());
#ifdef HACK_FOR_FR_FILENAMES
    tassertMsg(_SUCCESS(rc), "fillPage failed on %p return on "
	       "I/O request <%ld %ld %ld>, file is %s\n",
	       fileInfo, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc),
	       nameAtCreation);
#else
    tassertMsg(_SUCCESS(rc), "fillPage failed on %p return on "
	       "I/O request <%ld %ld %ld>\n",
	       fileInfo, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
#endif
    return 0;
}

/* virtual */ SysStatus
ServerFileBlockNFS::completeFillPage(uval virtAddr, uval objOffset, uval len,
                                     SysStatus rc)
{
#ifdef HACK_FOR_FR_FILENAMES
    tassertWrn(_SUCCESS(rc),
		   "locked_fillPage failed on %p return on "
		   "I/O request <%ld %ld %ld>, file is %s\n",
		   fileInfo, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc),
	       nameAtCreation);
#else
    tassertWrn(_SUCCESS(rc),
		   "locked_fillPage failed on %p return on "
		   "I/O request <%ld %ld %ld>\n",
		   fileInfo, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
#endif
    _IF_FAILURE_RET(rc);

    // zero from the end of the read to the end of the page
    if (PAGE_SIZE - len > 0) {
        memset((void*)(virtAddr + len), 0, PAGE_SIZE - len);
    }

    stubFR->stub._ioComplete(virtAddr, objOffset, rc);

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockNFS::_write(uval virtAddr, uval objOffset,
			   uval len, __XHANDLE xhandle)
{
    // AutoLock<LockType> al(&lock);	// locks now, unlocks on return
    // return locked_write(virtAddr, objOffset, len);
    uval finalSize = objOffset + len;

    SysStatus rc;
    rc = fsfNFS()->writeSynchronous((char *)virtAddr, len, objOffset);
    _IF_FAILURE_RET(rc);

    // update all the cached state
    acquireLock();

    FileSystemNFS::FileInfoNFS *fi;
    fi = FileSystemNFS::FINF(fileInfo->getToken());

    if (finalSize > fileLength) {
	fi->status.st_size = fileLength = finalSize;
    }
    releaseLock();

    return 0;
}

SysStatus
ServerFileBlockNFS::locked_fillPage(uval virtAddr, uval objOffset)
{
    _ASSERT_HELD_PTR(getLockPtr());
    SysStatus rc;
    uval size, s, total;
    //err_printf("request to objOffset %x\n", objOffset);
    total = 0;
    size = PAGE_SIZE;

    if (KernelInfo::OnSim()) {
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("P");
	}
    }
    while (size > 0) {
	s = size;
	// Not sure why there should be any limit, might be a problem in
	// other address space:
	if (s > FileSystemNFS::RPC_BUF_MAX) {
	  s = FileSystemNFS::RPC_BUF_MAX;
	}

	rc = fsfNFS()->readSynchronous((char *) virtAddr, s, objOffset);
	if (_FAILURE(rc)) {
	    err_printf("NFS fillpage failed: %016lx\n",rc);
	}
	_IF_FAILURE_RET(rc);

	s = _SGETUVAL(rc);
	if (s == 0) {
	    // if nothing read, then zero the page
	    memset((void*)virtAddr, 0, size);
	    return 0;
	} else {
	    // got something
	    total += s;
	    size -= s;
	    virtAddr += s;
	    objOffset += s;
	}
    }
//    err_printf("filling %08lx %08lx %016lx rc:%lx\n",
//	       virtAddr, objOffset,*(uval*)virtAddr,rc);

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockNFS::init(FSFile *finfo, ObjectHandle kptoh)
{
    FileSystemNFS::FileInfoNFS *fi;
    SysStatus rc;

    rc = ServerFileBlock<StubFRVA>::init(kptoh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    useType = FileLinux::FIXED_SHARED;

    fileInfo = finfo;
    tassertMsg((finfo->getFSFileType() == FSFile::FSFILE_NFS), "woops");
    fi = FileSystemNFS::FINF(fileInfo->getToken());

    fileLength = fi->status.st_size;

    removalPending = NULL;

    timeout = TIMEOUT_MIN;
    timestamp = Scheduler::SysTimeNow();
    timeoutUpdateStamp = Scheduler::SysTimeNow();

    CObjRootSingleRep::Create(this);
    MetaVAPageServerSFNFS::init();

    // initializing information for file destruction protocol
    entToFree.ref = (ServerFileRef) getRef();

    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::ftruncate(off_t len)
{
    SFTRACE("ServerFileBlockNFS::ftruncate");

    if (!validFileInfo()) return _SERROR(2174, 0, ESTALE);

    SysStatus rc;
    AutoLock<LockType> al(getLockPtr()); // locks now, unlocks on return

    // FIXME dilma: check credentials

    // if file length reduced, tell file system, otherwise, delay until sync
    // FIXME: notice that if we want to support truncating for making
    // the file bigger (Linux implements that), we can't bypass the file system
    if (len < (off_t)fileLength) {
	rc = fileInfo->ftruncate(len);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
    }

    /*
     * Should really set ctime and mtime here, but not talking to FS,
     * so without synchronized clocks not sure what to set it to.
     */

    // set file length in file rep
    rc = locked_setFileLength(len);
    return rc;
}

/* static */ SysStatus
ServerFileBlockNFS::Create(ServerFileRef &fref, FSFile *finfo,
			   ObjectHandle kptoh)
{
    SysStatus rc;
    ServerFileBlockNFS *file = new ServerFileBlockNFS;

    tassertMsg((finfo != NULL), "woops\n");

    tassert((file != NULL),
	     err_printf("failed allocate of ServerFileBlockNFS\n"));

    rc = file->init(finfo, kptoh);

    if (_FAILURE(rc)) {
	// FIXME: shouldn't delete passed in file here
	// file->invalidateToken();
	// delete file;
	fref = 0;
	return rc;
    }

    fref = (ServerFileRef)file->getRef();

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockNFS::locked_revalidate()
{
    _ASSERT_HELD_PTR(getLockPtr());

    if (!validFileInfo()) {
	// FIXME dilma: this wassert is for debugging, should go away
	tassertWrn(0, "An invalidated fileRef is being used\n");
	return _SERROR(1737, 0, ESTALE);
    }

    FileSystemNFS::FileInfoNFS *fi;
    fi = FileSystemNFS::FINF(fileInfo->getToken());

#if 0 // not dealing with ctime changes now
    /* keep old information that may be needed after validation updates them */
    time_t oldctime = fi->ctime;
#endif // not dealing with ctime changes now

    timestamp = Scheduler::SysTimeNow();
    SysStatus rc = fsfNFS()->reValidateToken(fileInfo->getToken());

    if (_SUCCESS(rc)) {
	// update timeout
	SysTime now = Scheduler::SysTimeNow();
	if (now > timeoutUpdateStamp + timeout) {
	    if ((timeout <<= 1) > TIMEOUT_MAX) {
		timeout = TIMEOUT_MAX;
	    }
	    timeoutUpdateStamp = now;
	}

	// check if the file has been modified in the remote side
	if (fi->status.st_mtime > fi->modtime) {
#ifdef DILMA_DEBUG_SDET
	    // FIXME: for now to see why SDET hits this
	    tassertMsg(0, "File has changed in remote server, "
		       "so we reloaded it and made timeout TIMEOUT_MIN\n");
#endif
	    if (stubFR != NULL) {
		rc = stubFR->stub._discardCachedPages();
		tassert(_SUCCESS(rc),
			err_printf("discardCachedPages() returned err\n"));
	    }
	    // file has changed, so let's make timeout for cacher smaller
	    timeout = TIMEOUT_MIN;
	    fi->modtime = fi->status.st_mtime;
	    fi->ctime = fi->status.st_ctime;
	    fileLength = fi->status.st_size;

	}

#if 0   // this needs to be fixed, since its logic is broken due to
	// many changes (it will deadlock, I think).
	// We need to detect changes (links created, removed) on remote side
	// since last checked ... being conservative here
	if (fi->status.st_ctime != oldctime) {
	    void *currDL = NULL, *dirRef;
	    ServerFileRef myRef = (ServerFileRef)getRef();
	    char hasValidEntry, hasInvalidEntry;
	    char foundInvalid = 0;
	    ParentSet *dp = getDirLinuxParents();
	    currDL = dp->getNext(currDL, dirRef);
	    while ( currDL != NULL) {
		rc =  DREF((DirLinuxFSRef)dirRef)->validateFileRef
		    (myRef, hasValidEntry, hasInvalidEntry,
		     (uval)fileInfo->status.st_ino);
		_IF_FAILURE_RET(rc);
		if (!hasValidEntry) {
		    DirLinuxFSRef delRef = dirRef;
		    currDL = dp->getNext(currDL, dirRef);
		    dp->remove(delRef);
		} else {
		    currDL = dp->getNext(currDL, dirRef);
		}
		if (hasInvalidEntry) {
		    foundInvalid = 1;
		}
	    }

	    if (foundInvalid) {
		rc =  _SERROR(1770, NLINK_CHANGED, 0);
	    }
	}
#endif /* #if 0   // this needs to be fixed, ... */
    } else {
	if (_SGENCD(rc) == ESTALE) { // cached token stale
#ifdef DEBUG_ESTALE
	    tassertWrn(0, "detected estale token\n");
#endif // #ifdef DEBUG_ESTALE
	    locked_detachFromDirLinuxFS();
	    rc = _SERROR(1738, 0, ESTALE);
	    tassertMsg(fileInfo != NULL, "how come?\n");
	    fileInfo->destroy();
	    fileInfo = NULL;
	}
    }

    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::unlink(FSFile *dirinfo, char *name, uval namelen,
			   DirLinuxFSRef dirRef)
{
    SFTRACE("ServerFileBlockNFS::unlink");

    SysStatus rc = 0;
    FileSystemNFS::FileInfoNFS *fi;
    fi = FileSystemNFS::FINF(fileInfo->getToken());

    acquireLock();
    // FIXME: should we force revalidation ?
    if (shouldRevalidate() == 1) rc = locked_revalidate();

    if (_FAILURE(rc)) {
	// if file is ESTALE, user should get the error; if there
	// is another type of errorr, e
	// should return error to user, for now it would be useful
	// to see what kind of error we got from the file system
	tassert(0, err_printf("Access to file to be unlinked failed\n"));
	releaseLock();
	return rc;
    } else {
	char found = parent->getParentSet()->remove(dirRef);
	tassertMsg(found,"element to be removed disappeared\n");
	// either we didn't go to the server
	// or we did go and the file is there (but shouldn't we force going to
	// server?)
	if (fi->status.st_nlink == 1) {
	    if (isEmptyExportedXObjectList()) {
		uval nlinkRemaining;
		rc = dirinfo->unlink(name, namelen, fileInfo, &nlinkRemaining);
		if (_FAILURE(rc)) goto return_rc;
		tassertWrn(nlinkRemaining ==0, "links changed\n");
		releaseLock();
		traceDestroy("NFS", "unlink");
		// free FRs, FCM
		destroy();
		return 0;
	    } else {
		// there are clients.
		// rename file; it will be deleted when last file client
		// goes away
		rc = locked_moveFile(dirinfo, name, namelen, dirRef);
		if (_FAILURE(rc)) goto return_rc;
		removalOnLastClose = 1;

		// If only FR as clients, we can
		// tell the FR the file is being removed
		if (locked_isThereExternalClient() == 0) {
		    removed = 1;	// maa debug
		    releaseLock();
		    frInteractionForDeletion();
		    return 0;
		}
	    }
	} else {
	    // nlink > 1
	    // timestamp should reflect that unlink retrieves attributes
	    timestamp = Scheduler::SysTimeNow();
	    rc = dirinfo->unlink(name, namelen, fileInfo);
	    if (_FAILURE(rc)) goto return_rc;
	}
    }

return_rc:
    releaseLock();
    return 0;
}

SysStatus
ServerFileBlockNFS::locked_moveFile(FSFile *dirinfo, char *name, uval namelen,
				    DirLinuxFSRef dirRef)
{
    SFTRACE("ServerFileBlockNFS::locked_moveFile");

    _ASSERT_HELD_PTR(getLockPtr());

    char newname[64]; // should be large enough
    uval newlen;

    SysStatus rc;
    FileToken newdirinfo;
    rc = ((FileSystemNFS::FSFileNFS *)dirinfo)->renameForUnlink(
	name, namelen, fileInfo->getToken(), newdirinfo, newname, newlen);
    _IF_FAILURE_RET(rc);

    removalPending = new RemovalData();
    removalPending->dirinfo = newdirinfo;
    removalPending->dirRef = dirRef;
    memcpy(removalPending->name, newname, newlen);
    removalPending->namelen = newlen;

    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::exportedXObjectListEmpty()
{
    SFTRACE("ServerFileBlockNFS::exportedXObjectListEmpty");

    acquireLock();

    // FIXME: the protocol for object destruction when the file has been
    // invalidated is not clear, i.e., I'm not sure what the current
    // implementation, is doing, and it's certainly not consistent. Let's
    // keep examining scenarios here
    if (doingDestroy != 0) {
#ifdef DILMA_DEBUG_DESTROY
	passertMsg(0, "look at this\n");
#endif
	return 0;
    }

    if (!validFileInfo()) {
	releaseLock();
	traceDestroy("NFS", "exportedXObjectListEmpty first if");
	destroy();
	return 0;
    }

    if (removalOnLastClose == 1) {
	tassertMsg(removalPending != NULL, "problem\n");
	tassertMsg(removalPending->dirRef != NULL, "problem\n");
	SysStatus rc;
	/* we need to lock DirLinuxFS dirRef, otherwise concurrent
	 * operations on the directory will conflict in terms of keeping
	 * the modtime information correct. It's safe to try to acquire
	 * this lock while holding the ServerFile object lock, since
	 * the directory doesn't know about this ServerFile, so it won't
	 * be redirecting invocations to it */
	rc = DREF(removalPending->dirRef)->acquireLockW();
	tassertMsg(_SUCCESS(rc), "shouldn't fail...rc 0x%lx\n", rc);

	/* FIXME dilma: argument fileInfo is not necessary here. We're
	 * passing it now so that we can have it on trace information, but
	 * it's not useful beyond debugging */
	FileSystemRef fs =
	    ((FileSystemNFS::FSFileNFS *)fileInfo)->getFS();
	rc = DREF(fs)->unlink(removalPending->dirinfo,
			      removalPending->name,
			      removalPending->namelen, fileInfo->getToken());
	rc = DREF(removalPending->dirRef)->releaseLockW();
	tassertMsg(_SUCCESS(rc), "shouldn't fail...\n");
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) != ESTALE && _SGENCD(rc) != ENOENT) {
		// let's look at this failure
		tassert(0, err_printf("FileSystem::unlink() failed\n"));
	    }
	}

	delete removalPending;
	removalPending = 0;
	removalOnLastClose = 0;
	removed = 0;			// maa debug
	releaseLock();
	traceDestroy("NFS", "exportedXObjectListEmpty 2nd if");
	destroy();
    } else if (entToFree.timeFreed == 0) {
	// add itself to FileSytem::FreeList
	fileInfo->freeServerFile(&entToFree);
	releaseLock();
    } else {
	releaseLock();
    }

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockNFS::fchown(uid_t uid, gid_t gid)
{
    SFTRACE("ServerFileBlockNFS::fchown");

    if (!validFileInfo()) return _SERROR(2170, 0, ESTALE);

    SysStatus rc = 0;
    acquireLock();

    if (shouldRevalidate() == 1) rc = locked_revalidate();

    if (_SUCCESS(rc)) {
	// FIXME dilma: check credentials
	rc = fileInfo->fchown(uid, gid);
	if (_FAILURE(rc) && _SGENCD(rc) == ESTALE) {
	    fileInfo->destroy();
	    fileInfo = NULL;
	    locked_detachFromDirLinuxFS();
	    /* this object probably still has external objects (e.g. the one
	     * invoking this operation), but in case the client has gone away,
	     * let's do the cleaning up */
	    if (isEmptyExportedXObjectList()) {
		releaseLock();
		traceDestroy("NFS", "fchown");
		destroy();
	    } else {
		releaseLock();
	    }
	    return _SERROR(2443, 0, ESTALE);
	}
    }
    releaseLock();
    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::getStatus(FileLinux::Stat *status)
{
    SFTRACE("ServerFileBlockNFS::getStatus");

    if (!validFileInfo()) return _SERROR(2173, 0, ESTALE);

    AutoLock<LockType> al(getLockPtr()); // locks now, unlocks on return
    SysStatus rc = 0;

    if (shouldRevalidate() == 1) rc = locked_revalidate();

    if (_SUCCESS(rc)) {
	// FIXME dilma: check credentials
	fileInfo->getStatus(status);
	/*
	 * The file system does not know about the real length, the
	 * real length is the size cached by ServerFile if a file is
	 * actively being changed.  Note, the numer of blocks will not
	 * be correct if the size cached is differnt from the file system
	 * size.
	 */
	status->st_size = fileLength;
    }
    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::fchmod(mode_t mode)
{
    SFTRACE("ServerFileBlockNFS::fchmod");

    if (!validFileInfo()) return _SERROR(2171, 0, ESTALE);

    SysStatus rc = 0;
    acquireLock();

    if (shouldRevalidate() == 1) rc = locked_revalidate();

    if (_SUCCESS(rc)) {
	// FIXME dilma: check credentials
	rc = fileInfo->fchmod(mode);
	if (_FAILURE(rc) && _SGENCD(rc) == ESTALE) {
	    fileInfo->destroy();
	    fileInfo = NULL;
	    locked_detachFromDirLinuxFS();
	    /* this object probably still has external objects (e.g. the one
	     * invoking this operation), but in case the client has gone away,
	     * let's do the cleaning up */
	    if (isEmptyExportedXObjectList()) {
		releaseLock();
		traceDestroy("NFS", "fchmod");
		destroy();
	    } else {
		releaseLock();
	    }
	    return _SERROR(1376, 0, ESTALE);
	}
    }
    releaseLock();
    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::utime(const struct utimbuf *utbuf)
{
    SFTRACE("ServerFileBlockNFS::utime");

    SysStatus rc = 0;
    acquireLock();

    if (shouldRevalidate() == 1) rc = locked_revalidate();

    if (_SUCCESS(rc)) {
	// FIXME dilma: check credentials
	rc = fileInfo->utime(utbuf);
	if (_FAILURE(rc) && _SGENCD(rc) == ESTALE) {
	    fileInfo->destroy();
	    fileInfo = NULL;
	    locked_detachFromDirLinuxFS();
	    /* this object probably still has external objects (e.g. the one
	     * invoking this operation), but in case the client has gone away,
	     * let's do the cleaning up */
	    if (isEmptyExportedXObjectList()) {
		releaseLock();
		traceDestroy("NFS", "utime");
		destroy();
	    } else {
		releaseLock();
	    }
	    return _SERROR(2444, 0, ESTALE);
	}
    }
    releaseLock();
    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::link(FSFile *newDirInfo, char *newname, uval newlen,
			 DirLinuxFSRef newDirRef)
{
    SFTRACE("ServerFileBlockNFS::link");

    AutoLock<LockType> al(getLockPtr()); // locks now, unlocks on return
    FileSystemNFS::FileInfoNFS *fi;

    fi = FileSystemNFS::FINF(fileInfo->getToken());

    SysStatus rc = 0;
    if (shouldRevalidate() == 1) rc = locked_revalidate();

    if (_SUCCESS(rc)) {
	// FIXME dilma: check credentials
	rc = fileInfo->link(newDirInfo, newname, newlen,
			    (ServerFileRef)getRef());
	_IF_FAILURE_RET(rc);

	parent->getParentSet()->add(newDirRef);

	// temporary assert for debugging
	tassertMsg(fi->status.st_nlink > 1,"after link st_nlink is %d\n",
		   (int)(fi->status.st_nlink));
    }
    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::open(uval oflag, ProcessID pid, ObjectHandle &oh,
			 uval &ut, TypeID &type)
{
    SFTRACE("ServerFileBlockNFS::open");

    SysStatus rc = 0;

    acquireLock();

     if (!validFileInfo()) {
	 releaseLock();
	 return _SERROR(2172, 0, ESTALE);
     }

    // force revalidation on open
    rc = locked_revalidate();
    if (_FAILURE(rc)) {
	if (_SGENCD(rc) == ESTALE) {
	    // locked_revalidate should have dealt with destroying fileInfo
	    // and detaching from parent directory
	    tassertMsg(fileInfo == NULL, "how come?");
	    tassertMsg(parent->getParentSetNoTassert() == NULL, "???");

	    releaseLock();
	    if (isEmptyExportedXObjectList()) {
		traceDestroy("NFS", "open");
		destroy();
	    }
	    /* rc already indicates an error but, but generating a new
	     * (equivalent) one here may simplify tracking errors */
	    rc = _SERROR(2167, 0, ESTALE);
	} else {
	    releaseLock();
	}
	return rc;
    }

    releaseLock();
    rc = ServerFile::open(oflag, pid, oh, ut, type);

    return rc;
}

/* virtual */ SysStatus
ServerFileBlockNFS::explicitFsync()
{
    if (stubFR != NULL) {
	return stubFR->stub._explicitFsync();
    } else {
	return 0;
    }
}

void
ServerFileBlockNFS::locked_detachFromDirLinuxFS()
{
#ifdef DEBUG_STALE
    tassertWrn(0, "In ServerFileBlockNFS::locked_detachFromDirLinuxFS()\n");
#endif

    _ASSERT_HELD_PTR(getLockPtr());
    ParentSet *dp = parent->getParentSet();
    void *currDL = NULL;
    DirLinuxFSRef dirRef;
    dirRef = NULL; // compiler is complaining about possible uninitialized use
    ServerFileRef myRef = (ServerFileRef)getRef();
    while ( (currDL = dp->next(currDL, dirRef)) != NULL) {
	tassertMsg(dirRef != NULL, "ops\n");
	(void) DREF(dirRef)->detachInvalidFile(myRef);
	TraceOSFSNFSStaleFileDetachment((uval) myRef, (uval) dirRef);
    }
    delete dp;
    parent->clearParentSet();
}
