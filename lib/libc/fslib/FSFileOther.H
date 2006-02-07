#ifndef __FS_FILE_OTHER_H_
#define __FS_FILE_OTHER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileOther.H,v 1.7 2005/05/11 00:12:02 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: This class is used to represent a pipe, socket,
 * or other service implemented in some other server.  It caches an
 * object handle and a reference to a fsFile to the file in the file
 * systme where the name is.  Any name operations get forwarded to the
 * base fsfile, any file specfiic operations are an error, open does
 * some form of give access that is service specific. Derived classes,
 * implemented below, are pipes and sockets.
 **************************************************************************/

#include <fslib/FSFile.H>

// class operates on a file, pure virtual class
class FSFileOther : public FSFile {
protected:
    FSFile *namedFile;			// the file in the name tree
    ObjectHandle otherOH;
public:
    FSFileOther(FSFile *nf) {otherOH.init(); namedFile = nf;}

    virtual FileToken getToken() {
	return namedFile->getToken();
    }
    virtual uval getFSFileType() { 
	passertMsg(0, "imp derived\n"); return 0;
    }
    virtual SysStatus destroy() { 
	namedFile->destroy();
	// invoked when file is unlinked
	return 0;
    }
    virtual SysStatus getStatus(FileLinux::Stat *status) { 
	return namedFile->getStatus(status);
    }
    virtual SysStatus fchown(uid_t uid, gid_t gid) { 
	return namedFile->fchown(uid, gid);
    }
    virtual SysStatus fchmod(mode_t mode) { 
	return namedFile->fchmod(mode);
    }
    virtual SysStatus utime(const struct utimbuf *utbuf) { 
	return namedFile->utime(utbuf);
    }
    virtual SysStatus ftruncate(off_t length) { 
	return namedFile->ftruncate(length);
    }
    virtual SysStatus link(FSFile *newDirInfo, char *newName,
			   uval newLen, ServerFileRef fref) { 
	passertMsg(0, "NYI\n"); return 0;
    }

    /* Free data blocks */
    virtual SysStatus deleteFile() { 
	passertMsg(0, "NYI\n"); return 0;
    }

    /* ---------- operations on directory ----------- */
    virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf,
				   uval len) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    // get rid of this in base class
    virtual SysStatus getFSFile(PathName *pathName, uval pathLen,
				PathName *pathNameTo,
				FSFile **file,
				FileLinux::Stat *status=NULL) { 
	passertMsg(0, "NYI\n"); return 0;
    }

    virtual SysStatus revalidate(char *name, uval namelen,
				 FileLinux::Stat *status) { 
	passertMsg(0, "NYI\n"); return 0;
    }

    /*
     * removes the name from directory entry, but does not delete
     * does decrement the number of links to file though, and returns
     * number of links remaining
     */
    virtual SysStatus unlink(
	char *name, uval namelen, FSFile *fileInfo=NULL,
	uval *nlinkRemain=NULL) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus createDirLinuxFS(
	DirLinuxFSRef &rf, PathName *pathName, uval pathLen,
	DirLinuxFSRef par) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus createServerFileBlock(ServerFileRef &ref) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus createServerFileChar(ServerFileRef &ref) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus openCreateServerFile(
	ServerFileRef &ref, uval oflag, ProcessID pid,
	ObjectHandle &oh, uval &useType, TypeID &type) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
			    FSFile **finfo) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatusUval readlink(char *buf, uval bufsize) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus symlink(char *compName, uval compLen,
			      char *oldpath) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus rmdir(char *name, uval namelen) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus rename(char *oldName, uval oldLen,
			     FSFile *newDirInfo, char *newName, uval newLen,
			     FSFile *renamedFinfo) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus createFile(char *name, uval namelen,
				 mode_t mode, FSFile **fileInfo,
				 FileLinux::Stat *status=NULL) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    /* FIXME: this version of createFile is meant only for file systems
     * that want to associate an arbitrary process with the file. This
     * could be in a subclass */
    virtual SysStatus createFile(char *name,
				 uval namelen, mode_t mode,
				 ObjectHandle vfoh,
				 FileLinux::Stat *status=NULL) { 
	passertMsg(0, "NYI\n"); return 0;
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
	FileLinux::Stat *status=NULL) { 
	passertMsg(0, "NYI\n"); return 0;
    }

    // operations that go on some list that you can somehow
    // get to from file specific to file system
    virtual SysStatus freeServerFile(FSFile::FreeServerFileNode *n) { 
	passertMsg(0, "NYI\n"); return 0;
    }
    virtual SysStatus unFreeServerFile(FSFile::FreeServerFileNode *n) { 
	passertMsg(0, "NYI\n"); return 0;
    }

    /* returns 1 if it succeeded in detaching; 0 if it can't detach now;
     * an error if the file is not found */
    virtual SysStatusUval detachMultiLink(ServerFileRef fref, uval ino) { 
	passertMsg(0, "NYI\n"); return 0;
    }

    virtual SysStatus statfs(struct statfs *buf) { 
	passertMsg(0, "NYI\n"); return 0;
    }

    virtual SysStatus sync() { 
	passertMsg(0, "NYI\n"); return 0;
    }

    virtual uval isReadOnly() { 
	passertMsg(0, "NYI\n"); return 0;
    }
};

/**
 * Class that actually implements pipes, main function here is creation
 */
class FSFileOtherPipe : public FSFileOther {
public: 
    DEFINE_GLOBAL_NEW(FSFileOtherPipe);
    FSFileOtherPipe(FSFile *nf):FSFileOther(nf){};

    virtual uval getFSFileType() { return FSFile::FSFILE_PIPE; }

    virtual SysStatus openCreateServerFile(
	ServerFileRef &ref, uval oflag, ProcessID pid,
	ObjectHandle &oh, uval &useType, TypeID &type);
};
    
// Class that actually implements socket
class FSFileOtherSocket : public FSFileOther {
public: 
    DEFINE_GLOBAL_NEW(FSFileOtherSocket);
    FSFileOtherSocket(FSFile *nf):FSFileOther(nf){};

    virtual uval getFSFileType() { return FSFile::FSFILE_SOCK; }

    virtual SysStatus openCreateServerFile(
	ServerFileRef &ref, uval oflag, ProcessID pid,
	ObjectHandle &oh, uval &useType, TypeID &type);

    virtual SysStatus bind(ObjectHandle oh);
    virtual SysStatus getSocketObj(ObjectHandle &oh, ProcessID pid);
};

#endif /* #ifndef __FS_FILE_OTHER_H_ */
