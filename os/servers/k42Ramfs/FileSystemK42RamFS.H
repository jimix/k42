#ifndef __FILE_SYSTEM_K42_RAM_FS_H_
#define __FILE_SYSTEM_K42_RAM_FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemK42RamFS.H,v 1.21 2005/04/12 19:13:55 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: interface to k42 ramfs
 ****************************************************************************/

#include <io/FileLinux.H>
#include "FileInfoK42RamFS.H"
#include <fslib/FileSystemGlobal.H>

#include <fslib/FileSystemList.H>
#include <fslib/PagingTransportPA.H>

class FileSystemK42RamFS : public FileSystemGlobal __xbase(Obj) {
private:
#ifndef _IN_STUBGEN
    // some number not in use for FS magic number in linux
    static const uval32 K42_K42RAMFS_SUPER_MAGIC = 0xadadad;
    static FileSystemList instances;
#endif //#ifndef _IN_STUBGEN

    // padded because only one, so might as well
    DEFINE_GLOBALPADDED_NEW(FileSystemK42RamFS);

    FileSystemRef init();

    SysStatus lookup(PathName *pathName, uval pathLen, PathName *pathNameTo,
		     FileToken *entryToken);
    SysStatus lookup(FileToken dirToken, char *entryName, uval entryLen,
		     FileToken *entryToken);

    FileToken root;

    SysStatus printStats();

public:

    static FileInfoK42RamFS* FINF(FileToken tk) {
	return (FileInfoK42RamFS*) tk; }
    static FileInfoK42RamFSDir* DINF(FileToken tk) {
	return (FileInfoK42RamFSDir*) tk;
    }
    static FileInfoK42RamFSSymLink* SINF(FileToken tk) {
	return (FileInfoK42RamFSSymLink*) tk;
    }

    static void Block();

    static SysStatus ClassInit(VPNum vp);
    static SysStatus Create(VPNum vp, char *mpath, uval isCoverable = 1);

    virtual SysStatus freeFileToken(FileToken fileToken);

    virtual SysStatus fchown(FileToken fileToken, uid_t uid, gid_t gid);
    virtual SysStatus chown(FileToken dirToken, char *entryName,
			    uval entryLen, uid_t uid, gid_t gid);

    virtual SysStatusUval readlink(FileToken fileInfo, char *buf,
				   uval bufsize);
    virtual SysStatus symlink(FileToken dirInfo, char *compName, uval compLen,
			      char *oldpath);
    virtual SysStatus fchmod(FileToken fileToken, mode_t mode);
    virtual SysStatus chmod(FileToken dirToken, char *entryName,
			    uval entryLen, mode_t mode);

    virtual SysStatus ftruncate(FileToken fileToken, off_t length);
    virtual SysStatus truncate(FileToken dirToken, char *entryName,
			       uval entryLen, off_t length);

    virtual SysStatus link(FileToken oldFileInfo, FileToken newDirInfo,
			   char *newName,  uval newLen, ServerFileRef fref);

    /* removes the name from directory entry */
    virtual SysStatus unlink(FileToken dirToken, char *name, uval namelen,
			     FileToken ftoken=INVTOK, uval *nlinkRemain=NULL);

    /* Free data blocks */
    virtual SysStatus deleteFile(FileToken fileToken);

    virtual SysStatus rename(FileToken oldDirInfo, char *oldName, uval oldLen,
			     FileToken newDirInfo, char *newName, uval newLen,
			     FileToken renamedFinfo);

    virtual SysStatus mkdir(FileToken dirToken, char *compName,
			    uval compLen, mode_t mode, FileToken *newDirInfo);

    virtual SysStatus rmdir(FileToken dirToken, char *name, uval namelen);

    virtual SysStatus utime(FileToken fileToken, const struct utimbuf *utbuf);
    virtual SysStatus utime(FileToken dirToken, char *entryName, uval entryLen,
			    const struct utimbuf *utbuf);

    virtual SysStatus createFile(FileToken dirToken, char *name, uval namelen,
				 mode_t mode, FileToken *fileToken,
				 FileLinux::Stat *status=NULL);

    virtual SysStatus getFileTokenOrServerFile(
	FileToken dirToken, char *entryName, uval entryLen,
	FileToken &entryInfo, ServerFileRef &ref,
	MultiLinkMgrLock* &lock,
	FileLinux::Stat *status=NULL);

    /*
     * returns (typically) cached status for token, copied into structure
     * passed in
     */
    virtual SysStatus getStatus(FileToken tok, FileLinux::Stat *status);

    // re-validate token, e.g., checks if changed for a file system like NFS
    // FIXME: can we get rid of this, i.e., can it be automatic when
    //        getStatus on a token???
    virtual SysStatus reValidateToken(FileToken fileToken,
				      FileLinux::Stat *status=NULL) {
	// nothing to do in ramfs for revalidate besides returning status
	return getStatus(fileToken, status);
    }
    virtual SysStatusUval getDents(FileToken fileToken, uval &cookie,
				   struct direntk42 *buf, uval len);

    // create a server file object to represent this block file
    virtual SysStatus createServerFileBlock(ServerFileRef &ref,
					    FSFile *fsFile);

    // FIXME: get rid of this function (Note: VFS has an open ...)
    virtual SysStatus open(PathName *pathName, uval pathLen, uval flags,
			   mode_t mode, FileToken finfo);

    // create a directory object
    virtual SysStatus createDirLinuxFS(DirLinuxFSRef &rf,
				       PathName *pathName, uval pathLen,
				       FSFile *fsFile,
				       DirLinuxFSRef par);

    virtual SysStatus unFreeServerFile(FSFile::FreeServerFileNode *n) {
	// nothing to do
	return 0;
    }
    virtual SysStatusUval tryDestroyFreeFiles(uval ticksecs) {
#ifndef _IN_STUBGEN
	tassertMsg(0, "look at this\n");
#endif //#ifndef _IN_STUBGEN
	return 0;
    }
    virtual  SysStatus freeServerFile(FSFile::FreeServerFileNode *n) {
	// nothing to do
	return 0;
    }

    virtual SysStatus statfs(struct statfs *buf);

#ifndef _IN_STUBGEN
    class FSFileK42RamFS : public FileSystemGlobal::FSFileGlobal {
    public:
	DEFINE_GLOBAL_NEW(FSFileK42RamFS);

	FSFileK42RamFS(FileSystemRef fsr, FileToken t, FSStats *s)
	    : FSFileGlobal(fsr,t,s) {}

	virtual uval getFSFileType() { return FSFILE_K42RAMFS; }

	// virtual function so can create FSFile of same type as we are
	// in derived classes
	virtual FSFile *createFSFile(FileSystemRef fsr, 
				     FileToken t) {

#ifdef GATHERING_STATS
	    return new FSFileK42RamFS(fsr, t, st);
#else  /* GATHERING_STATS */
	    return new FSFileK42RamFS(fsr, t, 0);
#endif /* GATHERING_STATS */
	}
    };
#endif /* _IN_STUBGEN */

__xprotected:
    static SysStatus _Mkfs(__inbuf(mpathLen) char *mpath,
			   __in uval mpathLen) __xa(none);
    static SysStatus _TestAlive(__inbuf(len) char *mpath, __in uval len)
	__xa(none);
    static SysStatus _PrintStats() __xa(none);
};
#endif /* #ifndef __FILE_SYSTEM_RAM_FS_H_ */

