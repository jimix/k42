/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFile.C,v 1.165 2005/09/06 20:40:12 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic common implementation of a file in server
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/stat.h>
#include <utime.h>
#include "ServerFile.H"
#include "DirLinuxFS.H"
#include <meta/MetaObj.H>
#include <cobj/Obj.H>

#include <time.h>

#include <scheduler/Scheduler.H>
#include <sys/ProcessSet.H>
#include <sys/KernelInfo.H>
#include <trace/traceFS.h>

#include "FSCreds.H"

#include <sys/ProcessSet.H>

// needed for definition of FileLinuxFile::LazyReOpenData
#include <io/FileLinuxFile.H>

#ifdef DEBUG_ONE_FILE
///* static */  char ServerFile::nameDebugOneFile[255] = "/tmp/sh-thd-";
/* static */  char ServerFile::nameDebugOneFile[255] = "PUT_NAME_HERE";
#endif // #ifdef DEBUG_ONE_FILE

void
ServerFile::ClientData::releaseSharingOffset()
{
    tassertMsg(isSharingOffset, "?");
    ClientData::SharedOffsetData *sharedRef = sharedOffRef;
    sharedRef->lock.acquire();
    sharedRef->usageCount--;
    if (sharedRef->usageCount == 0) {
	// there isn't anyone else able to use this ref
	delete sharedRef;
    } else {
	sharedRef->lock.release();
    }
}

/* virtual */
ServerFile::ClientData::~ClientData()
{
}

/* virtual */ SysStatus
ServerFile::init()
{
#ifdef HACK_FOR_FR_FILENAMES
    nameAtCreation = NULL;
#endif //#ifdef HACK_FOR_FR_FILENAMES

    lock.init();

    SFTRACE("init");

    parent = new Parent();

    readOnly = 1;

#if 0
    if (KernelInfo::ControlFlagIsSet(KernelInfo::NON_SHARING_FILE_OPT)) {
	// optimization for non-shared files is on
        useType = FileLinux::LAZY_INIT;
    } else {
	useType = FileLinux::FIXED_SHARED;
    }
#else
    useType = FileLinux::FIXED_SHARED;
#endif

    /* must do this in derived classes, since they create root
     *           entToFree.ref = getRef();
     */
    // indicating that object is not in the FileSystem::freeSF list
    entToFree.timeFreed = 0;

    doingDestroy = 0;
    removalOnLastClose = 0;
    removed = 0;			// maa debug

    fileLockList.init();

    return 0;
}

/* virtual */ SysStatus
ServerFile::giveAccessSetClientData(ObjectHandle &oh, ProcessID toProcID,
				    AccessRights match, AccessRights nomatch,
				    TypeID type)
{
    tassertMsg(doingDestroy == 0, "?");

    SysStatus retvalue;
    ClientData *clientData = new ClientData();
    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
				  type, (uval)clientData);
    return (retvalue);
}

/*static*/ void
ServerFile::BeingFreed(XHandle xhandle)
{
    ClientData *clientData;
    clientData = (ClientData*)(XHandleTrans::GetClientData(xhandle));
    if (clientData->isSharingOffset) {
	// can't be an FR client
	tassertMsg(XHandleTrans::GetTypeID(xhandle) == MetaFileLinuxServer::typeID(),
		   "how come?");
	clientData->releaseSharingOffset();
    }
    tassertMsg(clientData->hasFileLock == 0,
	       "Client still has locks %ld\n", clientData->hasFileLock);

    delete clientData;
}

/* virtual */ SysStatus
ServerFile::getStatus(FileLinux::Stat *status, uval isPathBasedRequest)
{
    lock.acquire();
    tassertMsg(doingDestroy == 0, "?");

    SFTRACE("getStatus");

    SysStatus rc;

#ifdef PERMISSION_CREDENTIALS_WORKING
    rc = permission(MAY_READ);
    if (_FAILURE(rc)) goto return_rc;
#endif // #ifdef PERMISSION_CREDENTIALS_WORKING

    if (isPathBasedRequest == 1) {
	/* invocation through pathname (instead of by specific client, i.e.,
	 * file descriptor) */
	if (useType == FileLinux::NON_SHARED && readOnly == 0) {
	    useType = locked_changeState(GETSTATUS);
	    (void) locked_waitForState();
	    tassertMsg(waitingForClients.isEmpty(), "?");

	    SFDEBUG_ARGS("out of getStatus", " useType %ld\n", useType);
	}
    }

    // FIXME dilma: check credentials
    rc = fileInfo->getStatus(status);
    if (_FAILURE(rc)) goto return_rc;

    /*
     * The file system does not know about the real length, the
     * real length is the size cached by ServerFile if a file is
     * actively being changed.  Note, the numer of blocks will not
     * be correct if the size cached is differnt from the file system
     * size.
     */
    status->st_size = fileLength;

 return_rc:
    lock.release();
    return 0;
}

/* virtual */ SysStatus
ServerFile::fchown(uid_t uid, gid_t gid)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("fchown");

    tassertMsg((fileInfo != NULL), "woops\n");
    tassertMsg(doingDestroy == 0, "?");

    if (fileInfo->isReadOnly()) {
	return _SERROR(2397, 0, EROFS);
    }

    // FIXME: we need to check permissions, something like Linux's
    //        inode_change_ok (easier to do that if we use getattr, setattr
    //        stuff)

    return fileInfo->fchown(uid, gid);
}

/* virtual */ SysStatus
ServerFile::fchmod(mode_t mode)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("fchmod");

    tassertMsg((fileInfo != NULL), "woops\n");
    tassertMsg(doingDestroy == 0, "?");

    if (fileInfo->isReadOnly()) {
	return _SERROR(2396, 0, EROFS);
    }

    // FIXME: we need to check permissions, something like Linux's
    //        inode_change_ok (easier to do that if we use getattr, setattr
    //        stuff)

    return fileInfo->fchmod(mode);
}

/* virtual */ SysStatus
ServerFile::utime(const struct utimbuf *utbuf)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("utime");

    tassertMsg((fileInfo != NULL), "woops\n");
    tassertMsg(doingDestroy == 0, "?");

    if (fileInfo->isReadOnly()) {
	return _SERROR(2395, 0, EROFS);
    }

    // FIXME: we need to check permissions, something like Linux's
    //        inode_change_ok (easier to do that if we use getattr, setattr
    //        stuff)

    return  fileInfo->utime(utbuf);
}

/* virtual */ SysStatus
ServerFile::ftruncate(off_t length)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("ftruncate");

    SysStatus rc;
    tassertMsg(doingDestroy == 0, "?");

#ifdef PERMISSION_CREDENTIALS_WORKING
    rc = permission(MAY_WRITE);
    _IF_FAILURE_RET(rc);
