/*****************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004, 2005
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DirLinuxFS.C,v 1.206 2005/07/14 21:36:59 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Caches names of files
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <io/DirBuf.H>

#include <sys/ProcessSet.H>

// needed for definition of FileLinuxDir::LazyReOpenData
#include <io/FileLinuxDir.H>

// FIXME GCC3 -JX. should not need this.
#if __GNUC__ >= 3
#ifndef __USE_GNU
#define __USE_GNU
#endif /* #ifndef __USE_GNU */
#endif /* #if __GNUC__ >= 3 */

#include <io/FileLinux.H>
#include "ServerFile.H"
#include "DirLinuxFS.H"
#include <misc/StringTable.I>
#include "FSFileOther.H"

#include <trace/traceFS.h>

/* static */ NameHolderInfo::LockType DirLinuxFS::rootRWNHLock;

// a more expensive routine that if unconverted the 
// stat of the file to see if its really a director
static inline uval IsReallyADir(NameHolderInfo *nhi) {
    FileLinux::Stat status;
    SysStatus rc;
    if (nhi->isDirSF()) return 1;

    // if its converted, and don't know its a dir, done
    if (nhi->isConverted()) return 0;

    // okay, need to stat it
    rc = nhi->fsFile->getStatus(&status);
    tassert(_SUCCESS(rc), err_printf("woops\n"));
    if (status.isDir()) return 1;
    return 0;
}
    

SysStatus
DirLinuxFS::init(PathName *pathName, uval pathLen, FSFile *theInfo,
		 DirLinuxFSRef par)
{
    FileLinux::Stat status;
    SysStatus rc;

    rc = theInfo->getStatus(&status);
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    tassert((status.isDir()), err_printf("is not a directory\n"));

    dirPathName    = pathName;
    dirPathNameLen = pathLen;
    if (par != NULL) {
	parent = new SingleParent();
	parent->setTheRef(par);
    } else {
	parent = NULL;		// says is top directory
    }
    removalOnLastClose = 0; // not actually used for directories ...
    removed = 0;			// maa debug
    doingDestroy = 0;

    fileInfo = theInfo;

    useType = FileLinux::FIXED_SHARED;

    dirlock.init();

    CObjRootSingleRep::Create(this);

    entToFree.ref = (ServerFileRef)getRef();

#ifdef HACK_FOR_FR_FILENAMES
    nameAtCreation = NULL;
#endif //#ifdef HACK_FOR_FR_FILENAMES

    // following for debugging ... to differentiate from ServerFile
    fileLength = 0xdddddddd;

    return 0;
}

/* static */ SysStatus
DirLinuxFS::CreateTopDir(DirLinuxFSRef &dirLinuxRef, char *up, FSFile *fi)
{
    PathNameDynamic<AllocGlobal> *dirPathName;
    uval      dirPathLen;

    tassertMsg((up != NULL), "woops\n");
    dirPathLen = PathNameDynamic<AllocGlobal>::Create(
	up, strlen(up), 0, 0, dirPathName, PATH_MAX+1);

    fi->createDirLinuxFS(dirLinuxRef, dirPathName, dirPathLen, 0);
    //traceFS_ref2_str1(TRACE_FS_DIR_CREATED, (uval)dirLinuxRef, (uval)fi,
    //		      "**for root**");

    // FIXME: this should be per-name tree, not global
    // initialize static variable
    rootRWNHLock.init();

    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::mknod(char *remainder, uval remainderLen, uval mode, uval dev)
{
    SysStatus rc;
    NameHolderInfo nameHolderInfo;
    FileLinux::Stat status;

#ifdef MARIA_DEBUG_DIRLINUXFS
    err_printf("In %s  mode=0x%lx\n", __PRETTY_FUNCTION__, mode);
#endif // ifdef MARIA_DEBUG_DIRLINUXFS

    // note, default is lock for write
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    // FIXME dilma and copied by maria: check credentials to search in this 
    // directory

    // finds and creates sub-object if corresponding file exists
    rc = locked_lookupOrAdd(remainder, remainderLen, &nameHolderInfo, &status, 
			    0 /* not for open */, 
			    1 /* dir locked for write */);
    if (_SUCCESS(rc)) {
	// it already exits.  This is an error condition for mknod
	rc = _SERROR(2876, FileLinux::NODE_ALREADY_EXISTS, EEXIST);
    } else {
	// didn't find it, see if we can create
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    return rc;
	}
	FSFile *newNode;
	FileLinux::Stat stat;
	if (_SGENCD(rc) != ENOENT) return rc;

	// FIXME: check credentials ... for now only detecting read-only
	// filesystems
	if (fileInfo->isReadOnly()) {
	    return _SERROR(2890, 0, EROFS);
	}

	// create the file
	// fileInfo is of class FSFile.  The specialization of this
	// fileInfo we are holding is that appropriate for the directory in
	// which we are creating the node.

	if (S_ISFIFO(mode)|S_ISSOCK(mode)|S_ISREG(mode)) {
	    rc = fileInfo->createFile(remainder, remainderLen, mode,
				      &newNode, &stat);
	} else if (S_ISBLK(mode) || S_ISCHR(mode)) {
            /* block or character devices not implemented */
	    // fix me should this be a call to createServerFileBlock/Char??
	    // needs interface to pass device no.
	    rc = _SERROR(2470, 0, EOPNOTSUPP);
	} else {
	    rc = _SERROR(2880, 0, EINVAL);
	}
        _IF_FAILURE_RET(rc);

	#ifdef MARIA_DEBUG_DIRLINUXFS
	err_printf("stat "
        	"dev=%ld ino=%ld mode=0x%x hard link=%ld "
        	"uid=%d gid=%d  rdev=0x%lx size=%ld  blksize=%ld "
        	"block=%ld\n",
        stat.st_dev, stat.st_ino, stat.st_mode,
        stat.st_nlink, stat.st_uid, stat.st_gid,
        stat.st_rdev, stat.st_size, stat.st_blksize,
        stat.st_blocks);
	#endif // ifdef MARIA_DEBUG_DIRLINUXFS


	rc = locked_convert(remainder, remainderLen, newNode, &stat, 
			    &nameHolderInfo, NULL);
	_IF_FAILURE_RET_VERBOSE(rc);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::bind(char *remainder, uval remainderLen, uval mode,
	         ObjectHandle serverSocketOH)
{
    // fixme  make a _locked_mknod which returns stat
    // fixme.  ok you got the fs logic atomic but what about the interaction
    // with the socket server?
    SysStatus rc;
    NameHolderInfo nameHolderInfo;
    FileLinux::Stat status;

#ifdef MARIA_DEBUG_DIRLINUXFS
    err_printf("In %s  mode=0x%lx\n", __PRETTY_FUNCTION__, mode);
#endif // ifdef MARIA_DEBUG_DIRLINUXFS

    rc = mknod(remainder, remainderLen, mode, 0);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::NODE_ALREADY_EXISTS) &&
	    (_SGENCD(rc) == EEXIST)) {
	    return _SERROR(0, 0, EADDRINUSE);//fixme errno
	} else {
	    // failure due to something other than already existing node
	    return rc;
	}
    }

    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    // get the nameHolderInfo 
    rc = locked_lookupOrAdd(remainder, remainderLen, &nameHolderInfo, &status, 
			    1 /* for open */, 
			    1 /* dir locked for write */);

    tassertMsg(nameHolderInfo.isFSFile(), "oops, gotta make this all atomic");
    tassertMsg(nameHolderInfo.fsFile->getFSFileType() == FSFile::FSFILE_SOCK,
	       "how did we get a fs which is not a socket? (is %ld)\n", 
	       nameHolderInfo.fsFile->getFSFileType());

    // see getObj() for comment on locks
    
    nameHolderInfo.rwlock->acquireW();
    // release the dir lock which probably leads to horrible deadlocks
    rc = ((FSFileOtherSocket*)nameHolderInfo.fsFile)->bind(serverSocketOH);
    nameHolderInfo.rwlock->releaseW();
    _IF_FAILURE_RET_VERBOSE(rc);

    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::getSocketObj(char *remainder, uval remainderLen, 
		         ObjectHandle &serverSocketOH, ProcessID pid)
{
    SysStatus rc;
    NameHolderInfo nameHolderInfo;
    FileLinux::Stat status;

#ifdef MARIA_DEBUG_DIRLINUXFS
    err_printf("In %s\n", __PRETTY_FUNCTION__);
#endif // ifdef MARIA_DEBUG_DIRLINUXFS

    // FIXME: could be acquired for read
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    rc = locked_lookupOrAdd(remainder, remainderLen, &nameHolderInfo, &status, 
			    1 /* for open */, 
			    1 /* locked for write */);
    _IF_FAILURE_RET(rc);

    if ((!nameHolderInfo.isFSFile()) || 
	(nameHolderInfo.fsFile->getFSFileType() != FSFile::FSFILE_SOCK)) {
	return _SERROR(2906, 0, ECONNREFUSED);
    }

    nameHolderInfo.rwlock->acquireW();
    rc = ((FSFileOtherSocket*)nameHolderInfo.fsFile)
				->getSocketObj(serverSocketOH, pid);
    nameHolderInfo.rwlock->releaseW();

    return rc;
}

/* virtual */ SysStatusUval
DirLinuxFS::getFullRootName(PathName *pathName, uval maxPathLen)
{
    if (getPar() != NULL) {
	return DREF(getPar())->getFullRootName(pathName, maxPathLen);
    } else {
	return pathName->appendPath(0, maxPathLen, dirPathName, dirPathNameLen);
    }
}

