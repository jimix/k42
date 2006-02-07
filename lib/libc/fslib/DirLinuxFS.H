#ifndef __DIR_LINUX_FS_H_
#define __DIR_LINUX_FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DirLinuxFS.H,v 1.106 2005/07/14 21:36:59 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Defines the fundamental interface to object that
 * maintains name space for other objects
 ****************************************************************************/

#include "ServerFile.H"
#include <sync/RWLock.H>
#include "fs_defines.H"
#include "DentryList.H"

class DirLinuxFS : public ServerFile {
protected:
#ifdef USING_READER_WRITER_LOCK_FOR_DIRECTORIES
    typedef RWBLock DirLockType;
    DirLockType dirlock;

    void upgradeLock() { dirlock.upgrade(); }
    void downgradeLock() { dirlock.downgrade(); }
    void acquireLockR() { dirlock.acquireR(); }
    void releaseLockR() { dirlock.releaseR(); }
#else
    typedef RWBLock DirLockType;
    DirLockType dirlock;

    void upgradeLock() {}
    void downgradeLock() {}
    void acquireLockR() { dirlock.acquire(); }
    void releaseLockR() { dirlock.release(); }
#endif //#ifdef USING_READER_WRITER_LOCK_FOR_DIRECTORIES

    DirLockType * getLockPtr() { return &dirlock; }

public:
    virtual SysStatus acquireLockW() { dirlock.acquireW(); return 0; }
    virtual SysStatus releaseLockW() { dirlock.releaseW(); return 0; }

protected:
#ifdef CACHE_LINEAR_LIST
    DentryListLinear children;
#else
    DentryListHash children;
#endif // #ifdef CACHE_LINEAR_LIST

    class SingleParent : public Parent {
    public:
	DEFINE_GLOBAL_NEW(SingleParent);
	SingleParent() { u.theRef = 0; }
	virtual ~SingleParent() {}
	virtual ParentSet *getParents() {
	    tassert(0, err_printf("Should not be called by directories\n"));
	    return 0;
	}
	virtual void clearParentSet() {
	    tassert(0, err_printf("clearParentSet only available to files\n"));
	}
	virtual ParentSet *FetchAndClearParentSet() {
	    tassert(0, err_printf("FetchAndClearParentSet only available to "
				  "files\n"));
	    return 0;
	}
	virtual DirLinuxFSRef getTheRef() {
	    return u.theRef;
	}
	virtual void setTheRef(DirLinuxFSRef d) {
	    u.theRef = d;
	}
	virtual DirLinuxFSRef FetchAndClearTheRef() {
  	    DirLinuxFSRef retvalue;
	    retvalue = (DirLinuxFSRef)
		FetchAndClearVolatile((uval *)&u.theRef);
	    return (retvalue);
	}
    };

    virtual DirLinuxFSRef getPar() {
	if (parent) {
	    return parent->getTheRef();
	} else {
	    return 0;
	}
    }

    // We get to a directory object through a nameHolder in the parent
    // directory. The locking protocols rely on holding this nameHolder lock
    // while invoking operations on the object itself, so that the object
    // can't go away as we are to request its services. The root doesn't
    // have a parent directory, so for it we create a single special lock
    static NameHolderInfo::LockType rootRWNHLock;

    // The following method is used by external clients (e.g. on rename()
    // one directory will ask the other to remove the entry that is being
    // replaced as the result of rename). For "local" calls (meaning that
    // the object issueing the call is the target for the call) we can
    // just use children.remove(name, namelen) directly.
    virtual SysStatus locked_deleteChild(char *name, uval namelen) {
	_ASSERT_HELD(dirlock);
	return children.remove(name, namelen);
    }

    virtual SysStatus giveAccessSetClientData(
	ObjectHandle &oh, ProcessID toProcID,
	AccessRights match, AccessRights nomatch, TypeID type = 0);

    virtual SysStatusUval _getDents(char *buf, uval len, __XHANDLE xhandle,
				    __CALLER_PID pid);
    virtual SysStatusUval _setFilePosition(sval position, uval at,
					   __XHANDLE xhandle);

    virtual SysStatus _dup(ProcessID pid, ObjectHandle &oh, __XHANDLE origXhandle);

    // relative file system path name from parent, except if the
    // parent ref is zero, then this is the absolute path of the top
    // directory
    PathName *dirPathName;
    uval      dirPathNameLen;

    /*
     * These two methods are overridden in volatile class
     * (blows away cached token if error was stale NFS handle)
     */
    virtual SysStatus handleErrorOnToken(char *name, uval namelen,
					 SysStatus error) {
	return 0;
    }
    virtual SysStatus locked_handleErrorOnToken(char *name, uval namelen,
						SysStatus error) {
	return 0;
    }

