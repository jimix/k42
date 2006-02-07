#ifndef __FILE_LINUX_DIR_H_
#define __FILE_LINUX_DIR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinuxDir.H,v 1.47 2005/07/15 17:14:21 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Adapter object to provide Linux personality
 * **************************************************************************/
#include <stub/StubFileLinuxServer.H>
#include <io/FileLinux.H>

class FileLinuxDir : public FileLinux {

protected:
    friend class DirLinuxFS;
    struct LazyReOpenData : public FileLinux::LazyReOpenData {
	// storing information about file position (cookie) sharing
	void *shdata;

	char pathBuf[PATH_MAX+1];
	uval pathLen;
	uval maxpthlen;
    };

    uval goMountP;                      // there is a mount point to visit
    uval localCookie;			// if 0, first read, go mount server

    // the pathname needed for merging in mount points
    uval pthlen, maxpthlen;
    PathNameDynamic<AllocGlobal> *pth;

    StubFileLinuxServer stub;

    FileLinuxDir(): stub(StubObj::UNINITIALIZED) { /* empty body */ }

    DEFINE_GLOBAL_NEW(FileLinuxDir);

    void init(ObjectHandle oh, const char *nm);

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
	return _SERROR(1419, 0, EINVAL);
    }

    virtual SysStatusUval writev(const struct iovec *vec, uval vecCount,
				ThreadWait **tw, GenState &moreAvail) {
	// Cannot modify to a directory
	moreAvail.state = FileLinux::INVALID;
	return _SERROR(1420, 0, EISDIR);
    }

    virtual SysStatusUval readv(struct iovec *vec, uval vecCount,
			       ThreadWait **tw, GenState &moreAvail) {
	moreAvail.state = FileLinux::INVALID;
	return _SERROR(1421, 0, EISDIR);
    }

    virtual SysStatusUval getDents(struct direntk42 *buf, uval len);

    virtual SysStatus fchdir();

    virtual SysStatusUval setFilePosition(sval position, At at) {
	return stub._setFilePosition(position, at);
    }

    virtual SysStatus getStatus(FileLinux::Stat *status) {
	return stub._getStatus(*status);
    }

    virtual SysStatus getFROH(ObjectHandle &FRoh, FRType frType) {
	// Not Aplicable
	(void) frType;
	return _SERROR(1425, 0, EINVAL);
    }

    virtual SysStatus fchown(uid_t uid, gid_t gid) {
	return stub._fchown(uid, gid);
    }

    virtual SysStatus fchmod(mode_t mode) {
	return stub._fchmod(mode);
    }

    virtual SysStatus ftruncate(off_t length) {
	// Cannot modify to a directory
	return (_SERROR(1690, 0, EINVAL));
    }

    static SysStatus Create(FileLinuxRef &r, ObjectHandle toh,
			    uval oflags, const char *nm);

    virtual SysStatus dup(FileLinuxRef& newFile);

    virtual SysStatus ioctl(uval request, va_list args) {
	// Not Applicable
	return (_SERROR(2550, 0, ENOTTY));
    }

    virtual SysStatus lazyGiveAccess(sval fd);
};

#endif /* #ifndef __FILE_LINUX_DIR_H_ */