/* virtual */ SysStatusUval
DirLinuxFS::getFullDirName(PathName *pathName, uval maxPathLen)
{
    SysStatusUval pathLen;

    if (getPar() != NULL) {
	pathLen = DREF(getPar())->getFullDirName(pathName, maxPathLen);
	if (_FAILURE(pathLen)) {
	    return pathLen;
	}
	pathLen = pathName->appendComp(_SGETUVAL(pathLen), maxPathLen,
				    dirPathName);
	return (pathLen);
    } else {
	return pathName->appendPath(0, maxPathLen, dirPathName, dirPathNameLen);
    }
}

// given a component name, it returns in fullPathName the full path
// name of the component. If the component is empty, it will return
// its full directory path followed by "/."
SysStatus
DirLinuxFS::getFullCompName(PathName* compName, uval compLen, uval maxLen,
			    PathName* fullPathName, uval &fullPathLen)
{
    uval tmpLen;
    SysStatus rc;

    rc = getFullDirName(fullPathName, maxLen);
    _IF_FAILURE_RET(rc);

    tmpLen = _SGETUVAL(rc);

    // FIXME: wouldn't it be equivalent (and simpler) to test for compLen==0?
    if (!(compName->isComp(compLen, compName))) {
	rc = fullPathName->appendComp(tmpLen, maxLen, ".", 1);
	_IF_FAILURE_RET(rc);
    } else {
	rc = fullPathName->appendPath(tmpLen, maxLen, compName, compLen);
	_IF_FAILURE_RET(rc);
    }
    fullPathLen = _SGETUVAL(rc);
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::locked_addComp(char  *name, uval len, uval8 flags, uval contents)
{
    _ASSERT_HELD_WRITE(dirlock);

    NameHolderInfo nh;
    nh.setFlags(flags);
    nh.fsFile = (FSFile*)contents;

    children.updateOrAdd(name, len, NULL, &nh);
    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::locked_lookup(char *name, uval namelen, NameHolderInfo *nhi)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::locked_lookup\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    _ASSERT_HELD(dirlock);

    SysStatus rc = children.lookup(name, namelen, nhi);

#ifdef GATHERING_STATS
    if (_SUCCESS(rc)) {
	fileInfo->incStat(FSStats::LOOKUP_SUCCESS);
    } else {
	fileInfo->incStat(FSStats::LOOKUP_FAILURE);
    }
#endif // #ifdef GATHERING_STATS

    return rc;
}

/* virtual*/ SysStatus
DirLinuxFS::locked_convert(char *name, uval len, FSFile *file, 
			   FileLinux::Stat *st, NameHolderInfo *resNHI, 
			   void *existingDentryListEntry)
{
    _ASSERT_HELD_WRITE(dirlock);
    // We assume resNHI != NULL
    tassertMsg(resNHI != NULL, "problem");

    SysStatus rc = 0;

    /*
     * okay, so I have a token to something different, check status
     * to see what it is
     */
    if (st->isDir()) {
	// create a directory object to represent this directory
	DirLinuxFSRef r;
	PathNameDynamic<AllocGlobal> *newPathName;
	uval        newPathLen;

	newPathLen = PathNameDynamic<AllocGlobal>::Create(name, len, 0, 0,
							  newPathName);

	rc = file->createDirLinuxFS(r, newPathName, newPathLen, getRef());
	if (_FAILURE(rc)) {
	    newPathName->destroy(newPathLen);
	    return rc;
	}

	resNHI->setDirSF(r);

	// make name NULL terminated for use in trace event
	//char bufname[PATH_MAX+1];
	//memcpy(bufname, name, len);
	//bufname[len] = '\0';
	//TraceOSFSDirCreated((uval)r, (uval)fileToken,
	//	       (uval) fileInfo, bufname);

	goto return_rc;
    } else if (st->isFile() || st->isBlock()) {
	// It has to have nlink == 1, otherwise we should have a ServerFile
	// for it already
	tassertMsg(st->st_nlink == 1, "expected nlink==1\n");

	ServerFileRef r;
	rc = file->createServerFileBlock(r);
	_IF_FAILURE_RET(rc);

	// make name NULL terminated for use in trace event
	//char bufname[PATH_MAX+1];
	//memcpy(bufname, name, len);
	//bufname[len] = '\0';
	tassertMsg( r, "woops\n");
	(void) DREF(r)->setDirLinuxRef(getRef());
	resNHI->setFileSF(r);

#ifdef HACK_FOR_FR_FILENAMES
	/* It is very rare that we want to add such big events as
	 * complete file names! For now let's leave this here as
	 * a template for how to get such things to print
	 */
	char fullname[512];
	SysStatus rcname;
	rcname = getStringFullDirName(name, len, fullname, sizeof(fullname));
	tassertMsg(_SUCCESS(rcname), "ops");
	DREF(r)->setNameAtCreation(fullname);
#endif //#ifdef HACK_FOR_FR_FILENAMES

	goto return_rc;
    } else if (st->isChar()) {
	ServerFileRef r;
	// I guess create and put it in tree, is this cool?
	rc = file->createServerFileChar(r);
	_IF_FAILURE_RET(rc);
	if (r) {
	    resNHI->setFileSF(r);
	} else {
	    // YUCK, making this look like other so 
	    // we don't keep calling convert.  FIXME
	    resNHI->setOther(file);
	}
	goto return_rc;
    } else if (st->isSymLink()) {
	resNHI->setSymLink(file);

        // KLUDGE error to say hit a symbolic link, class code 69
	rc = _SERROR(2669, FileLinux::HIT_SYMBOLIC_LINK, ECANCELED);
	goto return_rc;
    } else if (st->isFIFO()) {
	if (file->getFSFileType() == FSFile::FSFILE_PIPE) {
	    rc = 0;
	    tassertWrn(0, "YUCK, repeatdly trying to upgrade\n");
	    goto return_rc;
	}
	FSFileOtherPipe *pfsFile =  new FSFileOtherPipe(file);
	resNHI->setOther(pfsFile);
	rc = 0;
	goto return_rc;
    } else if (st->isSocket()) {
	if (file->getFSFileType() == FSFile::FSFILE_SOCK) {
	    rc = 0;
	    tassertWrn(0, "YUCK, repeatdly trying to upgrade\n");
	    goto return_rc;
	}
	tassertMsg( (file->getFSFileType() != FSFile::FSFILE_SOCK),
		    "oops\n");
	FSFileOtherSocket *sckFile = new FSFileOtherSocket(file);
	resNHI->setOther(sckFile);
	rc = 0;
	goto return_rc;
    }

    tassert(0, err_printf("unsupported file type\n"));
    rc = _SERROR(1433, 0, ENXIO);
    return rc;

 return_rc:
    children.updateOrAdd(name, len, existingDentryListEntry, resNHI);
    return rc;
}

SysStatus
DirLinuxFS::locked_lookupOrAdd(char *name, uval namelen,
			       NameHolderInfo *nameHolderInfo,
			       FileLinux::Stat *retStatus,
			       uval convert, uval lockedForWrite)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::locked_lookupOrAdd\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    tassertMsg(((lockedForWrite == 0 && dirlock.isLockedRead())
		|| (lockedForWrite == 1 && dirlock.isLockedWrite())),
		"lock not held as expected (lockedForWrite %ld)\n",
	       lockedForWrite);

    SysStatus rc = 0;
    FSFile *fsFile = NULL;
    ServerFileRef fileRef = NULL;
    FileLinux::Stat status, *statp;

    if (retStatus != NULL) {
	statp = retStatus;
    } else {
	statp = &status;
    }

    tassertMsg(namelen != 0, "a zero length name\n");

    SysStatus rcfound = locked_lookup(name, namelen, nameHolderInfo);

    if (_SUCCESS(rcfound)) { 

	// if going to open, and has not been opened, install it
	if ((convert) && (nameHolderInfo->isConverted() == 0))  {
	    if (lockedForWrite == 0) {
		return locked_retryLookupOrAdd(name, namelen, nameHolderInfo,
					       retStatus, convert);
	    }
	    _ASSERT_HELD_WRITE(dirlock);
	    rc = nameHolderInfo->fsFile->getStatus(statp);
	    if (_FAILURE(rc)) {
		locked_handleErrorOnToken(name, namelen, rc);
		return rc;
	    }
	    // we have the lock for writing, so it's safe to retrieve an entry
	    // pointer even in the case we're holding a data structure for
	    // DentryList that can grow and move entries around.
	    // We get an opaque pointer; we just want to be able to pass it to
	    // locked_convert, so it can ask the DentryList object
	    // to update the particular entry
	    void *existingEntry;
	    rc = children.lookup(name, namelen, existingEntry);
	    tassertMsg(_SUCCESS(rc) && existingEntry != NULL,
		       "it has to be there (rc 0x%lx)\n", rc);
	    rc = locked_convert(name, namelen, nameHolderInfo->fsFile, statp, 
				nameHolderInfo, existingEntry);
	} else if (retStatus != NULL) { // need to do a stat
	    if (nameHolderInfo->isFSFile()) {
		if (lockedForWrite == 0) {
		    return locked_retryLookupOrAdd(name, namelen,
						   nameHolderInfo,
						   retStatus, convert);
		}
		_ASSERT_HELD_WRITE(dirlock);
		rc = nameHolderInfo->fsFile->getStatus(statp);
		if (_FAILURE(rc)) {
		    locked_handleErrorOnToken(name, namelen, rc);
		}
	    } else {
#ifdef DEBUG_SERVER_FILE
		traceFS_ref1_str1(TRACE_FS_DEBUG_1UVAL_1STR,
				  (uval) nameHolderInfo->fref,
				  "will invoke getStatus");
		rc = DREF(nameHolderInfo->fref)->getStatus(statp);
#endif // #ifdef DEBUG_SERVER_FILE
	    }
	}
	// FIXME: this is a really gross kludge that happens to work
	// need to think out when we should be returning error, 
	// and where exactly
	if ((convert) && nameHolderInfo->isSymLinkFSFile()) {
	    rc = _SERROR(2909, FileLinux::HIT_SYMBOLIC_LINK, ECANCELED);
	}
	return rc;
    }

    if (lockedForWrite == 0) {
	return locked_retryLookupOrAdd(name, namelen, nameHolderInfo,
				       retStatus, convert);
    }
    _ASSERT_HELD_WRITE(dirlock);

    // didn't find it, so lookup in file system

    fsFile = NULL;
    MultiLinkMgrLock *tmplock;
    // returns back a server fiel if there are multiple links
    rc = fileInfo->getFSFileOrServerFile(name, namelen, &fsFile, fileRef,
					 tmplock, statp);
    _IF_FAILURE_RET(rc);

    if (fileRef != NULL) {
	// for the case of a file with hard links, its pre-converted
	// to be a ServerFile
	nameHolderInfo->setFileSF(fileRef);
	children.updateOrAdd(name, namelen, NULL, nameHolderInfo);

	(void) DREF(fileRef)->setDirLinuxRef(getRef());

	// release the lock received from getFSFileOrServerFile
	tmplock->release();
    } else if (convert == 0) {
	// its not converted, i.e., just an FSFile, don't know anythign about it
	nameHolderInfo->setFileFSFile(fsFile);
	children.updateOrAdd(name, namelen, NULL, nameHolderInfo);

    } else {
	
	// convert and insert it into children
	rc = locked_convert(name, namelen, fsFile, statp, nameHolderInfo, NULL);
    }

    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::locked_retryLookupOrAdd(char *name, uval namelen,
				    NameHolderInfo *nameHolderInfo,
				    FileLinux::Stat *retStatus,
				    uval convert)
{
    SysStatus rc;

#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::locked_retryLookupOrAdd\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    _ASSERT_HELD_READ(dirlock);
    upgradeLock();
    rc = locked_lookupOrAdd(name, namelen, nameHolderInfo, retStatus, convert, 
			    1 /*locked for write */);
    downgradeLock();
    _ASSERT_HELD_READ(dirlock);
    return rc;
}

/*
 * Called by DirLinuxFS::lookup, called external to object
 * dirref if sub-dir exists.
 * Parameters:
 * - name, namelen: identify the sub-directory being looked up;
 * - dr: dir ref returned if sub-dir exists;
 * - nhSubDirLock: is the lock of the NameHolder where sub-dir has been
 *   found (if it exists). The method acquires this lock before returning,
 *   thereby guaranteeing that the sub-directory for the object does not
 *   disappear before being released by the lookup method (once it uses it).
 *   After acquiring this lock, the method will release the directory lock.
 */

/* virtual */ SysStatus
DirLinuxFS::externalLookupDirectory(char *name, uval namelen,
				    DirLinuxFSRef &dr,
				    NameHolderInfo::LockType* &nhSubDirLock)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::externalLookupDirectory\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    SysStatus rc;
    acquireLockR();

    NameHolderInfo nameHolderInfo;
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo, NULL, 
			    1/*for open*/);
    if (_FAILURE(rc)) {
	releaseLockR();
	return rc;
    }

    if (nameHolderInfo.isDirSF()) {
	dr = nameHolderInfo.dref;
	nhSubDirLock = nameHolderInfo.rwlock;
	nameHolderInfo.rwlock->acquireR();
	releaseLockR();
	return 0;
    }

    releaseLockR();
    return _SERROR(1865, 0, ENOTDIR);

}