#endif // #ifdef PERMISSION_CREDENTIALS_WORKING

    // if file length reduced, tell file system, otherwise, delay until sync
    // FIXME: notice that if we want to support truncating for making
    // the file bigger (Linux implements that), we can't bypass the file system
    if (length < (off_t)fileLength) {
	// change information at file system
	tassertMsg((fileInfo != NULL), "woops\n");
	rc = fileInfo->ftruncate(length);
    } else {
	rc = 0;
    }

    // modify cached length
    if (_SUCCESS(rc)) {
	tassertWrn(uval(length)<uval(fileLength),
		   "shortened truncate: %lx %lx\n", length, fileLength);
	locked_setFileLength(length);
    }

    return rc;
}

/* virtual */ SysStatus
ServerFile::link(FSFile *newDirInfo, char *newname, uval newlen,
		 DirLinuxFSRef newDirRef)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("link");

    tassertMsg(doingDestroy == 0, "?");

    FileLinux::Stat status;
    SysStatus rc;

    // FIXME dilma: check credentials

    tassertMsg((fileInfo != NULL), "woops\n");
    rc = fileInfo->link(newDirInfo, newname, newlen, (ServerFileRef)getRef());
    _IF_FAILURE_RET(rc);
    parent->getParentSet()->add(newDirRef);

    // temporary assert for debugging
    fileInfo->getStatus(&status);
    tassert(status.st_nlink > 1, err_printf("after link st_nlink is %lu\n",
					    (uval)(status.st_nlink)));
    return rc;
}

/* virtual */ SysStatus
ServerFile::unlink(FSFile *dirinfo, char *name, uval namelen,
		   DirLinuxFSRef dirRef)
{
    SysStatus rc;

    SFTRACE("unlink");

    uval nlinkRemain;
    char found;

#ifdef PERMISSION_CREDENTIALS_WORKING
    rc = permission(MAY_WRITE);
    _IF_FAILURE_RET(rc);
#endif // #ifdef PERMISSION_CREDENTIALS_WORKING

    lock.acquire();
    tassertMsg((fileInfo != NULL), "woops\n");
    tassertMsg(doingDestroy == 0, "?");

    rc = dirinfo->unlink(name, namelen, fileInfo, &nlinkRemain);
    if (_FAILURE(rc)) goto return_rc;

    found = parent->getParentSet()->remove(dirRef);
    tassert(found, err_printf("element to be removed disappeared\n"));

    if (nlinkRemain == 0) {
	rc = locked_returnUnlocked_deletion();
	tassertMsg(_SUCCESS(rc), "?");
	return rc;
    }

return_rc:
    lock.release();
    return rc;
}

/* virtual */ SysStatus
ServerFile::renameSetParents(DirLinuxFSRef oldDirRef, DirLinuxFSRef newDirRef)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("renameSetParents");

    tassertMsg((oldDirRef != NULL && newDirRef != NULL), "wops\n");

    char found = parent->getParentSet()->remove(oldDirRef);
    tassert(found, err_printf("DirLinuxFSRef to be removed disappeared\n"));
    parent->getParentSet()->add(newDirRef);

    return 0;
}

/* virtual */ SysStatusUval
ServerFile::getDents(struct direntk42 *buf, uval len)
{
    tassert(0, err_printf("should be called in ServerFileDir\n"));
    return 0;
}

// ***********************************************
// functions to support server FileLinux interface
// ***********************************************

/* virtual */ SysStatus
ServerFile::_flush()
{
    return 0;
}

/* virtual */ SysStatusUval
ServerFile::_read(char *buf, uval len, uval offset, __XHANDLE xhandle)
{
    (void) buf, (void) len, (void) xhandle;
    return _SERROR(1393, 0, EINVAL);
}

/* virtual */ SysStatusUval
ServerFile::_write(const char *buf, uval len, uval offset, __XHANDLE xhandle)
{
    (void) buf, (void) len, (void) xhandle;
    return _SERROR(1394, 0, EINVAL);
}

/* virtual */ SysStatusUval
ServerFile::_getDents(char *buf, uval len, __XHANDLE xhandle, __CALLER_PID pid)
{
    (void) buf, (void) len, (void) xhandle;
    return _SERROR(1431, 0, EINVAL);
}

/* virtual */ SysStatusUval
ServerFile::_setFilePosition(sval position, uval at, __XHANDLE xhandle)
{
    SysStatusUval rc;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("_setFilePosition");

    ClientData *cl = Clnt(xhandle);
    uval *offAddr;
    if (cl->isSharingOffset) {
	offAddr = &(cl->sharedOffRef->filePosition);
    } else {
	offAddr = &(cl->filePosition);
    }

    switch (at) {
    case FileLinux::APPEND:
	*offAddr = fileLength + position;
	break;
    case FileLinux::RELATIVE:
	*offAddr += position;
	break;
    case FileLinux::ABSOLUTE:
	*offAddr = position;
	break;
    default:
	return _SERROR(1390, 0, EINVAL);
    }

    if (*offAddr < 0) {
	return _SERROR(2302, 0, EINVAL);
    }
    if (fileLength < *offAddr) {
	rc = locked_setFileLength(*offAddr);
	tassertMsg(_SUCCESS(rc), "setting file length failed\n");
    }
    return _SRETUVAL(*offAddr);
}

/* virtual */ SysStatus
ServerFile::locked_setFileLength(uval len)
{
    fileLength = len;
    return 0;
}

/* virtual */ SysStatus
ServerFile::_getStatus(struct stat &status, uval isPathBasedRequest)
{
    return getStatus(FileLinux::Stat::FromStruc(&status));
}

/* virtual */ SysStatus
ServerFile::_getFROH(__out ObjectHandle &oh, __CALLER_PID pid)
{
    return getFROH(oh, pid);
}

