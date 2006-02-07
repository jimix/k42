#ifndef __FILE_SYSTEM_GLOBAL_H_
#define __FILE_SYSTEM_GLOBAL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemGlobal.H,v 1.18 2005/04/12 19:13:47 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines interface a file system should
 * implement to use fslib.
 * This is a temporary hack for compatiblity, will go away real soon now
 * **************************************************************************/

#include "fs_defines.H"
#include "FSFile.H"
#include "MultiLinkManager.H"
#include "FreeList.H"
#include "PagingTransport.H"

class FileSystemGlobal;
typedef FileSystemGlobal    **FileSystemRef;


class FileSystemGlobal : public Obj {
public:
    static const FileToken INVTOK = (FileToken)uval(~0);
    virtual SysStatus freeFileToken(FileToken fileInfo)=0;

protected:
    FSStats st;
    MultiLinkManager multiLinkMgr;
    FreeList freeList;

    // for communication with MM
    PagingTransportRef tref;

public:
    virtual FSStats *getStatistics() { return &st; }

    virtual SysStatus freeServerFile(FSFile::FreeServerFileNode *n) {
	return freeList.freeServerFile(n);
    }
    virtual SysStatus unFreeServerFile(FSFile::FreeServerFileNode *n) {
	return freeList.unFreeServerFile(n);
    }
    virtual SysStatus tryDestroyFreeFiles(uval ticksecs) {
	return freeList.tryDestroyFreeFiles(ticksecs);
    }
    virtual SysStatus setupFileDestruction(uval secsWake=0, uval secsCall = 0) {
	return freeList.setupFileDestruction(secsWake, secsCall);
    }

    /* returns 1 if it succeeded in detaching; 0 if it can't detach now;
     * an error if the file is not found */
    virtual SysStatusUval detachMultiLink(ServerFileRef fref, uval ino);

    /*
     * returns (typically) cached status for token, copied into structure
     * passed in
     */
    virtual SysStatus getStatus(FileToken tok, FileLinux::Stat *status)=0;

    virtual SysStatus fchown(FileToken fileInfo, uid_t uid, gid_t gid)=0;

    virtual SysStatus fchmod(FileToken fileInfo, mode_t mode)=0;

    virtual SysStatus ftruncate(FileToken fileInfo, off_t length)=0;

    /*
     * if ServerFile != NULL, add to files with multiple lists keeping
     * track of
     */
    virtual SysStatus link(FileToken oldFileInfo, FileToken newDirInfo,
			   char *newName,  uval newLen, ServerFileRef fref)=0;

    /*
     * removes the name from directory entry, but does not delete
     * does decrement the number of links to file though, and returns
     * number of links remaining
     */
    virtual SysStatus unlink(FileToken dirInfo, char *name, uval namelen,
			     FileToken fileInfo=INVTOK,
			     uval *nlinkRemain=NULL)=0;

    /* Free data blocks */
    virtual SysStatus deleteFile(FileToken fileInfo)=0;

    virtual SysStatus rename(FileToken oldDirInfo, char *oldName, uval oldLen,
			     FileToken newDirInfo, char *newName, uval newLen,
			     FileToken renamedFinfo)=0;

    virtual SysStatus mkdir(FileToken dirInfo, char *compName, uval compLen,
			    mode_t mode, FileToken *finfo)=0;

    virtual SysStatusUval readlink(FileToken fileInfo, char *buf, 
				   uval bufsize)=0;

    virtual SysStatus symlink(FileToken dirInfo, char *compName, uval compLen,
			      char *oldpath)=0;
	
    virtual SysStatus rmdir(FileToken dirInfo, char *name, uval namelen)=0;

    virtual SysStatus utime(FileToken fileInfo, const struct utimbuf *utbuf)=0;

    /* parameter mode specifies permission and file type */
    virtual SysStatus createFile(FileToken dirInfo, char *name, uval namelen,
				 mode_t mode, FileToken *fileInfo,
				 FileLinux::Stat *status=NULL)=0;