/* virtual */ SysStatus
DirLinuxFS::utime(const struct utimbuf *utbuf)
{
    /*
     * same definition as ServerFile, but cannot override just one
     * method of a particular name in C++ and still invoke as Directory
     */
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    if (fileInfo->isReadOnly()) {
	return _SERROR(2499, 0, EROFS);
    }

    // FIXME: we need to check permissions, something like Linux's
    //        inode_change_ok (easier to do that if we use getattr, setattr
    //        stuff)

    return  fileInfo->utime(utbuf);
}

/* virtual */ SysStatus
DirLinuxFS::utime(char *name, uval namelen, const struct utimbuf *utbuf)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::utime(name)\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    SysStatus rc;
    NameHolderInfo nameHolderInfo;

    acquireLockR();
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo);
    if (_FAILURE(rc)) { releaseLockR(); return rc; }

    // proceed under protection of nameHolder lock
    nameHolderInfo.rwlock->acquireW();
    releaseLockR();

    if (nameHolderInfo.isFSFile()) {
	rc = nameHolderInfo.fsFile->utime(utbuf);
	if (_FAILURE(rc)) { handleErrorOnToken(name, namelen, rc); }
    } else {
	// have a real file/dir to work on
	if (nameHolderInfo.isDirSF()) {
	    rc = DREF(nameHolderInfo.dref)->utime(utbuf);
	} else {
	    rc = DREF(nameHolderInfo.fref)->utime(utbuf);
	}
    }

    nameHolderInfo.rwlock->releaseW();
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::fchown(uid_t uid, gid_t gid)
{
    /*
     * same definition as ServerFile, but cannot override just one
     * method of a particular name in C++ and still invoke as Directory
     */
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    if (fileInfo->isReadOnly()) {
	return _SERROR(2500, 0, EROFS);
    }

    // FIXME: we need to check permissions, something like Linux's
    //        inode_change_ok (easier to do that if we use getattr, setattr
    //        stuff)

    return  fileInfo->fchown(uid, gid);
}

/* virtual */ SysStatus
DirLinuxFS::chown(char *name, uval namelen, uid_t uid, gid_t gid)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::chown(name)\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    SysStatus rc;
    NameHolderInfo nameHolderInfo;

    if (fileInfo->isReadOnly()) {
	return _SERROR(2501, 0, EROFS);
    }

    // FIXME: we need to check permissions, something like Linux's
    //        inode_change_ok (easier to do that if we use getattr, setattr
    //        stuff)

    acquireLockR();
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo);
    if (_FAILURE(rc)) { releaseLockR(); return rc; }

    // proceed under protection of nameHolder lock
    nameHolderInfo.rwlock->acquireW();
    releaseLockR();

    if (nameHolderInfo.isFSFile()) {
	rc = nameHolderInfo.fsFile->fchown(uid, gid);
	if (_FAILURE(rc)) { handleErrorOnToken(name, namelen, rc); }
    } else {
	// have a real file/dir to work on
	if (nameHolderInfo.isDirSF() == 1) {
	    rc = DREF(nameHolderInfo.dref)->fchown(uid, gid);
	} else {
	    rc = DREF(nameHolderInfo.fref)->fchown(uid, gid);
	}
    }

    nameHolderInfo.rwlock->releaseW();
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::mkdir(char *name, uval namelen, mode_t mode)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::mkdir\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    SysStatus rc;
    NameHolderInfo nameHolderInfo;

    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    if (fileInfo->isReadOnly()) {
	return _SERROR(2502, 0, EROFS);
    }

    // FIXME: we need to check permissions (invoke permission(mask))
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo, NULL, 
			    1 /*for open*/, 
			    1 /*lockec for write*/);

    if (_SUCCESS(rc)) {
	rc = _SERROR(2564, 0, EEXIST);
	return rc;
    }

    FSFile *newDirInfo;
    rc = fileInfo->mkdir(name, namelen, mode, &newDirInfo);
    _IF_FAILURE_RET(rc);

    nameHolderInfo.setFileFSFile(newDirInfo);

    rc = children.updateOrAdd(name, namelen, NULL, &nameHolderInfo);
    return rc;
}

