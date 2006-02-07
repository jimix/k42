/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <io/FileLinux.H>
#include "FileInfoVirtFS.H"
#include "ServerFileVirtFS.H"
#include "ServerFileDirVirtFS.H"
#include <fslib/NameTreeLinuxFSVirtFile.H>

struct DirEntryTokenData : public DirEntry {
    FileTokenData ftd;
    DEFINE_GLOBAL_NEW(DirEntryTokenData);
    DirEntryTokenData(const char* nm, uval len, uval fileno,
		      FileInfoVirtFS* fivf, uval fileDesc) {
	finfo = &ftd;
	memcpy(name, nm, len);
	namelen = len;
	fileNumber = fileno;
	finfo->ref = fivf;
	finfo->fileDesc = fileDesc;
    };
};


/* virtual */ void
FileInfoVirtFS::init(mode_t mode, uval number) {
    lock.init();
    memset(&status,0,sizeof(status));
    status.st_mode = (mode & ~S_IFMT) | S_IFREG;
    if (number==0) {
	fileNumber = GetFileNumber();
    } else {
	fileNumber = number;
    }
    active.serverFile = NULL;
    parentDir = NULL;
    status.st_ino = fileNumber;
    status.st_atime = status.st_mtime = status.st_ctime = time(NULL);
}

/* virtual */ SysStatus
FileInfoVirtFS::destroy() {
    if (parentDir) {
	parentDir->remove(this);
    }
    AutoLock<LockType> al(&lock);
    //This will disentangle us from and ServerFileDirVirtFS,
    // or ServerFileVirtFS
    if (active.serverFile) {
	DREF(active.serverFile)->tryToDestroy();
    }
    delete this;
    return 0;
}


/* virtual */ SysStatus
FileInfoVirtFSDirBase::mountToNameSpace(const char *mpath)
{
    FileLinux::Stat stat;

    DirLinuxFSRef dref;
    SysStatus rc = DirLinuxFSVolatile::CreateTopDir(dref, ".", this);
    _IF_FAILURE_RET(rc);

    rc = lockGetStatus(&stat);
    _IF_FAILURE_RET(rc);

    NameTreeLinuxFSVirtFile::Create(mpath, dref);

    locked_setParent(this);
    stat.st_nlink=1;
    unlockPutStatus(&stat);
    return rc;
}


#include "../fs_defines.H"
#include "../MultiLinkManager.H"
#include "../FreeList.H"

struct FileInfoVirtFSBase::globalStruct{
    DEFINE_GLOBAL_NEW(globalStruct);
    FSStats st;
    MultiLinkManager multiLinkMgr;
    FreeList freeList;
};
FileInfoVirtFSBase::globalStruct *FileInfoVirtFSBase::glob = NULL;

/*static*/ void
FileInfoVirtFSBase::ClassInit()
{
    if (glob) return;
    glob = new globalStruct;
    glob->multiLinkMgr.init();
    glob->freeList.init();
#ifdef GATHERING_STATS
    glob->st.initStats();
#endif //#ifdef GATHERING_STATS
}

/* detachMultiLink() is part of the destruction protocol for ServerFiles.
 * It is invoked by a ServerFile that learns it has no clients and no
 * parent; its goal is to take the object from the list managed by the
 * multiLinkMgr.
 *
 * The argument ino is needed as a key for searching in the list.
 *
 * It returns: 1 if the file has been successfully removed from the list;
 *             0 if the file is in the list but it can't be removed now
 *             error if the file does not appear in the list.
 */
/* virtual */ SysStatusUval
FileInfoVirtFSBase::detachMultiLink(ServerFileRef fref, uval ino)
{
    SysStatusUval rc;
    MultiLinkManager::SFHolder *href;
    glob->multiLinkMgr.acquireLock();
    if (glob->multiLinkMgr.locked_search(ino, href)==0) {
	rc = _SERROR(2177, 0, ENOENT);
    } else {
	tassertMsg(href->fref == fref, "Something weird!\n");

	/* lock this SFHolder, so we're guaranteed that there is no in-flight
	 * use of this SFHolder by a getFSFileOrServerFile */
	href->lock.acquire();
	// interact with ServerFile
	rc = DREF(fref)->detachMultiLink();
	tassertMsg(_SUCCESS(rc), "ops\n");
	if (_SGETUVAL(rc) == 1) {
	    // ok to detach
	    // no need to release the lock since the entry will be removed,but...
	    href->lock.release();
	    (void) glob->multiLinkMgr.remove(ino);
	} else {
	    tassertMsg(_SGETUVAL(rc)==0, "ops\n");
	    href->lock.release();
	}
    }
    glob->multiLinkMgr.releaseLock();
    return rc;
}