    /* FIXME: this version of createFile is meant only for file systems
     * that want to associate an arbitrary process with the file. This
     * could be in a subclass */
    virtual SysStatus createFile(FileToken dirInfo, char *name, uval namelen,
				 mode_t mode, ObjectHandle vfoh,
				 FileLinux::Stat *status=NULL) {
	return _SERROR(2148, 0, ENOSYS);
    }

    /*
     * This is the operation really used to get files in normal
     * operation. In the case of multiple links, it will search for
     * ServerFile and return it. If it did not return a server file,
     * ref is NULL, and (on success) entryInfo has a valid fsFile.
     *
     * In the case where a ServerFile is returned (i.e., for file with multiple
     * links), a lock is also returned. This lock is acquired by
     * getFSFileOrServerFile and it will be released by the user of this
     * method once it's done with registering itself with the Serverfile.
     * This guarantees that the ServerFile won't disappear until it knows
     * about its "new client"
     */
    virtual SysStatus getFileTokenOrServerFile(
	FileToken dirInfo, char *entryName, uval entryLen,
	FileToken &entryInfo, ServerFileRef &ref,
	MultiLinkMgrLock* &lock,
	FileLinux::Stat *status=NULL)=0;

    /* for file systems where "external" operation can invalidate the
     * token or make the (possibly) cached stat data out of date (e.g. NFS)
     */
    virtual SysStatus reValidateToken(FileToken fileInfo,
				      FileLinux::Stat *status=NULL)=0;
    virtual SysStatus revalidate(FileToken dirInfo, char *name, uval namelen,
				 FileLinux::Stat *status) {
	// file systems that need this have to redefine it
	return _SERROR(2675, 0, EINVAL);
    }

    virtual SysStatusUval getDents(FileToken fileInfo, uval &cookie,
				   struct direntk42 *buf, uval len)=0;

    virtual SysStatus openCreateServerFile(ServerFileRef &ref,
					   uval oflag,
					   ProcessID pid,
					   ObjectHandle &oh,
					   uval &useType,
					   TypeID &type) {
	return _SERROR(2485, 0, ENOSYS);
    }

    // create a server file object to represent this block file
    virtual SysStatus createServerFileBlock(ServerFileRef &ref,
					    FSFile *fileInfo)=0;

    // create a server file object to represent this block file
    virtual SysStatus createServerFileChar(ServerFileRef &ref,
					   FSFile *fileInfo) {
	tassertWrn(0, "Char files not supported.\n");
	BREAKPOINT;
	return _SERROR(2465, 0, EPERM);
    }

    // create a directory object
    virtual SysStatus createDirLinuxFS(DirLinuxFSRef &rf,
				       PathName *pathName, uval pathLen,
				       FSFile *theInfo,
				       DirLinuxFSRef par)=0;

    virtual SysStatus open(PathName *pathName, uval pathLen, uval flags,
			   mode_t mode, FileToken finfo)=0;

    virtual SysStatus statfs(struct statfs *buf) {
	// file system specific code should define this if they offer statfs
	return _SERROR(2464, 0, ENOSYS);
    }

    /*
     * First generic implementation, get rid of this generic implementation
     * and combine into NFS and other implementations
     */
    class FSFileGlobal : public FSFile {
    protected:
	FileToken token;
	FileSystemRef fs;
    public:
	DEFINE_GLOBAL_NEW(FSFileGlobal);
#ifdef GATHERING_STATS
	FSFileGlobal(FileSystemRef fsr, FileToken t,
		     FSStats *s) : FSFile(s), token(t), fs(fsr) {
	    /* empty body */
	}
#else
	FSFileGlobal(FileSystemRef fsr, FileToken t,
		     FSStats *s) : token(t), fs(fsr) {
	    /* empty body */
	}
#endif /* GATHERING_STATS */

	FileSystemRef getFS() { return fs; }
	virtual FileToken getToken() { return token; }

