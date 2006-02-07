/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DirLinuxFSVolatile.C,v 1.68 2005/08/13 02:49:17 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: specializes validation of caches
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "ServerFile.H"
#include "DirLinuxFSVolatile.H"
#include <misc/StringTable.I>
#include <scheduler/Scheduler.H>
#include <trace/traceFS.h>

/* virtual*/ SysStatus
DirLinuxFSVolatile::locked_handleErrorOnToken(char *name, uval namelen,
					      SysStatus error)
{
    _ASSERT_HELD(dirlock);

    SysStatus rc = 0;
    if (_SGENCD(error) == ESTALE) {
	NameHolderInfo nhi;
	SysStatus rc2 = children.remove(name, namelen, &nhi);
	passertMsg(_SUCCESS(rc2), "things are wrong");
	// remove cache of token (need to handle stale file handle for NFS)
	err_printf("Invoking destroy from locked_handleErrorOnToken\n");
	nhi.fsFile->destroy();
    }

    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::utime(const struct utimbuf *utbuf)
{
    SysStatus rc = DirLinuxFS::utime(utbuf);
#ifdef DILMA_DEBUG
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	tassertWrn(0, "utime(utbuf) got stale file\n");
    }
#endif /* #ifdef DILMA_DEBUG */
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::utime(char *name, uval namelen,
			  const struct utimbuf *utbuf)
{
    SysStatus rc;

    if (shouldRevalidate() == 1) {
	rc = revalidate();
	_IF_FAILURE_RET(rc);
    }

    rc = DirLinuxFS::utime(name, namelen, utbuf);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	rc = DirLinuxFS::utime(name, namelen, utbuf);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::externalLookupDirectory(char *name, uval namelen,
					    DirLinuxFSRef &dr,
					    RWBLock* &nhSubDirLock)
{
    SysStatus rc;

    if (shouldRevalidate() == 1) {
	rc = revalidate();
	_IF_FAILURE_RET(rc);
    } else {
	// for lookups, let's clean up the children list at first, even if
	// full revalidation is not needed
	(void) checkDetachList();
    }

    rc = DirLinuxFS::externalLookupDirectory(name, namelen, dr, nhSubDirLock);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children (again!) list and talk to the
	 * server about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	rc = DirLinuxFS::externalLookupDirectory(name, namelen, dr,
						 nhSubDirLock);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::fchown(uid_t uid, gid_t gid)
{
    SysStatus rc = DirLinuxFS::fchown(uid, gid);

    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	(void) eliminateStaleDir();
    }

    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::chown(char *name, uval namelen, uid_t uid, gid_t gid)
{
    SysStatus rc;

    if (shouldRevalidate() == 1) {
	rc = revalidate();
	_IF_FAILURE_RET(rc);
    }

    rc = DirLinuxFS::chown(name, namelen, uid, gid);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	rc = DirLinuxFS::chown(name, namelen, uid, gid);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::fchmod(mode_t mode)
{
    SysStatus rc = DirLinuxFS::fchmod(mode);

    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	(void) eliminateStaleDir();
    }

    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::chmod(char *name, uval namelen, mode_t mode)
{
    SysStatus rc;

    if (shouldRevalidate() == 1) {
	rc = revalidate();
	_IF_FAILURE_RET(rc);
    }

    rc = DirLinuxFS::chmod(name, namelen, mode);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	rc = DirLinuxFS::chmod(name, namelen, mode);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::mkdir(char *name, uval namelen, mode_t mode)
{
    SysStatus rc;

    /* This method will change modification time for directory, so before
     * we do change it, we revalidate cached directory information (so
     * we don't miss any inconsistencies */
    rc = revalidate();
    _IF_FAILURE_RET(rc);

    rc = DirLinuxFS::mkdir(name, namelen, mode);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);

	rc = DirLinuxFS::mkdir(name, namelen, mode);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::rmdir(char *name, uval namelen)
{
    SysStatus rc;

    /* This method will change modification time for directory, so before
     * we do change it, we revalidate cached directory information (so
     * we don't miss any inconsistencies */
    rc = revalidate();
    _IF_FAILURE_RET(rc);

    rc = DirLinuxFS::rmdir(name, namelen);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);

	rc = DirLinuxFS::rmdir(name, namelen);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::unlink(char *name, uval namelen)
{
    SysStatus rc;

    /* This method will change modification time for directory, so before
     * we do change it, we revalidate cached directory information (so
     * we don't miss any inconsistencies */
    rc = revalidate();
    _IF_FAILURE_RET(rc);

    rc = DirLinuxFS::unlink(name, namelen);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	rc = DirLinuxFS::unlink(name, namelen);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::getObj(char *name, uval namelen, uval oflag, mode_t mode,
			   ProcessID pid, ObjectHandle &oh,
			   uval &uType, TypeID &type,
			   /* argument for simplifying gathering traces of
			    * file sharing information. This should go away. */
			   ObjRef &fref)
{
    SysStatus rc;

    rc = revalidate();
    _IF_FAILURE_RET(rc);

    /* directory lookup is very relevant to getObj, so let's start making
     * sure that we removed from children any entry we've already detected
     * as stale */
    (void) checkDetachList();

    rc = DirLinuxFS::getObj(name, namelen, oflag, mode, pid, oh, uType,
			    type,
			    /* argument for simplifying gathering traces of
			     * file sharing information. This should go
			     * away. */
			    fref);

    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);

	rc = DirLinuxFS::getObj(name, namelen, oflag, mode, pid, oh,
				uType, type,
				/* argument for simplifying gathering traces of
				 * file sharing information. This should go
				 * away. */
				fref);
    }
    return rc;
}

SysStatus
DirLinuxFSVolatile::CreateTopDir(DirLinuxFSRef &dirLinuxRef, char *up,
				 FSFile *file)
{
    PathNameDynamic<AllocGlobal> *dirPathName;
    uval      dirPathLen;

    tassertMsg((up != NULL), "woops\n");
    dirPathLen = PathNameDynamic<AllocGlobal>::Create(
	up, strlen(up), 0, 0, dirPathName, PATH_MAX+1);

    file->createDirLinuxFS(dirLinuxRef, dirPathName, dirPathLen, 0);
    return 0;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::validateFileRef(ServerFileRef fileRef, char &hasValidEntry,
				    char &hasInvalidEntry, uval origIno)
{
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    (void) locked_checkDetachList();

    SysStatus rc;
    char *entryName;
    uval entryLen;

    char pathBuf[PATH_MAX+1];
    PathName *tmpPathName;
    uval tmpPathLen;
    hasValidEntry = 0;
    hasInvalidEntry = 0;

    do {
	rc = children.lookup((ObjRef)fileRef, entryName, entryLen);
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) == ENOENT) {
		break; // done
	    } else {
		tassertMsg(0, "look at failure rc 0x%lx\n", rc);
	    }
	}
	/*
	 * FIXME: why are we doing this, could just use directory
	 *   filetoken and the name of the file inside this direcotry??
	 *
	 * check file existance by querying server with full path name
	 * can't fail so don't check rc
	     */
	PathName::PathFromBuf(pathBuf, 0, tmpPathName);
	rc = getFullDirName(tmpPathName, PATH_MAX+1);
	if (_FAILURE(rc)) {
	    tassertWrn(0, "getFullDirName failed\n");
	    continue;
	}
	tmpPathLen = _SGETUVAL(rc);
	rc = tmpPathName->appendComp(tmpPathLen, PATH_MAX+1, entryName, entryLen);
	if (_FAILURE(rc)) {
	    tassertWrn(0, "appendComp failed\n");
	    continue;
	}
	tmpPathLen = _SGETUVAL(rc);

	FSFile * tmpInfo = NULL;
	FileLinux::Stat tmpStatus;
	rc = fileInfo->getFSFile(tmpPathName, tmpPathLen, 0, &tmpInfo,
				 &tmpStatus);
	if (_FAILURE(rc)) {
	    if (_SGENCD(rc) == ENOENT) {
		// file does not exist anymore
		hasInvalidEntry = 1;
		rc = children.remove((ObjRef)fileRef);
		tassertWrn(_SUCCESS(rc), "remove entry failed\n");
	    } else {
		// FIXE dilma: we don't know the type of failure;
		// should we invalidate anyway?
		tassertWrn(_SUCCESS(rc), "getFSFile failed %lx\n", rc);
		return rc;
	    }
	} else {
	    // file exists; we need to check if it is the same one we had
	    // the link before
	    if (origIno == (uval) tmpStatus.st_ino) {
		hasValidEntry = 1;
	    } else {
		// file does not exist anymore
		hasInvalidEntry = 1;
		rc = children.remove((ObjRef)fileRef);
		tassertWrn(_SUCCESS(rc), "remove entry failed\n");
	    }
	    // FIXME dilma: free tmpInfo since won't be using it
	}
    } while (1);

    return 0;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::locked_purge()
{
    SysStatus rc;
    _ASSERT_HELD(dirlock);

    // clean up detachList
    (void) locked_checkDetachList();

    // This routine goes through list of remaining children: FSFile entries
    // are simply discarded (FIXME possible leak), for file or directory
    // objects we do a directory lookup to check if the entity still exists.
    // (Notice that a simple revalidation of the FileInfo  - e.g. NFS handle -
    // is not sufficient because mv does not imply necessarily into new
    // fhandles).
    // If lookup fails, we invoke object destruction. If lookup succeeds,
    // we check inode to verify we're refering to the same element.

    struct {
	ListSimple<ObjRef, AllocGlobal> trash;
	void add(ObjRef ref) {
	    trash.add(ref);
	}
	void checkAdd(FSFile *dirInfo, char *name, uval namelen,
		      ServerFileRef ref) {
	    SysStatus rc2;
	    FileLinux::Stat status;
	    rc2 = dirInfo->revalidate(name, namelen, &status);
	    if (_SUCCESS(rc2)) {
		FileLinux::Stat oldstat;
		(void) DREF(ref)->getStatus(&oldstat);
		if (status.st_ino != oldstat.st_ino) {
		    char buf[PATH_MAX+1];
		    memcpy(buf, name, namelen);
		    buf[namelen] = '\0';	
#if 0
		    tassertWrn(0, "checkAdd discarding file(%s) that seems to be"
			       " a new file in the server\n", buf);
#endif
		    // FIXME: ref has to be destroyed, for now leaking it
		    trash.add((ObjRef)ref);
#if 0
		    TraceOSFSDirectoryPurge(
			     (uval) dirInfo, (uval) ref);
#endif
		}
	    } else {
		tassertMsg(_SGENCD(rc2) == ENOENT, "?");
		char buf[PATH_MAX+1];
		memcpy(buf, name, namelen);
		buf[namelen] = '\0';
#if 0
		tassertWrn(0, "checkAdd discarding file(%s) that seems to have"
			   " disappeared under us\n", buf);
#endif
		// FIXME: ref has to be destroyed, for now leaking it
		trash.add((ObjRef)ref);
#if 0
		TraceOSFSDirectoryPurge((uval) dirInfo, (uval) ref);
#endif
	    }
	}
    } reval;

    NameHolderInfo nhi;
    void *curr = NULL;
    char *name;
    uval len;
    while ((curr = children.getNext(curr, &nhi)) != NULL) {
	// we should not return the entry for "."
	tassertMsg(nhi._obj != NULL, "shouldn't happen\n");
	if (nhi.isFSFile() == 1) {
	    // let's throw it away
	    reval.add(nhi._obj);
	} else { // we have an object, either file or dir
	    tassertMsg(nhi.fref != NULL, "wops\n");
	    rc = children.lookup((ObjRef)nhi.fref, name, len);
	    tassertMsg(_SUCCESS(rc), "?");
	    reval.checkAdd(fileInfo, name, len, nhi.fref);
	 }
     }


     while (!reval.trash.isEmpty()) {
	 // retrieve element to be removed from list
	 ObjRef ref;
	 uval ret = reval.trash.removeHead(ref);
	 tassertMsg(ret == 1, "?");

	 // FIXME: we need to free FSFile object ?!
	 rc = children.remove(ref);
	 tassertMsg(_SUCCESS(rc), "entry disappeared\n");
     }

     return 0;
 }

/* virtual */ SysStatus
DirLinuxFSVolatile::getStatus(FileLinux::Stat *status)
{
    SysStatus rc;

    if (shouldRevalidate() == 1) {
	rc = revalidate();
	_IF_FAILURE_RET(rc);
    }

    /* DirLinuxFS::getStatus can't return ESTALE for NFS, since
     * FileSystemNFS::getStatus only copies the stat data currently
     * being cached by FileSystemNFS, so
     *              return DirLinuxFS::getStatus(status);
     * would be enough here.
     * But in general a "volatile" file system could implement getStatus in
     * a way that returns ESTALE, so lets code this in a more general way
     */
    rc = DirLinuxFS::getStatus(status);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	(void) eliminateStaleDir();
    }

    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::getStatus(char *name, uval namelen,
			      FileLinux::Stat *retStatus, uval followLink)
{
    /* DirLinuxFSVolatile:getStatus(status) checks if it's time to
     * go to the file system to updated cached data (instead of
     * using DirLinuxFS::getStatus(status) definition, which only goes
     * to the file system, which in the NFS case only returns the
     * cached stat structure).
     * ServerFileBlockNFS(status) also checks if it's time for
     * revalidation, so a scheme of cleaning up followed by
     * retrying DirLinuxFS::getStatus(name, namelen, retStatus)
     * works. */
    SysStatus rc;

    if (shouldRevalidate() == 1) {
	rc = revalidate();
	_IF_FAILURE_RET(rc);
    }

    rc = DirLinuxFS::getStatus(name, namelen, retStatus, followLink);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* Either (1) the directory entry has become stale or
	 *        (2) the directory itself has become stale.
	 * On both cases, directory info has changed, so the best
	 * thing is to clean up the children list and talk to the server
	 * about the directory (reading it by name if it has becoming
	 * stale) Method revalidate() takes care of all of this.
	 */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	rc = DirLinuxFS::getStatus(name, namelen, retStatus, followLink);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::detachInvalidFile(ServerFileRef fileRef)
{
    detachList->insert(fileRef);
    return 0;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::locked_detachInvalidDir(DirLinuxFSRef dirRef)
{
    /* differently from detachInvalidFile, when a DirLinuxFSVolatile object
     * invokes detachInvalidDir, the object lock is not being hold, so
     * we can try to get this right now */
#ifdef DILMA_DEBUG
    err_printf("In detachInvalidDir: getting rid of %lx\n",  (uval) dirRef);
#endif /* #ifdef DILMA_DEBUG */

    NameHolderInfo nhi;
    SysStatus rc = children.remove((ObjRef)dirRef, &nhi);
    if (_SUCCESS(rc)) {
	// remove it
	nhi.rwlock->acquireW();
	// we're holding the directory lock, so we're the last ones
	// getting to this nameHolder, so no need to release the lock
	// before freeing the nameHolder, but let's do it anyway
	nhi.rwlock->releaseW();
	tassertWrn(_SUCCESS(rc), "children remove failed \n");
    } else {
	// FIXME: not sure now if we're really supposed to find it ...
	tassertMsg(0, "?");
	return rc;
    }
    return 0;
}

SysStatus
DirLinuxFSVolatile::locked_doDetachInvalidFile(ServerFileRef fileRef)
{
#ifdef DILMA_DEBUG
    err_printf("In locked_doDetachInvalidFile: for fileRef %lx\n",
	       (uval) fileRef);
#endif /* #ifdef DILMA_DEBUG */

    _ASSERT_HELD(dirlock);

    NameHolderInfo nhi;
    // FIXME: could we do a remove without a lookup (but what if someone
    // else had the lock to it?
    SysStatus rc = children.lookup((ObjRef)fileRef, &nhi);
    if (_SUCCESS(rc)) {
	// remove it
	nhi.rwlock->acquireW();
	// we're holding the directory lock, so we're the last ones
	// getting to this nameHolder, so no need to release the lock
	// before freeing the nameHolder, but let's do it anyway
	nhi.rwlock->releaseW();
	rc = children.remove((ObjRef)fileRef);
	tassertWrn(_SUCCESS(rc), "children remove failed \n");
    } else {
	/* if the element is not found, no problem: it means that before
	 * a stale file had the change to tell the directory to get rid of
	 * it, the directory itself (checking change in modification time)
	 * initiated the clean up
	 */
    }
    return 0;
}

SysStatus
DirLinuxFSVolatile::checkDetachList()
{
    AutoLock<DirLockType> al(&dirlock);
    return locked_checkDetachList();
}

SysStatus
DirLinuxFSVolatile::locked_checkDetachList()
{
    _ASSERT_HELD(dirlock);
    ServerFileRef ref;

    while ((ref = detachList->remove()) != 0) {
	// the method doesn't fail
	(void) locked_doDetachInvalidFile(ref);
    }

    return 0;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::locked_lookup(char *name, uval namelen,
				  NameHolderInfo *nameHolderInfo)
{
    _ASSERT_HELD(dirlock);
    (void) locked_checkDetachList();
    return DirLinuxFS::locked_lookup(name, namelen, nameHolderInfo);
}

/* virtual */ uval
DirLinuxFSVolatile::isDirectoryCacheEmpty()
{
    (void) checkDetachList();
    return DirLinuxFS::isDirectoryCacheEmpty();
}

#ifdef NEEDED_SPECIAL_locked_lookupOrAdd
/* virtual */ SysStatus
DirLinuxFSVolatile::locked_lookupOrAdd(char *name, uval namelen,
				       NameHolder **nameHolder,
				       FileLinux::Stat *retStatus,
				       uval forceSFCreate)
{
    (void) locked_checkDetachList();
    return DirLinuxFS::locked_lookupOrAdd(name, namelen, nameHolder, retStatus,
					  forceSFCreate);
}
#endif /* #ifdef NEEDED_SPECIAL_locked_lookupOrAdd */

/* virtual */ SysStatus
DirLinuxFSVolatile::init(PathName *pathName, uval pathLen, FSFile *theInfo,
			 DirLinuxFSRef par)
{
    detachList = new DetachList();
    return DirLinuxFS::init(pathName, pathLen, theInfo, par);
}

/* virtual */ SysStatus
DirLinuxFSVolatile::link(char *oldname, uval oldnamelen,
			 DirLinuxFSRef newDirRef, char *newname,
			 uval newlen)
{
    SysStatus rc;
    /* This method will change modification time for (target)directory, so before
     * we do change it, we revalidate cached directory information (so
     * we don't miss any inconsistencies */
    rc = DREF(newDirRef)->revalidate();
    _IF_FAILURE_RET(rc);

    /* get rid of children that we already know are stale */
    (void) checkDetachList();

    rc = DirLinuxFS::link(oldname, oldnamelen, newDirRef, newname, newlen);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* clean up and revalidation of cached data for both directories */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	rc = DREF(newDirRef)->revalidate();
	_IF_FAILURE_RET(rc);

	rc = DirLinuxFS::link(oldname, oldnamelen, newDirRef, newname, newlen);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFSVolatile::rename(char *oldname, uval oldnamelen,
			   DirLinuxFSRef newDirRef, char * newname,
			   uval newlen)
{
    SysStatus rc;
    DirLinuxFSRef oldDirRef = (DirLinuxFSRef)getRef();
    /* This method will change modification time for both "source" and
     * "target " directory, so before we do change them, we revalidate
     * cached directory information (so we don't miss any inconsistencies */
    rc = revalidate();
    _IF_FAILURE_RET(rc);
    if (oldDirRef != newDirRef) {
	rc = DREF(newDirRef)->revalidate();
	_IF_FAILURE_RET(rc);
    }
    rc = DirLinuxFS::rename(oldname, oldnamelen, newDirRef, newname, newlen);
    if (_FAILURE(rc) && (_SGENCD(rc)==ESTALE)) {
	/* clean up and revalidation of both directories */
	rc = revalidate();
	_IF_FAILURE_RET(rc);
	if (oldDirRef != newDirRef) {
	    rc = DREF(newDirRef)->revalidate();
	    _IF_FAILURE_RET(rc);
	}
	rc = DirLinuxFS::rename(oldname, oldnamelen, newDirRef, newname, newlen);
    }
    return rc;
}

SysStatus
DirLinuxFSVolatile::eliminateStaleDir(
    DirLinuxFSVolatileRef lockedParent /* = NULL */)
{
    // FIXME: we are not freeing the subtree properly!

    FSFile* f = fileInfo;
    fileInfo = (FSFile*)theDeletedObj;
    f->destroy();

    DirLinuxFSRef dref = getPar();
    tassertMsg(dref != NULL, "ops\n");
    if (lockedParent == NULL) {
	(void) DREF(dref)->detachInvalidDir(getRef());
    } else {
	(void) DREF((DirLinuxFSVolatileRef)dref)->
	    locked_detachInvalidDir(getRef());
    }
    if (isEmptyExportedXObjectList()) {
#ifdef DILMA_DEBUG
	err_printf("DirLinuxFSVolatile::eliminateStaleDir will invoke destroy\n");
#endif
	destroy();
    }
    return 0;
}

/* revalidate() is invoked to check if the cached file meta-data is
 * still up to date.
 * revalidate() should (1) clean up the children list and
 * (2) talk to the server about the directory, returning ESTALE error
 * if the file it has become stale) and (3) deal with getting the
 * cached data out if ESTALE has been detected.
 * (The part (2) is very file system specific, so
 * locked_revalidate() should be provided by the file system specific code
 */
SysStatus
DirLinuxFSVolatile::revalidate()
{
    AutoLock<DirLockType> al(&dirlock);
    (void) locked_checkDetachList();
    SysStatus rc = locked_revalidate();
    if (_FAILURE(rc) && _SGENCD(rc) == ESTALE) {
#ifdef DEBUG_ESTALE
	tassertMsg(0, "In DirLinuxFSVolatile::revalidate()\n");
#else
	tassertWrn(0, "In DirLinuxFSVolatile::revalidate()\n");
#endif // #ifdef DEBUG_ESTALE
	(void) eliminateStaleDir();
	return 0;
     }
    return rc;
}

/* revalidateWithParentLocked() is invoked by locked_purge). The idea
 * is that when if we decide to clean up this object, we have to take into
 * account that the parent directory is known to be estale, and is in the
 * process of being purged itself
 */
SysStatus
DirLinuxFSVolatile::revalidateWithParentLocked(DirLinuxFSVolatileRef par)
{
    AutoLock<DirLockType> al(&dirlock);
    (void) locked_checkDetachList();
    SysStatus rc = locked_revalidate();
    if (_FAILURE(rc) && _SGENCD(rc) == ESTALE) {
#ifdef DEBUG_ESTALE
	tassertMsg(0, "invoking eliminateStaleDir\n");
#else
	tassertWrn(0, "invoking eliminateStaleDir\n");
#endif // #ifdef DEBUG_ESTALE
	(void) eliminateStaleDir(par);
	return 0;
     }
    return rc;
}

/* destroy() is only invoked in situations where there are no more clients
 * attached and no more can be attached (file has
 * been either deleted or has became invalid/stale).
 */
/* virtual */ SysStatus
DirLinuxFSVolatile::destroy()
{
#if 0
    TraceOSFSDirLinuxFSVolatileDestroy((uval) getRef());
#endif

#if 0 // why do we need special code for volatile destroy?
    /* for debugging, let's check if the assumption that this method
     * is invoked at most once for each object */
    uval oldDestroy = FetchAndAddSignedVolatile(&doingDestroy, 1);
    tassertMsg(oldDestroy == 0, "Assumption doesn't hold!\n");
    // checking if there are no clients as we assume
    tassertMsg(isEmptyExportedXObjectList(),
	       "Assumption about no clients is false\n");

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    if (fileInfo != NULL) {
	fileInfo->destroy();
    }

#ifdef DILMA_DEBUG
    err_printf("DirLinuxFSVolatile invoking destroyUnchecked for %p\n",
	       getRef());
#endif

    // schedule the object for deletion
    destroyUnchecked();

#else
    return DirLinuxFS::destroy();
#endif
}

/* virtual */ SysStatus
DirLinuxFSVolatile::exportedXObjectListEmpty()
{
    dirlock.acquire();
    if (!validFileInfo()) {
	dirlock.release();
	destroy();
	return 0;
    } else {
	dirlock.release();
	return 0;
    }
}