/* virtual */ SysStatus
FileInfoVirtFS::openCreateServerFile(ServerFileRef &fref, uval oflag,
				     ProcessID pid, ObjectHandle &oh,
				     uval &useType, TypeID &type) {
    SysStatus rc = createServerFileBlock(fref);
    _IF_FAILURE_RET(rc);
    rc = DREF(fref)->open(oflag, pid, oh, useType, type);
    return rc;
}


FileInfoVirtFSDirBase::FileInfoVirtFSDirBase() {
};

/* virtual */ SysStatus
FileInfoVirtFSDirBase::init(mode_t mode) {
    fileNumber = GetFileNumber();
    memset(&status,0,sizeof(status));
    status.st_ino = fileNumber;
    status.st_mode = (mode & ~S_IFMT) | S_IFDIR;
    status.st_atime = status.st_mtime = status.st_ctime = time(NULL);
    return 0;
}

/* virtual */ SysStatus
FileInfoVirtFSDirBase::mkdir(char *compName, uval compLen,
			     mode_t mode, FSFile **newDirInfo)
{
    SysStatus rc=0;
#ifndef NDEBUG
    FileLinux::Stat st;
    rc = getStatus(&st);
    _IF_FAILURE_RET(rc);
    tassertMsg(st.isDir(), "Expected to be a directory\n");
#endif //#ifndef NDEBUG
    DirEntry entry;
    FileInfoVirtFSDirBase *ninfo = new FileInfoVirtFSDirStatic;
    rc = ninfo->init(mode);
    if (_SUCCESS(rc)) {
	rc = add(compName, compLen, ninfo);
    }
    if (_FAILURE(rc)) {
	ninfo->destroy();
    } else {
	*newDirInfo = ninfo;
    }
    return rc;
}


/*virtual*/ SysStatus
FileInfoVirtFSDirBase::getFSFileOrServerFile(char *entryName, uval entryLen,
					     FSFile **entryInfo,
					     ServerFileRef &ref,
					     MultiLinkMgrLock* &mmlock,
					     FileLinux::Stat *status)
{
    SysStatus rc;
    AutoLock<LockType> al(&lock);
    DirEntry entry;
    rc = locked_lookup(entryName, entryLen, entry);
    _IF_FAILURE_RET(rc);

    tassertMsg((entry.finfo->ref != NULL), "finfo got corrupted?!?\n");

    *entryInfo = entry.finfo->ref;

    if (status) {
	entry.finfo->ref->getStatus(status);
    }
    return 0;
}


/* virtual */ SysStatus
FileInfoVirtFSLocal::createServerFileBlock(ServerFileRef &fref)
{
    return ServerFileVirtFS::Create(fref, this);
}

/* virtual */ SysStatus
VirtFSFile::createServerFileBlock(ServerFileRef &fref)
{
    return ServerFileVirtFS::Create(fref, this);
}

/* static */ uval FileInfoVirtFS::nextFileNumber = 1;

/* virtual */ SysStatus
FileInfoVirtFSDirBase::createDirLinuxFS(DirLinuxFSRef &rf,
					PathName *pathName, uval pathLen,
					DirLinuxFSRef par)
{
    return ServerFileDirVirtFS::Create(rf, pathName, pathLen, this, par);
}

/* virtual */ SysStatus
FileInfoVirtFSDirBase::lookup(const char *name, uval namelen, DirEntry &entry)
{
    AutoLock<LockType> al(&lock);
    return locked_lookup(name, namelen, entry);
}



/* virtual */ SysStatus
FileInfoVirtFSDirStatic::destroy()
{

    // Can't hold lock during parent's destroy --- dtor may need lock
    // This means that caller must prevent a race with add() This
    // shouldn't be a problem because it's a race within ourselves
    // (i.e., race within devfs), that we should know how to handle.
    lock.acquire();
    if (entries.next()) {
	lock.release();
	return _SERROR(2430, 0, ENOTEMPTY);
    }
    lock.release();
    return FileInfoVirtFSDirBase::destroy();

}