	virtual uval getFSFileType() { return FSFILE_NONE; }

	// virtual function so can create FSFile of same type as we are
	// in derived classes
	virtual FSFile *createFSFile(FileSystemRef fsr,
				     FileToken t) {
#ifdef GATHERING_STATS
	    return new FSFileGlobal(fsr, t, DREF(fsr)->getStatistics());
#else  /* GATHERING_STATS */
	    return new FSFileGlobal(fsr, t, NULL);
#endif /* GATHERING_STATS */
	}

	virtual SysStatus reValidateToken(FileToken fileInfo,
					  FileLinux::Stat *status=NULL) {
	    return DREF(fs)->reValidateToken(fileInfo, status);
	}

	virtual SysStatus revalidate(char *name, uval namelen,
				     FileLinux::Stat *status) {
	    return DREF(fs)->revalidate(token, name, namelen, status);
	}

	virtual SysStatus destroy() {
	    DREF(fs)->freeFileToken(token);
	    delete this;
	    return 0;
	}

	virtual SysStatus getStatus(FileLinux::Stat *status) {
	    return DREF(fs)->getStatus(token, status);
	}

	virtual SysStatus fchown(uid_t uid, gid_t gid) {
	    return DREF(fs)->fchown(token, uid, gid);
	}

	virtual SysStatus fchmod(mode_t mode) {
	    return DREF(fs)->fchmod(token, mode);
	}

	virtual SysStatus utime(const struct utimbuf *utbuf) {
	    return DREF(fs)->utime(token, utbuf);
	}

	virtual SysStatus ftruncate(off_t length) {
	    return DREF(fs)->ftruncate(token, length);
	}

	virtual SysStatus link(FSFile *newDirInfo, char *newName,
			       uval newLen, ServerFileRef fref) {
	    return DREF(fs)->link(token, newDirInfo->getToken(),
				  newName, newLen, fref);
	}

	/* Free data blocks */
	virtual SysStatus deleteFile() {
	    return DREF(fs)->deleteFile(token);
	}

	/* ---------- operations on directory ----------- */
	virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf,
				       uval len) {
	    return DREF(fs)->getDents(token, cookie, buf, len);
	}
	/*
	 * removes the name from directory entry, but does not delete
	 * does decrement the number of links to file though, and returns
	 * number of links remaining
	 */
	virtual SysStatus unlink(char *name, uval namelen,
				 FSFile *fileInfo=NULL,
				 uval *nlinkRemain=NULL) {
	    FileToken ft = FileSystemGlobal::INVTOK;
	    if (fileInfo != NULL) ft = fileInfo->getToken();
	    return DREF(fs)->unlink(token, name, namelen, ft, nlinkRemain);
	}
	virtual SysStatus createDirLinuxFS(
	    DirLinuxFSRef &rf, PathName *pathName, uval pathLen,
	    DirLinuxFSRef par) {
	    return DREF(fs)->createDirLinuxFS(rf, pathName, pathLen, this,
					      par);
	}
	virtual SysStatus createServerFileBlock(ServerFileRef &ref) {
	    return DREF(fs)->createServerFileBlock(ref, this);
	}

