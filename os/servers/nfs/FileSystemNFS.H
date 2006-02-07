#ifndef __FILE_SYSTEM_NFS_H_
#define __FILE_SYSTEM_NFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemNFS.H,v 1.131 2005/07/21 13:59:35 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <io/DirBuf.H>
#include <io/FileLinux.H>

#include "NFSExport.H"
#include "NFSHandle.H"
#include "NFSClient.H"
#include <dirent.h>

#include <sync/Sem.H>
#include <misc/ListSimpleLocked.H>
#include <fslib/FSFile.H>
#include <fslib/FileSystemGlobal.H>
#include <fslib/FileSystemList.H>
#include <fslib/PagingTransportVA.H>

#include <sys/ProcessLinux.H>
#include <fslib/FSCreds.H>

class NFSFlag;
class NFSMode;
class NFSMount;
class NFSClient;
class ServerFileBlockNFS;
class FileSystemNFS;
typedef ServerFileBlockNFS	**ServerFileBlockNFSRef;
typedef FileSystemNFS		**FileSystemNFSRef;

class FileSystemNFS : public FileSystemGlobal __xbase(Obj) {
__xprivate:
    static SysStatus _Mkfs(__inbuf(serverLen) char *server,
			   __in uval serverLen,
			   __inbuf(rpathLen) char *rpath,
			   __in uval rpathLen,
			   __inbuf(mpathLen) char *mpath,
			   __in uval mpathLen,
			   __in sval uid = -1,
			   __in sval gid = -1,
			   __in uval isCoverable = 1) __xa(none);

    static SysStatus _TestAlive(__inbuf(len) char *mpath, __in uval len)
	__xa(none);
    static SysStatus _PrintStats() __xa(none);
    
#ifndef _IN_STUBGEN
private:

    static FileSystemList instances;

#ifndef _IN_STUBGEN
    friend class ServerFileBlockNFS;
#endif /* #ifndef _IN_STUBGEN */

    uval vaddrOfSpecialRegion;	// address of region used to map
				// phys pages into FS

    struct FileInfoNFS {
	FileLinux::Stat status;
	time_t modtime;
	time_t ctime;
	NFSHandle fhandle;
	NFSClient *client;

	// can't use delete, since non virtual
	static FileInfoNFS *Alloc() {
	    FileInfoNFS *f = (FileInfoNFS *)
		AllocGlobalPadded::alloc(sizeof(FileInfoNFS));
	    /* f->token = (Token) &f->fhandle;*/
	    f->client = NULL;
	    return f;
	}

	static void Free(FileInfoNFS *file) {
	    tassertMsg(file != (FileInfoNFS *)INVTOK, "nothing to free?\n");
	    AllocGlobalPadded::free(file, sizeof(FileInfoNFS));
	}
    };

    uval shouldIntercept() {
	return KernelInfo::ControlFlagIsSet(KernelInfo::NFS_INTERCEPTION);
    }
    /* returns 1 if revaliation requests should go to server, 0 otherwise
     */
    uval shouldRevalidate() {
	return KernelInfo::ControlFlagIsSet(KernelInfo::NFS_REVALIDATION_OFF)==0;
    }

public:
    // this is an inline macro to translate from
    // external token to internal structure
    static FileInfoNFS *FINF(FileToken t) {return (FileInfoNFS *)t;}

private:
    // temporary for performance debugging
    static uval getStatusCounter, reValidateTokenCounter;

    class PagingInfo {
    public:
        enum IOType {READ_IO, WRITE_IO};

	FileInfoNFS *finfo;
	const char *buf;
	uval32 nbytes;
	uval32 offset;
	ServerFileBlockNFSRef sf;
	IOType ioType;

	PagingInfo(FileInfoNFS *f, const char *b, uval32 n, uval32 o,
		   ServerFileBlockNFSRef sfi, IOType t)
            : finfo(f), buf(b), nbytes(n), offset(o), sf(sfi), ioType(t)
        { }