/* virtual */ SysStatus
ServerFile::open(uval oflag, ProcessID pid, ObjectHandle &oh,
		 uval &ut, TypeID &type)
{

    XHandle xh;
    SysStatus rc;
    FileLinux::Stat stat;

    // FIXME dilma: why are we doing this getStatus before locking?
    tassertMsg((fileInfo != NULL), "woops\n");
    rc = fileInfo->getStatus(&stat);
    if (_FAILURE(rc)) { return rc; }

    lock.acquire();

    SFTRACE("open");

    useType = locked_changeState(OPEN, 0, FileLinux::CALLBACK_INVALID,
				 (O_ACCMODE & oflag) != O_RDONLY);
    (void) locked_waitForState();
    tassertMsg(waitingForClients.isEmpty(), "?");

    /* The statement below improves performance in the scenario of
     * sequencial sharing involving many readers and one writer
     * (a bunch of clients open the file read-only,
     * but only one of them is actually manipulating
     * the file at a given moment):
     * if a writer comes, since only the client
     * actually performing operations on the file has it as NON_SHARED,
     * only one client needs (the one that forced the state into NON_SHARED)
     * needs to be contacted.
     */
#ifdef LAZY_SHARING_SETUP
    if (useType == FileLinux::NON_SHARED) {
	ut = FileLinux::LAZY_INIT;
    } else {
	ut = useType;
    }
#else
    if (useType == FileLinux::LAZY_INIT) {
	useType =  FileLinux::NON_SHARED;
    }
    ut = useType;
#endif // #ifdef LAZY_SHARING_SETUP

    if (readOnly == 1 && (O_ACCMODE & oflag) != O_RDONLY) {
	readOnly = 0;
    }

    // gathering data
    fileInfo->incStat(FSStats::OPEN);
    fileInfo->incStatCond(readOnly, FSStats::OPEN_RDONLY);

    // FIXME get rights correct
    rc = giveAccessByServer(oh, pid, (AccessRights)-1, MetaObj::none);
    lock.release();
    if (_FAILURE(rc)) { return rc; }

    // now get xhandle and initialize the fields not filled up by ClientData constructor
    xh = oh._xhandle;
    Clnt(xh)->flags = oflag;

    // if truncate mode, truncate this file
    if (oflag & O_TRUNC) { ftruncate(0); }

    // at this point, nothing can fail, so no need to free anything
    type = FileLinux_FILE;

    fileInfo->incStat(FSStats::CLIENT_CREATED);
#ifdef GATHERING_STATS
    if (ut == FileLinux::LAZY_INIT) {
#ifdef LAZY_SHARING_SETUP
	fileInfo->incStat(FSStats::CLIENT_CREATED_LAZY_INIT);
#else
	passertMsg(0, "?");
#endif // #ifdef LAZY_SHARING_SETUP
    } else if (ut == FileLinux::NON_SHARED) {
	fileInfo->incStat(FSStats::CLIENT_CREATED_NON_SHARED);
    } else if (ut == FileLinux::SHARED) {
	fileInfo->incStat(FSStats::CLIENT_CREATED_SHARED);
    } else if (ut == FileLinux::FIXED_SHARED) {
	fileInfo->incStat(FSStats::CLIENT_CREATED_FIXED_SHARED);
    } else {
	passertMsg(0, "ut is %ld\n", ut);
    }
#endif // #ifdef GATHERING_STATS
    fileInfo->incStat(FSStats::OPEN_SIZE, fileLength);

    Clnt(xh)->useType = (FileLinux::UseType) ut;

#ifdef LAZY_SHARING_SETUP
    passertMsg(ut != FileLinux::NON_SHARED, "shouldn't\n");
#else
    passertMsg(ut != FileLinux::LAZY_INIT, "?");
#endif // #ifdef LAZY_SHARING_SETUP

    return 0;
}

/* virtual */ SysStatus
ServerFile::_getLengthOffset(uval &flength, uval &offset, uval isWrite,
			     uval opLen, XHandle xhandle)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("_getLengthOffset");

#ifdef GATHERING_STATS
    if (isWrite) {
	if (useType == FileLinux::NON_SHARED) {
	    fileInfo->incStat(FSStats::MAX_WRITE_SIZE_NON_SHARED, opLen);
	} else {
	    fileInfo->incStat(FSStats::MAX_WRITE_SIZE_SHARED, opLen);
	}
    }
    fileInfo->incStat(FSStats::LENGTH_OFFSET);
#endif // #ifdef GATHERING_STATS

#if 0
    TraceOSFSGetLengthOffset((uval) getRef(), opLen);
#endif

    flength = fileLength;

    uval *offAddr;
    if (Clnt(xhandle)->isSharingOffset) {
	offAddr = &(Clnt(xhandle)->sharedOffRef->filePosition);
    } else {
	offAddr = &(Clnt(xhandle)->filePosition);
    }

    offset = *offAddr;

    if (isWrite) {
	if (Clnt(xhandle)->flags & O_APPEND) {
	    *offAddr = fileLength + opLen;
	} else {
	    *offAddr = offset + opLen;
	}
    } else {
	*offAddr = offset + opLen;
	if (*offAddr > fileLength) {
	    *offAddr = fileLength;
	}
    }

    return 0;
}

/* virtual */ SysStatus
ServerFile::_setLengthOffset(uval flength, uval offset, XHandle xhandle)
{
    SFTRACE("_setLengthOffset");

    SysStatus rc;
    // propagate flength
    rc = ftruncate((off_t) flength);
    tassertMsg(_SUCCESS(rc), "ftruncated failed\n");
    _IF_FAILURE_RET(rc);

    // set offset
    ClientData *clientData = Clnt(xhandle);
    if (clientData->isSharingOffset == 1) {
	tassertMsg(clientData->sharedOffRef != NULL, "wrong\n");
	clientData->sharedOffRef->filePosition = offset;
    } else {
	clientData->filePosition = offset;
    }
    return 0;
}

/* virtual */ SysStatus
ServerFile::_dup(ProcessID pid, uval originalUseType, uval &newUseType,
		 uval flength, uval offset,
		 ObjectHandle &oh, __XHANDLE origXhandle)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("_dup");

    ClientData *dupFromXh = Clnt(origXhandle);

    SFDEBUG_ARGS("In _dup",
	       "with originalUseType %ld and useType %ld, nb clients %ld\n",
	       originalUseType, useType, locked_getNumberClients());

    tassertMsg(!(originalUseType == FileLinux::SHARED
		 && useType == FileLinux::NON_SHARED),
	       "dupFromXh->useType %ld\n", dupFromXh->useType);

#ifdef LAZY_SHARING_SETUP
    passertMsg((originalUseType == FileLinux::SHARED ||
		originalUseType == FileLinux::FIXED_SHARED ||
		originalUseType == FileLinux::NON_SHARED ||
		originalUseType == FileLinux::LAZY_INIT),
	       "unexpected originalUseType %ld\n", originalUseType);
#else
    passertMsg((originalUseType == FileLinux::SHARED ||
		originalUseType == FileLinux::FIXED_SHARED ||
		originalUseType == FileLinux::NON_SHARED),
	       "unexpected originalUseType %ld\n", originalUseType);
#endif // #ifdef LAZY_SHARING_SETUP

