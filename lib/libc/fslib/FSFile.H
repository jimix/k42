#ifndef __FSFILE_H_
#define __FSFILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFile.H,v 1.17 2005/04/21 20:31:53 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the per-file system per-file interface,
 * object of this class is exported from the file system specific to
 * the file system independant layers.
 * **************************************************************************/

#include <fslib/fs_defines.H>

#include <io/FileLinux.H>
#include <fslib/FSStats.H>

class DirLinuxFS;
class ServerFile;
typedef DirLinuxFS    **DirLinuxFSRef;
typedef ServerFile    **ServerFileRef;

/*
 * For some operations, we get back a file as well as a lock,
 * the lock is released once the user of a file has registered itself
 * with the ServerFile to gaurantee that the ServerFile won't dispear
 * until it knows about its "new client"
 */
typedef BLock MultiLinkMgrLock;

/* each file system defines its token. If uval is not
 * appropriate, the FS can store in Token a ptr to the specific data */
typedef uval FileToken;

// class operates on a file, pure virtual class
class FSFile {

#ifdef GATHERING_STATS
protected:
    FSStats *st;
#endif /* GATHERING_STATS */
public:
    DEFINE_GLOBAL_NEW(FSFile);
    FSFile() {}

#ifdef GATHERING_STATS
    FSFile(FSStats *s) : st(s) {
	/* empty body */
    }
#endif /* GATHERING_STATS */

    class FreeServerFileNode : public AutoListNode {
    public:
	SysTime timeFreed;
	DEFINE_GLOBAL_NEW(FreeServerFileNode);
	/*
	 * DREF(ref)->tryToDestroy called with lock released,
	 * ServerFile should remove from list and either deal with or
	 * re-enqueue.
	 */
	ServerFileRef ref;

	~FreeServerFileNode() {};
    };

    // kludge internal to file system to enable asserts on type
    enum { FSFILE_NONE, FSFILE_GLOBAL, FSFILE_KFS, FSFILE_NFS,
	   FSFILE_K42RAMFS, FSFILE_EXT2, FSFILE_PIPE, FSFILE_SOCK,
	   FSFILE_REMOTE
    };

    virtual FileToken getToken()=0;
    virtual uval getFSFileType() { return FSFILE_NONE; }
    virtual SysStatus destroy()=0;
    virtual SysStatus getStatus(FileLinux::Stat *status)=0;
    virtual SysStatus fchown(uid_t uid, gid_t gid)=0;
    virtual SysStatus fchmod(mode_t mode)=0;
    virtual SysStatus utime(const struct utimbuf *utbuf)=0;
    virtual SysStatus ftruncate(off_t length)=0;
    virtual SysStatus link(FSFile *newDirInfo, char *newName,
			   uval newLen, ServerFileRef fref)=0;

    /* Free data blocks */
    virtual SysStatus deleteFile()=0;