/* virtual */ SysStatusUval
DirLinuxFS::readlink(char *name, uval namelen, char *buf, uval bufsize)
{
    SysStatus rc;
    NameHolderInfo nameHolderInfo;
    FileLinux::Stat stat;
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    // stat is not really necessary; we are using it for sanity check only
    // FIXME: we need to check permissions (invoke permission(mask))
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo, &stat, 
			    0 /* not for open */, 
			    1 /* locked for write*/);

    if (_FAILURE(rc)) {
	rc = _SERROR(2910, 0, EEXIST);
	return rc;
    }

    if (nameHolderInfo.isFSFile() && S_ISLNK(stat.st_mode)) {
	if (nameHolderInfo.symlink) {
	    rc = MIN(bufsize, nameHolderInfo.symlink->symlinkLength);
	    memcpy(buf, nameHolderInfo.symlink->symlinkValue, rc);
	} else {
	    rc = nameHolderInfo.fsFile->readlink(buf, bufsize);
	    if (_SUCCESS(rc)) {
		nameHolderInfo.symlink = (NameHolderInfo::SymlinkInfo *)
		    AllocGlobal::alloc(rc+sizeof(NameHolderInfo::SymlinkInfo));
		nameHolderInfo.symlink->symlinkLength = rc;
		memcpy(nameHolderInfo.symlink->symlinkValue, buf, rc);

		void *existingEntry;
		children.lookup(name, namelen, existingEntry);

		// Update the hashed nameHolderInfo
		children.updateOrAdd(name, namelen, existingEntry, 
				     &nameHolderInfo);
	    }
	}
    } else {
	// Can return error, since never create an object
	// to represent symbolic links
	rc = _SERROR(2678, 0, EINVAL);
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::symlink(char *name, uval namelen, char *oldpath)
{
    SysStatus rc;
    NameHolderInfo nameHolderInfo;

    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    if (fileInfo->isReadOnly()) {
	return _SERROR(2502, 0, EROFS);
    }

    // FIXME: we need to check permissions (invoke permission(mask))
    rc = locked_lookup(name, namelen, &nameHolderInfo);

    if (_SUCCESS(rc)) {
	rc = _SERROR(2651, 0, EEXIST);
	return rc;
    }

    rc = fileInfo->symlink(name, namelen, oldpath);
    _IF_FAILURE_RET(rc);

    // Again to finally add it to the cache
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo, NULL, 
			    1 /*convert*/, 
			    1 /*dir locked for write */);

    tassertMsg(nameHolderInfo.isSymLinkFSFile(), "Not a symlink? %s:%p\n",
	       name, &nameHolderInfo);

    // cache the symlink value for future use
    nameHolderInfo.symlink = (NameHolderInfo::SymlinkInfo *)
	AllocGlobal::alloc(strlen(oldpath) +
			   sizeof(NameHolderInfo::SymlinkInfo));
    nameHolderInfo.symlink->symlinkLength = strlen(oldpath);
    memcpy(nameHolderInfo.symlink->symlinkValue, oldpath,
	   nameHolderInfo.symlink->symlinkLength);

    void *existingEntry;
    children.lookup(name, namelen, existingEntry);

    // Update the hashed nameHolderInfo
    children.updateOrAdd(name, namelen, existingEntry, &nameHolderInfo);

    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::rmdir(char *name, uval namelen)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::rmdir\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    // FIXME: detect that directory is busy (should return EBUSY if
    // directory is the current working directory or root directory of a
    // process)

    SysStatus rc, rcfound;
    NameHolderInfo nameHolderInfo;

    /*
     * This function is unusual, in that it has to hold the lock both on the
     * container directory and the child directory, since the removal of a
     * directory involves atomically changing both to avoid races. Hence
     * lock held throughout this operation.
     */
    AutoLock<DirLockType> al(&dirlock);

    if (fileInfo->isReadOnly()) {
	return _SERROR(2503, 0, EROFS);
    }

    // FIXME: we need to check permissions

    rcfound = locked_lookup(name, namelen, &nameHolderInfo);

    if (_SUCCESS(rcfound)) {
	if (!IsReallyADir(&nameHolderInfo)) {
	    return _SERROR(1855, 0, ENOTDIR);
	}
	// if have object for the directory to be removed
	if (nameHolderInfo.isFSFile() == 0) {
	    // is the directory empty ?
	    // FIXME: we are holding the lock ... should it be released?
	    if (DREF(nameHolderInfo.dref)->isDirectoryCacheEmpty() != 1) {
		// can't remove, directory is not empty
		return  _SERROR(2567, 0, ENOTEMPTY);
	    }
	}
	nameHolderInfo.rwlock->acquireW();
	if (nameHolderInfo.isFSFile() == 0) {
	    // again, if have obj for directory being removed
	    rc = DREF(nameHolderInfo.dref)->rmdir(fileInfo, name, namelen);
	    tassertMsg(_SUCCESS(rc), "DirLinux::rmdir method failed\n");
	    goto return_rc;
	} else { // only have token for it
	    rc = fileInfo->rmdir(name, namelen);
	    if (_FAILURE(rc)) {
		goto return_rc;
	    }
	    rc = nameHolderInfo.fsFile->deleteFile();
	}
    } else { // nothing is known about the dir being deleted
	rc = fileInfo->rmdir(name, namelen);
    }

return_rc:
    if (_SUCCESS(rc)) {
	if (_SUCCESS(rcfound)) {
	    // FIXME: we're holding the directory lock, so we're the last one
	    // to acquire the lock, no need to release
	    nameHolderInfo.rwlock->releaseW();
	    // remove from children
	    children.remove(name, namelen);
	}
	// FIXME: Where are we actually removing the object? It shouldn't
	// go away while it has clients attached to it
    } else {
	if (_SUCCESS(rcfound)) {
	    nameHolderInfo.rwlock->releaseW();
	}
    }

    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::fchmod(mode_t mode)
{
    /*
     * same definition as ServerFile, but cannot override just one
     * method of a particular name in C++ and still invoke as Directory
     */
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    if (fileInfo->isReadOnly()) {
	return _SERROR(2504, 0, EROFS);
    }

    // FIXME: we need to check permissions, something like Linux's
    //        inode_change_ok (easier to do that if we use getattr, setattr
    //        stuff)

    // FIXME dilma: check credentials
    return  fileInfo->fchmod(mode);
}

/* virtual */ SysStatus
DirLinuxFS::chmod(char *name, uval namelen, mode_t mode)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::chmod\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    SysStatus rc;
    NameHolderInfo nameHolderInfo;

    acquireLockR();
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo);
    if (_FAILURE(rc)) { releaseLockR(); return rc; }

    // proceed under protection of nameHolder lock
    nameHolderInfo.rwlock->acquireW();
    releaseLockR();

    if (nameHolderInfo.isFSFile()) {
	// do under protection of my lock
	rc = nameHolderInfo.fsFile->fchmod(mode);
	if (_FAILURE(rc)) { handleErrorOnToken(name, namelen, rc); }
    } else {
	// have a real file/dir to work on
	if (nameHolderInfo.isDirSF() == 1) {
	    rc = DREF(nameHolderInfo.dref)->fchmod(mode);
	} else {
	    rc = DREF(nameHolderInfo.fref)->fchmod(mode);
	}
    }

    nameHolderInfo.rwlock->releaseW();
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::truncate(char *name, uval namelen, off_t length)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::truncate\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    SysStatus rc;
    NameHolderInfo nameHolderInfo;
    ServerFileRef fileRef;

    acquireLockR();

    /*
     * force creation of ServerFile if don't have one to avoid having
     * to hold directory lock for a long time, and because we are likely
     * to access truncated file in a short while
     */
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo, NULL, 1);
    if (_FAILURE(rc)) {
	releaseLockR();
	return rc;
    }

    // proceed under protection of nameHolder lock
    nameHolderInfo.rwlock->acquireW();
    releaseLockR();

    // now we must have a real object to talk to, important
    if (nameHolderInfo.isDirSF()) {
	tassertWrn(0, "can't truncate a dir\n");
	rc = _SERROR(1689, 0, EISDIR);
	goto return_rc;
    }
    // we know about this file; do a ftruncate on it
    fileRef = nameHolderInfo.fref;

    rc = DREF(fileRef)->ftruncate(length);

return_rc:
    nameHolderInfo.rwlock->releaseW();
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::getStatus(char *name, uval namelen, FileLinux::Stat *retStatus,
		      uval followLink)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    char buf[PATH_MAX+1];
    memcpy(buf, name, namelen);
    buf[namelen] = '\0';
    err_printf("In DirLinuxFS::getStatus(name %s)\n", buf);
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    SysStatus rc;
    NameHolderInfo nameHolderInfo;

    // FIXME: check for credentials

    acquireLockR();
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo);
    if (_FAILURE(rc)) {
	releaseLockR();
	return rc;
    }

    // proceed under protection of nameHolder lock
    nameHolderInfo.rwlock->acquireW();
    releaseLockR();

    if (nameHolderInfo.isFSFile()) {
	rc = nameHolderInfo.fsFile->getStatus(retStatus);
	if (_FAILURE(rc)) { handleErrorOnToken(name, namelen, rc); }

	if (retStatus->isSymLink()) {
	    if (followLink) {
		// KLUDGE error to say hit a symbolic link, class code 69
		rc = _SERROR(2767, FileLinux::HIT_SYMBOLIC_LINK, ECANCELED);
	    }
	}
    } else {
	// have a real file/dir to work on
	if (nameHolderInfo.isDirSF() == 1) {
	    rc = DREF(nameHolderInfo.dref)->getStatus(retStatus);
	} else {
#ifdef INSTRUMENTING_FILE_SHARING
	    SysStatusUval nc = DREF(nameHolderInfo.fref)->numberClientsExcFR();
	    tassertMsg(_SUCCESS(nc), "numberClientsExcFR() failed\n");
	    TraceOSFSNameTreeAccess(
			   (uval) nameHolderInfo.fref, Scheduler::SysTimeNow(),
			   _SGETUVAL(nc), "getStatus");
#endif // ifdef INSTRUMENTING_FILE_SHARING
	    rc = DREF(nameHolderInfo.fref)->getStatus(retStatus, 1);
	}
    }

    nameHolderInfo.rwlock->releaseW();