#ifdef LAZY_SHARING_SETUP
    if ((uval) dupFromXh->useType != originalUseType) {
	if (dupFromXh->useType == FileLinux::TRANSITION) {
	    // keep originalUseType the same, make new LAZY_INIT
	    newUseType = FileLinux::LAZY_INIT;
	} else {
	    passertMsg(0, "dupFromXh %ld, orig %ld\n",
		       (uval) dupFromXh->useType, originalUseType);
	}
    } else {
	if (originalUseType == FileLinux::SHARED
	    || originalUseType == FileLinux::FIXED_SHARED) {
	    newUseType = originalUseType;
	} else if (originalUseType == FileLinux::LAZY_INIT) {
	    passertMsg(!dupFromXh->useTypeCallBackStub.getOH().valid(), "?\n");
	    /* even if ServerFile is now SHARED, it's not a good approach to
	     * make the client to become SHARED now, since when it actually
	     * ends up starting its work the ServerFile may have become
	     * non-shared */
	    newUseType = originalUseType;
	} else {
	    passertMsg(originalUseType == FileLinux::NON_SHARED, "ops\n");
	    if (dupFromXh->useTypeCallBackStub.getOH().valid()) {
		// original keeps as NON_SHARED
		newUseType = FileLinux::LAZY_INIT;
	    } else {
		originalUseType =
		    dupFromXh->useType = FileLinux::LAZY_INIT;
		newUseType = FileLinux::LAZY_INIT;
	    }
	}
    }
#else
    useType = locked_changeState(DUP, 0, FileLinux::CALLBACK_INVALID, 1);
    // It's safe to wait for state to change while blocked because we know
    // that a client doesn't invoke dup while locked ... So in the case we
    // waiting an ack for a switch request from the same client requesting
    // the dup, we don't have a deadlock ...
    (void) locked_waitForState();
    tassertMsg(waitingForClients.isEmpty(), "?");
    passertMsg(useType != FileLinux::LAZY_INIT, "?");
    newUseType = dupFromXh->useType = useType;

#endif // #ifdef LAZY_SHARING_SETUP

    uval currOffset = 0;
    if (dupFromXh->useType == FileLinux::NON_SHARED) {
	if (offset != uval(~0)) {
	    currOffset = offset;
	}
    } else {
	if (dupFromXh->isSharingOffset == 1) {
	    currOffset = dupFromXh->sharedOffRef->filePosition;
	} else {
	    currOffset = dupFromXh->filePosition;
	}
    }

    XHandle xh;
    SysStatus rc;

    // FIXME get rights correct
    rc = giveAccessByServer(oh, pid, (AccessRights)-1, MetaObj::none);
    if (_FAILURE(rc)) {
	return rc;
    }

    // now get xhandle and initialize
    xh = oh._xhandle;

    Clnt(xh)->flags = dupFromXh->flags;
    ClientData::SharedOffsetData *shdata;
    if (dupFromXh->isSharingOffset) {
	shdata = Clnt(xh)->sharedOffRef = dupFromXh->sharedOffRef;
	dupFromXh->sharedOffRef->filePosition = currOffset;
	shdata->lock.acquire();
	shdata->usageCount++;
	shdata->lock.release();
    } else {
	shdata = new ClientData::SharedOffsetData(currOffset);
	shdata->usageCount++;
	dupFromXh->sharedOffRef = shdata;
	dupFromXh->isSharingOffset = 1;
	Clnt(xh)->sharedOffRef = shdata;
    }

    Clnt(xh)->isSharingOffset = 1;
    Clnt(xh)->useType = (FileLinux::UseType) newUseType;

    SFDEBUG_ARGS("leaving _dup",
		 "w/  currOffset is %ld, fileLength %ld, "
		 "current useTypes are: orighandler %ld, newhandler %ld, "
		 "instance %ld\n", currOffset, fileLength,
		 (uval) dupFromXh->useType,
		 (uval) Clnt(xh)->useType, (uval) useType);

    fileInfo->incStat(FSStats::DUP);

    return 0;
}

/* virtual*/ SysStatus
ServerFile::exportedXObjectListEmpty()
{
    lock.acquire();

    SFTRACE("exportedXObjectListEmpty");

    // invalid files should be taken care of by file system specific code
    tassertMsg(doingDestroy == 1 || validFileInfo(),
	       "file system specific code needed for this\n");

    if (removalOnLastClose == 1) {
	tassertMsg(validFileInfo(), "?");
	SysStatus rc = fileInfo->deleteFile();
	if (_FAILURE(rc)) {
	    tassertMsg(0, "let's look at this failure\n");
	    lock.release();
	    return rc;
	}
	removalOnLastClose = 0;
	lock.release();
	traceDestroy("fslib", "exportedXObjectListEmpty");
	destroy();
	return 0;
    } else if (entToFree.timeFreed == 0) {
	if (validFileInfo()) {
	    // add itself to FileSytem::FreeList
	    fileInfo->freeServerFile(&entToFree);
	}
    }

    lock.release();
    return 0;
}

/* virtual */ SysStatus
ServerFile::handleXObjFree(XHandle xhandle)
{
    /*
     * Currently allocating same state for all clients, even FR that
     * doesn't make sense.   When we stop doing tihs, check type, i.e.
     * (XHandleTrans::GetTypeID(xhandle) == MetaFileLinuxServer::typeID())
     */

    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("handleXObjFree");

    XHandleTrans::SetBeingFreed(xhandle, BeingFreed);

    ClientData *clientData;
    clientData = (ClientData*)(XHandleTrans::GetClientData(xhandle));
    clientData->lock.acquire();
    if (clientData->smb && clientData->smbAddr) {
	ProcessID pid = XHandleTrans::GetOwnerProcessID(xhandle);
	DREF(clientData->smb)->unShare(clientData->smbOffset, pid);
	DREF(clientData->smb)->_unShare(clientData->smbOffset, XHANDLE_NONE);
    }

    fileLockList.removeFileLock(xhandle);

    if (XHandleTrans::GetOwnerProcessID(xhandle) != _KERNEL_PID) {
	// only consider external clints (not stubFR, not client for lazy
	// reopen of file descriptor after fork)
	useType = locked_changeState(DETACH, xhandle);
    }

    return 0;
}

/* virtual */ SysStatus
ServerFile::setDirLinuxRef(DirLinuxFSRef d) {
    AutoLock<LockType> al(&lock); // locks now, unlocks on return
    parent->getParentSet()->add(d);
    return 0;
}

void
ServerFile::detachFromDirectory()
{
    // FIXME dilma: new protocol replaces this
    tassertMsg(0, "shouldn't be called\n");
}

/*
 * check for read/write/execute permissions on a file.
 * We use "fsuid" for this, letting us set arbitrary permissions
 * for filesystem access without changing the "normal" uids which
 * are used for other things..
 */
