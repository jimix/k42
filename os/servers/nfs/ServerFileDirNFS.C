/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "ServerFileDirNFS.H"
#include "FileSystemNFS.H"
#include <scheduler/Scheduler.H>
#include <trace/traceFS.h>

// Definition of minimum timeout value as 30 seconds
/* static */ SysTime
ServerFileDirNFS::TIMEOUT_MIN = Scheduler::TicksPerSecond()*30;
// Definition of maximum timeout value as 60 seconds
/* static */ SysTime
ServerFileDirNFS::TIMEOUT_MAX = Scheduler::TicksPerSecond()*60;
/* FIXME: since now the system doesn't stay up that long, let's assume
 *        that it has changed in the last 10 minutes, we should be
 *        neurotic about it. But in the future it's reasonable to use
 *        1 hours as the threashold
 */
/* static */ SysTime
ServerFileDirNFS::STRICT_THRESHOLD = Scheduler::TicksPerSecond()*60*10;

/* virtual */ inline uval
ServerFileDirNFS::shouldRevalidate()
{
    // if directory has changed recently, be neurotic about it
    SysTime now = Scheduler::SysTimeNow();
    if (now < mtimeChangeStamp + STRICT_THRESHOLD) {
	return 1;
    } else if (now > timestamp + timeout) {
	return 1;
    } else {
	return 0;
    }
}

/* virtual */ SysStatus
ServerFileDirNFS::locked_revalidate()
{
    _ASSERT_HELD_PTR(getLockPtr());

    if (!validFileInfo()) {
	tassertMsg(0, "I thought no one is invalidating file info now\n");
	// FIXME dilma: this wassert is for debugging, should go away
	tassertWrn(0, "An invalidated fileRef is being used\n");
	return _SERROR(2092, 0, ESTALE);
    }

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

	// check if directory has been changed in the remote side
	if (FileSystemNFS::FINF(fileInfo->getToken())->status.st_mtime
	    > FileSystemNFS::FINF(fileInfo->getToken())->modtime) {
	    TraceOSFSNFSDirRevalidateMtimeChanged((uval) getRef(),
		     (uval) fileInfo,
		     (uval) FileSystemNFS::FINF(fileInfo->getToken())->
		     status.st_mtime,
		     (uval) FileSystemNFS::FINF(fileInfo->getToken())->modtime);
#ifdef DEBUG_ESTALE
	    tassertMsg(0, "fileInfo 0x%lx, status.st_mtime 0x%lx, "
		       "modtime 0x%lx\n", (uval) fileInfo,
		       (uval) FileSystemNFS::FINF(fileInfo->getToken())->
		       status.st_mtime,
		       (uval) FileSystemNFS::FINF(fileInfo->getToken())->
		       modtime);
#else
#if 0
	    tassertWrn(0, "fileInfo 0x%lx, status.st_mtime 0x%lx, "
		       "modtime 0x%lx\n", (uval) fileInfo,
		       (uval) FileSystemNFS::FINF(fileInfo->getToken())->
		       status.st_mtime,
		     (uval) FileSystemNFS::FINF(fileInfo->getToken())->modtime);
#endif
#endif // #ifdef DEBUG_ESTALE

	    rc = locked_purge();
	    timeout = TIMEOUT_MIN;
	    mtimeChangeStamp = now;
	    FileSystemNFS::FINF(fileInfo->getToken())->modtime =
		FileSystemNFS::FINF(fileInfo->getToken())->status.st_mtime;
	}
    } else if (_SGENCD(rc)==ESTALE) {
#ifdef DILMA_DEBUG
	err_printf("locked_revalidate got stale file\n");
#endif
	// getting the cached data out is done by DirLinuxFSVolatile::revalidate
    }

    return rc;
}

/* virtual */ SysStatus
ServerFileDirNFS::init(PathName *pathName, uval pathLen, FSFile *theInfo, 
		       DirLinuxFSRef par)
{
    DirLinuxFSVolatile::init(pathName, pathLen, theInfo, par);

    timeout = TIMEOUT_MIN;
    SysTime now = Scheduler::SysTimeNow();
    timestamp = now;
    timeoutUpdateStamp = now;
    // let's mark that we didn't see any change for the given threshold
    mtimeChangeStamp = 0;
    return 0;
}

/* virtual */ SysStatus
ServerFileDirNFS::sync()
{
    SysStatus rc;
    ListSimple<DirLinuxFSRef, AllocGlobal> list;

    lock.acquire();
    void* curr = NULL;
    NameHolderInfo nhi;
    children.lookup(".", 1, &nhi);
    DirLinuxFSRef dot = nhi.dref;
    children.lookup("..", 2, &nhi);
    DirLinuxFSRef dotdot = nhi.dref;

    while ((curr = children.getNext(curr, &nhi))) {
	if (nhi.isFSFile()) {
	    continue;
	}
	if (nhi.isDirSF()) {
	    if (nhi.dref != dot && nhi.dref != dotdot) {
		//err_printf("Adding dref %p to the list\n", nhi.dref);
		list.add(nhi.dref);
	    }
	} else if (!nhi.isOtherFSFile() && !nhi.isSymLinkFSFile()) {
	    //err_printf("calling explicitFsync() for fref %p\n", nhi.fref);
	    rc = DREF(nhi.fref)->explicitFsync();
	}
    }
    lock.release();

    DirLinuxFSRef dref;
    while (list.removeHead(dref)) {
	//err_printf("going recursively in directory %p\n", dref);
	DREF(dref)->sync();
    }
    return 0;
}
