#ifndef __FILE_INFO_VIRT_FS_H_
#define __FILE_INFO_VIRT_FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileInfoVirtFS.H,v 1.19 2005/04/21 20:31:56 okrieg Exp $
 *****************************************************************************/

#include <io/FileLinux.H>
#include <fslib/ServerFile.H>
#include <io/VirtFile.H>
#include <time.h>
#include <cobj/CObjRootSingleRep.H>
#include <fslib/FSFile.H>
#include <sync/FairBLock.H>


class ServerFileVirtFS;
typedef ServerFileVirtFS** ServerFileVirtFSRef ;

class ServerFileDirVirtFS;
typedef ServerFileDirVirtFS** ServerFileDirVirtFSRef;

class FileInfoVirtFSDirBase;
typedef FileInfoVirtFSDirBase FileInfoVirtFSDir;
class FileInfoVirtFS;

struct FileTokenData{
    uval fileDesc;		//Specifies file
    FileInfoVirtFS* ref;	//Specifies who can answer questions about file
};

struct DirEntry : public AutoListNode {
    char name[PATH_MAX+1];
    uval namelen;
    uval fileNumber;
    FileTokenData *finfo;
    DEFINE_GLOBAL_NEW(DirEntry);
    virtual void dup(DirEntry &de) {
	memcpy(de.name, name, namelen);
	de.namelen = namelen;
	de.fileNumber = fileNumber;
	de.finfo = finfo;
	AutoListNode::dup(de);
    };
};

//Skeleton class matching virtual methods defined by FileInfoVirtFS
class FileInfoVirtFSBase: public FSFile {
protected:
    virtual SysStatus locked_setParent(FileInfoVirtFSDir* dir) = 0;
    virtual void init(mode_t mode, uval number=0) = 0;
    struct globalStruct;
    static globalStruct *glob;

public:
    typedef FairBLock LockType;
    static void ClassInit();
    virtual SysStatusUval detachMultiLink(ServerFileRef fref, uval ino);


    virtual SysStatusUval getServerFileType()=0;
    virtual SysStatus deleteFile()=0;
    virtual SysStatus getNumber(__ino64_t &num)=0;
    virtual SysStatus openCreateServerFile(ServerFileRef &fref,
					   uval oflag,
					   ProcessID pid,
					   ObjectHandle &oh,
					   uval &useType,
					   TypeID &type) = 0;
    virtual SysStatus lockGetStatus(FileLinux::Stat *stat) = 0;
    virtual SysStatus unlockPutStatus(FileLinux::Stat *stat) = 0;
    virtual SysStatus getStatus(FileLinux::Stat *stat) = 0;
    virtual SysStatus createServerFileBlock(ServerFileRef &fref) = 0;

    virtual SysStatus getVirtFile(VirtFile* &ref) = 0;
    virtual SysStatus getOH(ObjectHandle &oh) = 0;
    virtual SysStatus getParent(FileInfoVirtFSDir* &dir) = 0;
    virtual SysStatus setParent(FileInfoVirtFSDir* dir) = 0;
    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
			    FSFile **finfo) = 0;
    virtual SysStatus unlink(char *name, uval namelen,
			     FSFile* finfo=NULL, uval *nlinkRemain=NULL) = 0;
    virtual SysStatus rmdir(char *name, uval namelen) = 0;
    virtual SysStatus rename(char *oldName, uval oldLen,
			     FSFile *newDirInfo, char *newName, uval newLen,
			     FSFile *renamedFinfo) = 0;
    virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf,
				   uval len) = 0;

    virtual SysStatus createDirLinuxFS(DirLinuxFSRef &rf, PathName *pathName,
				       uval pathLen, DirLinuxFSRef par) {
	return _SERROR(2579, 0, ENOTDIR);
    }

    virtual SysStatus add(const char *nm, uval len, FileInfoVirtFS* finfo) {
	return _SERROR(2574, 0, ENOTDIR);
    }
    // These are called from ServerFile.C when clients go away
    virtual SysStatus freeServerFile(FreeServerFileNode *n) {
	// FIXME: not doing anything for VirtFS yet
	return 0;
    }
    virtual SysStatus unFreeServerFile(FreeServerFileNode *n) {
	// FIXME: not doing anything for VirtFS yet
	return 0;
    }
    virtual SysStatus createFile(char *name, uval namelen,
				 mode_t mode, FSFile** fileInfo,
				 FileLinux::Stat *status=NULL) {
	tassertWrn(0, "VirtFS createFile shouldn't be invoked\n");
	return _SERROR(2134, 0, ENOSYS);
    }
    virtual SysStatus getFSFileOrServerFile(char *entryName, uval entryLen,
					    FSFile **entryInfo,
					    ServerFileRef &ref,
					    MultiLinkMgrLock* &lock,
					    FileLinux::Stat *status=NULL) {
	return _SERROR(2580, 0, ENOTDIR);
    }


};