/* virtual */ SysStatus
ServerFile::permission(uval mask)
{
#ifdef NO_CREDENTIAL_CHECKING
    return 0;
#else // leave credential checking in
    uval mode;
    FileLinux::Stat status;
    SysStatus rc;

    tassertMsg((fileInfo != NULL), "woops\n");
    rc = fileInfo->getStatus(&status);
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    mode = status.st_mode;
    ProcessLinux::creds_t *creds = FSCreds::Get();

    // check if file system has been mounted as  read only
    // (nobody gets write access to a read-only fs)
    if (mask & MAY_WRITE) {
	if (fileInfo->isReadOnly() &&
	    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode))) {
	    return _SERROR(2398, 0, EROFS);
	}
    }

    /* FIXME: check if file system has been mounted as immutable
     * (nobody gets write access to an immutable file)
     * it will be somethind like :
     * if ((mask & S_IWOTH) && IS_IMMUTABLE) {
     *	(S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode))) {
     *     return error EACCES;
     * }
     */

    if (creds->fsuid == status.st_uid) {
	mode >>= 6;
    } else if (creds->fsgid == status.st_gid) { // FIXME FIXME FIXME
	/* has to check supplemental groups also, something like
	 * linux in_group_p */
	mode >>= 3;
    }

    /*
     * If the DACs are ok we don't need any capability check.
     */
    if (((mode & mask & (MAY_READ|MAY_WRITE|MAY_EXEC)) == mask)) {
	return 0;
    }

    if (((mode & mask & S_IRWXO) == mask)
	|| creds->capable(ProcessLinux::creds_t::CAP_DAC_OVERRIDE)) {
	return 0;
    }

    /* read and search access */
    if ((mask == S_IROTH) ||
	(S_ISDIR(status.st_mode)  &&
	 !(mask & ~(S_IROTH | S_IXOTH)))) {
	if (creds->capable(ProcessLinux::creds_t::CAP_DAC_READ_SEARCH)) {
	    return 0;
	}
    }

    tassertMsg(0, "***** Returning error on end of permission()\n");
    return _SERROR(1876, 0, EACCES);
#endif // #ifdef NO_CREDENTIAL_CHECKING
}

/* virtual */ SysStatusUval
ServerFile::_ioctl(__in uval req,
		   __inoutbuf(size:size:size) char* buf,
		   __inout uval &size)
{
    return _SERROR(1341, 0, ENOTTY);
}

/* virtual */ SysStatus
ServerFile::tryToDestroy()
{
    SysStatusUval rc;

    lock.acquire();

    SFTRACE("tryToDestroy");

    rc = fileInfo->unFreeServerFile(&entToFree);
    tassertMsg(_SUCCESS(rc), "should return 0\n");
    lock.release();

    rc = runFreeProtocol(1);
    // FIXME: deal with error
    tassertMsg(_SUCCESS(rc), "need to deal with this\n");
    if (_SGETUVAL(rc) == 1) {
	entToFree.timeFreed = 0;
	// object can go away
	traceDestroy("fslib", "tryToDestroy");
	destroy();
    } else {
	lock.acquire();
	if (isEmptyExportedXObjectList() == 1) {
	    // still a good candidate for destruction;  re-append to list
	    entToFree.init();
	    fileInfo->freeServerFile(&entToFree);
	} else {
	    entToFree.timeFreed = 0;
	}
	lock.release();
    }

    return 0;
}

SysStatus
ServerFile::FileLockList::getLock(struct flock &fileLock)
{
    if (currLock == NULL) {
	fileLock.l_type = F_UNLCK;
    } else {
	memcpy(&fileLock, currLock, sizeof(fileLock));
    }

    // Cannot Fail!
    return 0;
}

SysStatus
ServerFile::FileLockList::setLock(struct flock &fileLock, ProcessID pid,
				  XHandle xh)
{
    SysStatus rc;

    switch (fileLock.l_type) {
    case F_RDLCK:
	tassertWrn(0, "no Reader locks implemented yet, treat as writer.\n");
	// FALLTHRU
    case F_WRLCK:
	if (currLock == NULL) {
	    currLock = new FileLockData(fileLock, pid, xh);
	    if (currLock == NULL) {
		return _SERROR(2510, 0, ENOMEM);
	    }
	    rc = 0;

	    // update client data
	    updateClientData(xh, 1);
	} else {
	    if (currLock->processID == pid) {
		// FIXME  Do we report Dead Lock here?
#ifdef REPORT_DEADLOCK_ON_SET
		rc = _SERROR(2511, 0, EDEADLK);
#else
		rc = 0;
#endif
	    } else {
		rc = _SERROR(2512, 0, EAGAIN);
	    }
	}
	break;
    case F_UNLCK:
	if (currLock == NULL) {
	    // do nothing
	    rc = 0;
	} else {
	    if (currLock->processID == pid) {
		unlock();
		rc = 0;
	    } else {
		// can't unlock what you don't hold
		rc = _SERROR(2512, 0, EACCES);
	    }
	}
	break;
    default:
	rc = _SERROR(2513, 0, EINVAL);
	break;
    }
    return rc;
}

SysStatus
ServerFile::FileLockList::setLockWait(struct flock &fileLock, uval &key,
				      ProcessID pid, XHandle xh)
{
    SysStatus rc;
    // sanity
    key = 0;
    switch (fileLock.l_type) {
    case F_RDLCK:
	tassertWrn(0, "no Reader locks implemented yet, treat as writer.\n");
	// FALLTHRU
    case F_WRLCK:
	if (currLock == NULL) {
	    currLock = new FileLockData(fileLock, pid, xh);
	    if (currLock == NULL) {
		return _SERROR(2514, 0, ENOMEM);
	    }
	    rc = 0;
	} else {
	    if (currLock->processID == pid) {
		// FIXME  Do we report Dead Lock here?
		rc = _SERROR(2515, 0, EDEADLK);
	    } else {
		// add to waiting list
		// FIXME: incomplete
		FileLockData *fld = new FileLockData(fileLock, pid, xh);
		if (fld == NULL) {
		    return _SERROR(2516, 0, ENOMEM);
		}
		// add to waiters list
		waiters.addToEndOfList(xh, fld);
		key = (uval)fld;
		// FIXME: cannot send key AND error
		rc = _SRETUVAL(EAGAIN);
	    }
	}

	// update client data
	updateClientData(xh, 1);
	break;
    case F_UNLCK:
	if (currLock == NULL) {
	    // do nothing
	    rc = 0;
	} else {
	    if (currLock->processID == pid) {
		unlock();
		rc = 0;
	    } else {
		// can't unlock what you don't hold
		rc = _SERROR(2517, 0, EACCES);
	    }
	}
	break;
    default:
	rc = _SERROR(2518, 0, EINVAL);
	break;
    }
    return rc;
}

void
ServerFile::FileLockList::unlock(void)
{

    updateClientData(currLock->xhandle, -1);

    delete currLock;

    if (waiters.isEmpty()) {
	currLock = NULL;
    } else {
	XHandle xh;
	waiters.removeHead(xh, currLock);
	tassertMsg(xh == currLock->xhandle,
		   "XHandles do not match.\n");
	currLock->wakeClient();
    }
}

