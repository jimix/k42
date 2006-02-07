/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockHFS.C,v 1.12 2001/08/20 17:44:10 mostrows Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubFRPA.H>
#include <io/FileLinux.H>
#include <fslib/DirLinuxFS.H>
#include <stdlib.h>
#include "fileserver.h"	// OS_BLOCK_SIZE

#include "FileSystemHurricane.H"
#include "ServerFileBlockHFS.H"
#include "PAFileServerHFS.H"

struct ServerFileBlockHFS::StubFRHolder
{
    StubFRPA stub;	// fr in memory manager for this file

    StubFRHolder():stub(StubObj::UNINITIALIZED) { };
    SysStatus init(ObjectHandle myOH, uval size);
    ~StubFRHolder() {

#ifdef HFS_DEBUG
	err_printf("deleting StubFRHolder %p\n", this);
#endif // HFS_DEBUG

    }
    DEFINE_GLOBAL_NEW(StubFRHolder);
};

SysStatus
ServerFileBlockHFS::StubFRHolder::init(ObjectHandle myOH, uval size)
{
    ObjectHandle oh;
    SysStatus rc;

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::StubFRHolder::init() was called.\n");
#endif // HFS_DEBUG

    rc = StubFRPA::_Create(oh, myOH, size);
    tassert(_SUCCESS(rc), err_printf("woops\n"));
    stub.setOH(oh);
    // err_printf("constructing StubFRHolder %p\n", this);
    return 0;
};

