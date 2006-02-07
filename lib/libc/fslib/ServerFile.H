#ifndef __SERVER_FILE_H_
#define __SERVER_FILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFile.H,v 1.124 2005/07/14 21:36:59 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic common implementation of a file in server
 * **************************************************************************/

#include <io/FileLinuxServer.H>
#include <cobj/XHandleTrans.H>
#include <meta/MetaFileLinuxServer.H>
#include <misc/ListSimpleKey.H>
#include <misc/ListSimpleLocked.H>
#include <stub/StubCallBackObj.H>
#include <scheduler/Scheduler.H>
#include <sys/ppccore.H>
#include <fslib/FSFile.H>
#include <sys/ShMemBuf.H>

#include <trace/traceFS.h>

#include <fslib/fs_defines.H>

#ifdef DEBUG_TRACE_SERVER_FILE
#define SFTRACE(method) traceMethod(method);
#define SFTRACE_USETYPE(method, useType) traceUseType(method, useType);
#else // from #ifdef DEBUG_TRACE_SERVER_FILE
#ifdef DEBUG_ONE_FILE
#define SFTRACE(method) printMethod(method);
#define SFTRACE_USETYPE(method, useType) printUseType(method, useType);
#else // neither DEBUG_TRACE_SERVER_FILE or DEBUG_ONE_FILE
#define SFTRACE(method)
#define SFTRACE_USETYPE(method, useType)
#endif // #ifdef DEBUG_ONE_FILE
#endif // #ifdef DEBUG_TRACE_SERVER_FILE

#ifdef DEBUG_USE_TYPE
#ifdef HACK_FOR_FR_FILENAMES
#define SFDEBUG_ARGS(method, args...) { \
                                     char *name; \
                                     char nameUnknown[8] = "UNKNOWN"; \
                                     name = (nameAtCreation ? nameAtCreation \
                                                     : nameUnknown); \
                                     err_printf("%s(%s): ", method, name); \
                                     err_printf(args); \
                                   }
#else
#define SFDEBUG_ARGS(method, args...) err_printf("%s: ", method); \
                                    err_printf(args);
#endif // #ifdef HACK_FOR_FR_FILENAMES
#else
#define SFDEBUG_ARGS(method, args...)
#endif // #ifdef DILMA_USE_TYPE

class DirLinuxFS;
class ServerFile;
typedef DirLinuxFS      **DirLinuxFSRef;
typedef ServerFile      **ServerFileRef;

class ServerFile : public FileLinuxServer {
protected:
    typedef BLock LockType;
    LockType      lock;			// lock on object

    LockType * getLockPtr() { return &lock; }

    DEFINE_REFS(ServerFile);

    DEFINE_ACCESS_SERVER(FileLinuxServer, MetaObj::controlAccess,
			 MetaObj::none);

    typedef ListSimpleLocked<DirLinuxFSRef,AllocGlobal> ParentSet;
    // To keep track of DirLinuxFS instances refering to this ServerFile
    // we use a data structure able to store either a set of references
    // or a single one. This simplifies the consistency checking done
    // for directories.
    class Parent {
    protected:
	union {
	    ParentSet *parentSet; // used for files
	    DirLinuxFSRef theRef; /* used for directories. We keep a different
				   * one to simplify checking for
				   * inconsistencies */
	} u;
    public:
	virtual ParentSet *getParentSet() {
	    tassert(u.parentSet != 0,
		    err_printf("parentSet data structure not there\n"));
	    return u.parentSet;
	}
	// for debugging only
	virtual ParentSet *getParentSetNoTassert() { return u.parentSet; }

