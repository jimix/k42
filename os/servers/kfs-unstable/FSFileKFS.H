#ifndef __FSFILEKFS_H_
#define __FSFILEKFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFileKFS.H,v 1.7 2005/04/08 16:29:18 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the per-file system per-file interface,
 * object of this class is exported from the file system specific to
 * the file system independant layers.
 * **************************************************************************/

#ifndef KFS_TOOLS
#include <fslib/FSStats.H>
#include <fslib/fs_defines.H>    // for MS_RDONLY
#include <fslib/FSFile.H>
#endif

#include <sys/vfs.h>             // for 'struct statfs'

#include "RecordMapBase.H"
#include "KFSGlobals.H"
#include "LSOBasic.H"
#include "LSOBasicDir.H"
#include "SuperBlock.H"          // statfs()

/*
 * First generic implementation, get rid of this generic implementation
 * and combine into NFS and other implementations
 */
class FSFileKFS : public FSFile {
    // some number not in use for FS magic number in linux
    static const uval32 KFS_SUPER_MAGIC = 0x42;
    static uval UsesPhysAddr;
protected:
    KFSGlobals *globals;
    union {
	LSOBase *baseLSO;
	LSOBasicDir *dirLSO;
    };
public:
    DEFINE_GLOBAL_NEW(FSFileKFS);
    FSFileKFS(KFSGlobals *g, ObjTokenID *id, RecordMapBase *r) {
	globals = g;
	baseLSO = (LSOBase *)r->getObj(id);
#ifdef GATHERING_STATS
	st = &g->st;
#endif
    }

    /*
     * Deleting a FSFileKFS should cause a "chain reaction" and all
     * KFS objects related to this file (ObjTokens, LSOs, PSOs, etc)
     * will be deleted too.
     */
    virtual ~FSFileKFS () {
	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::~FSFileKFS() IN baseLSO %p\n", baseLSO);

	// SuperBlock::sync may need to guarantee that ServerObjects
	// don't go away while it's trying to flush them
	globals->super->syncLockAcquireR();

	if (baseLSO) {
	    delete baseLSO;
	}

	globals->super->syncLockReleaseR();

	KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		    "FSFileKFS::~FSFileKFS() OUT\n");
    }

    virtual uval getToken() {
	tassertMsg(0, "delete from base class\n");
	return 0;
    }

    ObjTokenID lookup(const char *fileName, int nameLength) {
	ObjTokenID otokID;

	tassertMsg(dirLSO != NULL, "?");
	if (dirLSO->matchDir(fileName, nameLength, &otokID) < 0) {
	    otokID.id = 0;
	}
	return otokID;
    }

    virtual uval getFSFileType() { return FSFILE_KFS; }

    // virtual function so can create FSFile of same type as we are
    // in derived classes
    virtual FSFile *createFSFile(KFSGlobals *g, ObjTokenID *id, 
				 RecordMapBase *r) {
	return new FSFileKFS(g, id, r);
    }

    virtual SysStatus reValidateFSFile(FileLinux::Stat *status=NULL);

    virtual SysStatus destroy() {
	delete this;
	return 0;
    }

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

    /*
     * removes the name from directory entry, but does not delete
     * does decrement the number of links to file though, and returns
     * number of links remaining
     */
    virtual SysStatus unlink(char *name, uval namelen,
			     FSFile *fileInfo=NULL,
			     uval *nlinkRemain=NULL);

    virtual SysStatus createDirLinuxFS(
	DirLinuxFSRef &rf, PathName *pathName, uval pathLen,
	DirLinuxFSRef par);

    virtual SysStatus createServerFileBlock(ServerFileRef &ref);

    // FIXME: why is this is basic interface
    virtual SysStatus createServerFileChar(ServerFileRef &ref) {
	tassertMsg(0, "woops, not supported KFS\n");
	return 0;
    }
    virtual SysStatus openCreateServerFile(
	ServerFileRef &ref, uval oflag, ProcessID pid,
	ObjectHandle &oh, uval &useType, TypeID &type) {
	tassertMsg(0, "woops, not supported KFS\n");
	return 0;
    }
    virtual SysStatus createFile(char *name, uval namelen, mode_t mode,
				 ObjectHandle vfoh,
				 FileLinux::Stat *status=NULL) {
	tassertMsg(0, "woops, not supported KFS\n");
	return 0;
    }
    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
			    FSFile **finfo);

    virtual SysStatus rmdir(char *name, uval namelen);

    virtual SysStatus rename(char *oldName, uval oldLen,
			     FSFile *newDirInfo, char *newName, uval newLen,
			     FSFile *renamedFinfo);

    virtual SysStatus createFile(char *name, uval namelen,
				 mode_t mode, FSFile **fileInfo,
				 FileLinux::Stat *status=NULL);

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
	FileLinux::Stat *status=NULL);

    virtual SysStatus freeServerFile(FSFile::FreeServerFileNode *n);

    virtual SysStatus unFreeServerFile(FSFile::FreeServerFileNode *n);

    /* returns 1 if it succeeded in detaching; 0 if it can't detach now;
     * an error if the file is not found */
    virtual SysStatusUval detachMultiLink(ServerFileRef fref, uval ino);

    virtual SysStatus statfs(struct statfs *buf) {
	memset((void *)buf, 0, sizeof(struct statfs));
	globals->super->statfs(buf);
	buf->f_type = KFS_SUPER_MAGIC;
	return 0;
    }

    virtual SysStatus sync();

    virtual SysStatus writeBlockPhys(uval paddr, uval32 length,
				     uval32 offset);

    virtual SysStatus readBlockPhys(uval paddr, uval32 offset);

    virtual uval isReadOnly() {
	return (globals->super->getFlags() & MS_RDONLY);
    }

    virtual SysStatus fsync();

    virtual SysStatusUval readlink(char *buffer, uval buflen);
    virtual SysStatus symlink(char *linkName, uval linkLen, char *linkValue);
    virtual SysStatus symlink(char *linkName, uval linkLen, char *linkValue,
			      FSFile **newSymlink = NULL);

    static SysStatus PageNotFound();
};
#endif /* #ifndef __FSFILEKFS_H_ */