void
ServerFile::FileLockList::removeFileLock(XHandle xh)
{
    FileLockData *dummy;

    // remove all waiters associated with this xhandle
    while (waiters.remove(xh, dummy)) {
	//tassertWrn(0, "waiter removed after detach.\n");
	updateClientData(xh, -1);
    }

    if (currLock != NULL && currLock->xhandle == xh) {
	//tassertWrn(0, "Lock released after detach.\n");
	unlock();
    }
}

void
ServerFile::FileLockList::updateClientData(XHandle xh, sval inc)
{
    ServerFile::ClientData *clnt;

    clnt = (ServerFile::ClientData *)
	(XHandleTrans::GetClientData(xh));

    clnt->hasFileLock += inc;

    tassertMsg(clnt->hasFileLock >= 0, "hasFileLock is negative.\n");

}

ServerFile::FileLockList::FileLockData::FileLockData(flock &f,
						     ProcessID pid,
						     XHandle xh)
{
    ProcessLinux::LinuxInfo linuxInfo;
    SysStatus rc;

    memcpy(this, &f, sizeof (f));

    // get linux pid
    rc = DREFGOBJ(TheProcessLinuxRef)->getInfoNativePid(pid, linuxInfo);
    tassertMsg(_SUCCESS(rc), "failed getting Linux Info. rc= %lx\n", rc);
    l_pid = linuxInfo.pid;

    processID = pid;
    xhandle = xh;
}

void
ServerFile::FileLockList::FileLockData::wakeClient(void)
{
    // WARNING: It is possible to try to wake the client and
    // it not be waiting anymore.
    ServerFile::ClientData *clnt;
    clnt = (ServerFile::ClientData *)(XHandleTrans::GetClientData(xhandle));

    clnt->lockCallBackStub._callBack((uval)this);
    return;
}

/* virtual */ SysStatus
ServerFile::_registerCallback(__in ObjectHandle callback,
			      __in uval callBackType, __inout uval &ut,
			      __out uval &flength, __out uval &offset,
			      __XHANDLE xhandle)
{
    SFTRACE("_registerCallBack");

    passertMsg((callBackType == FileLinux::LOCK_CALL_BACK
		|| callBackType == FileLinux::USETYPE_CALL_BACK),
	       "invalid call back type\n");

    ClientData *clientData = Clnt(xhandle);
    if (callBackType == FileLinux::LOCK_CALL_BACK) {
	clientData->setLockCallBackOH(callback);
	return 0;
    }

    passertMsg(callBackType == FileLinux::USETYPE_CALL_BACK, "ops\n");

    fileInfo->incStat(FSStats::REGISTER_CALLBACK);

    clientData->setUseTypeCallBackOH(callback);

    passertMsg(ut != FileLinux::FIXED_SHARED, "how come?\n");

#ifdef LAZY_SHARING_SETUP
    lock.acquire();
    useType = locked_changeState(REG_CB);
    (void) locked_waitForState();
    tassertMsg(waitingForClients.isEmpty(), "?");
    if (useType == FileLinux::LAZY_INIT) {
	useType = FileLinux::NON_SHARED;
    }
    ut = useType;
    clientData->useType = (FileLinux::UseType) ut;

    flength = fileLength;
    if (clientData->isSharingOffset == 1) {
	offset = clientData->sharedOffRef->filePosition;
    } else {
	offset = 0;
    }

    lock.release();

#else

    lock.acquire();
    (void) locked_waitForState();
    if (useType == FileLinux::LAZY_INIT) {
	useType = FileLinux::NON_SHARED;
    }

    ut = useType;
    clientData->useType = (FileLinux::UseType) ut;

    flength = fileLength;
    if (clientData->isSharingOffset == 1) {
	offset = clientData->sharedOffRef->filePosition;
    } else {
	offset = 0;
    }

    SFDEBUG_ARGS("Out of _registerCallback",
		 "with useType %ld, flength %ld and offset %ld\n",
		 useType, flength, offset);

    lock.release();

#endif // #ifdef LAZY_SHARING_SETUP

    tassertMsg(clientData->useType == FileLinux::SHARED ||
	       clientData->useType == FileLinux::NON_SHARED, "useType is "
	       "%ld\n", clientData->useType);

    return 0;
}

/* detachParent is part of destruction protocol.
 * A ServerFile can detach itself from a parent if it is in a state
 * where it could go away (it does not have any active client).
 * This method is invoked by the DirLinuxFS object in a locked state (the
 * lock is not released until the DirLinuxFS object removes the entry
 * for this ServerFile), so there's guarantee that no work will
 * be delegated to this object
 * while this call is in flight or later on.
 *
 * The method returns 1 if it successfully detached the parent, 0
 * if the parent can't be returned, and an error (value < 0) if
 * the given DirLinuxFSRef does not appear in the parent set.
 */
/* virtual */ SysStatusUval
ServerFile::detachParent(DirLinuxFSRef p)
{
    uval ret = 0;
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("detachParent");

    // if there is any cliet (even a FR), deny detachParent
    if (isEmptyExportedXObjectList()) {
	// try to upate list of parents
	ParentSet *pset = parent->getParentSet();
	if (pset->remove(p) == 0) {
	    return _SERROR(2175, 0, ENOENT);
	}
	return 1;
    }
    return ret;
}

/* virtual */ SysStatusUval
ServerFile::tryDetachParents()
{
    DirLinuxFSRef p;
    void *curr;
    SysStatusUval rc;

    lock.acquire();
    ParentSet *pset = parent->getParentSet();
    curr = pset->next(NULL, p);

    while (curr != NULL) {
	// lock is held here
	lock.release();
	rc = DREF(p)->detachChild(getRef());
	/* FIXME: we have released the lock, so it's possible that the
	 * parent doesn't know about this file (e.g. file became stale, etc).
	 * We have to deal with this!
	 */
	tassertMsg(_SUCCESS(rc), "parent may have disappeared\n");
	if (_SGETUVAL(rc) == 1) {
	    // detachChild succeeded in removal from parent
	    lock.acquire();
	    curr = pset->next(NULL, p);
	} else {
	    tassertMsg(_SGETUVAL(rc) == 0, "should be 0 or 1 or error...\n");
	    return 0;
	}
    }

    lock.release();
    return 1;
}

/* virtual */ SysStatusUval
ServerFile::detachMultiLink()
{
   AutoLock<LockType> al(&lock); // locks now, unlocks on return

   SFTRACE("detachMultiLink");

   if (isEmptyExportedXObjectList()) {
       return 0;
   }
   DirLinuxFSRef foo;
   ParentSet *pset = parent->getParentSet();
   pset->acquireLock();
   void *curr = pset->next(NULL, foo);
   pset->releaseLock();
   if (curr != NULL) {
       return 0;
   }

   return 1;
}