	virtual void clearParentSet() {
	    tassert(u.parentSet != 0,
		    err_printf("parentSet data structure already 0\n"));
	    u.parentSet = NULL;
	}
	virtual ParentSet *FetchAndClearParentSet() {
	    ParentSet *retvalue;
	    retvalue = (ParentSet*)
		FetchAndClearVolatile((uval *)&u.parentSet);
	    return (retvalue);
	}
	virtual DirLinuxFSRef getTheRef() {
	    tassert(0, err_printf("getTheRef available only for directory\n"));
	    return 0;
	}
	virtual void setTheRef(DirLinuxFSRef d) {
	    tassert(0, err_printf("setTheRef available only for "
				  "directories\n"));
	}
	virtual DirLinuxFSRef FetchAndClearTheRef() {
	    tassert(0, err_printf("FetchAndClearTheRef available only for "
				  "directories\n"));
	    return 0;
	}
	DEFINE_GLOBAL_NEW(Parent);
	Parent() {
	    u.parentSet = new ListSimpleLocked<DirLinuxFSRef,AllocGlobal>;
	}
	virtual ~Parent() {
	    if (u.parentSet) {
		delete u.parentSet;
	    }
	}
    };

    Parent *parent;

    /* identifies file to lower layers, is INVTOK if invalid) */
    FSFile *fileInfo;

    uval fileLength;

    uval readOnly;      /*
			 * as far as all clients of the file opened
			 * it read only, the offset and length can
			 * be kept in the application, i.e., the file can
			 * behave as having a single client
			 */

    class BlockedQueue {
    protected:
	ListSimple<ThreadID, AllocGlobal> list;
    public:
	DEFINE_GLOBAL_NEW(BlockedQueue);
	void add(ThreadID tid) { list.add(tid); }
	SysStatus remove(ThreadID tid) { return list.remove(tid); }
	uval isEmpty() { return list.isEmpty();	}
	void wakeAll();
    };
    volatile uval useType; /* shared or used by a single client */
    ListSimple<XHandle, AllocGlobal> waitingForClients;
    BlockedQueue waitingQueue;
    static const uval TIMEOUT_TICKS = 2000;
    typedef enum {GETSTATUS, OPEN, REG_CB, ACK_CB,
		  DETACH, LAZYREOPEN, DUP} EventType;

    sval doingDestroy;			// flag, in midst of destroy

    uval removalOnLastClose;

    uval removed;			// maa debug

    // structure queued up when file becomes free
    FSFile::FreeServerFileNode entToFree;

    struct FileLockList {
	// FIXME
	// No record locking just lock the damn thing
	struct FileLockData : public flock {
	    ProcessID processID;
	    XHandle xhandle;
	    void wakeClient();
	    DEFINE_GLOBAL_NEW(FileLockData);
	    FileLockData(flock &f, ProcessID pid,  XHandle xh);
	};
	FileLockData  *currLock;

	ListSimpleKey <XHandle,FileLockList::FileLockData *, AllocGlobal>
	    waiters;
	DEFINE_NOOP_NEW(FileLockList);

	void init(void) {currLock = NULL;}

	SysStatus getLock(struct flock &fileLock);
	SysStatus setLock(struct flock &fileLock, ProcessID pid,
			  XHandle xh);
	SysStatus setLockWait(struct flock &fileLock, uval &key,
			      ProcessID pid, XHandle xh);
	void updateClientData(XHandle xh, sval inc);
	void removeFileLock(XHandle xh);
	void unlock();
    };

    FileLockList fileLockList;

    // class maintains per-client state
    struct ClientData {
	uval flags;			// open flags
	uval useType;     // regarding shared vs non-shared access
	struct SharedOffsetData {
	    ServerFile::LockType lock;
	    uval filePosition;
	    uval usageCount;
	    DEFINE_GLOBAL_NEW(SharedOffsetData);
	    SharedOffsetData(uval pos) {
		lock.init();
		filePosition = pos;
		usageCount = 1;
	    }
	};
	union {
	    uval    filePosition;	/* cursor for this FS
					 * N.B. a file can have several FS
					 * objects with seperate cursors */
	    SharedOffsetData *sharedOffRef; // for sharing of offset
	};
	uval isSharingOffset;

	ServerFile::LockType lock;
	ShMemBufRef smb;
	uval smbAddr;
	uval smbOffset;
	uval smbLen;
	void releaseSharingOffset();
	DEFINE_GLOBAL_NEW(ClientData);

	ClientData() : lockCallBackStub(StubObj::UNINITIALIZED),
		       useTypeCallBackStub(StubObj::UNINITIALIZED) {
	    flags = filePosition = isSharingOffset = 0;
	    useType = FileLinux::INVALID_UT;
	    hasFileLock = 0;
	    smb = NULL;
	    smbAddr = smbLen = smbOffset = 0;
	    lock.init();
	    ObjectHandle foooh;
	    foooh.init();
	    setUseTypeCallBackOH(foooh);
	}