    static SysStatus CheckOpenFlags(uval oflag, uval isDir, uval isNew);

protected:
    DEFINE_REFS(DirLinuxFS);
    virtual SysStatus init(PathName *pathName, uval pathLen, FSFile *theInfo,
			   DirLinuxFSRef par);

protected:
    // gets the full path name of this directory
    // returns its length, or error if it doesn't fit
    virtual SysStatusUval getFullDirName(PathName *pathName, uval maxLen);
    // this will get full dir name and append a component "name" if name not NULL
    virtual SysStatus getStringFullDirName(char *name, uval len,
					   char *fullname, uval maxnamelen);

    // gets the full path name for the root of tree. Returns its lengh, or
    // error if there is not enough space for it
    virtual SysStatusUval getFullRootName(PathName *pathName, uval maxLen);

    // given a component name, it returns in fullPathName the full path
    // name of the component. If the component is empty, it will return
    // its full directory path followed by "/."
    // Only invoked internally to the object (ie no cross object call)
    SysStatus getFullCompName(PathName* compName, uval compLen,
			      uval maxLen,PathName* fullPathName,
			      uval &fullPathLen);

    // adds a single component
    virtual SysStatus locked_addComp(char *name, uval len, uval8 flags, 
				     uval contents);

    /**
     * convert FSFile to an object of the correct type to be opened
     * e.g., a ServerFile or a FSFileOther.. and put in cache and put
     * in nameHolder 
     * @param existingDentryListEntry re-using existing
     *    entry
     */
    SysStatus locked_convert(
	char *name, uval len, FSFile *fl, FileLinux::Stat *st,
	NameHolderInfo *resNHI, void *existingDentryListEntry);

    // if finds a match, returns NameHolderInfo
    virtual SysStatus locked_lookup(char *name, uval namelen,
				    NameHolderInfo *nameHolderInfo);

    /**
     * Looks up file in local cache, if doesn't find it, looks up
     * token and adds to cache.  NameHolder may contain
     * the correct file, or just a token to it.
     * This is only called internal to object.
     * @param convert creates ServerFile or special FSFiles... as 
     *   needed if file is to be subsequently accessed
     * @param lockedForWrite indicates that the thread invoking
     *   this method was helding the lock in write mode. This is needed
     *   for avoiding making intermediate state visible through
     *   operations such as link/rename
     */
    virtual SysStatus locked_lookupOrAdd(
	char *name, uval namelen, NameHolderInfo *nameHolder,
	FileLinux::Stat *retStatus = NULL, uval convert = 0,
	uval lockedForWrite = 0);

    virtual SysStatus locked_retryLookupOrAdd(
	char *name, uval namelen, NameHolderInfo *nameHolder,
	FileLinux::Stat *retStatus = NULL, uval convert = 0);

    /**
     * Called by lookup below, called external to object
     * dirref if sub-dir exists.
     * @param name identifies the sub-directory being looked up;
     * @param dr ref returned if sub-dir exists;
     * @param nhSubDirLock is the lock of the NameHolder where sub-dir has been
     *   found (if it exists). The method acquires this lock before returning,
     *   thereby guaranteeing that the sub-directory for the object does not
     *   disappear before being released by the lookup method (once it uses it).
     *   After acquiring this lock, the method will release the directory lock.
     */
    virtual SysStatus externalLookupDirectory(
	char *name, uval namelen, DirLinuxFSRef &dr,
	NameHolderInfo::LockType* &nhSubDirLock);

    virtual uval isDirectoryCacheEmpty();

    DEFINE_GLOBAL_NEW(DirLinuxFS);

    // calls to support FileLinuxServer interface
    // These are meningless to directories
    // functions specific to file system
    virtual SysStatus ftruncate(off_t length) {
	return _SERROR(2025, 0, EISDIR);
    }
    virtual SysStatus getFROH(ObjectHandle & oh, ProcessID pid) {
	return _SERROR(2026, 0, EISDIR);
    }

public:

    // call from file system to garbage collect free directories
    virtual SysStatus tryToDestroy();

    /** 
     * detachChild(ServerFileRef fref) is part of destruction protocols for 
     * files.
     * The directory object interacts with the file to make sure it is in a
     * state it can go away.
     * This method returns 1 if the file is successfully removed from the
     * cached children, 0 if the file can't be removed now, and an error (<0)
     * if the ServerFileRef does not appear in the children list
     */
    virtual SysStatusUval detachChild(ServerFileRef fref);

    // returns current len and pathName
    virtual SysStatusUval getDirName(char *buf, uval bufLen,
				     PathName *&pathName);

    /**
     * lookup a directory
     * pathName, pathLen, define a path name buffer and contents
     * for Dir requests that the whole path name be processed to find
     * a directory - normally the last path component is skipped and returned
     * in pathName - presumable its a file name to be opened in the found
     * directory.  
     *
     * lookup returns:
     * - dirLinuxRef: a reference to the DirLinuxFS object result for the lookup
     * - nhLock: a lock corresponding to the nameHolder for dirLinuxRef (for
     *   root, nhLock is class variable rootNHLock). This lock
     *   is acquired by lookup, and it will be released by whoever invoked
     *   lookup after it's used. This guarantees that object returned by lookup
     *   won't disappear as it's being passed for use.
     * - compName is a pointer to component (if any) left
     * - compLen is the length of the remaining component
     */
    virtual SysStatus lookup(PathName *pathName, uval pathLen, 
			     char *&compName, uval &compLen,
			     DirLinuxFSRef &dirLinuxRef, RWBLock* &lock);

