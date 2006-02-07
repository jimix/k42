#ifndef __FILE_LINUX_FILE_H_
#define __FILE_LINUX_FILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxFile.H,v 1.76 2005/04/15 17:39:34 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality
 * **************************************************************************/

#include "Buffer.H"
#include <stub/StubFileLinuxServer.H>
#include <stub/StubFRCRW.H>
#include <io/FileLinux.H>
#include <io/CallBackObj.H>
#include <meta/MetaCallBackObj.H>
#include <misc/HashSimple.H>
#include <sync/BlockedThreadQueues.H>

#include <fslib/fs_defines.H>
#include <trace/traceFS.h>

class FileLinuxFile;
typedef FileLinuxFile** FileLinuxFileRef;
class FileLinuxFile : public FileLinux {
public:
    struct LazyReOpenData :  FileLinux::LazyReOpenData {
	uval useType;  	/* indicates file can behave as having one
			 * exclusive client */
	// storing information about file position sharing
	void *shdata;
    };

protected:
    friend class ServerFile;
    Buffer *buffer; /* not an embedded object because we may instantiate
		     * different Buffer subclasses according to file
		     * characteristics as file size x*/

    struct {
	uval fileLength;
	uval initialOffset;
    } bufferInitData;

    StubFileLinuxServer stub;

    typedef BLock StreamLockType;
    StreamLockType streamLock;

    uval          useType;              // indicate that the manipulation
                                        // of file length and file offset
                                        // can be optimized for the scenario
                                        // where the file has a single client

    FileLinuxFile(): stub(StubObj::UNINITIALIZED),
		     callBackLockRegistered(0),
		     callBackUseTypeRegistered(0) {
	bufferInitData.fileLength = uval(~0);
	bufferInitData.initialOffset = 0;
	buffer = NULL;
    }

    DEFINE_GLOBAL_NEW(FileLinuxFile);