	StubCallBackObj lockCallBackStub;
	void setLockCallBackOH(ObjectHandle oh) {lockCallBackStub.setOH(oh);};
	sval hasFileLock;

	StubCallBackObj useTypeCallBackStub;
	void setUseTypeCallBackOH(ObjectHandle oh) {
	    useTypeCallBackStub.setOH(oh);
	};
	virtual ~ClientData();
    };

    virtual SysStatus init();
    virtual SysStatus giveAccessSetClientData(
	ObjectHandle &oh, ProcessID toProcID,
	AccessRights match, AccessRights nomatch, TypeID type = 0);

    // functions overridden to handle clients going away
    static void BeingFreed(XHandle xhandle);

    inline uval validFileInfo() {
	if (fileInfo == NULL) {
	    return 0;
	}
	return 1;
    }

    void invalidateFileInfo() {
	lock.acquire();
	if (fileInfo != NULL) {
	    fileInfo->destroy();
	    fileInfo = NULL;
	}
	lock.release();
    };

    DEFINE_GLOBALPADDED_NEW(ServerFile);

    void detachFromDirectory();

    virtual uval locked_isThereNonFRClient();
    virtual uval locked_isThereExternalClient();

    // ServerFile base class doesn't involve FRs
    virtual void frInteractionForDeletion() {}

    // ---------- The following methods are defined in ServerFileSharing.C
    SysStatus locked_changeState(EventType event, XHandle xhandle = 0,
				 uval req = FileLinux::CALLBACK_INVALID,
				 uval openForWrite = 0);
    uval locked_changeFromUninitialized(EventType event);
    uval locked_changeFromNonShared(EventType event, XHandle xhandle,
				    uval openForWrite = 0);
    uval locked_inspectClients(FileLinux::RequestType req,
			       uval openForWrite = 0);
    SysStatus locked_contactClient(XHandle xh, ClientData *cl,
				   FileLinux::RequestType req);
    uval locked_changeFromShared(EventType event);
    uval locked_changeFromTransition(EventType event, XHandle xhandle,
				     uval req);
    uval locked_changeFromFixedShared(EventType event);
    uval locked_findClient();
    uval locked_ackFromClient(XHandle xhandle, uval req);
    uval locked_determineState();
    SysStatus locked_waitForState();

#if defined(DEBUG_TRACE_SERVER_FILE) || defined (DEBUG_ONE_FILE)
    // for trace/debugging only
    uval locked_getNumberClients();
#endif // #if defined(DEBUG_TRACE_SERVER_FILE) || defined (DEBUG_ONE_FILE)
    // ---------- end of methods defined in ServerFileSharing.C

#ifdef HACK_FOR_FR_FILENAMES
    char *nameAtCreation;

public:

    virtual void setNameAtCreation(char *name) {
	passertMsg(name != NULL, "ops");
	uval size = strlen(name);
	nameAtCreation = (char *) AllocGlobalPadded::alloc(size+1);
	memcpy(nameAtCreation, name, size+1);
    }
#endif //#ifdef HACK_FOR_FR_FILENAMES

public:

    SysStatus acquireLock() { lock.acquire(); return 0; }
    SysStatus releaseLock() { lock.release(); return 0; }

    /* FileSystem methods such as rename require the file for
     * both "source" (oldpath) and "target" (newpath) of the operation.
     * Therefore we need a way to query a ServerFile about its file token
     */
    virtual SysStatus getFSFile(FSFile **fi) {
	*fi = fileInfo;
	return 0;
    }

    virtual SysStatus revalidate() {
	return 0;
    }

    virtual SysStatus exportedXObjectListEmpty(); // last client gone away
    virtual SysStatus handleXObjFree(XHandle xhandle);

    virtual SysStatus setDirLinuxRef(DirLinuxFSRef d);

    // check for read/write/execute permissions on a file.
    virtual SysStatus permission(uval mask);

