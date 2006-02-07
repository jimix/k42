#ifndef __FSFILE_EXT2_H_
#define __FSFILE_EXT2_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileExt2.H,v 1.24 2004/11/04 03:54:05 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

#include <fslib/FSFile.H>
#include <fslib/PagingTransport.H>

class FSFileExt2 : public FSFile {
protected:
    PagingTransportRef tref;  // for communication with MM
    void *mnt;    // pointer to struct vfsmount
    void *dentry; // pointer to dentry associated with this file/dir
                  // ??? Do we need to differentiate between inode and
                  // dentry?
public:
    DEFINE_GLOBAL_NEW(FSFileExt2);
    FSFileExt2(PagingTransportRef ptref, void *m, void *d) 
	: tref(ptref), mnt(m), dentry(d) {}

    virtual uval getToken() {
	tassertMsg(0, "NIY\n");
	return 0;
    }

    /* We implement as a stub because FSFile defines it as pure virtual.  */
    virtual SysStatus destroy() { return 0; }

    virtual SysStatus getStatus(FileLinux::Stat *status);

    virtual SysStatus fchown(uid_t uid, gid_t gid);

    virtual SysStatus fchmod(mode_t mode);

    virtual SysStatus utime(const struct utimbuf *utbuf);

    virtual SysStatus ftruncate(off_t length);

    virtual SysStatus link(FSFile *newDirInfo, char *newName,
			   uval newLen, ServerFileRef fref);

    /* Free data blocks */
    virtual SysStatus deleteFile();

    /* ---------- operations on directory ----------- */
    virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf,
				   uval len);

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
	// not needed by ext2
	return _SERROR(2815, 0, EINVAL);
    }

    /*
     * removes the name from directory entry, but does not delete
     * does decrement the number of links to file though, and returns
     * number of links remaining
     */
    virtual SysStatus unlink(char *name, uval namelen,
			     FSFile *fileInfo, uval *nlinkRemain);

    virtual SysStatus createDirLinuxFS(
	DirLinuxFSRef &rf, PathName *pathName, uval pathLen,
	DirLinuxFSRef par);
    
    virtual SysStatus createServerFileBlock(ServerFileRef &ref);

    virtual SysStatus createServerFileChar(ServerFileRef &ref) {
	tassertWrn(0, "Char files not supported.\n");
	return _SERROR(2811, 0, EPERM);
    }
    virtual SysStatus openCreateServerFile(
	ServerFileRef &ref, uval oflag, ProcessID pid,
	ObjectHandle &oh, uval &useType, TypeID &type) {
	return _SERROR(2812, 0, ENOSYS);
    }
    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
		    FSFile **finfo);

    virtual SysStatusUval readlink(char *buf, uval bufsize);

    virtual SysStatus symlink(char *compName, uval compLen, char *oldpath);

    virtual SysStatus rmdir(char *name, uval namelen);

    virtual SysStatus rename(char *oldName, uval oldLen,
			     FSFile *newDirInfo, char *newName, uval newLen,
			     FSFile *renamedFinfo);

    virtual SysStatus createFile(char *name, uval namelen, mode_t mode,
				 FSFile **fileInfo,
				 FileLinux::Stat *status = NULL);

    /* FIXME: this version of createFile is meant only for file systems
     * that want to associate an arbitrary process with the file. This
     * could be in a subclass */
    virtual SysStatus createFile(char *name,
				 uval namelen, mode_t mode,
				 ObjectHandle vfoh,
				 FileLinux::Stat *status=NULL) {
	return _SERROR(2813, 0, ENOSYS);
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
	FileLinux::Stat *status=NULL);

    // operations that go on some list that you can somehow
    // get to from file specific to file system
    virtual SysStatus freeServerFile(FreeServerFileNode *n)  {
	tassertWrn(0, "freeServerFile NIY, not important\n");
	return 0;
    }
    virtual SysStatus unFreeServerFile(FreeServerFileNode *n) {
	tassertWrn(0, "unFreeServerFile NIY, not important\n");
	return 0;
    }

    /* returns 1 if it succeeded in detaching; 0 if it can't detach now;
     * an error if the file is not found */
    virtual SysStatusUval detachMultiLink(ServerFileRef fref, uval ino)  {
	tassertMsg(0, "NIY\n");
	return 0;
    }

    virtual SysStatus statfs(struct statfs *buf);

    virtual SysStatus sync();

    virtual uval isReadOnly();

    virtual SysStatus updateFileLength(uval fileLength);

    virtual SysStatus writeBlockPhys(uval paddr, uval32 length,
				     uval32 offset);

    virtual SysStatus readBlockPhys(uval paddr, uval32 offset);

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
#endif /* #ifndef __FSFILE_EXT2_H_ */