    virtual SysStatus init(ObjectHandle oh, uval oflag, UseType useType);

public:
    virtual SysStatus getType(TypeID &type);

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch) {
	return stub._giveAccess(oh,toProcID,match,nomatch);
    }

    virtual SysStatus giveAccessByClient(ObjectHandle &oh,
					 ProcessID toProcID) {
	return stub._giveAccess(oh,toProcID);
    }

    virtual SysStatus destroy();

    virtual SysStatus detach();

    virtual SysStatus getOH(ObjectHandle &oh) {oh = stub.getOH(); return 0;}

    virtual SysStatus flush();

    virtual SysStatusUval readv(struct iovec *iov, uval vecCount,
				ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval writev(const struct iovec *vec, uval vecCount,
				 ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval pread(const char *buf, uval nbytes, uval offset);

    virtual SysStatusUval pwrite(const char *buf, uval nbytes, uval offset);

    virtual SysStatusUval setFilePosition(sval position, At at);

    virtual SysStatus getStatus(FileLinux::Stat *status);

    virtual SysStatus getFROH(ObjectHandle &FRoh, FRType frType) {
	SysStatus rc;
	rc = stub._getFROH(FRoh);
	if (_FAILURE(rc)) return rc;
	if (frType == COPYONWRITE) {
	    // Make a copy on write copy mapping
	    ObjectHandle copyFROH;
	    rc = StubFRCRW::_Create(copyFROH, FRoh);
	    Obj::ReleaseAccess(FRoh);
	    FRoh.initWithOH(copyFROH);
	}
	return rc;
    }

    virtual SysStatus fchown(uid_t uid, gid_t gid) {
	return stub._fchown((uval) uid, (uval) gid);
    }

    virtual SysStatus fchmod(mode_t mode) {
	return stub._fchmod((uval) mode);
    }

    virtual SysStatus ftruncate(off_t length);

    static SysStatus Create(FileLinuxRef &r, ObjectHandle toh, uval oflag,
			    uval useType);

    virtual SysStatus dup(FileLinuxRef& newFile);

    virtual SysStatus ioctl(uval request, va_list args);

    virtual SysStatus lazyGiveAccess(sval fd);
    static SysStatus LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh,
				char *buf, uval bufLen);

    virtual void getStateTransferData(char *transferData);
    virtual uval getStateTransferDataSize() const;

    // Register IONotif object for notifications of state changes
    virtual SysStatus notify(ThreadWait **tw) {
	tassert(0, err_printf("NYI\n"));
	return _SERROR(2540, 0, ENOSYS);
    };

    /* --------------- ASI interface ------------------- */
    /*
     * allocate a buffer of date for reading.  The buffer
     * will actually contain the values "read"
     * returns the actual length if at end of file
     */
    virtual SysStatusUval readAlloc(uval len, char * &buf, ThreadWait **tw);

    virtual SysStatusUval readAllocAt(uval len, uval off, At at,
				      char * &bf, ThreadWait **tw);

    // indicate that the buffer is no longer needed
    virtual SysStatus readFree(char *ptr);

    // allocate an output buffer to be filled
    virtual SysStatusUval writeAlloc(uval len, char * &buf, ThreadWait **tw);

    virtual SysStatusUval writeAllocAt(uval len, uval off, At at,
				       char * &bf, ThreadWait **tw);

    // indicate that the buffer has been written
    virtual SysStatus writeFree(char *ptr);

    virtual SysStatus lock();
    virtual SysStatus unLock();

    virtual SysStatus discardCachedPages() {
	return stub._discardCachedPages();
    }

    class CallBackLock;

    // call back for fie locking operations
    typedef CallBackLock **CallBackLockRef;
    class CallBackLock : public CallBackObj {
	HashSimpleLocked<uval, ThreadID, AllocGlobal, 0> callBackList;
    public:
	DEFINE_REFS(CallBackLock);

	DEFINE_ACCESS_SERVER(CallBackObj,
			     MetaObj::controlAccess,MetaObj::none);

	DEFINE_GLOBAL_NEW(CallBackLock);
	virtual __async SysStatusUval _callBack(__in uval arg, __XHANDLE xh);
	static SysStatus Create(CallBackLockRef &ref);
	virtual SysStatus add(uval k, ThreadID tid) {
	    callBackList.add(k, tid);
	    return 0;
	}
	virtual SysStatus find(uval k, ThreadID tid) {
	    callBackList.find(k, tid);
	    return 0;
	}
	virtual SysStatus remove(uval k, ThreadID tid) {
	    callBackList.remove(k, tid);
	    return 0;
	}
    };

    uval callBackLockRegistered;
    CallBackLockRef callBackLockObjRef;
    SysStatus registerCallBackLock();

    // System V discretionary locks (see fcntl(2))
    virtual SysStatus getLock(struct flock &alock) {
	return stub._getLock(alock);
    }
    virtual SysStatus setLock(struct flock &alock) {
	return stub._setLock(alock);
    }
    virtual SysStatus setLockWait(struct flock &alock);

    // call back for switching between exclusive and shared use
    class CallBackUseType;
    typedef CallBackUseType **CallBackUseTypeRef;
    class CallBackUseType : public CallBackObj {
    private:
	FileLinuxFileRef flref;
	volatile uval64 *destroyAckSyncAddr;
    public:
	DEFINE_REFS(CallBackUseType);

	DEFINE_ACCESS_SERVER(CallBackObj,
			     MetaObj::controlAccess,MetaObj::none);

	DEFINE_GLOBAL_NEW(CallBackUseType);
	CallBackUseType(FileLinuxFileRef f,  volatile uval64 *addr) {
	    flref = f;
	    destroyAckSyncAddr = addr;
	}
	virtual __async SysStatusUval _callBack(__in uval arg, __XHANDLE xh);
	static SysStatus Create(CallBackUseTypeRef &ref, FileLinuxFileRef f,
				volatile uval64 *addr);
	virtual SysStatus destroy();

    };

    volatile uval64 destroyAckSync;
    uval callBackUseTypeRegistered;
    CallBackUseTypeRef callBackUseTypeObjRef;
    SysStatus registerCallBackUseType() {
	AutoLock<StreamLockType> al(&streamLock);
	return locked_registerCallBackUseType();
    }
    SysStatus locked_registerCallBackUseType();
    virtual SysStatus processCallBack(uval request);

    /*
     * Operations same as above alloc ones, but lock is acquired and
     * freed explicitly before calling them. They typically assert lock
     * acquisition.
     */
protected:
    virtual SysStatusUval locked_readAlloc(uval len, char * &buf,
					   ThreadWait **tw);

    virtual SysStatusUval locked_readAllocAt(uval len, uval off, At at,
					     char *&bf, ThreadWait **tw) {
	tassert(0, err_printf("FileLinuxFile::locked_readAllocAt NYI\n"));
	return 0;
    }

    virtual SysStatusUval locked_readRealloc(char *prev, uval oldlen,
					     uval newlen, char * &buf,
					     ThreadWait **tw) {
	tassert(0, err_printf("FileLinuxFile::locked_readRealloc NYI\n"));
	return 0;
    }

    virtual SysStatus locked_readFree(char *ptr);

    virtual SysStatusUval locked_writeAllocAt(uval len, uval off, At at,
					      char *&bf, ThreadWait **tw) {
	tassert(0, err_printf("FileLinuxFile::locked_writeAllocAt NYI\n"));
	return 0;

    }

    virtual SysStatusUval locked_writeAlloc(uval len, char * &buf,
					    ThreadWait **tw);

    virtual SysStatusUval locked_writeRealloc(char *prev, uval oldlen,
					      uval newlen, char * &buf,
					      ThreadWait **tw) {
	tassert(0, err_printf("FileLinuxFile::locked_writeRealloc NYI\n"));
	return 0;
    }

    virtual SysStatus locked_writeFree(char *ptr);

    virtual SysStatus locked_flush(uval release = 0);

    /* --------------- end ASI interface ------------------- */

    virtual void locked_initBuffer(uval len) {
	tassertMsg(buffer == NULL, "?");

#ifdef LAZY_SHARING_SETUP
	if (useType == LAZY_INIT) {
	    tassertMsg(callBackUseTypeRegistered == 0, "?\n");
	    SysStatus rc = locked_registerCallBackUseType();
	    tassertMsg(_SUCCESS(rc), "ops");
	    tassertMsg(bufferInitData.fileLength != uval(~0), "?");
	    buffer = new Buffer(bufferInitData.fileLength,
				bufferInitData.initialOffset,
				openFlags, stub.getOH(), useType, len);
	} else {
	    buffer = new Buffer(openFlags, stub.getOH(), useType, len);
	}
#else
	if (useType == NON_SHARED) {
	    tassertMsg(callBackUseTypeRegistered == 1, "?\n");
	    tassertMsg(bufferInitData.fileLength != uval(~0), "?");
	    buffer = new Buffer(bufferInitData.fileLength,
				bufferInitData.initialOffset,
				openFlags, stub.getOH(), useType, len);
	} else {
	  buffer = new Buffer(openFlags, stub.getOH(), useType, len);
	}
#endif // #ifdef LAZY_SHARING_SETUP
    }


};

#endif /* #ifndef __FILE_LINUX_FILE_H_ */