    /* detachParent is part of destruction protocol.
     * A ServerFile can detach itself from a parent if it is in a state
     * where it could go away (it does not have any active client).
     * This method is invoked by the DirLinuxFS object in a locked state (the
     * lock is not released until the DirLinuxFS object removes the entry
     * for this ServerFile), so there's guarantee that no work will
     * be delegated to this object
     * while this call is in flight or later on.
     *
     * The method returns 1 if it successfully detached the parent, 0
     * if the parent can't be returned, and an error (value < 0) if
     * the given DirLinuxFSRef does not appear in the parent set.
     */
    virtual SysStatusUval detachParent(DirLinuxFSRef p);

    /* tryDetachParents() is part of destruction protocol.
     * It returns 1 if it succeeded in detaching itself from all its parents,
     * 0 otherwise.
     */
    virtual SysStatusUval tryDetachParents();

    /* detachMultiLink() is invoked by the FileSystem object when trying
     * to remove a ServerFile from the list managed by the MultiLinkMgr.
     * Its task is to verify if there is no possible use of the ServerFile
     * currently, i.e., it does not have clients and it does not have parents.
     *
     * The method returns 1 if it's ok to detach it, 0 otherwise.
     */
    virtual SysStatusUval detachMultiLink();

    /* runFreeProtocol() detects if the file is free for destruction.
     * It returns 1 if file can be destroyed, 0 otherwise.
     */
    virtual SysStatusUval runFreeProtocol(uval nextPhase);

    virtual SysStatus open(uval oflag, ProcessID pid, ObjectHandle &oh,
			   uval &useType, TypeID &type);

    // calls from FileLinux
    virtual SysStatus getFROH(ObjectHandle &oh, ProcessID pid)=0;
    virtual SysStatus getStatus(FileLinux::Stat *status,
				uval isPathBasedRequest = 0);
    virtual SysStatus getDents(struct direntk42 *buf, uval len);

    static const uval NLINK_CHANGED = 2;

    // functions to support internal FileLinux interface
    virtual SysStatus fchown(uid_t uid, gid_t gid);
    virtual SysStatus fchmod(mode_t mode);
    virtual SysStatus ftruncate(off_t length);

    // functions invoked through DirLinuxFS
    virtual SysStatus utime(const struct utimbuf *utbuf);
    virtual SysStatus link(FSFile *newDirInfo, char *newname, uval newlen,
			   DirLinuxFSRef newDirRef);
    virtual SysStatus unlink(FSFile *dirinfo, char *name, uval namelen,
			     DirLinuxFSRef dirRef);
    virtual SysStatus renameSetParents(DirLinuxFSRef oldDirRef,
				       DirLinuxFSRef newDirRef);

    /* The following three methods are used if there is a paging transport
     * with the memory manager (so it really applies to ServerFileBlock and
     * its descendents, therefore returning error here) */
    virtual SysStatus startWrite(uval virtAddr, uval objOffset,
				 uval len, XHandle xhandle) {
	// Each file system is implementing this
	return _SERROR(2846, 0, ENOSYS);
    }

    virtual SysStatus startFillPage(uval virtAddr, uval objOffset,
				    XHandle xhandle) {
	// Each file system is implementing this
	return _SERROR(2847, 0, ENOSYS);
    }
    virtual SysStatus frIsNotInUse() {
	// only file systems with FRs will implement this
	return _SERROR(2848, 0, ENOSYS);
    }

    virtual SysStatus explicitFsync() {
	// not all file systems offer this option, so base class returns error
	return _SERROR(1871, 0, ENOSYS);
    }

    // call from file system to garbage collect free files
    virtual SysStatus tryToDestroy();

    virtual SysStatus destroy() {
	/* So far we have ServerFileBlock::destroy() defined, but not one
	 * for destroying objects that don't have FRs
	 */
	tassertMsg(0, "ServerFile::destroy() NIY\n");
	return 0;
    }

    virtual SysStatus locked_returnUnlocked_cleanForDeletion(DirLinuxFSRef);

#ifdef HACK_FOR_FR_FILENAMES
    virtual SysStatusUval _getFileName(char *buf, uval bufsize);
#endif // #ifdef HACK_FOR_FR_FILENAMES

protected:
    virtual SysStatus locked_setFileLength(uval len);

