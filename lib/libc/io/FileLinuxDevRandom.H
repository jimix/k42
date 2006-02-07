#ifndef __FILE_LINUX_DEV_RANDOM_H_
#define __FILE_LINUX_DEV_RANDOM_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxDevRandom.H,v 1.1 2005/03/04 00:43:57 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality to 
 * /dev/random
 * **************************************************************************/

#include <stub/StubFileLinuxServer.H>
#include <io/FileLinux.H>

class FileLinuxDevRandom : public FileLinux {
protected:
    StubFileLinuxServer stub;

    FileLinuxDevRandom(): stub(StubObj::UNINITIALIZED) { /* empty body */ }

    void init(ObjectHandle oh) {
	stub.setOH(oh);
	FileLinuxRef none = NULL;
	FileLinux::init(none);
    }
    DEFINE_GLOBAL_NEW(FileLinuxDevRandom);

    virtual void FileLinuxDevRandom::fillIOVec(struct iovec *vec);

public:
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

    SysStatus getOH(ObjectHandle &oh) {
	oh = stub.getOH();
	return 0;
    }

    virtual SysStatus flush() {
	return _SRETUVAL(0);
    }

    virtual SysStatusUval writev(const struct iovec *vec, uval vecCount,
				 ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval readv(struct iovec *vec, uval vecCount,
				ThreadWait **tw, GenState &moreAvail);

    virtual SysStatusUval setFilePosition(sval position, At at) {
	return _SRETUVAL(0);
    }

    virtual SysStatus getStatus(FileLinux::Stat *status) {
	return stub._getStatus(*status);
    }

    virtual SysStatus getFROH(ObjectHandle &FRoh, FRType frType);

    virtual SysStatus fchown(uid_t uid, gid_t gid) {
	return stub._fchown((uval) uid, (uval) gid);
    }

    virtual SysStatus fchmod(mode_t mode) {
	return stub._fchmod((uval) mode);
    }

    virtual SysStatus ftruncate(off_t length) {
	return _SRETUVAL(0);
    }

    static SysStatus Create(FileLinuxRef &r, ObjectHandle toh, uval oflags);

    virtual SysStatus dup(FileLinuxRef& newFile);

    virtual SysStatus ioctl(uval request, va_list args) {
	// Not Applicable
	return (_SERROR(1586, 0, ENOTTY));
    }

    virtual SysStatus lazyGiveAccess(sval fd);
    static SysStatus LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh,
				char *buf, uval bufLen);

};

#endif /* #ifndef __FILE_LINUX_DEV_RANDOM_H_ */