	DEFINE_GLOBAL_NEW(PagingInfo);
    };
    // NOTE: upping this number will cause lots of warnins as async
    //       buffers fill up until we clean up retry mechanisms
    enum {MAX_ASYNC_CONCURRENT = 5};	// max number of concurrent pagers
    sval numberOfPagers;		// number of threads currently paging
    ListSimpleLocked<PagingInfo*,AllocGlobal> pagerInfoList;
    NFSMount *mount;

    //Presently there is only one client instance, and only
    //one method can execute simutaneouly (to protect the
    //glibc rpc client structure) in the client. Pthread
    //threading issues will have to be studied, when there
    //are multiple clients accessing the rpc libraries.

    NFSClient *client;
    Semaphore clientControl;

    NFSHandle rootHandle;
    uval rootPathLen;

    DEFINE_GLOBALPADDED_NEW(FileSystemNFS);

    SysStatus lookup(PathName *pathName, uval pathLen, PathName *pathNameTo,
		     NFSHandle *fhandle, NFSStat &nfsStat);

    SysStatus setAttribute(sattrargs &saargs, FileInfoNFS *finfo);

    // File operations

    // SysStatus create(PathName *pathName, uval pathLen, NFSMode nfsMode,
    // FileInfoNFS *finfo);
    SysStatus create(FileInfoNFS *dirInfo, char *name, uval namelen,
		     NFSMode nfsMode, FileInfoNFS *finfo);

    SysStatus fchown(NFSHandle *fhandle, uval32 uid, uval32 gid,
		     FileInfoNFS *finfo = 0);

    SysStatus fchmod(NFSHandle *fhandle, mode_t mode, FileInfoNFS *finfo=0);

    SysStatus utime(NFSHandle *fhandle, const struct utimbuf *utbuf,
		    FileInfoNFS *finfo = 0);

    SysStatus truncate(FileInfoNFS *dirinfo, char *entryName,
		       uval entryLen,  uval32 length);
    SysStatus ftruncate(NFSHandle *fhandle, uval32 length,
			FileInfoNFS *finfo=0);

    SysStatus mkdir(FileInfoNFS *dinfo, char *compName, uval compLen,
		    NFSMode nfsMode, FileToken *finfo);

    SysStatus link(NFSHandle *oldFileHandle, FileInfoNFS *newDirInfo,
		   char *newName, uval newLen,
		   FileInfoNFS *oldinfo=0, ServerFileRef fref=0);

    SysStatus readlink(NFSHandle *fhandle, char *rlbuf, uval maxbuflen);

    SysStatus symlink(FileInfoNFS *dinfo, char *newname, uval newlen,
		      char *oldname);

    SysStatus revalidate(FileToken fileInfo, FileLinux::Stat *status,
			 uval forceRevalidation);

    // handles any remaining outstanding async i/o
    SysStatus doPendingIO();

    //Use thinwire to provide state information
    static void GetRootHostAndPath(char *host, char *path);
    static void GetUIDAndGID(sval &uid, sval &gid);

    ListSimpleLocked<NFSClient*,AllocGlobal> clientList[Scheduler::VPLimit];

    SysStatus init(char *host, char *proto, char *path, sval gid, sval uid);

    FileSystemNFS() {}
    ~FileSystemNFS();
    static char host[256];
    static char path[PATH_MAX+1];
    static sval defuid;
    static sval defgid;

public:

    virtual SysStatusUval readSynchronous(FileToken finfo, const char *buf,
                                          uval32 nbytes, uval32 offet);
    virtual SysStatus startRead(FileToken finfo, const char *virtAddr,
                                uval32 len, uval32 objOffset,
                                ServerFileBlockNFSRef sf);

    /*
     * FIXME: this should just be an internal function, shouldn't have
     * any need to write a block synchronously, currently there is a
     * synchronous request from FR to file system for some page outs
     * in some gross forward progress cases, FIXME!!!!
     */
    virtual SysStatusUval writeSynchronous(FileToken finfo, const char *buf,
					   uval32 nbytes, uval32 offset);