    // A kludge to type cheat XBaseObj clientData to (protected struct)
    // ClientData
    static ClientData *Clnt(XHandle xhandle) {
	return (ClientData *) (XHandleTrans::GetClientData(xhandle));
    }

    // functions to support server FileLinux interface
    virtual SysStatus _flush();

    virtual SysStatus _getSharedBuf(__out ObjectHandle &oh, __out uval &offset,
				    __inout uval &length, __XHANDLE xhandle,
				    __CALLER_PID pid) {
	return _SERROR(2602, 0, EOPNOTSUPP);
    }

    virtual SysStatus _syncSharedBuf(__in uval newLength,
				     __in uval start, __in uval modLen,
				     __in uval release,
				     __XHANDLE xh, __CALLER_PID pid) {
	return _SERROR(2603, 0, EOPNOTSUPP);
    }

    virtual SysStatusUval _read(char *buf, uval len,
				uval offset, __XHANDLE xhandle);
    virtual SysStatusUval _write(const char *buf, uval len, uval offset,
				 __XHANDLE xhandle);
    virtual SysStatusUval _getDents(char *buf, uval len, __XHANDLE xhandle,
				    __CALLER_PID pid);
    virtual SysStatusUval _setFilePosition(sval position, uval at,
					   __XHANDLE xhandle);
    virtual SysStatus _getStatus(struct stat &status,
				 uval isPathBasedRequest = 0);
    virtual SysStatus _fchown(uval uid, uval gid, __CALLER_PID pid) {
	return fchown((uid_t) uid, (gid_t) gid);
    }
    virtual SysStatus _fchmod(uval mode, __CALLER_PID pid) {
	    return fchmod((mode_t) mode);
    }
    virtual SysStatus _ftruncate(uval length, __CALLER_PID pid) {
	return ftruncate((off_t) length);
    }
    virtual SysStatus _getFROH(__out ObjectHandle &oh, __CALLER_PID pid);

    virtual SysStatus _getLengthOffset(__out uval &flength, __out uval &offset,
				       __in uval isWrite, __in uval opLen,
				       __XHANDLE xhandle);

    virtual SysStatus _setLengthOffset(__in uval flength, __in uval offset,
				       __XHANDLE xhandle);
    virtual SysStatus _dup(__in ProcessID pid,
			   __in uval originalUseType,
			   __out uval &newUseType,
			   __in uval flenth,
			   __in uval offset,
			   __out ObjectHandle &oh,
			   __XHANDLE origXhandle);

    virtual SysStatus _ioctl(__in uval req,
			     __inoutbuf(size:size:size) char* buf,
			     __inout uval &size);

    // System V discretionary locks (see fcntl(2))
    virtual SysStatus _getLock(__inout struct flock &fileLock) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return fileLockList.getLock(fileLock);
    }
    virtual SysStatus _setLock(__inout struct flock &fileLock,
			       __CALLER_PID pid,
			       __XHANDLE xhandle) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return fileLockList.setLock(fileLock, pid, xhandle);
    }
    virtual SysStatusUval _setLockWait(__inout struct flock &fileLock,
				       __out uval &key,
				       __CALLER_PID pid,
				       __XHANDLE xhandle) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return fileLockList.setLockWait(fileLock, key, pid, xhandle);
    }

    virtual SysStatus _registerCallback(ObjectHandle callbackOH,
					__XHANDLE xhandle) {
	tassertMsg(0, "this should not be called\n");
	return 0;
    }

    virtual SysStatus _registerCallback(ObjectHandle callbackOH,
					__in uval callBackType,
					__inout uval &useType,
					__out uval &flength,
					__out uval &offset,
					__XHANDLE xhandle);

    virtual SysStatus _ackUseTypeCallBack(__in uval ut,
					  __in uval fileLength,
					  __in uval fileOffset,
					  __XHANDLE xhandle);

    virtual SysStatus _lazyGiveAccess(__XHANDLE xhandle,
				      __in sval file, __in uval type,
				      __in sval closeChain,
				      __inbuf(dataLen) char *data,
				      __in uval dataLen);

    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __inoutbuf(datalen:datalen:datalen) char *data,
				  __inout uval& datalen,
				  __XHANDLE xhandle);

    virtual SysStatus _giveAccess(ObjectHandle& oh, ProcessID toProcID,
				  __in AccessRights newmatch,
				  __in AccessRights newnomatch,
				  __XHANDLE xhandle) {
	SFTRACE("_giveAccess");
	SysStatus rc = Obj::_giveAccess(oh, toProcID, newmatch, newnomatch,
					xhandle);
	// for now we assume this method is only explictly invoked when
	// support for non-shared files is off
	passertMsg(Clnt(xhandle)->useType == FileLinux::FIXED_SHARED, "?");
	ClientData *cl = Clnt(oh.xhandle());
	cl->useType = Clnt(xhandle)->useType;
	return rc;
    }
    SysStatus locked_returnUnlocked_deletion();