    virtual SysStatus getObj(char *name, uval namelen, uval oflag, mode_t mode,
			     ProcessID pid, ObjectHandle &oh,
			     uval &useType, TypeID &type,
			     /* argument for simplifying gathering traces of
			      * file sharing information. This should go away.
			      */
			     ObjRef &fref);

    virtual SysStatus open(uval oflag, ProcessID pid, ObjectHandle &oh,
			   uval &useType, TypeID &type);

    /**
     * create a top level directory,
     * grabs first component of passed pathname and uses
     */
    static SysStatus CreateTopDir(DirLinuxFSRef &dirLinuxRef, char *path,
				  FSFile *file);

    virtual SysStatus mknod(char *remainder, uval remainderLen, uval mode,
			    uval dev);

    virtual SysStatus bind(char *remainder, uval remainderLen, uval mode,
			   ObjectHandle ServerSocketOH);
    virtual SysStatus getSocketObj(char *remainder, uval remainderLen,
				   ObjectHandle &ServerSocketOH,
				   ProcessID pid);

    virtual SysStatus getStatus(char *name, uval namelen,
				FileLinux::Stat *retStatus,
				uval followLink);
    virtual SysStatus getStatus(FileLinux::Stat *status);

    virtual SysStatus fchown(uid_t uid, gid_t gid);
    virtual SysStatus chown(char *name, uval namelen,
			    uid_t uid, gid_t gid);

    virtual SysStatus fchmod(mode_t mode);
    virtual SysStatus chmod(char *name, uval namelen, mode_t mode);

    virtual SysStatus truncate(char *name, uval namelen, off_t length);

    virtual SysStatus mkdir(char *name, uval namelen, mode_t mode);

    virtual SysStatusUval readlink(char *name, uval namelen, char *buf, 
				   uval bufsize);
    virtual SysStatus symlink(char *name, uval namelen, char *oldpath);

    virtual SysStatus rmdir(char *name, uval namelen);
    virtual SysStatus rmdir(FSFile *dirInfo, char *name, uval namelen);

    virtual SysStatus utime(const struct utimbuf *utbuf);

    virtual SysStatus utime(char *name, uval namelen,
			    const struct utimbuf *utbuf = NULL);

    virtual SysStatus link(char *oldname, uval oldnamelen,
			   DirLinuxFSRef newDirRef, char *newname,
			   uval newlen);

    virtual SysStatus unlink(char *name, uval namelen);

    virtual SysStatus rename(char *oldname, uval oldnamelen,
			     DirLinuxFSRef newDirRef, char * newname,
			     uval newlen);

    virtual SysStatus statfs(struct statfs *buf);

    virtual SysStatus sync();

    virtual SysStatus destroy();

    static void BeingFreed(XHandle xhandle);

    virtual SysStatus handleXObjFree(XHandle xhandle) {
	XHandleTrans::SetBeingFreed(xhandle, BeingFreed);
	return 0;
    }

    // FIXME: invoked only by NameTreeLinuxFSVirtFile, so we could define
    //        in a subclass
    virtual SysStatus createVirtFile(char *name, uval namelen,
				     mode_t mode, ObjectHandle vfoh);

    /** 
     * This method is only used by DirLinuxFSVolatile, but we need to
     * define it so we can invoke through a DREF(DirLinuxFSRef)
     */
    virtual SysStatus revalidate() {
	return _SERROR(2132, 0, ENOSYS);
    }

    /**
     * This method is only used by DirLinuxFSVolatile, but we need to
     * define it so we can invoke through a DREF(DirLinuxFSRef)
     */
    virtual SysStatus detachInvalidFile(ServerFileRef ref) {
	return _SERROR(2168, 0, ENOSYS);
    }

    /**
     * This method is only used by DirLinuxFSVolatile, but we need to
     * define it so we can invoke through a DREF(DirLinuxFSRef)
     */
    virtual SysStatus detachInvalidDir(DirLinuxFSRef ref) {
	return _SERROR(2169, 0, ENOSYS);
    }

    /**
     * method used when a directory is being renamed
     */
    virtual SysStatus changeParent(DirLinuxFSRef newDirRef) {
	/* nameholder used to invoke this is locked, so there can't
	 * be a concurrent execution of a method changing the parent ref */
	parent->setTheRef(newDirRef);
	return 0;
    }

    virtual SysStatus _lazyGiveAccess(__XHANDLE xhandle,
				      __in sval file, __in uval type,
				      __in sval closeChain,
				      __inbuf(dataLen) char *data,
				      __in uval dataLen);

    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __inoutbuf(datalen:datalen:datalen)char *data,
				  __inout uval& datalen,
				  __XHANDLE xhandle);
};

#endif /* #ifndef __DIR_LINUX_FS_H_ */