    virtual SysStatusUval startWrite(FileToken finfo, const char *buf,
				     uval32 nbytes, uval32 offset,
				     ServerFileBlockNFSRef sf);

#if defined(TARGET_mips64)
    // mips on NUMAchine hardware has 1 ethernet packet limit
    enum {RPC_BUF_MAX = 1024};
#elif defined(TARGET_powerpc)
    enum {RPC_BUF_MAX = 8192};
#elif defined(TARGET_amd64)
    enum {RPC_BUF_MAX = 2048};
#elif defined(TARGET_generic64)
    enum {RPC_BUF_MAX = 4096};
#else /* #if defined(TARGET_mips64) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_mips64) */

    static SysStatus ClassInit(VPNum vp);

    static SysStatus Create(char* server,
			    char *remotepath, char *mpath,
			    sval uid = -1, sval gid = -1, uval coverable = 1);

    // call to block starting thread
    static void Block();

    static SysStatus Die();

    virtual SysStatusUval getMMSharedRegionAddr() {
	return vaddrOfSpecialRegion;
    }

    // interface exported by a FileSystem
private:

    // looks up for a component given the directory handle
    SysStatus lookupComp(FileInfoNFS *dirinfo, char *compName,
			 uval compLen, NFSHandle *fhandle, NFSStat &nfsStat);
    NFSClient *getFCL(uval uid = uval(~0), uval gid = uval(~0)) {
	clientControl.P();

#ifdef NFS_AUTHENTICATION_CHANGED
	/* At this point, we either got an uid and gid from the invoker
	 * (if it is a read/write request as a result of startFillPage or
	 * startWRiter) or we have acquired credentials.
	 * This means that we can have NFSClient to use an authentication
	 * object for the uid and gid in question, instead of simply using
	 * the uid/gid for the developer running k42.
	 * We still need to extend NFSClient to be able to associate
	 * authentication objects with uid/gid, instead of using the
	 * one created on file system creation (and cloned for other
	 * instances of NFSClient */

	if (uid != uval(~0)) {
	    tassertMsg(gid != uval(~0), "??");
	    tassertMsg(uid != 888 && uid != 999, "uid %ld\n", uid);
	    err_printf("*%ld", uid);
	} else {
	    ProcessLinux::creds_t *creds = FSCreds::Get(0/*assert*/);
	    if (creds == NULL) {
		/* it's null for operations being performed on behalf
		 * of FileSystemNFS::Create for / and /knfs, i.e.,
		 * from pid 0x1 which is not a linux process */
		err_printf("cred NULL");
	    } else {
		err_printf("uid %ld gid %ld\n", (uval) creds->uid,
			   (uval) creds->gid);
	    }
	}
#endif // #ifdef NFS_AUTHENTICATION_CHANGED
	NFSClient *cl;
	if (!clientList[Scheduler::GetVP()].removeHead(cl)) {
	    cl = client->clone();
	}
	return cl;
    }
    void putFCL(NFSClient* c) {
	clientList[Scheduler::GetVP()].addToEndOfList(c);
	clientControl.V();
    }
    static void SetFCL(FileToken finfo, NFSClient* c) {
	((FileInfoNFS*)finfo)->client = c;
    }

public:
    static NFSHandle *FHPTR(FileToken finfo) {
	return &(((FileInfoNFS*)finfo)->fhandle);
    }
    static NFSHandle *FHPTR(FileInfoNFS *finfo) {
	return &(finfo->fhandle);
    }

    virtual SysStatus freeFileToken(FileToken fi) {
	FileInfoNFS::Free((FileInfoNFS*)fi);
	return 0;
    }

    virtual SysStatus ftruncate(FileToken finfo, off_t length) {
	return ftruncate(FHPTR(finfo), (uval32) length, FINF(finfo));
    }

    // uid_t should be converted into uval32, but they are both defined
    // to be unsigned int now ...

