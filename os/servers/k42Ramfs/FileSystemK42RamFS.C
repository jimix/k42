/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemK42RamFS.C,v 1.49 2005/04/14 22:51:48 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <misc/baseStdio.H>
#include <io/FileLinux.H>
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaFileSystemK42RamFS.H>
#include <scheduler/Scheduler.H>
#include <sys/ppccore.H>
#include <sys/time.h>
#include <sys/types.h>
#include <fslib/NameTreeLinuxFS.H>
#include <fslib/PagingTransportPA.H>
#include "FileSystemK42RamFS.H"
#include "FileInfoK42RamFS.H"
#include "ServerFileBlockK42RamFS.H"
#include "ServerFileDirK42RamFS.H"
#include <trace/traceFS.h>

#include <stub/StubKernelPagingTransportPA.H>

#include <fslib/FileSystemList.H>
/* static */ FileSystemList FileSystemK42RamFS::instances;

static ThreadID BlockedThread = Scheduler::NullThreadID;

/* static */ void
FileSystemK42RamFS::Block()
{
    BlockedThread = Scheduler::GetCurThread();
    while (BlockedThread != Scheduler::NullThreadID) {
	// NOTE: this object better not go away while deactivated
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
}

FileSystemRef
FileSystemK42RamFS::init()
{
    SysStatus rc;
    FileSystemRef fsRef;

    FileInfoK42RamFSDir *rootToken;
    rootToken = new FileInfoK42RamFSDir(
	    (mode_t) 0777,
	    0 /* useCredential*/); /* for this directory it's not safe to
				    * use credentials, because we may be
				    * started from baseServers, prior to
				    * proper ProcessLinux initialization */

    if (rootToken == NULL) return NULL;
    rc = rootToken->add(".", 1, rootToken);
    if (_FAILURE(rc)) return NULL;
    rc = rootToken->add("..", 2, rootToken);
    if (_FAILURE(rc)) return NULL;
    root = (FileToken) rootToken;

    // create the clustered object for the file system
    fsRef = (FileSystemRef)CObjRootSingleRep::Create(this);
    return fsRef;
}

/* static */ SysStatus
FileSystemK42RamFS::ClassInit(VPNum vp)
{
    if (vp != 0) {
	return 0;	// nothing to do proc two
    }

    MetaFileSystemK42RamFS::init();
    PagingTransportPA::ClassInit(0);

    return 0;
}

/* static */ SysStatus
FileSystemK42RamFS::Create(VPNum vp, char *mpath, uval isCoverable /* = 1 */)
{
    DirLinuxFSRef dir;
    FileSystemRef fsRef;

    if (vp != 0) {
	return 0;
    }

    FileSystemK42RamFS *ramfs;
    ramfs = new FileSystemK42RamFS();

    fsRef = ramfs->init();
    if (fsRef == NULL) {
	delete ramfs;
	return _SERROR(2255, 0, ENOMEM);
    }

    FSFile *fi = new FSFileK42RamFS((FileSystemRef)fsRef,
				    (FileToken)ramfs->root,
				    ramfs->getStatistics());
    DirLinuxFS::CreateTopDir(dir, ".", fi);

    // description of mountpoint
    char tbuf[64];
    sprintf(tbuf, "ramfs pid 0x%lx",
	    _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

    NameTreeLinuxFS::Create(mpath, dir, tbuf, strlen(tbuf), isCoverable);

    instances.add((ObjRef)fsRef, mpath);

    ObjectHandle fsptoh;
    SysStatus rc = PagingTransportPA::Create(ramfs->tref, fsptoh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    // asks the kernel to create a KernelPagingTransport

    ObjectHandle kptoh, sfroh;
    rc = StubKernelPagingTransportPA::_Create(fsptoh, kptoh, sfroh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    DREF(ramfs->tref)->setKernelPagingOH(kptoh, sfroh);

    err_printf("ramfs started with pid 0x%lx\n",
	       _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::freeFileToken(FileToken fileToken)
{
    delete FINF(fileToken);
    return 0;
}

SysStatus
FileSystemK42RamFS::lookup(FileToken dirToken, char *entryName, uval entryLen,
			   FileToken *entryToken)
{
    /* it used to be that if we didn't find in the cache kept by the
     * file system independent layer, than we would find it here also.
     * But now we have symbolic links that are not being cached yet
     * in the fslib layer. */
    FileInfoK42RamFSDir *di = FileSystemK42RamFS::DINF(dirToken);
    tassertMsg(di->status.isDir(), "?");
    FileInfoK42RamFSDir::DirEntry *entry = di->lookup(entryName, entryLen);
    if (entry == NULL) {
	return _SERROR(2256, 0, ENOENT);
    } else {
	/* it has to be a symbolic link */
	FileInfoK42RamFS *finfo = entry->GetFinfo();
	passertMsg(S_ISLNK(finfo->status.st_mode), "?");
	*entryToken = (FileToken) finfo;
	return 0;
    }
}

// if pathNameTo is provided, lookup file before that
SysStatus
FileSystemK42RamFS::lookup(PathName *pathName, uval pathLen,
			   PathName *pathNameTo, FileToken *retToken)
{
    PathName *currentName = pathName;
    PathName *endName;
    char buf[PATH_MAX+1];
    FileToken rtoken, entryToken;

    if (pathNameTo) {
	endName = pathNameTo;
    } else {
	endName = (PathName*)(uval(pathName) + pathLen);
    }
    tassertMsg((currentName <= endName), "currentName > endName");

    SysStatus rc;

    rtoken = root;

    uval currentNameLen = pathLen;
    while (currentName < endName) {
	currentNameLen = currentName->getCompLen(currentNameLen);
	memcpy(buf, currentName->getCompName(currentNameLen), currentNameLen);
	buf[currentNameLen] = 0;

	rc = lookup(rtoken, buf, currentNameLen, &entryToken);
	_IF_FAILURE_RET(rc);

	rtoken = entryToken;
	currentName = currentName->getNext(currentNameLen);
    }

    *retToken = rtoken;

    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::fchown(FileToken fileToken, uid_t uid, gid_t gid)
{
    FileLinux::Stat *stat = &FINF(fileToken)->status;
    stat->st_uid = uid;
    stat->st_gid = gid;
    stat->st_ctime = time(0);
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::chown(FileToken dirToken, char *entryName,
			  uval entryLen, uid_t uid, gid_t gid)
{
    passertMsg(0, "NIY\n");
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::fchmod(FileToken fileToken, mode_t mode)
{
    FileLinux::Stat *stat = &FINF(fileToken)->status;
    mode_t fmt = stat->st_mode & S_IFMT;
    stat->st_mode = fmt|mode;
    stat->st_ctime = time(0);
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::chmod(FileToken dirToken, char *entryName,
			  uval entryLen, mode_t mode)
{
    passertMsg(0, "NIY\n");
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::ftruncate(FileToken fileToken, off_t length)
{
    FileLinux::Stat *stat = &FINF(fileToken)->status;
    stat->st_size = length;
    stat->st_ctime = stat->st_mtime = time(0);
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::truncate(FileToken dirToken, char *entryName,
			     uval entryLen, off_t length)
{
    passertMsg(0, "NIY\n");
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::link(FileToken oldFileInfo, FileToken newDirInfo,
			 char *newName,  uval newLen, ServerFileRef fref)
{
    FileInfoK42RamFSDir *di = DINF(newDirInfo);
    if (di->status.isDir()) {
	SysStatus rc;
	FileInfoK42RamFSDir::DirEntry *entry = di->lookup(newName, newLen);
	if (entry == NULL) {
	    rc = di->add(newName, newLen, (FileInfoK42RamFS*) oldFileInfo);
	    _IF_FAILURE_RET(rc);
	} else {
	    return _SERROR(2151, 0, EEXIST);
	}
    } else {
	// FIXME: this tassertWrn is for debugging only
	tassertWrn(0,
		   "In FileSystemK42RamFS::link with dinfo not a directory\n");
        return _SERROR(2467, 0, ENOTDIR);
    }

    FileLinux::Stat *stat = &FINF(oldFileInfo)->status;
    stat->st_nlink++;
    if (fref != NULL && stat->st_nlink == 2) {
	// just became multiple link
	MultiLinkManager::SFHolder *href = MultiLinkManager::AllocHolder(fref);
	multiLinkMgr.add((uval)(stat->st_ino), href);
    }

    // FIXME: do we need to update newDirInfo ctime?
    FINF(newDirInfo)->status.st_mtime = stat->st_ctime = time(0);
    return 0;
}

/* removes the name from directory entry */
/* virtual */ SysStatus
FileSystemK42RamFS::unlink(FileToken dirToken, char *name, uval namelen,
			   FileToken ftoken /* = INVTOK */,
			   uval *nlinkRemain /* = NULL */)
{
#if 0
    err_printf("In unlink with dirToken 0x%ld\n", dirToken);
#endif

    if (ftoken == INVTOK) {
	// file was not being cached in file system independent layer, so
	// it does not exist!
	return _SERROR(2411, 0, ENOENT);
    }

    FileInfoK42RamFSDir *di = DINF(dirToken);
    if (di->status.isDir()) {
	FileInfoK42RamFSDir::DirEntry *dentry = di->lookup(name, namelen);
	if (dentry == NULL) {
	    return _SERROR(2261, 0, ENOENT);
	}
	FileInfoK42RamFS *finfo = dentry->GetFinfo();
	if (finfo->status.isFile() || finfo->status.isSymLink()
	    || finfo->status.isFIFO() || finfo->status.isSocket()) {
	    uval ret = di->remove(name, namelen);
	    tassertMsg(ret == 1, "?");
	    // No need to delete dentry ... remove took care of that
	} else {
	    tassertMsg(dentry->GetFinfo()->status.isDir(), "?");
	    return _SERROR(2425, 0, EISDIR);
	}
    } else {
	// FIXME: this tassertWrn is for debugging only
	tassertWrn(0,
		   "In FileSystemK42RamFS::unlink with dinfo not directory\n");
        return _SERROR(2262, 0, ENOTDIR);
    }

    FileLinux::Stat *stat = &FINF(ftoken)->status;

    stat->st_nlink--;
    if (stat->st_nlink == 1) { // it was multilinked, now single link
	multiLinkMgr.remove(stat->st_ino);
    }

    if (nlinkRemain != NULL) {
	*nlinkRemain = stat->st_nlink;
    }

    FINF(dirToken)->status.st_mtime = stat->st_ctime = time(0);
    return 0;
}

/* free data blocks */
/* virtual */ SysStatus
FileSystemK42RamFS::deleteFile(FileToken fileToken)
{
    passertMsg(FINF(fileToken)->status.st_nlink == 0, "?");
    // token will be explicity removed on ServerFile destruction
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::rename(FileToken oldDirInfo, char *oldName, uval oldLen,
			   FileToken newDirInfo, char *newName, uval newLen,
			   FileToken renamedFinfo)
{
#if 0
    err_printf("In rename with oldDirInfo 0x%ld newDirInfo 0x%ld "
	       "renamedFinfo 0x%lx\n", oldDirInfo, newDirInfo, renamedFinfo);
#endif

    FileInfoK42RamFSDir *dold = DINF(oldDirInfo);
    tassertMsg(dold->status.isDir(), "?");
    FileInfoK42RamFSDir *dnew = DINF(newDirInfo);
    tassertMsg(dnew->status.isDir(), "?");

    FileInfoK42RamFSDir::DirEntry *oldDentry, *newDentry;
    oldDentry = dold->lookup(oldName, oldLen);
    if (oldDentry == NULL) { // not found
	return _SERROR(2412, 0, ENOENT);
    }
    newDentry = dnew->lookup(newName, newLen);

    // checking for errors: if new name is a dir, old name has also to be
    if (newDentry != NULL && newDentry->GetFinfo()->status.isDir()) {
	if (!oldDentry->GetFinfo()->status.isDir()) {
	    return _SERROR(2413, 0, EISDIR);
	}

	/* FIXME: check if we're trying to make a directory a subdirectory
	 * of itself */

        // if new name is a dir, it has to be empty
	FileInfoK42RamFSDir *dtmp = (FileInfoK42RamFSDir*) newDentry->_obj;
	if (!dtmp->isEmpty()) {
	    return _SERROR(2414, 0, ENOTEMPTY);
	}
    }

    uval ret;
    ret = dold->remove(oldName, oldLen);
    tassertMsg(ret == 1, "?");
    if (newDentry) {
	// This is safe in terms of concurrency because the file system
	// independent layer has locked the object if there is one
	// (if there're multiple paths to get to this object,
	// we're guaranteed to have an object representing it)
	if (!newDentry->GetFinfo()->status.isDir()) {
	    newDentry->GetFinfo()->status.st_nlink--;
	}
	tassertMsg(newDentry->GetFinfo()->status.st_nlink >= 0, "?");
	ret = dnew->remove(newName, newLen);
	tassertMsg(ret == 1, "?");
    }
    ret = dnew->add(newName, newLen, oldDentry->GetFinfo());
    tassertMsg(_SUCCESS(ret), "?");

    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::mkdir(FileToken dirToken, char *compName,
			  uval compLen, mode_t mode, FileToken *newDirInfo)
{
    FileInfoK42RamFSDir *di = FileSystemK42RamFS::DINF(dirToken);
    if (di->status.isDir()) {
	FileInfoK42RamFSDir::DirEntry *entry = di->lookup(compName, compLen);
	if (entry == NULL) {
	    SysStatus rc;
	    FileInfoK42RamFSDir *ninfo = new FileInfoK42RamFSDir(mode);
	    if (ninfo == NULL) {
		return _SERROR(2269, 0, ENOMEM);
	    }
	    rc = ninfo->add(".", 1, ninfo);
	    _IF_FAILURE_RET(rc);
	    rc = ninfo->add("..", 2, di);
	    _IF_FAILURE_RET(rc);
	    rc = di->add(compName, compLen, ninfo);
	    _IF_FAILURE_RET(rc);
	    di->status.st_nlink++;
	    *newDirInfo = (FileToken) ninfo;
#if 0
	    char buf[PATH_MAX+1];
	    memcpy(buf, compName, compLen);
	    buf[compLen] = '\0';
	    err_printf("created directory %s with ino 0x%lx\n", buf,
		       ninfo->status.st_ino);
#endif
	} else {
	    return _SERROR(2259, 0, EEXIST);
	}
    } else {
	// FIXME: this tassertWrn is for debugging only
	tassertWrn(0,
		   "In FileSystemK42RamFS::mkdir with dinfo not a directory\n");
        return _SERROR(2260, 0, ENOTDIR);
    }
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::rmdir(FileToken dirToken, char *name, uval namelen)
{
    FileInfoK42RamFSDir *di = DINF(dirToken);
    if (di->status.isDir()) {
	FileInfoK42RamFSDir::DirEntry *dentry = di->lookup(name, namelen);
	if (dentry == NULL) {
	    return _SERROR(2266, 0, ENOENT);
	}
	if (dentry->GetFinfo()->status.isDir()) {
	    FileInfoK42RamFSDir *subdir = (FileInfoK42RamFSDir*) dentry->_obj;
	    if (subdir->isEmpty()) {
		uval ret = subdir->prepareForRemoval();
		tassertMsg(ret == 1, "?");
		// The remove below gets hids of the dentry object
		ret = di->remove(name, namelen);
		tassertMsg(ret == 1, "?");
		delete subdir;
	    } else {
		return _SERROR(2263, 0, ENOTEMPTY);
	    }
	} else {
	    return _SERROR(2267, 0, ENOTDIR);
	}
    } else {
	// FIXME: this tassertWrn is for debugging only
	tassertWrn(0,
		   "In FileSystemK42RamFS::unlink with dinfo not a directory\n");
        return _SERROR(2264, 0, ENOTDIR);
    }

    FileLinux::Stat *stat = &FINF(dirToken)->status;
    stat->st_mtime = stat->st_ctime = time(0);
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::utime(FileToken fileToken, const struct utimbuf *utbuf)
{
    FileLinux::Stat *stat = &FINF(fileToken)->status;
    if (utbuf == NULL) {
	time_t now = time(NULL);
	stat->st_atime = (u_int) now;
	stat->st_mtime = (u_int) now;
    } else {
	stat->st_atime = (u_int)utbuf->actime;
	stat->st_mtime = (u_int)utbuf->modtime;
    }
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::utime(FileToken dirToken, char *entryName, uval entryLen,
			  const struct utimbuf *utbuf)
{
    passertMsg(0, "NIY");
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::createFile(FileToken dirToken, char *name, uval namelen,
			       mode_t mode, FileToken *ftoken,
			       FileLinux::Stat *status)
{
    FileInfoK42RamFSDir *di = DINF(dirToken);
    if (di->status.isDir()) {
	// We know that file doesn't exist, otherwise the file system
	// independent layer would have found it!
	SysStatus rc;
	FileInfoK42RamFS *finfo = NULL;
	if (S_ISREG(mode)) {
	   finfo = new FileInfoK42RamFS(mode);
	} else if (S_ISFIFO(mode)) {
	    finfo = new FileInfoK42RamFSPipe(mode);
	} else if (S_ISSOCK(mode)) {
	    finfo = new FileInfoK42RamFSSocket(mode);
	} else {
	    passertMsg(0, "unknown file type? mode %o\n", mode);
	}
	if (finfo == NULL) {
	    return _SERROR(2270, 0, ENOMEM);
	}
	rc = di->add(name, namelen, finfo);
	_IF_FAILURE_RET(rc);
	*ftoken = (FileToken) finfo;
	if (status != NULL) {
	    memcpy(status, &finfo->status, sizeof(FileLinux::Stat));
	}
#if 0
	char buf[PATH_MAX+1];
	memcpy(buf, name, namelen);
	buf[namelen] = '\0';
	err_printf("createFile for %s ftoken %p\n", buf, finfo);
#endif
    } else {
	// FIXME: this tassertWrn is for debugging only
	tassertWrn(0, "In FileSystemK42RamFS::createFile with dinfo not "
		   "directory\n");
	return _SERROR(2471, 0, ENOTDIR);
    }

    return 0;
}

/* virtual */ SysStatusUval
FileSystemK42RamFS::getDents(FileToken dirToken, uval &cookie,
			     struct direntk42 *buf, uval len)
{
#if 0
    err_printf("In FileSystemK42RamFS::getDents with cookie %ld\n", cookie);
#endif

    FileInfoK42RamFSDir *di = DINF(dirToken);
    if (di->status.isDir()) {
	return di->getDents(cookie, buf, len);
    } else {
	tassertWrn(0, "getDents invoked for non dir\n");
	return _SERROR(2468, 0, ENOTDIR);
    }
}

// create a server file object to represent this block file
/* virtual */ SysStatus
FileSystemK42RamFS::createServerFileBlock(ServerFileRef &fref, FSFile *fsFile)
{
    ObjectHandle oh;
    SysStatus rc = DREF(tref)->getKptoh(oh);
    tassertMsg(_SUCCESS(rc) && oh.valid(), "?");
    return ServerFileBlockK42RamFS::Create(fref, fsFile, oh);
}

// FIXME: get rid of this function (Note: VFS has an open ...)
/* virtual */ SysStatus
FileSystemK42RamFS::open(PathName *pathName, uval pathLen, uval flags,
			 mode_t mode, FileToken ftoken)
{
    passertMsg(0, "NIY\n");
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::createDirLinuxFS(DirLinuxFSRef &rf,
				     PathName *pathName, uval pathLen,
				     FSFile *fsFile, DirLinuxFSRef par)
{
#if 0
    // fix assert
    tassertMsg(S_ISDIR(FINF(token)->status.st_mode), "not a dir.\n");
#endif

    SysStatus retvalue;
    retvalue = ServerFileDirK42RamFS::Create(rf, pathName, pathLen, fsFile,
					     par);
    return (retvalue);
}

/* virtual */ SysStatus
FileSystemK42RamFS::getStatus(FileToken token, FileLinux::Stat *status)
{
    memcpy(status, &FINF(token)->status, sizeof(FileLinux::Stat));
    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::getFileTokenOrServerFile(
    FileToken dirToken, char *entryName, uval entryLen,
    FileToken &entryInfo, ServerFileRef &ref,
    MultiLinkMgrLock* &lock,
    FileLinux::Stat *status)
{
    /* it used to be that if we didn't find in the cache kept by the
     * file system independent layer, than we would find it here also.
     * But now we have symbolic links that are not being cached yet
     * in the fslib layer. */
    SysStatus rc = lookup(dirToken, entryName, entryLen, &entryInfo);
    _IF_FAILURE_RET(rc);
    ref = NULL;
    if (status) {
	(void) getStatus(entryInfo, status);
    }
    // return _SERROR(2258, 0, ENOENT);  // put this back when we start caching
    return 0;
}

/* static */ SysStatus
FileSystemK42RamFS::_TestAlive(char *mpath, uval len)
{
    FileSystemK42RamFSRef fsref =
	    (FileSystemK42RamFSRef) instances.find(mpath, len);
    if (fsref) {
	return 0;
    } else {
	return _SERROR(2776, 0, ENOENT);
    }
}

/* static */ SysStatus
FileSystemK42RamFS::_PrintStats()
{
    SysStatus rc;
    void *curr = NULL;
    FileSystemK42RamFSRef fsref = NULL;

    if (instances.isEmpty()) {
	err_printf("No ramfs file system mounted\n");
	return 0;
    }

    while ((curr = instances.next(curr, (ObjRef&)fsref))) {
	rc = DREF(fsref)->printStats();
	tassertWrn(_SUCCESS(rc), "error on printStats call\n");
    }
    return 0;
}

SysStatus
FileSystemK42RamFS::printStats()
{
#ifdef GATHERING_STATS
    FSStats *stats = getStatistics();
    stats->printStats();
    stats->initStats();
#else
    err_printf("Stats not available\n");
#endif // #ifdef GATHERING_STATS

    return 0;
}

/* virtual */ SysStatus
FileSystemK42RamFS::statfs(struct statfs *buf)
{
    memset((void *)buf, 0, sizeof(struct statfs));
    buf->f_type = K42_K42RAMFS_SUPER_MAGIC;
    return 0;
}

/* virtual */ SysStatusUval
FileSystemK42RamFS::readlink(FileToken fileInfo, char *buf, uval bufsize)
{
    FileInfoK42RamFSSymLink *syml = SINF(fileInfo);
    if (!S_ISLNK(syml->status.st_mode)) {
	return _SERROR(2660, 0, EINVAL);
    }

    char *path;
    uval plen = syml->getPath(path);
    if (plen > bufsize) {
	plen = bufsize; // truncate
    }

    memcpy(buf, path, plen);
    return _SRETUVAL(plen);
}

/* virtual */ SysStatus
FileSystemK42RamFS::symlink(FileToken dirInfo, char *compName, uval compLen,
			    char *oldpath)
{
    FileInfoK42RamFSDir *di = DINF(dirInfo);
    if (di->status.isDir()) {
	FileInfoK42RamFSDir::DirEntry *dentry = di->lookup(compName, compLen);
	if (dentry != NULL) {
	    return _SERROR(2661, 0, EEXIST);
	}
	FileInfoK42RamFSSymLink *sinfo = new FileInfoK42RamFSSymLink(oldpath);
	if (sinfo == NULL) {
	    return _SERROR(2663, 0, ENOMEM);
	}
	SysStatus rc = di->add(compName, compLen, sinfo);
	_IF_FAILURE_RET(rc);
    } else {
        return _SERROR(2662, 0, ENOTDIR);
    }

    di->status.st_mtime = di->status.st_atime = di->status.st_ctime = time(0);
    return 0;
}

/* static */ SysStatus
FileSystemK42RamFS::_Mkfs(__inbuf(mpathLen) char *mpath,
			  __in uval mpathLen)
{
    char mp[PATH_MAX+1];
    memcpy(mp, mpath, mpathLen);
    mp[mpathLen] = '\0';
    return Create(0, mp);
}