#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("Out of getStatus(name)\n");
#endif // #ifdef DILMA_DEBUG_DIRLINUXFS

    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::link(char *oldname, uval oldnamelen, DirLinuxFSRef newDirRef,
		 char *newname, uval newnamelen)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::link\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    FileLinux::Stat status;
    SysStatus rc, rctmp;
    ServerFileRef fileRef;

    /* Determine order to acquire locks (to avoid deadlock if we have
     * a concurrent link from new to old */
    DirLinuxFSRef oldDirRef = getRef();
    if (oldDirRef == newDirRef) {
	acquireLockW();
    } else if (oldDirRef < newDirRef) {
	acquireLockW();
	rc = DREF(newDirRef)->acquireLockW();
	tassertMsg(_SUCCESS(rc), "acquireLock for newDirRef failed\n");
    } else {
	rc = DREF(newDirRef)->acquireLockW();
	tassertMsg(_SUCCESS(rc), "acquireLock for newDirRef failed\n");
	acquireLockW();
    }

    NameHolderInfo oldNameHolderInfo, newNameHolderInfo;
    // Force creation of a ServerFile for this entry if there isn't one
    rc = locked_lookupOrAdd(oldname, oldnamelen, &oldNameHolderInfo,
			    &status, 1, 1);
    if (_FAILURE(rc)) {
	/* return_rc assumes that the source directory lock is only hold
	 * to the end if source and target directory are the same */
	if (oldDirRef != newDirRef) releaseLockW();
	goto return_rc;
    }

    // proceed under protection of nameHolder lock
    oldNameHolderInfo.rwlock->acquireW();
    if (oldDirRef != newDirRef) { // if same dir, we should hold lock
	releaseLockW();
    }

    if (status.isDir()) {		// can't link to directory
	oldNameHolderInfo.rwlock->releaseW();
	rc = _SERROR(1766, 1, EPERM);
	goto return_rc;
    }

    // if target is the same as source, return EEXIST
    if (oldDirRef == newDirRef && oldnamelen == newnamelen
	&& strncmp(oldname, newname, oldnamelen) == 0) {
	oldNameHolderInfo.rwlock->releaseW();
	rc = _SERROR(2101, 0, EEXIST);
	goto return_rc;
    }

    rc = DREF(newDirRef)->locked_lookup(newname, newnamelen,
					&newNameHolderInfo);

    if (_SUCCESS(rc)) {
	oldNameHolderInfo.rwlock->releaseW();
	rc = _SERROR(2102, 0, EEXIST);
	goto return_rc;
    } else {
	if (_SGENCD(rc) != ENOENT) {
	    /* locked_lookup returns 0 or error ENOENT, so the error has to be
	       related to the PPC invocation ?? */
	    tassertMsg(0, "look at failure");
	    oldNameHolderInfo.rwlock->releaseW();
	    goto return_rc;
	}
    }

    fileRef = oldNameHolderInfo.fref;
    tassert( (fileRef != NULL), err_printf("woops\n"));
    FSFile *newDirFileInfo;
    rc = DREF(newDirRef)->getFSFile(&newDirFileInfo);
    tassertMsg(_SUCCESS(rc), "ops\n");

    rc = DREF(fileRef)->link(newDirFileInfo, newname, newnamelen, newDirRef);

    oldNameHolderInfo.rwlock->releaseW();
    if (_SUCCESS(rc)) {
	// add nameHolder for file in newDirRef
	rctmp = DREF(newDirRef)->locked_addComp(newname, newnamelen, 
						0, (uval)fileRef);
	tassertMsg(_SUCCESS(rctmp), "ops\n");
    }

return_rc:
    if (oldDirRef != newDirRef) {
	rctmp = DREF(newDirRef)->releaseLockW();
	tassertMsg(_SUCCESS(rctmp), "releaseLock for newDirRef failed\n");
    } else {
	// the lock is being held only if it's same dir for source and target
	releaseLockW();
    }
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::unlink(char *name, uval namelen)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::unlink\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    // FIXME: check permissions: for now only detecting read-only filesystems
    if (fileInfo->isReadOnly()) {
	return _SERROR(2505, 0, EROFS);
    }

    SysStatus rc = 0, rcfound;
    NameHolderInfo nameHolderInfo;

    /*
     * This function is unusual, in that it has to hold the lock both on the
     * container directory and the child file, since the removal of a
     * file involves atomically changing both to avoid races. Hence
     * lock held throughout this operation.
     */
    AutoLock<DirLockType> al(&dirlock);

    rcfound = locked_lookup(name, namelen, &nameHolderInfo);

    if (_SUCCESS(rcfound)) {		// got something
	if (IsReallyADir(&nameHolderInfo)) {
	    return _SERROR(2565, 0, EISDIR);
	}
	// make sure on going use of this nameHolder finishes
	nameHolderInfo.rwlock->acquireW();
	if (nameHolderInfo.isFSFile() == 0) {	// have file to get rid of
	    rc = DREF(nameHolderInfo.fref)->unlink(fileInfo, name, namelen,
						   getRef());
	    // since we're holding the directory lock, we are guaranteed to be
	    // the last one with access to this nameHolder
	    nameHolderInfo.rwlock->releaseW();
	    SysStatus rctmp = children.remove(name, namelen);
	    tassertMsg(_SUCCESS(rctmp), "child to be deleted disappeared\n");
#if 1 // just for debugging
	    SysStatus rctmp2 = locked_lookup(name, namelen, &nameHolderInfo);
	    tassertMsg(_FAILURE(rctmp2), "how come?");
#endif
	    return rc;
	} else {
	    FSFile *fi = nameHolderInfo.fsFile;
	    // since we're holding the directory lock, we are guaranteed to be
	    // the last one with access to this nameHolder
	    nameHolderInfo.rwlock->releaseW();
	    SysStatus rctmp = children.remove(name, namelen);
	    tassertMsg(_SUCCESS(rctmp), "child to be deleted disappeared\n");
	    rc = fileInfo->unlink(name, namelen, nameHolderInfo.fsFile);
	    tassertMsg(_SUCCESS(rc), "rc is 0x%lx\n", rc);

	    // get rid of the fileInfo for this entry, we don't want to leak
	    // FSFiles
	    fi->destroy();

	    return rc;
	}
    }
    rc = fileInfo->unlink(name, namelen);
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::rename(char *oldname, uval oldnamelen, DirLinuxFSRef newDirRef,
		   char *newname, uval newnamelen)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::rename\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    // FIXME: check permissions: for now only detecting read-only filesystems
    if (fileInfo->isReadOnly()) {
	return _SERROR(2506, 0, EROFS);
    }

    SysStatus rc, rctmp;
    /* Determine order to acquire locks (to avoid deadlock if we have
     * a concurrent rename from new to old */
    uval oldnewOrder = 1;
    DirLinuxFSRef oldDirRef = getRef();
    if (oldDirRef == newDirRef) {
	acquireLockW();
	if (oldnamelen == newnamelen
	    && strncmp(oldname, newname, oldnamelen) == 0) {
	    // rename source and target are the same, nothing to do, just return EINVAL
	    releaseLockW();
	    return _SERROR(2927, 0, EINVAL);
	}
    } else if (oldDirRef < newDirRef) {
	acquireLockW();
	rc = DREF(newDirRef)->acquireLockW();
	tassertMsg(_SUCCESS(rc), "acquireLock for newDirRef failed\n");
    } else {
	oldnewOrder = 0;
	rc = DREF(newDirRef)->acquireLockW();
	tassertMsg(_SUCCESS(rc), "acquireLock for newDirRef failed\n");
	acquireLockW();
    }

    // info for file being renamed
    FSFile *oldfinfo = NULL;

    NameHolderInfo oldNameHolderInfo, newNameHolderInfo;

    /* FIXME FIXME FIXME FIXME: We're doing a locked_lookup for oldname and newname
     * without a requirement of actually creating objects for these elements if
     * they are not there. This has the advantage of not wasting time creating objects
     * that are being moved, but it has the HUGE disadvantage of not allowing for
     * all necessary error  checking here: if we are doing a rename of an unknown
     * file/dir into an unknown file/dir, we rely on the file system to do all error
     * checking! I think we should do "locked_lookupOrAdd" and eliminate all the
     * checcking for SUCCESS(rcOld) or _SUCCESS(rcNew).
     */

    SysStatus rcOld, rcNew;
    rcOld = locked_lookup(oldname, oldnamelen, &oldNameHolderInfo);

    rcNew = DREF(newDirRef)->locked_lookup(newname, newnamelen,
					   &newNameHolderInfo);
    if (_FAILURE(rcNew) && _SGENCD(rcNew) != ENOENT) {
	// locked_lookup always returned 0 or error ENOENT,
	// so the error has to come from
	// the fact that the invocation is being done through DREF ...
	passertMsg(0, "DREF(newDirRef)->locked_lookup() returned unexpected "
		   "error\n");
	rc = rcNew;
	goto return_rc;
    }

    // checking for error conditions
    /* Fixme: we're only checking stuff if we know already about the file; if not
     * we assuming that the file system will check the errors! */
    if (_SUCCESS(rcNew) && IsReallyADir(&newNameHolderInfo)) {
	// if new name is a dir, old name has also to be
	if (_SUCCESS(rcOld) && !IsReallyADir(&oldNameHolderInfo)) {
	    rc = _SERROR(2097, 0, EISDIR);
	    goto return_rc;
	}

	// if new name is a dir, it has to be empty
	rc = DREF(newNameHolderInfo.dref)->isDirectoryCacheEmpty();
	if (_SGETUVAL(rc) != 1) {
	    // can't remove, directory is not empty
	    rc =  _SERROR(2098, 0, ENOTEMPTY);
	    goto return_rc;
	}
    }

    // more error conditions
    /* check if we're trying to make a directory a subdirectory
     * of itself */
    if (_SUCCESS(rcOld) && IsReallyADir(&oldNameHolderInfo)) {
	char oldString[PATH_MAX+1], newString[PATH_MAX+1];
	SysStatus rctmp = getStringFullDirName(oldname, oldnamelen,
					       oldString, PATH_MAX);
	tassertMsg(_SUCCESS(rctmp), "rctmp 0x%lx\n", rctmp);
	rctmp = DREF(newDirRef)->getStringFullDirName(newname, newnamelen,
						      newString, PATH_MAX);
	tassertMsg(_SUCCESS(rctmp), "rctmp 0x%lx\n", rctmp);
	if (strncmp(oldString, newString, strlen(oldString)) == 0
	    && newString[strlen(oldString)] == '/') {
	    rc = _SERROR(2926, 0, EINVAL);
	    goto return_rc;
	}
    }

    // locking is needed to make sure ongoing work on these name holders has
    // finished
    if (_SUCCESS(rcOld)) {
	oldNameHolderInfo.rwlock->acquireW();
	if (oldNameHolderInfo.isFSFile() != 0) {
	    oldfinfo = oldNameHolderInfo.fsFile;
	} else {
	    rc = DREF(oldNameHolderInfo.fref)->getFSFile(&oldfinfo);
	    tassertMsg(_SUCCESS(rc), "ops");
	}
    }
    if (_SUCCESS(rcNew)) {
	newNameHolderInfo.rwlock->acquireW();
    }

    FSFile *newDirFileInfo;
    rc = DREF(newDirRef)->getFSFile(&newDirFileInfo);
    tassertMsg(_SUCCESS(rc), "ops\n");

    if (_SUCCESS(rcNew)) {
	if (newNameHolderInfo.isFSFile() == 0) {
	    if (IsReallyADir(&newNameHolderInfo) == 0) {
		/* We're holding directory lock, so there is no point where
		 * a user could find neither the old nor the new name.
		 * But rename will need to update information (eg nlink) for
		 * this file (which is being replaced), so we need to guarantee
		 * no one else can get to it */
		rc = DREF(newNameHolderInfo.fref)->acquireLock();
		tassertMsg(_SUCCESS(rc), "ops\n");
	    } else {
		/* FIXME: deal with scenario where directory to be replaced
		 * is BUSY, i.e., it has users now */
	    }
	}
    }

    // don't commit
    if (newnamelen == oldnamelen && oldDirRef == newDirRef
	&& strncmp(oldname, newname, oldnamelen) == 0) { 
	err_printf("both names equal 0\n");
    }
    // perform operation
    rc = fileInfo->rename(oldname, oldnamelen, newDirFileInfo, newname,
			  newnamelen, oldfinfo);

    // start clean up
    if (_SUCCESS(rc) && _SUCCESS(rcNew)) {
	if (newNameHolderInfo.isFSFile() == 0) {
	    if (IsReallyADir(&newNameHolderInfo) == 0) {
		SysStatus rctmp2 = DREF(newNameHolderInfo.fref)->
		    locked_returnUnlocked_cleanForDeletion(newDirRef);
		tassertMsg(_SUCCESS(rctmp2), "?");
	    } else {
		// FIXME: we have to destroy directory objects also!
	    }
	}
    }

    if (_SUCCESS(rcOld)) {
	oldNameHolderInfo.rwlock->releaseW();
    }
    if (_SUCCESS(rcNew)) {
	newNameHolderInfo.rwlock->releaseW();
    }

    /* NameHolder locks have been released, but directory are still locked,
     * so no one can access the nameHolders
     */

    if (_FAILURE(rc)) goto return_rc;

    if (_SUCCESS(rcOld)) {
	// If we had it cached in the old place, let's get the cache moved
	// to the new place. For cache entry with isFSFile == 1, moving is
	// an optimization, but for isFSFile == 0 it's mandatory, otherwise
	// the file/directory object becomes orphan (therefore inaccessible)
	rctmp = locked_deleteChild(oldname, oldnamelen);
	tassertMsg(_SUCCESS(rctmp), "child to be deleted disappeared\n");
	if (_SUCCESS(rcNew)) {
	    // before adding new entry, get rid of this one
	    rctmp = DREF(newDirRef)->locked_deleteChild(
		newname, newnamelen);
	    tassertMsg(_SUCCESS(rctmp), "child to be deleted disappeared\n");
	}
	// create one, and put the right stuff there.
	rctmp = DREF(newDirRef)->locked_addComp(
	    newname, newnamelen, oldNameHolderInfo.getFlags(),
	    (uval) oldNameHolderInfo._obj);
	// locked_addComp returns 0, so if failure, it's from DREF
	tassertMsg(_SUCCESS(rctmp), "call failure\n");

	if (oldNameHolderInfo.isFSFile() == 0) { // real objects
	    if (oldNameHolderInfo.isDirSF() == 1) {
		rctmp = DREF(oldNameHolderInfo.dref)->changeParent(newDirRef);
		// the method always returns 0, so if it failed, is due to DREF
		tassertMsg(_SUCCESS(rctmp), "shouldn't fail\n");
	    } else {
		rctmp = DREF(oldNameHolderInfo.fref)->renameSetParents
		    (oldDirRef, newDirRef);
		// FIXME: deal with failure?
		tassertMsg(_SUCCESS(rctmp), "why failed?\n");
	    }
	}
    }