/* static */ SysStatus
ServerFileBlockHFS::Create(ServerFileRef & fref,
			   FileSystem::FileInfo * fileInfo, FileSystemRef fs)
{
    SysStatus rc;
    ServerFileBlockHFS *file = new ServerFileBlockHFS;

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::Create() was called.\n");
#endif // HFS_DEBUG

    tassert((file != NULL),
	    err_printf("failed allocate of ServerFileBlockHFS\n"));

    rc = file->init(fileInfo, fs);
    if (_FAILURE(rc)) {
	delete file;
	fref = NULL;
	return rc;
    }

    fref = (ServerFileRef) file->getRef();

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockHFS::destroy()
{

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::destroy() was called.\n");
#endif // HFS_DEBUG

    // FIXME dilma: for debugging purposes, let's tassert to make
    // sure that the file
    // being destroyed has been deleted (that is the only situation where
    // we destroy files; new situations (for paging) will appear
    // NOTICE that if the method destroy() is called without prior deletion,
    // we may need to detach the file for dirLinuxFS (that's why we have
    // a #ifdef DESTROY_NOT_CAUSED_BY_DELETION
    tassert(hasBeenDeleted, err_printf("Are we destroying files that "
				       "have not been deleted?\n"));
    uval oldDoingDestroy = FetchAndAddSignedVolatile(&doingDestroy, 1);
    if (oldDoingDestroy) {
	return 0;
    }

    stubFR->stub._destroy();

    // destroy the object that provides interface from kernel
    DREF(pagingObj)->detachFile();

#ifdef DESTROY_NOT_CAUSED_BY_DELETION
    // If the file has been unlink/deleted, than the directory cache
    // has already been updated; if not, we have to take care of it, i.e.,
    // detach from directory
    locked_detachFromDirLinuxFS();
#endif

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if(_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    if (stubFR) {
	delete stubFR;
	stubFR = 0;
    }

    DREF(fileSystem)->freeFileInfo(fileInfo);

    // schedule the object for deletion
    destroyUnchecked();

    return 0;
}

SysStatus
ServerFileBlockHFS::init(FileSystem::FileInfo * finfo, FileSystemRef fs)
{
    SysStatus rc;
    ObjectHandle pageOH;	// object handle to obj provids paging
    // interface to kernel

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::init() was called.\n");
#endif // HFS_DEBUG

    rc = ServerFile::init();
    _IF_FAILURE_RET(rc);

    fileInfo = finfo;
    fileLength = finfo->status.st_size;
    fileSystem = fs;

    CObjRootSingleRep::Create(this);

    // create the object that exports the interface to the kernel
    PAFileServerHFS::Create(pagingObj, getRef());

    // create an object handle for upcall from kernel FR
    DREF(pagingObj)->giveAccessByServer(pageOH, _KERNEL_PID);

    stubFR = new StubFRHolder();
    rc = stubFR->init(pageOH, finfo->status.st_size);

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockHFS::setFileLength(uval len)
{
    SysStatus rc = stubFR->stub._setFileLengthFromFS(len);

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::setFileLength() was called.\n");
#endif // HFS_DEBUG

    _IF_FAILURE_RET(rc);
    fileLength = len;
    fileInfo->status.st_size = len;
    return 0;
}

/* virtual */ SysStatus
ServerFileBlockHFS::getFROH(ObjectHandle &oh, ProcessID pid)
{
#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::getFROH() was called.\n");
#endif // HFS_DEBUG

    // FIXME: should restrict authorization given to client
    return stubFR->stub._giveAccess(oh, pid);
}

/* virtual */ SysStatus
ServerFileBlockHFS::startWrite(uval physAddr, uval objOffset, uval len)
{
    AutoLock < LockType > al(&lock);	// locks now, unlocks on return

    SysStatus rc = 0;

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::startWrite() was called.\n");
#endif // HFS_DEBUG

    if (!hasBeenDeleted) {
	rc = DREF(fsHFS())->writeBlockPhys(fileInfo->token, physAddr,
					   (uval32) len, objOffset);
	//FIXME dilma: update fileInfo->status.modTime
	if (objOffset + len > fileLength) {
	    fileInfo->status.st_size = objOffset + len;
	    fileLength = objOffset + len;
	}
    }

    stubFR->stub._putPageComplete(physAddr, objOffset, rc);

    return 0;
}


/* virtual */ SysStatus
ServerFileBlockHFS::startFillPage(uval physAddr, uval objOffset)
{
    AutoLock < LockType > al(&lock);	// locks now, unlocks on return

    SysStatus rc;

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::startFillPage() was called.\n");
#endif // HFS_DEBUG

    rc = DREF(fsHFS())->readBlockPhys(fileInfo->token, physAddr, objOffset);
    // FIXME dilma: update fileStatus.atime
    stubFR->stub._fillPageComplete(physAddr, objOffset, rc);
    err_printf("H");

    return 0;
}

SysStatus
ServerFileBlockHFS::_setFileLengthFromFR(uval len)
{
    AutoLock < LockType > al(&lock);	// locks now, unlocks on return

    SysStatus rc = 0;

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::_setFileLengthFromFR(() was called.\n");
#endif // HFS_DEBUG

    if (len < fileLength) {
	rc = DREF(fileSystem)->ftruncate(fileInfo, len);
	tassert(_SUCCESS(rc), err_printf("failure\n"));
    }

    fileLength = len;

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockHFS::_write(uval physAddr, uval objOffset, uval len)
{
    AutoLock < LockType > al(&lock);	// locks now, unlocks on return

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::_write() was called.\n");
#endif // HFS_DEBUG

    tassert((len == PAGE_SIZE), err_printf("woops\n"));
    SysStatus rc;
    rc = DREF(fsHFS())->writeBlockPhys(fileInfo->token, objOffset, physAddr,
				       len);
    _IF_FAILURE_RET(rc);

    if (objOffset + len > fileLength) {
	fileLength = objOffset + len;
    }

    fileInfo->status.st_size = fileLength;

    return 0;
}

#ifdef WE_REALLY_NEED_THIS
SysStatus
ServerFileBlockHFS::getStatus(FileLinux::Stat * status)
{
    AutoLock < LockType > al(&lock);	// locks now, unlocks on return
    SysStatus rc = DREF(fileSystem)->getAttribute(fileInfo);

#ifdef HFS_DEBUG
    err_printf("ServerFileBlockHFS::getStatus() was called.\n");
#endif // HFS_DEBUG

    memcpy(status, &fileInfo->status, sizeof(FileLinux::Stat));
    return rc;
}
#endif //WE_REALLY_NEED_THIS
