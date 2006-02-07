#ifndef __FILE_LINUX_VIRT_FILE_H_
#define __FILE_LINUX_VIRT_FILE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxVirtFile.H,v 1.8 2004/01/16 16:37:39 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stub/StubFileLinuxServer.H>
#include <io/FileLinux.H>
#include <misc/HashSimple.H>
#include <sync/BlockedThreadQueues.H>

class FileLinuxVirtFile : public FileLinux {
protected:
    static const uval MAX_IO_LOAD;
    StubFileLinuxServer stub;
    FileLinuxVirtFile(): stub(StubObj::UNINITIALIZED) {
	/* empty body */
    }

    typedef BLock StreamLockType;
    StreamLockType streamLock;

    DEFINE_GLOBAL_NEW(FileLinuxVirtFile);

    virtual SysStatus init(ObjectHandle oh, uval openFlags);

public:
    static SysStatus CreateFile(char *name, mode_t mode, ObjectHandle vfoh);

    static SysStatus Create(FileLinuxRef &r, ObjectHandle toh, uval openFlags);

    virtual SysStatus giveAccessByClient(ObjectHandle &oh, ProcessID toProcID,
					 AccessRights match,
					 AccessRights nomatch) {
	return stub._giveAccess(oh,toProcID,match,nomatch);
    }

    virtual SysStatus giveAccessByClient(ObjectHandle &oh,
					 ProcessID toProcID) {
	return stub._giveAccess(oh,toProcID);
    }

    virtual SysStatusUval readv(struct iovec *iov, uval vecCount,
				ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval writev(const struct iovec *vec, uval vecCount,
				 ThreadWait **tw, GenState &moreAvail);

    virtual SysStatus destroy();

    virtual SysStatus getOH(ObjectHandle &oh) {oh = stub.getOH(); return 0;}

    virtual SysStatus flush() {
	return _SERROR(2103, 0, ENOSYS);
    }

    virtual SysStatusUval setFilePosition(sval position, At at) {
	return stub._setFilePosition(position, at);
    }

    virtual SysStatus getStatus(FileLinux::Stat *status) {
	return stub._getStatus(*status);
    }

    virtual SysStatus getFROH(ObjectHandle &FRoh, FRType frType) {
	(void) frType;
	return _SERROR(2105, 0, EINVAL);
    }

    virtual SysStatus fchown(uid_t uid, gid_t gid) {
	return stub._fchown((uval) uid, (uval) gid);
    }

    virtual SysStatus fchmod(mode_t mode) {
	return stub._fchmod((uval) mode);
    }

    virtual SysStatus ftruncate(off_t length) {
	return stub._ftruncate((uval) length);
    }

    virtual SysStatus dup(FileLinuxRef& newFile);

    virtual SysStatus ioctl(uval request, va_list args) {
	tassertWrn(0, "FileLinuxVirtFile::ioctl not implemented\n");
	return _SERROR(2107, 0, ENOSYS);
    }

    virtual SysStatus lazyGiveAccess(sval fd);
    static SysStatus LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh,
				char *buf, uval bufLen);

    // Register IONotif object for notifications of state changes
    virtual SysStatus notify(ThreadWait **tw) {
	tassert(0, err_printf("NYI\n"));
	return _SERROR(2108, 0, ENOSYS);
    };

    /* --------------- ASI interface ------------------- */
    /*
     * allocate a buffer of date for reading.  The buffer
     * will actually contain the values "read"
     * returns the actual length if at end of file
     */
    virtual SysStatusUval readAlloc(uval len, char * &buf, ThreadWait **tw) {
	return _SERROR(2109, 0, ENOSYS);
    }

    virtual SysStatusUval readAllocAt(uval len, uval off, At at,
				      char * &bf, ThreadWait **tw) {
	return _SERROR(2110, 0, ENOSYS);
    }

    // indicate that the buffer is no longer needed
    virtual SysStatus readFree(char *ptr) {
	return _SERROR(2111, 0, ENOSYS);
    }

    // allocate an output buffer to be filled
    virtual SysStatusUval writeAlloc(uval len, char * &buf, ThreadWait **tw) {
	return _SERROR(2112, 0, ENOSYS);
    }

    virtual SysStatusUval writeAllocAt(uval len, uval off, At at,
				       char * &bf, ThreadWait **tw) {
	return _SERROR(2113, 0, ENOSYS);
    }

    // indicate that the buffer has been written
    virtual SysStatus writeFree(char *ptr) {
	return _SERROR(2114, 0, ENOSYS);
    }

protected:
    virtual SysStatusUval locked_readAlloc(uval len, char * &buf,
					   ThreadWait **tw) {
	return _SERROR(2115, 0, ENOSYS);
    }

    virtual SysStatusUval locked_readAllocAt(uval len, uval off, At at,
					     char *&bf, ThreadWait **tw) {
	return _SERROR(2116, 0, ENOSYS);
    }

    virtual SysStatusUval locked_readRealloc(char *prev, uval oldlen,
					     uval newlen, char * &buf,
					     ThreadWait **tw) {
	return _SERROR(2117, 0, ENOSYS);
    }

    virtual SysStatus locked_readFree(char *ptr) {
	return _SERROR(2118, 0, ENOSYS);
    }

    virtual SysStatusUval locked_writeAllocAt(uval len, uval off, At at,
					      char *&bf, ThreadWait **tw) {
	return _SERROR(2119, 0, ENOSYS);
    }

    virtual SysStatusUval locked_writeAlloc(uval len, char * &buf,
					    ThreadWait **tw) {
	return _SERROR(2145, 0, ENOSYS);
    }

    virtual SysStatusUval locked_writeRealloc(char *prev, uval oldlen,
					      uval newlen, char * &buf,
					      ThreadWait **tw) {
	return _SERROR(2120, 0, ENOSYS);
    }

    virtual SysStatus locked_writeFree(char *ptr) {
	return _SERROR(2121, 0, ENOSYS);
    }

    virtual SysStatus locked_flush(uval release = 0) {
	return _SERROR(2122, 0, ENOSYS);
    }

    /* --------------- end ASI interface ------------------- */

};
#endif /* #ifndef __FILE_LINUX_FILE_H_ */