return_rc:
    if (oldDirRef == newDirRef) {
	releaseLockW();
    } else if (oldnewOrder == 1) {
	rctmp = DREF(newDirRef)->releaseLockW();
	tassertMsg(_SUCCESS(rctmp), "releaseLock for newDirRef failed\n");
	releaseLockW();
    } else {
	releaseLockW();
	rctmp = DREF(newDirRef)->releaseLockW();
	tassertMsg(_SUCCESS(rctmp), "releaseLock for newDirRef failed\n");
    }
    return rc;

}

/* virtual */ SysStatus
DirLinuxFS::open(uval oflag, ProcessID pid, ObjectHandle &oh,
		 uval &uType, TypeID &type)
{
    // FIXME: check credentials

    XHandle xh;
    SysStatus rc;

    uType = FileLinux::SHARED;
    type = FileLinux_DIR;

    rc = CheckOpenFlags(oflag, 1, 0);
    if (_FAILURE(rc)) return rc;

    // FIXME get rights correct
    rc = giveAccessByServer(oh, pid, (AccessRights)-1, MetaObj::none);
    if (_FAILURE(rc)) {
	return rc;
    }

    // now get xhandle and initialize the fields not filled up by ClientData
    // constructor
    xh = oh._xhandle;
    Clnt(xh)->flags = oflag;

    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::getObj(char *name, uval namelen, uval oflag, mode_t mode,
		   ProcessID pid, ObjectHandle &oh, uval &uType,
		   TypeID &type,
		   /* argument for simplifying gathering traces of
		    * file sharing information. This should go away. */
		   ObjRef &fref)
{
    SysStatus rc;
    NameHolderInfo nameHolderInfo;
    FileLinux::Stat status;
    uval isnew = 0;

    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    // FIXME dilma: check credentials to search in this directory

    // finds and creates sub-object if corresponding file exists
    rc = locked_lookupOrAdd(name, namelen, &nameHolderInfo, &status, 1, 1);
#ifdef MARIA_DEBUG_DIRLINUXFS
    uval debugfifo = 0;
    if (_SUCCESS(rc)&&
        (S_ISFIFO(status.st_mode)||S_ISSOCK(status.st_mode))) {
	debugfifo = 1;
        err_printf("%s  good rc from locked_lookupOrAdd, mode=0x%x\n",
		 __PRETTY_FUNCTION__, status.st_mode);
    }
    if (debugfifo) {
	err_printf("nhi isDir=%ld  isFSFile=%ld  islink=%ld\n",
	nameHolderInfo.isDirSF(), nameHolderInfo.isFSFile(),
	nameHolderInfo.isSymLinkFSFile());
    }
#endif // ifdef MARIA_DEBUG_DIRLINUXFS

    if (_FAILURE(rc)) { // didn't find it, see if we should create
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    return rc;
	}
	FSFile *newFile;
	FileLinux::Stat stat;
	if (!((_SGENCD(rc) == ENOENT) && (oflag & O_CREAT))) return rc;

	// FIXME: check credentials ... for now only detecting read-only
	// filesystems
	if (fileInfo->isReadOnly()) {
	    return _SERROR(2507, 0, EROFS);
	}

	// create the file
	isnew = 1;
	rc = fileInfo->createFile(name, namelen,
				  (mode & ~S_IFMT) | S_IFREG,
				  &newFile, &stat);
	if (_FAILURE(rc)) { return rc; }

	rc = locked_convert(name, namelen, newFile, &stat, &nameHolderInfo, 
			    NULL);
	_IF_FAILURE_RET(rc);
    }

    rc = CheckOpenFlags(oflag, IsReallyADir(&nameHolderInfo), isnew);
#ifdef MARIA_DEBUG_DIRLINUXFS
    if (debugfifo) {
	err_printf("%s: from CheckOpenFlags ", __PRETTY_FUNCTION__);
	_SERROR_EPRINT(rc);
    }
#endif // ifdef MARIA_DEBUG_DIRLINUXFS
    _IF_FAILURE_RET(rc);

    // Even holding the directory lock (which is enough to guarantee that
    // the fref can not go away) we still need the nameHolder lock because
    // ServerFile::open() may invoke ftruncate (oflag may involve O_TRUNC),
    // and if there is a ftruncate going one, it should finish before we
    // open (ServerFile serializes it, but it may serialize in a way that
    // open truncate returns a file with size != 0)
    if (nameHolderInfo.isFSFile()) {
	// path for open of fifo comes here and fails.  This
	// can fix fixed by different logic here to check fifo
	// before isFSFile, possibly chaging isFIFO to something
	// more generic, or by converting in convert to object.
	// I don't understand why we don't just do that and move
	// the special logic there.
	ServerFileRef ref;
	nameHolderInfo.rwlock->acquireW();
	rc = nameHolderInfo.fsFile->openCreateServerFile(ref, oflag, pid, oh,
							 uType, type);
	nameHolderInfo.rwlock->releaseW();
	fref = (ObjRef) ref;
    } else {
	nameHolderInfo.rwlock->acquireW();
	rc = DREF(nameHolderInfo.fref)->open(oflag, pid, oh, uType, type);
	nameHolderInfo.rwlock->releaseW();

	/* argument for simplifying gathering traces of
	 * file sharing information. This should go away. */
	fref = (ObjRef) nameHolderInfo.fref;
    }

#ifdef MARIA_DEBUG_DIRLINUXFS
    if (debugfifo) {
	err_printf("%s: returning ", __PRETTY_FUNCTION__);
	_SERROR_EPRINT(rc);
    }
#endif // ifdef MARIA_DEBUG_DIRLINUXFS
    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::lookup(PathName *pathName, uval pathLen,
		   char *&remainder, uval &remainderLen,
		   DirLinuxFSRef &dirLinuxRef, RWBLock* &lockRef)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::lookup\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    // FIXME: check credentials

    PathName *pathComp;  // Work with this one
    PathName *pathCompNext = pathName;
    uval compLen;
    uval compNextLen = pathLen;
    SysStatus rc;

    tassertMsg((getPar() == 0), "this should only be called on top dir\n");

    DirLinuxFSRef currDLRef = getRef();

    // We always lock the nameholder corresponding to a directory while invoking
    // externalLookupDirectory(), thereby guaranteeing that the DirLinuxFS
    // object doesn't go away as we request its service. The root is a special
    // case, since it doesn't correspond to a nameHolder object in its
    // (inexistent) parent. We have a class variable for it.
    NameHolderInfo::LockType *nhParentLock = &rootRWNHLock;
    nhParentLock->acquireR();
    RWBLock *nhSubDirLock;

    // Handle "./" or "./<file>"
    if (pathName->isLastComp(pathLen) ||	// root is the container
	// root is it's own container
	(!(pathName->isComp(pathLen, pathName)))) {
	dirLinuxRef = currDLRef;
	lockRef = nhParentLock;
	remainder = pathName->getCompName(pathLen);
	remainderLen = pathName->getCompLen(pathLen);
	return 0;
    }

    // we stop at the container directory so don't process the
    // last component
    do {
	DirLinuxFSRef nextDir;
	pathComp = pathCompNext;
	compLen = compNextLen;

	rc = DREF(currDLRef)->externalLookupDirectory(
	    pathComp->getCompName(compLen), pathComp->getCompLen(compLen),
	    nextDir, nhSubDirLock);
	if (_FAILURE(rc)) {
	    nhParentLock->releaseR();
	    return rc;
	}
	currDLRef = nextDir;
	pathCompNext = pathComp->getNext(compLen, compNextLen);

	// unlock nameholder for previous currDLRef. The nameholder for
	// nextDir has been returned locked by externalLookupDirectory()
	nhParentLock->releaseR();
	nhParentLock = nhSubDirLock;
    } while (!(pathCompNext->isLastComp(compNextLen)));

    remainder = pathCompNext->getCompName(compNextLen);
    remainderLen = pathCompNext->getCompLen(compNextLen);

    dirLinuxRef = currDLRef;
    lockRef = nhParentLock;

    return rc;
}