/* virtual */ SysStatus
FileInfoVirtFSDirStatic::locked_lookup(const char *name, uval namelen,
				       DirEntry &entry)
{
    DirEntry *dnode = (DirEntry*) entries.next();
    while (dnode != NULL) {
	if (dnode->namelen == namelen
	    && strncmp(dnode->name, name, namelen) == 0) {
	    dnode->dup(entry);
	    return 0;
	}
	dnode = (DirEntry*) dnode->next();
    }
    return _SERROR(2220, 0, ENOENT);
}

/* virtual */ SysStatus
FileInfoVirtFSDirStatic::removeAll()
{
    AutoLock<LockType> al(&lock);
    DirEntry *dnode = (DirEntry*) entries.next();
    while (dnode != NULL) {
	FileLinux::Stat stat;
	DirEntry *next = (DirEntry*) dnode->next();
	dnode->finfo->ref->lockGetStatus(&stat);
	dnode->finfo->ref->locked_setParent(NULL);
	stat.st_nlink--;
	dnode->finfo->ref->unlockPutStatus(&stat);
	dnode->detach();
	delete dnode;
	dnode = next;
    }
    return 0;
}


SysStatus
FileInfoVirtFSDirStatic::remove(FileInfoVirtFS* finfo)
{
    SysStatus rc = 0;
    FileLinux::Stat stat;
    FileTokenData ftd;
    ftd.ref = finfo;
    AutoLock<LockType> al(&lock);

    rc = finfo->lockGetStatus(&stat);
    _IF_FAILURE_RET(rc);

    //Make sure DirLinuxFS object looks at us again
    if (active.dir) {  DREF(active.dir)->markCacheInvalid(); };

    DirEntry *dnode = (DirEntry*) entries.next();
    while (dnode != NULL) {
	if (dnode->finfo->ref == finfo) {
	    break;
	}
	dnode = (DirEntry*) dnode->next();
    }
    if (!dnode) {
	rc = _SERROR(2368, 0, ENOENT);
    } else {
	finfo->parentDir = NULL;
	dnode->detach();
	delete dnode;
    }
    finfo->unlockPutStatus(&stat);

    return rc;
}

SysStatus
FileInfoVirtFSDirStatic::add(const char *nm, uval len, FileInfoVirtFS* finfo)
{
    SysStatus rc=0;
    FileLinux::Stat stat;
    FileTokenData ftd;
    DirEntry entry;
    DirEntry *dentry;
    ftd.ref = finfo;


    uval i=0;
    while (i<len) {
	if (nm[i]=='/') {
	    if ((i==2 && nm[0]=='.' && nm[1]=='.')) {
		//must be a character after "/"
		if (!parentDir || len-i<=1) return _SERROR(2429, 0, ENOENT);
		return parentDir->add(&nm[i+1], len-i-1, finfo);
	    } else if (i==1 && nm[0]) {
		len-=2;
		nm+=2;
	    } else {
		rc = locked_lookup(nm, i, entry);
		_IF_FAILURE_RET(rc);
		return entry.finfo->ref->add(nm+i+1, len-i-1, finfo);
	    }

	}
	++i;
    }

    AutoLock<LockType> al(&lock);

    tassertMsg(finfo != this,
	       "Can't add ourselves to dir\n");
    rc = finfo->lockGetStatus(&stat);
    _IF_FAILURE_RET(rc);

    rc = locked_lookup(nm, len, entry);
    if (_SUCCESS(rc) ||
	(nm[0]=='.' && nm[1]=='\0') ||
	(nm[0]=='.' && nm[1]=='.' && nm[2]=='\0')) {
	rc = _SERROR(2472, 0, EEXIST);
	goto finish;
    }
    rc = 0;

    dentry = new DirEntryTokenData(nm, len, stat.st_ino, finfo, 0);
    entries.prepend(dentry);

    finfo->locked_setParent(this);
    if (stat.isDir()) {
	stat.st_nlink=2;
    } else {
	stat.st_nlink=1;
    }

finish:
    finfo->unlockPutStatus(&stat);
    return rc;
}