    virtual SysStatus fchown(FileToken finfo, uid_t uid, gid_t gid) {
	return fchown(FHPTR(finfo), (uval32)uid, (uval32)gid, FINF(finfo));
    }

    virtual SysStatus fchmod(FileToken finfo, mode_t mode) {
	return fchmod(FHPTR(finfo), mode, FINF(finfo));
    }

    virtual SysStatus utime(FileToken finfo, const struct utimbuf *utbuf) {
	return utime(FHPTR(finfo), utbuf, FINF(finfo));
    }

    virtual SysStatus mkdir(FileToken dinfo, char *compName, uval compLen,
			    mode_t mode, FileToken *newDirInfo) {
	NFSMode nfsMode(mode);
	return mkdir(FINF(dinfo), compName, compLen, nfsMode, newDirInfo);
    }

    virtual SysStatus rmdir(FileToken dinfo, char *name, uval namelen);

    virtual SysStatus link(FileToken oldFileInfo, FileToken newDirInfo,
			   char *newName, uval newLen, ServerFileRef fref) {
	return link(FHPTR(oldFileInfo), FINF(newDirInfo), newName, newLen,
		    FINF(oldFileInfo), fref);
    }

    virtual SysStatus unlink(FileToken dirinfo, char *name, uval namelen,
			     FileToken finfo=INVTOK,uval *nlinkRemain=NULL);

    virtual SysStatus deleteFile(FileToken fileInfo) {
	// Do nothing for NFS. The server side should take care of actually
	// freeing the deleted inodes.
	return 0;
    }

    virtual SysStatus rename(FileToken oldDirInfo, char *oldName, uval oldLen,
			     FileToken newDirInfo, char *newName, uval newLen,
			     FileToken renamedFinfo);

    virtual SysStatus symlink(FileToken dirInfo, char *compName, uval compLen,
			      char *oldpath) {
	return symlink(FINF(dirInfo), compName, compLen, oldpath);
    }

    virtual SysStatusUval readlink(FileToken fileInfo, char *buf, 
				   uval bufsize) {
	SysStatus rc = readlink(FHPTR(fileInfo), buf, bufsize);
	if (_FAILURE(rc)) return rc;
	return strnlen(buf,bufsize);
    }

    virtual SysStatus open(PathName *pathName, uval pathLen, uval flags,
			   mode_t mode, FileToken finfo) {
	// For NFS, all the "open" work has been done in fslib. This
	// is not even called
	return 0;
    }

    virtual SysStatus createFile(FileToken dirInfo, char *name, uval namelen,
				 mode_t mode, FileToken *fileInfo,
				 FileLinux::Stat *status=NULL) {
	SysStatus rc;
	FileInfoNFS *fi;
	NFSMode nfsMode(mode);
	fi = FileInfoNFS::Alloc();
	*fileInfo = (FileToken)fi;
	// FIXME: free token if failed
	rc = create(FINF(dirInfo), name, namelen, nfsMode, fi);
	if (_SUCCESS(rc)) {
	    if (status) {
		rc = getStatus(*fileInfo, status);
	    }
	}
	return rc;
    }

    // create a server file object to represent this block file
    virtual SysStatus createServerFileBlock(ServerFileRef &ref,
					    FSFile *finfo);

    virtual SysStatus getFileTokenOrServerFile(
	FileToken dirInfo, char *entryName, uval entryLen,
	FileToken &entryInfo, ServerFileRef &ref,
	MultiLinkMgrLock* &lock, FileLinux::Stat *status=NULL);

    /* for file systems where "external" operation can invalidate the
     * token or make the (possibly) cached stat data out of date (e.g. NFS)
     * FIXME: can we get rid of this, i.e., can it be automatic when
     *        getStatus on a token??? */
    virtual SysStatus reValidateToken(FileToken fileInfo,
				      FileLinux::Stat *status=NULL) {
	return revalidate(fileInfo, status, 0);
    }
    