protected:
    // following methods for trace/debugging only
#ifdef DEBUG_ONE_FILE
    static char nameDebugOneFile[255];
#endif // #ifdef DEBUG_ONE_FILE
    void traceDestroy(char *fs, char *method)
#ifdef DILMA_DEBUG_DESTROY
	{
#ifdef HACK_FOR_FR_FILENAMES
	    char *name = nameAtCreation;
#else
	    char *name = "UNKNOWN";
#endif // #ifdef HACK_FOR_FR_FILENAMES
	    TraceOSFSSFDestroy((uval) getRef(), name,
			   fs, method);
	}
#else
	{}
#endif // #ifdef DILMA_DEBUG_DESTROY

#ifdef DEBUG_TRACE_SERVER_FILE
    void traceMethod(char *method) {
#ifdef HACK_FOR_FR_FILENAMES
	(nameAtCreation ? traceFS_ref1_str2(TRACE_FS_DEBUG_1UVAL_2STR,
					    (uval) this, nameAtCreation,
					    method)
	 : traceFS_ref1_str1(TRACE_FS_DEBUG_1UVAL_1STR, (uval) this, method));
#else
	traceFS_ref1_str1(TRACE_FS_DEBUG_1UVAL_1STR, (uval) this, method);
#endif // #ifdef HACK_FOR_FR_FILENAMES
    }
    void traceUseType(char *method, uval useType) {
#ifdef HACK_FOR_FR_FILENAMES
	(nameAtCreation ? traceFS_ref2_str2(TRACE_FS_DEBUG_2UVAL_2STR,
					    (uval) this, useType,
					    nameAtCreation, method)
	 : traceFS_ref2_str1(TRACE_FS_DEBUG_2UVAL_1STR,
			     (uval) this, useType, method));
#else
	traceFS_ref1_str1(TRACE_FS_DEBUG_1UVAL_1STR, (uval) this, method);
#endif // #ifdef HACK_FOR_FR_FILENAMES
    }
#else // from #ifdef DEBUG_TRACE_SERVER_FILE
#ifdef DEBUG_ONE_FILE
#ifdef HACK_FOR_FR_FILENAMES
    void printMethod (char *method) {
	if (nameAtCreation) {
	    uval len = strlen(ServerFile::nameDebugOneFile);
	    tassertMsg(len < 255, "?");
	    if (len == 0 || strncmp(nameAtCreation,
				    ServerFile::nameDebugOneFile, len) == 0) {
		err_printf("file %s obj %p method %s\n", nameAtCreation, this,
			   method);
	    }
	}
    }
    void printUseType (char *method, uval useType) {
	if (nameAtCreation) {
	    uval len = strlen(ServerFile::nameDebugOneFile);
	    tassertMsg(len < 255, "?");
	    if (len == 0 || strncmp(nameAtCreation,
				    ServerFile::nameDebugOneFile, len) == 0) {
		err_printf("file %s obj %p method %s useType %ld\n",
			   nameAtCreation, this, method, useType);
	    }
	}
    }
#else // for #ifdef HACK_FOR_FR_FILENAMES
    void printMethod(char *method) {}
    void printUseType(char *method, uval useType) {}
#endif //#ifdef HACK_FOR_FR_FILENAMES
#endif // #ifdef DEBUG_ONE_FILE
#endif // #ifdef DEBUG_SERVER_FILE
};

#endif /* #ifndef __SERVER_FILE_H_ */