    /* ---------- operations on directory ----------- */
    virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf,
				   uval len)=0;
    // get rid of this in base class
    virtual SysStatus getFSFile(PathName *pathName, uval pathLen,
				PathName *pathNameTo,
				FSFile **file,
				FileLinux::Stat *status=NULL) {
	tassertMsg(0, "only used for NFS\n");
	return 0;
    }
    virtual SysStatus revalidate(char *name, uval namelen,
				 FileLinux::Stat *status) {
	// needed by NFS and other "volatile" FS
	return _SERROR(2674, 0, EINVAL);
    }

    /*
     * removes the name from directory entry, but does not delete
     * does decrement the number of links to file though, and returns
     * number of links remaining
     */
    virtual SysStatus unlink(
	char *name, uval namelen, FSFile *fileInfo=NULL,
	uval *nlinkRemain=NULL)=0;
    virtual SysStatus createDirLinuxFS(
	DirLinuxFSRef &rf, PathName *pathName, uval pathLen,
	DirLinuxFSRef par)=0;
    virtual SysStatus createServerFileBlock(ServerFileRef &ref)=0;
    virtual SysStatus createServerFileChar(ServerFileRef &ref) {
	tassertWrn(0, "Char files not supported.\n");
	return _SERROR(2508, 0, EPERM);
    }
    virtual SysStatus openCreateServerFile(
	ServerFileRef &ref, uval oflag, ProcessID pid,
	ObjectHandle &oh, uval &useType, TypeID &type) {
	return _SERROR(2484, 0, ENOSYS);
    }
    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
			    FSFile **finfo)=0;
    virtual SysStatusUval readlink(char *buf, uval bufsize)=0;
    virtual SysStatus symlink(char *compName, uval compLen,
			      char *oldpath)=0;
    virtual SysStatus rmdir(char *name, uval namelen)=0;
    virtual SysStatus rename(char *oldName, uval oldLen,
			     FSFile *newDirInfo, char *newName, uval newLen,
			     FSFile *renamedFinfo)=0;
    virtual SysStatus createFile(char *name, uval namelen,
				 mode_t mode, FSFile **fileInfo,
				 FileLinux::Stat *status=NULL)=0;
    /* FIXME: this version of createFile is meant only for file systems
     * that want to associate an arbitrary process with the file. This
     * could be in a subclass */
    virtual SysStatus createFile(char *name,
				 uval namelen, mode_t mode,
				 ObjectHandle vfoh,
				 FileLinux::Stat *status=NULL) {
	return _SERROR(2470, 0, ENOSYS);
    }

    /**
     * This is the operation really used to get files in normal
     * operation. In the case of multiple links, it will search for
     * ServerFile and return it. If it did not return a server file,
     * ref is NULL, and (on success) entryInfo has a valid fsFile.
     *
     * In the case where a ServerFile is returned (i.e., for file
     * with multiple links), a lock is also returned. This lock is
     * acquired by getFSFileOrServerFile and it will be
     * released by the user of this method once it's done with
     * registering itself with the Serverfile.  This guarantees
     * that the ServerFile won't disappear until it knows about
     * its "new client"
     */
    virtual SysStatus getFSFileOrServerFile(
	char *entryName, uval entryLen, FSFile **entryInfo,
	ServerFileRef &ref, MultiLinkMgrLock* &lock,
	FileLinux::Stat *status=NULL)=0;

    // operations that go on some list that you can somehow
    // get to from file specific to file system
    virtual SysStatus freeServerFile(FreeServerFileNode *n)=0;
    virtual SysStatus unFreeServerFile(FreeServerFileNode *n)=0;

    /* returns 1 if it succeeded in detaching; 0 if it can't detach now;
     * an error if the file is not found */
    virtual SysStatusUval detachMultiLink(ServerFileRef fref, uval ino)=0;

    virtual SysStatus statfs(struct statfs *buf) {
	// file system specific code should define this if they offer statfs
	return _SERROR(2463, 0, ENOSYS);
    }

    virtual SysStatus sync() {
	// file system specific code should redefine this if they need to
	// sync data
	return 0;
    }

    virtual uval isReadOnly() = 0;

#ifdef GATHERING_STATS
    void incStat(FSStats::StatType type, uval extra_arg=0) {
	if (getFSFileType() != FSFILE_NONE) {
	    tassertMsg(st != NULL,  "stat structure not initialized\n");
	    st->incStat(type, extra_arg);
	}
    }
    void incStatCond(uval cond, FSStats::StatType type, uval extra_arg=0) {
	if (cond) {
	    if (getFSFileType() != FSFILE_NONE) {
		tassertMsg(st != NULL, "stat structure not initialized\n");
		st->incStat(type, extra_arg);
	    }
	}
    }
    void printStats() {
	if (getFSFileType() != FSFILE_NONE) {
	    tassertMsg(st != NULL, "stat structure not initialized\n");
	    st->printStats();
	}
    }

    void initStats() {
	if (getFSFileType() != FSFILE_NONE) {
	    tassertMsg(st != NULL, "stat structure not initialized\n");
	    st->initStats();
	}
    }
#else
    void incStat(FSStats::StatType type, uval extra_arg=0) {
    }
    void incStatCond(uval cond, FSStats::StatType type, uval extra_arg=0) {
    }
    void printStats() {
    }
    void initStats() {
    }
#endif /* GATHERING_STATS */
};
#endif /* #ifndef __FSFILE_H_ */