    virtual SysStatus revalidate(FileToken dirInfo, char *name, uval namelen,
				 FileLinux::Stat *status);

    virtual SysStatusUval getDents(FileToken dinfo, uval &cookie,
				   struct direntk42 *buf, uval len);

    // create a directory object
    virtual SysStatus createDirLinuxFS(DirLinuxFSRef &rf,
				       PathName *pathName, uval pathLen,
				       FSFile *theInfo,
				       DirLinuxFSRef par);

    /*
     * returns (typically) cached status for token, copied into structure
     * passed in
     */
    virtual SysStatus getStatus(FileToken tok, FileLinux::Stat *status);

    virtual SysStatus statfs(struct statfs *buf);

    /* renameForUnlink is invoked by ServerFileBlockNFS for unlinking a file
     * with nlink==1 and outstanding clients */
    virtual SysStatus renameForUnlink(FileToken olddinfo, char *name,
				      uval namelen, FileToken finfo,
				      FileToken &dirinfo, char *newname,
				      uval &newlen);


    virtual SysStatus getFSFile(PathName *pathName, uval pathLen,
				PathName *pathNameTo,
				FSFile **file);

    class FSFileNFS : public FileSystemGlobal::FSFileGlobal {
	friend class FileSystemNFS;
	
	DEFINE_GLOBAL_NEW(FSFileNFS);
    public:
	FSFileNFS(FileSystemRef fsr, FileToken t, FSStats *s)
	    : FSFileGlobal(fsr,t,s) {};

	virtual uval getFSFileType() { return FSFILE_NFS; }

	FileSystemNFSRef fsNFS() { return (FileSystemNFSRef)fs; }
	
	// virtual function so can create FSFile of same type as we are
	// in derived classes
	virtual FSFile *createFSFile(FileSystemRef fsr, 
				     FileToken t) {
#ifdef GATHERING_STATS
	    return new FSFileNFS(fsr, t, st);
#else  /* GATHERING_STATS */
	    return new FSFileNFS(fsr, t, 0);
#endif /* GATHERING_STATS */
	}



	virtual SysStatus getFSFile(PathName *pathName, uval pathLen,
				    PathName *pathNameTo,
				    FSFile **file) {
	    return DREF(fsNFS())->getFSFile(pathName, pathLen, pathNameTo,
					    file);
	}

	virtual SysStatusUval readSynchronous(const char *buf, uval32 nbytes, 
					      uval32 offet) {
	    return DREF(fsNFS())->readSynchronous(token, buf, nbytes, offet);
	}
	virtual SysStatus startRead(const char *virtAddr, uval32 len,
				    uval32 objOffset, ServerFileBlockNFSRef sf){
	    return DREF(fsNFS())->startRead(token, virtAddr, len, objOffset,
					    sf);
	}
	virtual SysStatusUval writeSynchronous(const char *buf, uval32 nbytes,
					       uval32 offset) {
	    return DREF(fsNFS())->writeSynchronous(token, buf, nbytes, offset);
	}
	
	virtual SysStatusUval startWrite(const char *buf, uval32 nbytes, 
					 uval32 offset, 
					 ServerFileBlockNFSRef sf) {
	    return DREF(fsNFS())->startWrite(token, buf, nbytes, offset, sf);
	}

	uval getMMSharedRegionAddr() {
	    SysStatusUval rc = DREF(fsNFS())->getMMSharedRegionAddr(); 
	    tassert(_SUCCESS(rc), err_printf("woops\n"));
	    return _SGETUVAL(rc);
	}
	virtual SysStatus renameForUnlink(char *name,
					  uval namelen, FileToken finfo,
					  FileToken &dirinfo, char *newname,
					  uval &newlen) {
	    return DREF(fsNFS())->renameForUnlink(token, name,
						  namelen, finfo,
						  dirinfo, newname, newlen);
	}
    };

    SysStatus printStats();
    
#endif /* #ifndef _IN_STUBGEN */
};

#endif /* #ifndef __FILE_SYSTEM_NFS_H_ */