// Base class object that represents entries in a virtFS file system
class FileInfoVirtFS : public FileInfoVirtFSBase{
public:
    union ActiveRefUnion {
	ServerFileRef serverFile;
	ServerFileVirtFSRef virtFS;
	ServerFileDirVirtFSRef dir;
    };
protected:
    static uval nextFileNumber;
    static uval GetFileNumber() {
	uval fn = FetchAndAdd(&nextFileNumber, 1);
        return fn;
    }
    LockType lock;
    FileLinux::Stat status;
    uval fileNumber;
    FileInfoVirtFSDir* parentDir;


    ActiveRefUnion active;

    friend class FileInfoVirtFSDirBase;
    friend class FileInfoVirtFSDirStatic;
    virtual SysStatus locked_setParent(FileInfoVirtFSDir* dir) {
	parentDir = dir;
	return 0;
    }

public:
    DEFINE_GLOBAL_NEW(FileInfoVirtFS);
    virtual void init(mode_t mode, uval number=0);
    virtual uval getToken() { return (uval)this; };
    virtual SysStatus fchown(uid_t uid, gid_t gid) {
	// FIXME: Need to be able properly chown/chmod /dev/pts/* devices
	//	return _SERROR(2551, 0, EPERM);
	return 0;
    }
    virtual SysStatus fchmod(mode_t mode) {
	// FIXME: Need to be able properly chown/chmod /dev/pts/* devices
	//	return _SERROR(2551, 0, EPERM);
	return 0;
    }
    virtual SysStatus utime(const struct utimbuf *utbuf) {
	return _SERROR(2553, 0, EPERM);
    }
    virtual SysStatus ftruncate(off_t length) {
	return _SERROR(2554, 0, EPERM);
    }
    virtual SysStatus link(FSFile *newDirInfo, char *newName,
			   uval newLen, ServerFileRef fref) {
	return _SERROR(2555, 0, EPERM);
    }
    virtual uval isReadOnly() { return 0; };


    virtual SysStatus destroy();
    FileInfoVirtFS() { }
    virtual ~FileInfoVirtFS() {};

    FileInfoVirtFS(mode_t mode,  uval number=0) {
	init(mode, number);
    }
    //Type of ServerFile object created.
    // Local -- ServerFile talks to object in this address space
    // Remote -- ServerFile talks to object in another address space
    // VirtFSInfo -- ServerFile talks to FileInforVirtFS type object
    enum { VirtLocal, VirtRemote, VirtFSInfo, None };
    virtual SysStatusUval getServerFileType()=0;
    virtual SysStatus deleteFile()=0;
    virtual SysStatus getNumber(__ino64_t &num) {
	num = fileNumber;
	return 0;
    }

    virtual SysStatus setActiveServerFile(void** ref) {
	active.serverFile = (ServerFileRef)ref;
	return 0;
    }

    // Provides an OH that is returned to FileLinux::Create
    // May provide a ServerFile ref (but that may not be necessary
    // depending on "type" and the type of the OH.
    virtual SysStatus openCreateServerFile(ServerFileRef &fref,
					   uval oflag,
					   ProcessID pid,
					   ObjectHandle &oh,
					   uval &useType,
					   TypeID &type);

    //Allow status of file to be edited externally
    //Requires the object to be locked while this occurs.
    //These interfaces will get/put Stat and ensure the object
    //is locked in the meantime (prevents other clients from geting
    //bad state returned by getStatus
    virtual SysStatus lockGetStatus(FileLinux::Stat *stat) {
	lock.acquire();
	memcpy(stat, &status, sizeof(FileLinux::Stat));
	return 0;
    }
    virtual SysStatus unlockPutStatus(FileLinux::Stat *stat) {
	memcpy(&status, stat, sizeof(FileLinux::Stat));
	lock.release();
	return 0;
    }