/* runFreeProtocol() detects if the file is free for destruction.
 * It returns 1 if file can be destroyed, 0 otherwise.
 *
 * It works in fases: (1) check if there are external clients,
 * (2) interacts with DirLinuxFS parents and (4) interacts
 * with MultiLink manager. (There is no phase 3. We're following
 * the same ids for phases as when we have FRs to interact with).
 * When a phase fails (e.g., DirLinuxFS
 * tries to detach this file, but then the file has new clients
 * so it doesn't go ahead), it simply gives up (returning 0).
 * If the phase succeeds, it will invoke runFreeProtocol recursively
 * with the next phase. This means that previous phases are
 * rechecked (with lock held), and things have changed (checks for
 * each phase that have succeeded in the past now fail), it
 * will give up.
 *
 * This method is specialized in ServerFileBlock with an extra
 * phase that checks if the FR can be detached.
 */
/* virtual */ SysStatusUval
ServerFile::runFreeProtocol(uval nextPhase)
{
    SFTRACE("runFreeProtocol");

    tassertMsg((nextPhase >= 1 && nextPhase <= 4), "invalid nextPhase value\n");
    tassertMsg(nextPhase != 2, "there is no phase 2 in this protocol\n");

    lock.acquire();
    if (isEmptyExportedXObjectList()==0) {
	lock.release();
	// can't free this file now
	return 0;
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
	SysStatusUval rc;
	FileLinux::Stat status;
	tassertMsg((fileInfo != NULL), "woops\n");
	rc = fileInfo->getStatus(&status);
	tassertMsg(_SUCCESS(rc), "wops?\n");
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
	    tassertMsg(_SGETUVAL(rc)==0, "it has to be 0 or 1\n");
	    return 0;
	}
    }

    lock.release();
    return 1;
}

/* virtual */ SysStatus
ServerFile::_ackUseTypeCallBack(uval responseType, uval flength, uval offset,
				__XHANDLE xhandle)
{
    passertMsg((responseType == FileLinux::CALLBACK_REQUEST_INFO
		|| responseType == FileLinux::CALLBACK_REQUEST_SWITCH),
	       "invalid responseType %ld\n", responseType);

    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("_ackUseTypeCallBack");

    //TraceOSFSAckUseTypeCallBack((uval) getRef(), flength, offset,
    //     (uval) xhandle);

    fileInfo->incStat(FSStats::ACK_USETYPE);

#ifdef DILMA_DEBUG_SWITCH
    err_printf("In server side of ack fileLength is %ld off is %ld\n",
	       fileLength, offset);
#endif //#ifdef DILMA_DEBUG_SWITCH

    // The following is only necessary if LAZY_SHARING_SETUP is not being used
    // I'm not guarding it because it's getting to ugly to separate the LAZY
    // case, and it's correct in both situations (of course if LAZY we'd be
    // doing extra comparison for nothing)
    if (flength != uval(~0)) {
	if (responseType == FileLinux::CALLBACK_REQUEST_SWITCH) {
	    // If this is a reply to a switch request, at this point we
	    // should have received the new file length already
	    // (through a _ftruncate) (unless the client didn't send it
	    // because it was not actually using it (only possible if we're
	    // not using LAZY initialization of NON_SHARED)
	    passertMsg(fileLength == flength, "unexpected\n");
	    uval *offAddr;
	    if (Clnt(xhandle)->isSharingOffset) {
		offAddr = &(Clnt(xhandle)->sharedOffRef->filePosition);
	    } else {
		offAddr = &(Clnt(xhandle)->filePosition);
	    }
	    passertMsg((*offAddr >= 0 && *offAddr <=fileLength),
		       "offset invalid\n");
#ifdef DILMA_DEBUG_SWITCH
	    tassertWrn(0, "In ServerFile::_ack offset going from %ld to %ld\n",
		       *offAddr, offset);
#endif /* #ifdef DILMA_DEBUG_SWITCH */
	    *offAddr = offset;
	} else {
	    tassertMsg(responseType == FileLinux::CALLBACK_REQUEST_INFO, "?");
	    fileLength = flength;
	}
    }

    //err_printf("Got _ack from client\n");
    useType = locked_changeState(ACK_CB, xhandle, responseType);
    Clnt(xhandle)->useType = (FileLinux::UseType) useType;

#ifdef DEBUG_USE_TYPE
    uval *offAddr;
    if (Clnt(xhandle)->isSharingOffset) {
	offAddr = &(Clnt(xhandle)->sharedOffRef->filePosition);
    } else {
	offAddr = &(Clnt(xhandle)->filePosition);
    }
    SFDEBUG_ARGS("Leaving ::_ack",
		 "with useType %ld, fileLength %ld, offset %ld\n",
		 useType, fileLength, *offAddr);
#endif // #ifdef DEBUG_USE_TYPE

    return 0;
}

/* virtual */ SysStatus
ServerFile::_lazyGiveAccess(__XHANDLE xhandle,
			    __in sval file, __in uval type,
			    __in sval closeChain,
			    __inbuf(dataLen) char *data, __in uval dataLen)
{
    BaseProcessRef pref;
    SysStatus rc;
    AccessRights match, nomatch;
    ProcessID dummy;
    ObjectHandle oh;
    ProcessID procID;

    tassertMsg(type == FileLinux_FILE, "?");
    tassertMsg(dataLen == sizeof(FileLinuxFile::LazyReOpenData),
	       "wrong datalen");

    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("_lazyGiveAccess");

    // go a giveacessfromserver on object to kernel, passing same rights
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    // Note: if we ever change things so that the lazyReOpen
    // information is not kept in the kernel, we have to change method
    // locked_isThereExternalClient, because there we assume external
    // clients mean clients different from the kernel ... in the
    // future this may mean different from certain servers also.
    // Ditto to method BeingFreed: it checks the type of the xhandle
    // going away by looking at pid being _KERNEL_PID (besides
    // checking xhandle type)
    rc = giveAccessByServer(oh, _KERNEL_PID, match, nomatch);
    if (_FAILURE(rc)) {
	return rc;
    }

    XHandle newxh = oh._xhandle;

    // get process from this call's xhandle
    procID = XHandleTrans::GetOwnerProcessID(xhandle);
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(procID, pref);
    if (_FAILURE(rc)) {
	DREFGOBJ(TheXHandleTransRef)->free(xhandle);
	return rc;
    }

    /* The parent client may go away, but we still want to keep the file position
     * information around to be used on lazyReOpen */
    ClientData *cdata = Clnt(xhandle);
    FileLinuxFile::LazyReOpenData *d =
	(FileLinuxFile::LazyReOpenData *) data;
    ClientData::SharedOffsetData *shdata;
    if (cdata->isSharingOffset) {
	shdata = cdata->sharedOffRef;
	shdata->lock.acquire();
	// Account for the fact that the lazyReOpen information kept in the kernel
	// (possibly in the future moved to the ProcessLinuxObject) refers to this file
	// position information
	shdata->usageCount++;
	shdata->lock.release();
    } else {
	shdata = new ClientData::SharedOffsetData(cdata->filePosition);
	// Account for the fact that the lazyReOpen information kept in the kernel
	// (possibly in the future moved to the ProcessLinuxObject) refers to this file
	// position information
	shdata->usageCount++;
	cdata->sharedOffRef = shdata;
	cdata->isSharingOffset = 1;
    }

    Clnt(newxh)->isSharingOffset = 1;
    Clnt(newxh)->sharedOffRef = shdata;
    d->shdata = (void*) shdata;

    // make a call on process object to pass along new object handle and
    //    data
    rc = DREF(pref)->lazyGiveAccess(file, type, oh, closeChain, match, nomatch,
				    data, dataLen);
    return rc;
}