/* virtual */ SysStatusUval
DirLinuxFS::getDirName(char *buf, uval bufLen, PathName *&pathName)
{
    /* name is const so no locking needed
     * assume rename of directory will close existing and reopen
     */
    if (!getPar()) {
	// starting it
	dirPathName->dup(dirPathNameLen, buf, bufLen);
	pathName = (PathName *)buf;
	return dirPathNameLen;
    }

    SysStatusUval rcUval = DREF(getPar())->getDirName(buf, bufLen, pathName);
    if (!_SUCCESS(rcUval)) {
	return rcUval;
    }

    uval tmpPathLen = _SGETUVAL(rcUval);

    return pathName->appendPath(tmpPathLen, bufLen, dirPathName, dirPathNameLen);
}

/* virtual */ uval
DirLinuxFS::isDirectoryCacheEmpty()
{
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return
    return children.isEmpty();
}

/* static */ SysStatus
DirLinuxFS::CheckOpenFlags (uval oflag, uval isDir, uval isNew)
{
    if (!isNew) {
    	// handle O_EXCL
	if ((oflag & O_CREAT) && (oflag &O_EXCL)) {
	    return  _SERROR(1756, 0, EEXIST);
	}
    }
    if (isDir) {
	if (oflag & O_TRUNC) {
	    return _SERROR(1755, 0, EISDIR);
	}
	uval accmode = O_ACCMODE & oflag;
	if ((accmode == O_WRONLY) || (accmode == O_RDWR)) {
	    return _SERROR(1741, 0, EISDIR);
	}
    } else {
	// handle O_DIRECTORY (Linux specific)
	if (oflag & O_DIRECTORY) {
	    return _SERROR(2568, 0, ENOTDIR);
	}
    }

    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::giveAccessSetClientData(ObjectHandle &oh, ProcessID toProcID,
				    AccessRights match,
				    AccessRights nomatch, TypeID type)
{
    SysStatus retvalue;
    ClientData *clientData = new ClientData();
    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
			      type, (uval)clientData);
    clientData->useType = FileLinux::FIXED_SHARED;
    return (retvalue);
}

/* virtual */ SysStatusUval
DirLinuxFS::_getDents(char *buf, uval len, __XHANDLE xhandle, __CALLER_PID pid)
{
    // FIXME: check credentials

    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return
    SysStatusUval retvalue;

    if (len < sizeof(*buf)) {
	return _SERROR(1714, 0, EINVAL);
    }
    uval cookie;
    if (Clnt(xhandle)->isSharingOffset) {
	cookie = Clnt(xhandle)->sharedOffRef->filePosition;
    } else {
	cookie = Clnt(xhandle)->filePosition;
    }
    retvalue = fileInfo->getDents(cookie, (struct direntk42*)buf, len);
    if (Clnt(xhandle)->isSharingOffset) {
	Clnt(xhandle)->sharedOffRef->filePosition = cookie;
    } else {
	Clnt(xhandle)->filePosition = cookie;
    }

    return (retvalue);
}

/* virtual */ SysStatusUval
DirLinuxFS::_setFilePosition(sval pos, uval at, __XHANDLE xhandle)
{
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    ClientData *cl = Clnt(xhandle);
    uval *cookieAddr;
    if (cl->isSharingOffset) {
	cookieAddr = &(cl->sharedOffRef->filePosition);
    } else {
	cookieAddr = &(cl->filePosition);
    }
    switch (at) {
    case FileLinux::ABSOLUTE:
	*cookieAddr = pos;
	break;
	// It is known that this results in garbage garbage
    case FileLinux::RELATIVE:
	*cookieAddr += pos;
	break;
    case FileLinux::APPEND:
    {
	SysStatus rc;
	FileLinux::Stat status;
	rc = fileInfo->getStatus(&status);
	tassertMsg(_SUCCESS(rc), "?");
	*cookieAddr = status.st_size + pos;
    }
    	break;
    default:
	tassertMsg(0, "Illegal seek type.\n");
	break;
    }

    if (*cookieAddr < 0) {
	return _SERROR(2584, 0, EINVAL);
    }
    return _SRETUVAL(*cookieAddr);
}

/* virtual */ SysStatus
DirLinuxFS::getStatus(FileLinux::Stat *status)
{
    // FIXME: check credentials
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

#ifdef DEBUG_SERVER_FILE
    traceFS_ref1_str1(TRACE_FS_DEBUG_1UVAL_1STR,
		      (uval) this, "DirLinuxFS::getStatus");
#endif // #ifdef DEBUG_SERVER_FILE

    return fileInfo->getStatus(status);
}

/* static */ void
DirLinuxFS::BeingFreed(XHandle xhandle)
{
    ClientData *clientData = Clnt(xhandle);
    if (clientData->isSharingOffset) {
	clientData->releaseSharingOffset();
    }

    delete clientData;
}

/* virtual */ SysStatus
DirLinuxFS::statfs(struct statfs *buf)
{
    // FIXME: check credentials
    tassertMsg((getPar() == 0), "this should only be called on top dir\n");
    return fileInfo->statfs(buf);
}

/* virtual */ SysStatus
DirLinuxFS::sync()
{
    // FIXME: check credentials
    tassertMsg((getPar() == 0), "this should only be called on top dir\n");
    return fileInfo->sync();
}

// FIXME: invoked only by NameTreeLinuxFSVirtFile, so we could define
//        in a subclass
/* virtual */ SysStatus
DirLinuxFS::createVirtFile(char *name, uval namelen, mode_t mode,
			   ObjectHandle vfoh)
{
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return
    return fileInfo->createFile(name, namelen, mode, vfoh);
}

