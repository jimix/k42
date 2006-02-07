#ifndef __FSFILE_H_
#define __FSFILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFile.H,v 1.4 2003/08/18 01:40:06 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the per-file system per-file interface,
 * object of this class is exported from the file system specific to
 * the file system independant layers.
 * **************************************************************************/
#include <io/FileLinux.H>
#include <sync/BLock.H>
#include <fslib/FSStats.H>
#include <sys/sysTypes.H>
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

// class operates on a file, pure virtual class
class FSFile {
#ifdef GATHERING_STATS
    FSStats *st;
#endif /* GATHERING_STATS */
public:
    class FreeServerFileNode {
    public:
	SysTime timeFreed;
      //DEFINE_GLOBAL_NEW(FreeServerFileNode);
	/*
	 * DREF(ref)->tryToDestroy called with lock released,
	 * ServerFile should remove from list and either deal with or
	 * re-enqueue.
	 */
	ServerFileRef ref;
	
	~FreeServerFileNode() {};
    };

    // kludge internal to file system to enable asserts on type
    enum { FSFILE_NONE, FSFILE_GLOBAL, FSFILE_KFS, FSFILE_NFS };
    virtual uval getFSFileType() { return FSFILE_NONE; }
    virtual uval getToken()=0;	// FIXME: KLUDGE
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
//    // get rid of this in base class
//    virtual SysStatus getFSFile(PathName *pathName, uval pathLen,
//				PathName *pathNameTo,
//				FSFile **file,
//				FileLinux::Stat *status=NULL) {
//	tassertMsg(0, "only used for NFS\n");
//	return 0;
//    }
    /*
     * removes the name from directory entry, but does not delete
     * does decrement the number of links to file though, and returns
     * number of links remaining
     */
    virtual SysStatus unlink(
	char *name, uval namelen, FSFile *fileInfo=NULL, 
	uval *nlinkRemain=NULL)=0;
//    virtual SysStatus createDirLinuxFS(
//	DirLinuxFSRef &rf, PathName *pathName, uval pathLen,
//	DirLinuxFSRef par)=0;
    virtual SysStatus createServerFileBlock(ServerFileRef &ref)=0;
    virtual SysStatus createServerFileChar(ServerFileRef &ref) {
	tassertWrn(0, "Char files not supported.\n");
	return _SERROR(2509, 0, EPERM);
    }
    virtual SysStatus openCreateServerFile(
	ServerFileRef &ref, uval oflag, ProcessID pid,
  	ObjectHandle &oh, uval &useType, TypeID &type) {
  	return _SERROR(2225, 0, ENOSYS);
    }
    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode, 
			    FSFile **finfo)=0;
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
	return _SERROR(2469, 0, ENOSYS);
    }
    
    /*
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
	return _SERROR(2462, 0, ENOSYS);
    }
#ifdef GATHERING_STATS
    void incStat(FSStats::StatType type, uval extra_arg=0) {
	tassertMsg((st != NULL), "stat structure not initialized\n");
	st->incStatus(type, extra_arg);
    }
    void incStatCond(uval cond, FSStats::StatType type, uval extra_arg=0) {
	tassertMsg((st != NULL), "stat structure not initialized\n");
	if (cond) {
	    st->incStatus(type, extra_arg);
	}
    }
    void printStats() {
	tassertMsg((st != NULL), "stat structure not initialized\n");
	st->printStats();
    }
	
    void initStats() {
	tassertMsg((st != NULL), "stat structure not initialized\n");
	st->initStats();
    }
#else

  //    void incStat(FSStats::StatType type, uval extra_arg=0) {
  //    }
  //    void incStatCond(uval cond, FSStats::StatType type, uval extra_arg=0) {
  //    }
    void printStats() {
    }
    void initStats() {
    }
#endif /* GATHERING_STATS */
};
#endif /* #ifndef __FSFILE_H_ */
