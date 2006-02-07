#ifndef __DIR_LINUX_FSVOLATILE_H_
#define __DIR_LINUX_FSVOLATILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 ****************************************************************************/
/****************************************************************************
 * Module Description:  Specializes DirLinuxFS behavior for file systems
 *                      where cached data (token, status) can change
 *                      "externally" to the file system.
 ****************************************************************************/

#include <fslib/DirLinuxFS.H>

class DetachList {
    typedef BLock LockType;
    LockType   detlock;
    static const uval MAX=10;
    ServerFileRef list[MAX];
    uval listSize;
public:
    DEFINE_GLOBAL_NEW(DetachList);
    DetachList() {
	detlock.init();
	listSize = 0;
    }
    void insert(ServerFileRef ref) {
	AutoLock <LockType> al(&detlock); // locks now, unlocks on return
	tassert(listSize<MAX, err_printf("list too small\n"));
	list[listSize++] = ref;
    }

    ServerFileRef remove() {
	AutoLock <LockType> al(&detlock); // locks now, unlocks on return
	if (listSize) {
	    ServerFileRef ret = list[--listSize];
	    return ret;
	} else {
	    return 0;
	}
    }

    void removeAll() {
	AutoLock <LockType> al(&detlock); // locks now, unlocks on return
	listSize = 0;
    }
};

typedef DirLinuxFS **DirLinuxFSRef;
class DirLinuxFSVolatile;
typedef DirLinuxFSVolatile **DirLinuxFSVolatileRef;

class DirLinuxFSVolatile : public DirLinuxFS {
public:
    // create a top level directory,
    // grabs first component of passed pathname and uses
    static SysStatus CreateTopDir(DirLinuxFSRef &dir, char *path, FSFile *file);
    DEFINE_GLOBAL_NEW(DirLinuxFSVolatile);
protected:
    DetachList *detachList;

    virtual SysStatus init(PathName *pathName, uval pathLen, FSFile *theInfo, 
			   DirLinuxFSRef par);

    virtual SysStatus handleErrorOnToken(char *name, uval namelen,
					 SysStatus error) {
	AutoLock<DirLockType> al(&dirlock);
	return locked_handleErrorOnToken(name, namelen, error);
    }
    // blows away cached token if error was stale NFS handle)
    virtual SysStatus locked_handleErrorOnToken(char *name, uval namelen,
						SysStatus error);

    virtual SysStatus locked_purge();	// blow cached context away

    // only invoked internally to the object (i.e., no cross object call)
    SysStatus checkDetachList();
    SysStatus locked_checkDetachList();

    SysStatus locked_doDetachInvalidFile(ServerFileRef fileRef);

    virtual uval isDirectoryCacheEmpty();

    /**
     * revalidate() is invoked to check if the cached file meta-data is
     * still up to date.
     * revalidate() should (1) clean up the children list and
     * (2) talk to the server about the directory, returning ESTALE error
     * if the file it has become stale) and (3) deal with getting the
     * cached data out if ESTALE has been detected.
     * (The part (2) is very file system specific, so
     * locked_revalidate() should be provided by the file system specific code
     */
    virtual SysStatus revalidate();
    virtual SysStatus revalidateWithParentLocked(
	DirLinuxFSVolatileRef lockedParent);

    virtual SysStatus locked_revalidate() {
	// this should be implemented by file system specific code
	tassertMsg(0, "This should not be called\n");
	return _SERROR(2091, 0, ENOSYS);
    }

    SysStatus eliminateStaleDir(DirLinuxFSVolatileRef lockedParent = NULL);

    // to be implemente by file system specific code. It should return 1
    // if it's time to refetch data in the cache, 0 otherwise
    virtual uval shouldRevalidate() {
	// this should be implemented by file system specific code
	tassertMsg(0, "This should not be called\n");
	return _SERROR(2092, 0, ENOSYS);
    }

public:
    virtual SysStatus utime(const struct utimbuf *utbuf);
    virtual SysStatus fchown(uid_t uid, gid_t gid);
    virtual SysStatus fchmod(mode_t mode);
    virtual SysStatus getStatus(FileLinux::Stat *retStatus);

    /*
     * Operations that re-try in the case of a stale handle.
     * These operatoins first validate, then try, then revalidate, then try
     * again.  See documentation for how/why this works. Note, we could
     * have made the only functions that re-try the lookup ones, but that
     * would have required an extra virtual function invocation on each
     * function. Also, still need above operations that operate on a single
     * directory to re-try.
     */
    virtual SysStatus rmdir(char *name, uval namelen);
    virtual SysStatus unlink(char *name, uval namelen);
    virtual SysStatus utime(char *name, uval namelen,
			    const struct utimbuf *utbuf);
    virtual SysStatus chown(char *name, uval namelen, uid_t uid, gid_t gid);
    virtual SysStatus chmod(char *name, uval namelen, mode_t mode);
    virtual SysStatus mkdir(char *name, uval namelen, mode_t mode);
    virtual SysStatus getObj(char *name, uval namelen, uval oflag, mode_t mode,
			     ProcessID pid, ObjectHandle &oh,
			     uval &useType, TypeID &type,
			     /* argument for simplifying gathering traces of
			      * file sharing information. This should go away.
			      */
			     ObjRef &fref);
    virtual SysStatus externalLookupDirectory(char *name, uval namelen,
					      DirLinuxFSRef &dr,
					      RWBLock* &nhSubDirLock);
    virtual SysStatus getStatus(char *name, uval namelen,
				FileLinux::Stat *retStatus,
				uval followLink);
    virtual SysStatus link(char *oldname, uval oldnamelen,
			   DirLinuxFSRef newDirRef, char *newname,
			   uval newlen);
    virtual SysStatus rename(char *oldname, uval oldnamelen,
			     DirLinuxFSRef newDirRef, char * newname,
			     uval newlen);

    // if finds a match, returns nameHolder
    virtual SysStatus locked_lookup(char *name, uval namelen,
				    NameHolderInfo *nameHolderInfo);

    virtual SysStatus destroy();

    virtual SysStatus validateFileRef(ServerFileRef fileRef,
				      char &hasValidEntry,
				      char &hasInvalidEntry, uval origIno);

    virtual SysStatus detachInvalidFile(ServerFileRef fileRef);
    virtual SysStatus detachInvalidDir(DirLinuxFSRef fileRef) {
	    AutoLock<DirLockType> al(&dirlock);
	    return locked_detachInvalidDir(fileRef);
    }
    virtual SysStatus locked_detachInvalidDir(DirLinuxFSRef fileRef);

    virtual SysStatus exportedXObjectListEmpty();
};

typedef DirLinuxFSVolatile **DirLinuxFSVolatileRef;

#endif /* #ifndef __DIR_LINUX_FSVOLATILE_H_ */