/* virtual */ SysStatus
DirLinuxFS::tryToDestroy()
{
    // for now, just put back on list
    // tassertMsg((entToFree.ref == getRef()), "free entry not initialized\n");
    // DREF(fileSystem)->freeServerFile(&entToFree);
    return 0;
}

/* detachChild(ServerFileRef fref) is part of destruction protocols for files.
 * The directory object interacts with the file to make sure it is in a
 * state it can go away.
 * This method returns 1 if the file is successfully removed from the
 * cached children, 0 if the file can't be removed now, and an error (<0)
 * if the ServerFileRef does not appear in the children list
 */
/* virtual */ SysStatusUval
DirLinuxFS::detachChild(ServerFileRef fref)
{
#ifdef DILMA_DEBUG_DIRLINUXFS
    err_printf("In DirLinuxFS::detachChild\n");
#endif // ifdef DILMA_DEBUG_DIRLINUXFS

    uval ret; // compiler is complaining about reaching end without ret value
    AutoLock<DirLockType> al(&dirlock);
    NameHolderInfo nameHolderInfo;
    SysStatus rctmp = children.remove((ObjRef)fref, &nameHolderInfo);
    _IF_FAILURE_RET(rctmp);

    // wait all other current users of this nameHolder to finish
    nameHolderInfo.rwlock->acquireW();

    SysStatusUval rc = DREF(nameHolderInfo.fref)->detachParent(getRef());
    if (_SUCCESS(rc)) {
	if (_SGETUVAL(rc) == 1) { // detachParent went ok
	    nameHolderInfo.rwlock->releaseW();
	    ret = 1;
	} else {
	    tassertMsg(_SGETUVAL(rc)==0, "something wrong\n");
	    ret = 0;
	}
    } else {
	tassertMsg(0, "detachParent shouldn't return error\n");
	ret = rc;
    }

    return rc;
}

/* virtual */ SysStatus
DirLinuxFS::_lazyGiveAccess(__XHANDLE xhandle,
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

    tassertMsg(type == FileLinux_DIR, "wrong type\n");
    passertMsg(dataLen == sizeof(FileLinuxDir::LazyReOpenData), "wrong len");

    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return
    // go a giveacessfromserver on object to kernel, passing same rights
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    rc=giveAccessByServer(oh, _KERNEL_PID, match, nomatch);
    if (_FAILURE(rc)) {
	return rc;
    }

    XHandle newxh = oh._xhandle;

    // get process from xhandle
    procID = XHandleTrans::GetOwnerProcessID(xhandle);
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(procID, pref);
    if (_FAILURE(rc)) {
	DREFGOBJ(TheXHandleTransRef)->free(xhandle);
	return rc;
    }
    /* The parent client may go away, but we still want to keep the file position
     * information around to be used on lazyReOpen */
    ClientData *cdata = Clnt(xhandle);
    FileLinuxDir::LazyReOpenData *d =
	(FileLinuxDir::LazyReOpenData *) data;
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
DirLinuxFS::_lazyReOpen(__out ObjectHandle & oh,
			__in ProcessID toProcID,
			__in AccessRights match,
			__in AccessRights nomatch,
			__inoutbuf(datalen:datalen:datalen) char *data,
			__inout uval& datalen,
			__XHANDLE xhandle)
{
    XHandle xh;
    SysStatus rc;

    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    // FIXME get rights correct
    rc = giveAccessByServer(oh, toProcID, match, nomatch);
    if (_FAILURE(rc)) {
	passertMsg(0, "look at failure\n");
	return rc;
    }

    tassertMsg(datalen == sizeof(FileLinuxDir::LazyReOpenData),
	       "bad datalen %ld\n", datalen);
    FileLinuxDir::LazyReOpenData *ldata = (FileLinuxDir::LazyReOpenData*)data;
    ClientData::SharedOffsetData *shdata = (ClientData::SharedOffsetData*)
	ldata->shdata;
    tassertMsg(shdata->usageCount >= 1, "?");

    xh = oh._xhandle;
    Clnt(xh)->flags = ldata->openFlags;
    Clnt(xh)->sharedOffRef = shdata;
    Clnt(xh)->isSharingOffset = 1;
    shdata->lock.acquire();
    shdata->usageCount++;
    shdata->lock.release();

    return 0;
}

// if name != NULL, we append name into the dir name
/* virtual */ SysStatus
DirLinuxFS::getStringFullDirName(char *name, uval len,
				 char *fullname, uval maxnamelen)
{
    PathNameDynamic<AllocGlobal> *dPathName;
    SysStatusUval dirPathLen;
    dirPathLen = PathNameDynamic<AllocGlobal>::Create("", 0, 0, 0,
						      dPathName,
						      PATH_MAX+1);
    passertMsg(dirPathLen == 0, "how come?\n");
    dirPathLen = getFullDirName(dPathName, PATH_MAX+1);
    passertMsg(_SUCCESS(dirPathLen), "ops\n");
    SysStatusUval fullnamelen;
    fullnamelen = dPathName->getUPath(dirPathLen, fullname, maxnamelen, 0);
    passertMsg(_SUCCESS(fullnamelen), "ops\n");
    passertMsg(_SGETUVAL(fullnamelen)+len < maxnamelen, "not enough space\n");
    if (name) {
	uval pos = _SGETUVAL(fullnamelen);
	if (pos != 2) {
	    fullname[pos-1] = '/';
	} else {
	    pos--;
	}
	memcpy(&fullname[pos], name, len);
	fullname[pos+len] = '\0';
    }
    dPathName->destroy(PATH_MAX+1);

    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::rmdir(FSFile *dirInfo, char *name, uval namelen)
{
    // FIXME: add permission check

    lock.acquire();
    SysStatus rc = dirInfo->rmdir(name, namelen);
    _IF_FAILURE_RET(rc);

    // FIXME: we're not checking if other processes have this directory open!

    rc = locked_returnUnlocked_deletion();
    tassertMsg(_SUCCESS(rc), "Not dealing with errors\n");

    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::destroy()
{
/* for debugging, let's check if the assumption that this method
     * is invoked at most once for each object */
    uval oldDestroy = FetchAndAddSignedVolatile(&doingDestroy, 1);
#ifdef DILMA_DEBUG
    tassertMsg(oldDestroy==0, "ServerFilBlock::destroy(): Assumption of "
	       "destroy called no more than once doesn't hold!\n");
#else
    tassertWrn(oldDestroy==0, "ServerFilBlock::destroy(): Assumption of "
	       "destroy called no more than once doesn't hold!\n");
    if (oldDestroy != 0) {
	return 0;
    }
#endif // #ifdef DILMA_DEBUG

    // FIXME: need to invoke closeExportedXObjectList

/*
    if (fileInfo != NULL) {
	fileInfo->destroy();
 	fileInfo = NULL;
    }
*/

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    // FIXME: clean up client data?

    // FIXME destroy path name

    children.destroy();

    destroyUnchecked();

    return 0;
}

/* virtual */ SysStatus
DirLinuxFS::_dup(ProcessID pid, ObjectHandle &oh, __XHANDLE origXhandle)
{
    AutoLock<DirLockType> al(&dirlock); // locks now, unlocks on return

    ClientData *dupFromXh = Clnt(origXhandle);
    uval currCookie;
    if (dupFromXh->isSharingOffset == 1) {
	currCookie = dupFromXh->sharedOffRef->filePosition;
    } else {
	currCookie = dupFromXh->filePosition;
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
	dupFromXh->sharedOffRef->filePosition = currCookie;
	shdata->lock.acquire();
	shdata->usageCount++;
	shdata->lock.release();
    } else {
	shdata = new ClientData::SharedOffsetData(currCookie
);
	shdata->usageCount++;
	dupFromXh->sharedOffRef = shdata;
	dupFromXh->isSharingOffset = 1;
	Clnt(xh)->sharedOffRef = shdata;
    }

    Clnt(xh)->isSharingOffset = 1;

    return 0;
}

#include <defines/template_bugs.H>
#ifdef EXPLICIT_TEMPLATE_INSTANTIATION
// TEMPLATE INSTANTIATION
template
  NameHolder *
  STE<NameHolder>::
  allocEntry(uval16);
template
  NameHolder *
  STE<NameHolder>::
  allocEntry(NameHolder *);
template
  void
  STE<NameHolder>::
  init(uval16, uval16);
template
  void
  STE<NameHolder>::
  init(NameHolder *, uval16);
template
  _StrTable<NameHolder> *
  _StrTable<NameHolder>::
  Init(NameHolder *, void *, uval16);
template
  NameHolder *
  _StrTable<NameHolder>::
  allocEntry(NameHolder *);
template
  NameHolder *
  _StrTable<NameHolder>::
  allocEntry(uval16);
template
  SysStatus
  _StrTable<NameHolder>::
  deleteEntryWithPtr(NameHolder *);
template
  NameHolder *
  _DynamicStrTable<NameHolder,AllocGlobal>::
  allocEntry(uval16);
template
  _StrTable<NameHolder> *
  _DynamicStrTable<NameHolder,AllocGlobal>::
  copyEntries(void *, uval);
template
  void
  _DynamicStrTable<NameHolder,AllocGlobal>::
  doubleSize(void);
template
  void
  _DynamicStrTable<NameHolder,AllocGlobal>::
  init(NameHolder *);
#endif /* #ifdef EXPLICIT_TEMPLATE_INSTANTIATION */