inline uval
getRecLen(uval lengthString) {
    uval recLen = sizeof(struct direntk42);
    recLen += lengthString - sizeof(((direntk42 *)0)->d_name) + 1;
    recLen = ALIGN_UP(recLen, sizeof(uval64));
    return recLen;
}

/* virtual */ SysStatusUval
FileInfoVirtFSDirStatic::getDents(uval &cookie, struct direntk42 *buf, uval len)
{
    SysStatus rc=0;
    DirEntry *node = (DirEntry*) entries.next();
    struct direntk42 *dp;
    struct direntk42 *nextdp;
    uval dpend;
    uval i;

    for (i=2; i < cookie && node != NULL; i++) {
	node = (DirEntry*) node->next();
    }

    tassert(len >= sizeof(struct direntk42),
	    err_printf("buf not large enough for struct dirent\n"));

    dpend = (uval)buf + len;
    dp = nextdp = buf;

    while (node != NULL) {
	uval namlen;
	if (cookie==0) {
	    dp->d_reclen = getRecLen(1);
#if defined DT_UNKNOWN && defined _DIRENT_HAVE_D_TYPE
	    dp->d_type	  = DT_UNKNOWN;
#endif /* #if defined DT_UNKNOWN && defined ... */
#if defined _DIRENT_HAVE_D_NAMLEN
	    dp->d_namlen      = 1;
#endif /* #if defined _DIRENT_HAVE_D_NAMLEN */
	    dp->d_name[0] = '.';
	    dp->d_name[1] = '\0';
	    ++cookie;
	    namlen = 1;
	} else if (cookie==1) {
	    dp->d_reclen = getRecLen(2);
#if defined DT_UNKNOWN && defined _DIRENT_HAVE_D_TYPE
	    dp->d_type	  = DT_UNKNOWN;
#endif /* #if defined DT_UNKNOWN && defined ... */
#if defined _DIRENT_HAVE_D_NAMLEN
	    dp->d_namlen      = 2;
#endif /* #if defined _DIRENT_HAVE_D_NAMLEN */
	    dp->d_name[0] = '.';
	    dp->d_name[1] = '.';
	    dp->d_name[2] = '\0';
	    ++cookie;
	    namlen = 2;
	} else {
	    rc = node->finfo->ref->getNumber(dp->d_ino);
	    tassertMsg(_SUCCESS(rc),
		       "FileInfoVirtFSDir::getDents: failure on getNumber\n");
	    ++cookie;
	    dp->d_reclen      = getRecLen(node->namelen);
#if defined DT_UNKNOWN && defined _DIRENT_HAVE_D_TYPE
	    dp->d_type	  = DT_UNKNOWN;
#endif /* #if defined DT_UNKNOWN && defined ... */
#if defined _DIRENT_HAVE_D_NAMLEN
	    dp->d_namlen      = node->namelen;
#endif /* #if defined _DIRENT_HAVE_D_NAMLEN */
	    memcpy(dp->d_name, (const char *)node->name, node->namelen);
	    *(dp->d_name + node->namelen) = '\0';
	    namlen = node->namelen;

	    node = (DirEntry*) node->next();
	}
	nextdp = (struct direntk42 *)((uval)dp + dp->d_reclen);

	// Make sure we can fit another
	if (((cookie<2) || (node != NULL)) &&
	    (((uval)nextdp + getRecLen(namlen)) < dpend)) {
	    dp->d_off = (uval)nextdp - (uval)buf;
	    dp = nextdp;
	} else {
	    dp->d_off = 0;
	    break;
	}
    }
    return _SRETUVAL((uval)nextdp - (uval)buf);
}

/* virtual*/ SysStatusUval
FileInfoVirtFSDirStatic::makeEmpty()
{
    if (this==parentDir) {
	return 1;
    }
    return 0;
}

/* virtual */ SysStatusUval
FileInfoVirtFSDirStatic::deleteFile()
{
    err_printf("Shouldn't call this yet: %s\n",__func__);
    return 0;
}

/* virtual */ SysStatusUval
FileInfoVirtFSDirStatic::getServerFileType()
{
    err_printf("Shouldn't call this yet: %s\n",__func__);
    return 0;
}

/* virtual */ SysStatus
FileInfoVirtFSDirStatic::createServerFileBlock(ServerFileRef &fref)
{
    err_printf("Shouldn't call this yet: %s\n",__func__);
    return 0;
}