    virtual SysStatus getStatus(FileLinux::Stat *stat) {
	AutoLock<LockType> al(&lock);
	memcpy(stat, &status, sizeof(FileLinux::Stat));
	return 0;
    }

    virtual SysStatus createServerFileBlock(ServerFileRef &fref) = 0;

    virtual SysStatus getVirtFile(VirtFile* &ref) {
	return _SERROR(2228, 0, ENOSYS);
    }
    virtual SysStatus getOH(ObjectHandle &oh) {
	return _SERROR(2229, 0, ENOSYS);
    }
    virtual SysStatus getParent(FileInfoVirtFSDir* &dir) {
	AutoLock<LockType> al(&lock);
	dir = parentDir;
	return 0;
    }

    virtual SysStatus setParent(FileInfoVirtFSDir* dir) {
	AutoLock<LockType> al(&lock);
	parentDir = dir;
	return 0;
    }

public:
    virtual SysStatusUval readlink(char *buf, uval bufsize) {
	tassertWrn(0, "In FileInfoVirtFS::readlink, not a directory\n");
        return _SERROR(2652, 0, ENOTDIR);
    }
    virtual SysStatus symlink(char *compName, uval compLen,
			      char *oldpath) {
	tassertWrn(0, "In FileInfoVirtFS::symlink, not a directory\n");
        return _SERROR(2653, 0, ENOTDIR);
    }
    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
			    FSFile **finfo) {
	tassertWrn(0, "In FileInfoVirtFS::mkdir, not a directory\n");
        return _SERROR(2127, 0, ENOTDIR);
    }

    virtual SysStatus unlink(char *name, uval namelen,
			     FSFile* finfo=NULL, uval *nlinkRemain=NULL) {
	return _SERROR(2166, 0, EPERM);
    }

    virtual SysStatus rmdir(char *name, uval namelen) {
	return _SERROR(2155, 0, EPERM);
    }

    virtual SysStatus rename(char *oldName, uval oldLen,
			     FSFile *newDirInfo, char *newName, uval newLen,
			     FSFile *renamedFinfo) {
	return _SERROR(2428, 0, EPERM);
    }

    virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf,
				   uval len) {
	return _SERROR(2138, 0, ENOTDIR);
    }

};


//A file in a virt FS where we know the data contained within the file
//at the time it is create (the data contents of the file are
//determined by the VirtFile object).  Note: data contents are provided
//from the same address space.
class FileInfoVirtFSLocal: public FileInfoVirtFS {
    VirtFile* vfr;
public:
    DEFINE_GLOBALPADDED_NEW(FileInfoVirtFSLocal);
    virtual ~FileInfoVirtFSLocal() {};
    FileInfoVirtFSLocal(mode_t mode, VirtFile* vf):
	FileInfoVirtFS(mode), vfr(vf) {
    }
    virtual SysStatus createServerFileBlock(ServerFileRef &fref);
    virtual SysStatusUval getServerFileType() { return VirtLocal; };
    virtual SysStatus getVirtFile(VirtFile* &ref) {
	ref = vfr;
	return 0;
    }
};

class VirtFSFile;
typedef VirtFSFile **VirtFSFileRef;

//Similar idea to FileInfoVirtFSLocal, except this object acts as both
//the FileInfoVirtFS object, and the VirtFile interface.
class VirtFSFile: public FileInfoVirtFS{
public:
    virtual SysStatusUval getServerFileType() { return VirtFSInfo; };
    virtual SysStatus createServerFileBlock(ServerFileRef &fref);

    // synchronous read interface where offset is passed as argument
    virtual SysStatusUval _readOff(__outbuf(__rc:length) char  *buf,
				   __in uval length, __in uval offset,
				   __in uval userData, uval token = 0)
	__xa(none) {
	return _SERROR(2474, 0, EINVAL);
    }
    // synchronous read interface without offset
    virtual SysStatusUval _read(__outbuf(__rc:buflength) char  *buf,
				__in uval buflength, __in uval userData,
				__in uval token = 0)
	__xa(none) {
	return _SERROR(2475, 0, EINVAL);
    }
    virtual SysStatusUval _write(__inbuf(length) const char *buf,
				 __in uval length, __in uval userData,
				 __in uval token = 0)
	__xa(none) {
	return _SERROR(2476, 0, EINVAL);
    }
    virtual SysStatus _getMaxReadSize(__out uval &max_read_size, uval token=0)
	__xa(none) {
	return _SERROR(2477, 0, EINVAL);
    }
    virtual SysStatus _open(__in uval flags, __in uval userData, uval &token)
	__xa(none) {
	return _SERROR(2478, 0, EINVAL);
    }
    virtual SysStatus _close(__in uval userData, uval token=0)
	__xa(none) {
	return _SERROR(2479, 0, EINVAL);
    }
    virtual SysStatus _setFilePosition(__in sval position, __in uval at,
				       __in uval userData, uval token=0)
	__xa(none) {
	return _SERROR(2480, 0, ENOSYS);
    }

};

