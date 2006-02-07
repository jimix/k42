#ifndef __SERVER_FILE_BLOCK_NFS_H_
#define __SERVER_FILE_BLOCK_NFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockNFS.H,v 1.84 2005/07/14 21:40:19 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: The root object for a NFS file system file
 ****************************************************************************/

#include "NFSHandle.H"
#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <cobj/XHandleTrans.H>
#include <fslib/ServerFileBlock.H>
#include <stub/StubFRVA.H>
#include "FileSystemNFS.H"

class DirLinuxFS;
class ServerFileBlockNFS;
typedef ServerFileBlockNFS	**ServerFileBlockNFSRef;
typedef DirLinuxFS		**DirLinuxFSRef;

class ServerFileBlockNFS : public ServerFileBlock<StubFRVA> {
protected:
    FileSystemNFS::FSFileNFS *fsfNFS() {
	passertMsg(fileInfo != NULL, "fileInfo is NULL\n");
	return (FileSystemNFS::FSFileNFS *)fileInfo; 
    }
    // defines mininum and maximum values used for timeout
    static SysTime TIMEOUT_MIN, TIMEOUT_MAX;
    /* defines timeout for cached information. The policy is that it
     * starts from TIMEOUT_MIN and doubles each time we revalidate the
     * cached information successfully, up to TIMEOUT_MAX. */
    SysTime timeout;
    // timestamp for last time cached information has been retrieved
    SysTime timestamp;
    // timestamp for last time we updated timeout in terms of doubling it
    SysTime timeoutUpdateStamp;

    // if file is unlinked, we need to store information needed
    // to finally unlink the file on the file system when the last
    // client goes away
    struct RemovalData{
	FileToken dirinfo;
	char name[PATH_MAX+1];
	uval namelen;
	DirLinuxFSRef dirRef;
	DEFINE_GLOBAL_NEW(RemovalData);
    };

    RemovalData *removalPending;

    DEFINE_REFS(ServerFileBlockNFS);

    SysStatus locked_fillPage(uval virtAddr, uval objOffset);

    DEFINE_GLOBALPADDED_NEW(ServerFileBlockNFS);

    // invoked with stubDetachLock locked
    virtual SysStatus locked_createFR();

public:
    // next two functions called from object providing interface to
    // paging system
    virtual SysStatus startWrite(uval virtAddr, uval objOffset,
				 uval len, XHandle xhandle);

    virtual SysStatus startFillPage(uval virtAddr, uval objOffset,
				    XHandle xhandle);

    // FIXME: these functions are called synchronously, and should be deleted
    virtual SysStatus _write(__in uval virtAddr, __in uval objOffset,
			     __in uval len, __XHANDLE xhandle);

    SysStatus locked_moveFile(FSFile *dirinfo, char *name, uval namelen, 
			      DirLinuxFSRef dirRef);

protected:
    virtual SysStatus init(FSFile *finfo, ObjectHandle kptoh);

    /* returns 1 if cached info has timed out, 1 otherwise */
    inline uval shouldRevalidate() {
	if (Scheduler::SysTimeNow() > timestamp + timeout) {
	    return 1;
	} else {
	    return 0;
	}
    }

    virtual SysStatus locked_revalidate();

    void locked_detachFromDirLinuxFS();

public:
    virtual SysStatus _fchown(uval uid, uval gid, __CALLER_PID pid) {
	// init thread specific state to caller creds
	SysStatus rc = FSCreds::Acquire(pid); 
	_IF_FAILURE_RET(rc);
	return fchown((uid_t) uid, (gid_t) gid);
    }
    virtual SysStatus _fchmod(uval mode, __CALLER_PID pid) {
	// init thread specific state to caller creds
	SysStatus rc = FSCreds::Acquire(pid); 
	_IF_FAILURE_RET(rc);
	return fchmod((mode_t) mode);
    }
    virtual SysStatus _ftruncate(uval length, __CALLER_PID pid) {
	// init thread specific state to caller creds
	SysStatus rc = FSCreds::Acquire(pid); 
	_IF_FAILURE_RET(rc);
	return ftruncate((off_t) length);
    }
    static SysStatus Create(ServerFileRef &fref, FSFile * finfo,
			    ObjectHandle kptoh);

    virtual SysStatus open(uval oflag, ProcessID pid, ObjectHandle &oh,
			   uval &useType, TypeID &type);

    virtual SysStatus exportedXObjectListEmpty(); // last client gone away

    virtual SysStatus fchown(uid_t uid, gid_t gid);
    virtual SysStatus unlink(FSFile *dirinfo, char *name,
			     uval namelen, DirLinuxFSRef dirRef);
    virtual SysStatus getStatus(FileLinux::Stat *status);
    virtual SysStatus fchmod(mode_t mode);
    virtual SysStatus ftruncate(off_t length);
    virtual SysStatus link(FSFile *newDirInfo, char *newname, uval newlen, 
			   DirLinuxFSRef newDirRef);

    // functions invoked through DirLinuxFS
    virtual SysStatus utime(const struct utimbuf *utbuf);

    virtual SysStatus explicitFsync();

    // call back from FileSystem
    virtual SysStatus completeWrite(uval virtAddr, uval objOffset,
				    uval len, SysStatus rc);
    virtual SysStatus completeFillPage(uval virtAddr, uval objOffset,
                                       uval len, SysStatus rc);
    // call from kernel to re-open this file
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh, 
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) {
	passertMsg(0, "NYI\n");
	return 0;
    }

    virtual SysStatus revalidate() {
	err_printf("In ServerFileBLockNFS::revalidate\n");
	AutoLock<LockType> al(getLockPtr()); // locks now, unlocks on return
	return locked_revalidate();
    }
};

#endif /* #ifndef __SERVER_FILE_BLOCK_NFS_H_ */

