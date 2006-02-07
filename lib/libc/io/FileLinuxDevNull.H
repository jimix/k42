#ifndef __FILE_LINUX_DEV_NULL_H_
#define __FILE_LINUX_DEV_NULL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxDevNull.H,v 1.24 2005/07/15 17:14:21 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality to /dev/null
 * **************************************************************************/

#include <stub/StubFileLinuxServer.H>
#include <io/FileLinux.H>

class FileLinuxDevNull : public FileLinux {
protected:
    StubFileLinuxServer stub;

    FileLinuxDevNull(): stub(StubObj::UNINITIALIZED) { /* empty body */ }

    void init(ObjectHandle oh) {
	stub.setOH(oh);
	FileLinuxRef none = NULL;
	FileLinux::init(none);
    }
    DEFINE_GLOBAL_NEW(FileLinuxDevNull);
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
				ThreadWait **tw, GenState &moreAvail) {
	moreAvail.state = FileLinux::READ_AVAIL | FileLinux::WRITE_AVAIL;
	return _SRETUVAL(vecLength(vec, vecCount));
    }

    virtual SysStatusUval readv(struct iovec *vec, uval vecCount,
			       ThreadWait **tw, GenState &moreAvail) {
	moreAvail.state = FileLinux::READ_AVAIL | FileLinux::WRITE_AVAIL;
	return 0;
    }

    virtual SysStatusUval setFilePosition(sval position, At at) {
	return _SRETUVAL(0);
    }

    virtual SysStatus getStatus(FileLinux::Stat *status) {
	return stub._getStatus(*status);
    }

    virtual SysStatus getFROH(ObjectHandle &FRoh, FRType frType) {
	// Not Aplicable
	(void) frType;
	return _SERROR(2539, 0, EINVAL);
    }

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
	return (_SERROR(2549, 0, ENOTTY));
    }

    virtual SysStatus lazyGiveAccess(sval fd);
    static SysStatus LazyReOpen(FileLinuxRef &flRef, ObjectHandle oh,
				char *buf, uval bufLen);
};

#endif /* #ifndef __FILE_LINUX_DEV_NULL_H_ */