class ServerFileDirVirtFS;
typedef ServerFileDirVirtFS **ServerFileDirVirtFSRef;

//Base class for providing directories with FileInfoVirtFS
class FileInfoVirtFSDirBase : public FileInfoVirtFS {
protected:
    virtual SysStatus locked_lookup(const char *name,
				    uval namelen, DirEntry &entry)=0;
    virtual SysStatus removeAll() = 0;
public:
    DEFINE_GLOBAL_NEW(FileInfoVirtFSDirBase);
    FileInfoVirtFSDirBase();
    virtual SysStatus init(mode_t mode);
    virtual ~FileInfoVirtFSDirBase() {};
    virtual SysStatusUval makeEmpty() { return 0; };

    virtual SysStatus lookup(const char *name, uval namelen, DirEntry &entry);

    virtual SysStatus add(const char *nm, uval len, FileInfoVirtFS* finfo) = 0;
    virtual SysStatus remove(FileInfoVirtFS* finfo)=0;

    virtual SysStatusUval getDents(uval &cookie,
				   struct direntk42 *buf, uval len) = 0;

    virtual SysStatusUval getServerFileType()=0;
    virtual SysStatus createServerFileBlock(ServerFileRef &fref)=0;

    virtual SysStatus mkdir(char *compName, uval compLen, mode_t mode,
			    FSFile **finfo);

    virtual SysStatus createDirLinuxFS(DirLinuxFSRef &rf,
				       PathName *pathName, uval pathLen,
				       DirLinuxFSRef par);

    virtual SysStatus getFSFileOrServerFile(char *entryName, uval entryLen,
					    FSFile **entryInfo,
					    ServerFileRef &ref,
					    MultiLinkMgrLock* &lock,
					    FileLinux::Stat *status=NULL);
    virtual SysStatus mountToNameSpace(const char *mpath);
};

//Directory support for cases where directory contents are known at
//all times, as opposed to cases where directory listing is
//dynamically computed with each lookup.  (This object presents
//objects that it finds in a list as the contents of a directory)
class FileInfoVirtFSDirStatic : public FileInfoVirtFSDirBase {
public:
    AutoList entries;
    DEFINE_GLOBAL_NEW(FileInfoVirtFSDirStatic);
    virtual ~FileInfoVirtFSDirStatic() {};
    virtual SysStatus destroy();
    virtual SysStatusUval makeEmpty();
    virtual SysStatus removeAll();
    virtual SysStatus init(mode_t mode) {
	entries.init();
	return FileInfoVirtFSDirBase::init(mode);
    };

    virtual SysStatus locked_lookup(const char *name, uval namelen,
				    DirEntry &entry);

    virtual SysStatus add(const char *nm, uval len, FileInfoVirtFS* finfo);
    virtual SysStatus remove(FileInfoVirtFS* finfo);

    virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf, uval len);
    virtual SysStatus deleteFile();
    virtual SysStatusUval getServerFileType();
    virtual SysStatus createServerFileBlock(ServerFileRef &fref);
};

#ifdef VOLATILEDIR
class FileInfoVirtFSDirVolatile : public FileInfoVirtFSDirBase {
public:
    DEFINE_GLOBAL_NEW(FileInfoVirtFSDirVolatile);


    virtual ~FileInfoVirtFSDirVolatile() {};

    virtual SysStatus locked_lookup(const char *name,
				    uval namelen, DirEntry &entry);

    virtual SysStatus add(const char *nm, uval len, FileInfoVirtFS* finfo);

    virtual SysStatusUval getDents(uval &cookie, struct direntk42 *buf, 
				   uval len);
};
#endif /* VOLATILEDIR */


#endif /* #ifndef __FILE_INFO_VIRT_FS_H_ */