	virtual SysStatus createServerFileChar(ServerFileRef &ref) {
	    return DREF(fs)->createServerFileChar(ref, this);
	}
	virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
				FSFile **finfo) {
	    SysStatus rc;
	    FileToken ft = FileSystemGlobal::INVTOK;
	    rc = DREF(fs)->mkdir(token, compName, compLen, mode, &ft);
	    if (_SUCCESS(rc)) {
		*finfo = createFSFile(fs, ft);
	    }
	    return rc;
	}
	
	virtual SysStatusUval readlink(char *buf, uval bufsize) {
	    return DREF(fs)->readlink(token, buf, bufsize);
	}

	virtual SysStatus symlink(char *compName, uval compLen,
				  char *oldpath) {
	    return DREF(fs)->symlink(token, compName, compLen, oldpath);
	}
	
	virtual SysStatus rmdir(char *name, uval namelen) {
	    return DREF(fs)->rmdir(token, name, namelen);
	}
	virtual SysStatus rename(char *oldName, uval oldLen,
				 FSFile *newDirInfo, char *newName, uval newLen,
				 FSFile *renamedFinfo) {
	    FileToken ftnd, rnft;
	    ftnd = newDirInfo->getToken();
	    rnft = renamedFinfo->getToken();
	    return DREF(fs)->rename(token, oldName, oldLen, ftnd, newName,
				    newLen, rnft);
	}
	virtual SysStatus createFile(char *name, uval namelen,
				     mode_t mode, FSFile **fileInfo,
				     FileLinux::Stat *status=NULL) {
	    SysStatus rc;
	    FileToken ft = FileSystemGlobal::INVTOK;
	    rc = DREF(fs)->createFile(token, name, namelen, mode, &ft,
				      status);
	    if (_SUCCESS(rc)) {
		*fileInfo = createFSFile(fs, ft);
	    }
	    return rc;
	}
	virtual SysStatus createFile(char *name, uval namelen, mode_t mode,
				     ObjectHandle vfoh,
				     FileLinux::Stat *status=NULL) {
	    return DREF(fs)->createFile(token, name, namelen, mode, vfoh,
					status);
	}

	/*
	 * This is the operation really used to get files in normal
	 * operation. In the case of multiple links, it will search for
	 * ServerFile and return it. If it did not return a server file,
	 * ref is NULL, and (on success) entryInfo has a valid fsFile.
	 *
	 * In the case where a ServerFile is returned (i.e., for file
	 * with multiple links), a lock is also returned. This lock is
	 * acquired by getFileTokenOrServerFile and it will be
	 * released by the user of this method once it's done with
	 * registering itself with the Serverfile.  This guarantees
	 * that the ServerFile won't disappear until it knows about
	 * its "new client"
	 */
	virtual SysStatus getFSFileOrServerFile(
	    char *entryName, uval entryLen, FSFile **entryInfo,
	    ServerFileRef &ref, MultiLinkMgrLock* &lock,
	    FileLinux::Stat *status=NULL) {
	    SysStatus rc;
	    FileToken ft;

	    ft = FileSystemGlobal::INVTOK;

	    rc = DREF(fs)->getFileTokenOrServerFile(
		token, entryName,  entryLen, ft, ref, lock, status);
	    *entryInfo = NULL;
	    if (_SUCCESS(rc)) {
		if (ref == NULL) {
		    *entryInfo = createFSFile(fs, ft);
		}
	    }
	    return rc;
	}

	// operations that go on some list that you can somehow
	// get to from file specific to file system
	virtual SysStatus freeServerFile(FSFile::FreeServerFileNode *n) {
	    return DREF(fs)->freeServerFile(n);
	}
	virtual SysStatus unFreeServerFile(FSFile::FreeServerFileNode *n) {
	    return DREF(fs)->unFreeServerFile(n);
	}

	/* returns 1 if it succeeded in detaching; 0 if it can't detach now;
	 * an error if the file is not found */
	virtual SysStatusUval detachMultiLink(ServerFileRef fref, uval ino) {
	    return DREF(fs)->detachMultiLink(fref, ino);
	}

	virtual SysStatus statfs(struct statfs *buf) {
	    // file system specific code should define this if they offer statfs
	    return DREF(fs)->statfs(buf);
	}

	virtual uval isReadOnly() {
	    /* FIXME dilma: the correct approach would be:
	     *     return DREF(fs)->isReadOnly();
	     * but for now let's assume that the file system specific layer
	     * didn't care to define isReadOnly(), then it's not dealing
	     * with read only mounts */
	    return 0; // not read only
	}
    };
protected:
    void init();
};
#endif //__FILE_SYSTEM_GLOBAL_H_