/* virtual */ SysStatus
ServerFile::_lazyReOpen(__out ObjectHandle & oh,
			__in ProcessID toProcID,
			__in AccessRights match,
			__in AccessRights nomatch,
			__inoutbuf(datalen:datalen:datalen) char *data,
			__inout uval& datalen,
			__XHANDLE xhandle)
{
    XHandle xh;
    SysStatus rc;

    passertMsg(datalen == sizeof(FileLinuxFile::LazyReOpenData), "wrong len");

    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    SFTRACE("_lazyReOpen");

    // FIXME get rights correct
    rc = giveAccessByServer(oh, toProcID, match, nomatch);
    if (_FAILURE(rc)) {
	passertMsg(0, "look at failure\n");
	return rc;
    }

    FileLinuxFile::LazyReOpenData * ldata =
	(FileLinuxFile::LazyReOpenData*)data;
    ClientData::SharedOffsetData *shdata = (ClientData::SharedOffsetData*)
	ldata->shdata;
    tassertMsg(shdata->usageCount >= 1, "?");

    // now get xhandle and initialize
    xh = oh._xhandle;
    Clnt(xh)->flags = ldata->openFlags;
    Clnt(xh)->sharedOffRef = shdata;
    Clnt(xh)->isSharingOffset = 1;
    shdata->lock.acquire();
    shdata->usageCount++;
    shdata->lock.release();

#ifdef LAZY_SHARING_SETUP
    if (ldata->useType == FileLinux::FIXED_SHARED) {
	Clnt(xh)->useType = FileLinux::FIXED_SHARED;
    } else {
	//err_printf("in lazyReOpen old useType %ld\n", ldata->useType);
	Clnt(xh)->useType = FileLinux::LAZY_INIT;
    }
#else
    useType = locked_changeState(LAZYREOPEN, 0, FileLinux::CALLBACK_INVALID,
				 (O_ACCMODE & ldata->openFlags) != O_RDONLY);
    (void) locked_waitForState();
    tassertMsg(waitingForClients.isEmpty(), "?");
    if (useType == FileLinux::LAZY_INIT) {
	useType = FileLinux::NON_SHARED;
    }
    ldata->useType = Clnt(xh)->useType = useType;
#endif //   #ifdef LAZY_SHARING_SETUP

    SFDEBUG_ARGS("leaving _lazyReOpen",
		 "with useType %ld/%ld, nb clients %ld\n",
		 (uval) Clnt(xh)->useType, useType, locked_getNumberClients());

    return 0;
}

SysStatus
ServerFile::locked_returnUnlocked_deletion()
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_returnUnlocked_deletion");

    if (isEmptyExportedXObjectList()) {
	tassertMsg(fileInfo != NULL, "?");
	// no client in the file
	SysStatus rc = fileInfo->deleteFile();
	lock.release();
	_IF_FAILURE_RET(rc);
	traceDestroy("fslib", "locked_returnUnlocked_deletion\n");
	destroy();
    } else {
	removalOnLastClose = 1;
	// If only FR as client, we can
	// tell the FR the file is being removed
	if (locked_isThereNonFRClient() == 0) {
	    removed = 1;		// maa debug
	    lock.release();
	    frInteractionForDeletion();
	} else {
	    lock.release();
	}
    }
    return 0;
}

SysStatus
ServerFile::locked_returnUnlocked_cleanForDeletion(DirLinuxFSRef dirRef)
{
    _ASSERT_HELD(lock);

    SFTRACE("locked_returnUnlocked_cleanForDeletion");

    uval found = parent->getParentSet()->remove(dirRef);
    tassert(found, err_printf("element to be removed disappeared\n"));
    SysStatus rc;
    FileLinux::Stat status;
    rc = fileInfo->getStatus(&status);
    tassertMsg(_SUCCESS(rc), "ops");
    if (status.st_nlink == 0) {
	rc = locked_returnUnlocked_deletion();
	tassertMsg(_SUCCESS(rc), "?");
	return rc;
    } else {
	lock.release();
	return 0;
    }
}

/* virtual */ uval
ServerFile::locked_isThereNonFRClient()
{
    _ASSERT_HELD_PTR(getLockPtr());

    SFTRACE("locked_isThereNonFRClient");

    /* ServerFile objects that are not ServerFileBlock don't have
     * FR, so the implementation is trivial (this method is specialized
     * in ServerFileBlock to account for it maybe having a FR)
     */

    if (isEmptyExportedXObjectList() == 1) {
	return 0;
    } else {
	    return 1;
    }
}

/* virtual */ uval
ServerFile::locked_isThereExternalClient()
{
    _ASSERT_HELD_PTR(getLockPtr());

    SFTRACE("locked_isThereExternalClient");

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

    while (xh != XHANDLE_NONE) {
	// see if external client (external means not the kernel for
	// lazyGiveAccess or for an FR, if there is one
	// (ServerFileBlock instances have FR),
	if (XHandleTrans::GetOwnerProcessID(xh) != _KERNEL_PID) {
	    ret = 1;
	    break;			// found a client
	}
	xh = getNextExportedXObjectList(xh);
    }

    unlockExportedXObjectList();

    return ret;
}

#ifdef HACK_FOR_FR_FILENAMES
/* virtual */ SysStatusUval
ServerFile::_getFileName(char *buf, uval bufsize)
{
    tassertMsg(nameAtCreation != NULL, "?");
    uval len = strlen(nameAtCreation);
    len = (len < bufsize ? len : bufsize - 1);
    memcpy(buf, nameAtCreation, len);
    buf[len] = '\0';
    return _SRETUVAL(len + 1);
}
#endif // #ifdef HACK_FOR_FR_FILENAMES
