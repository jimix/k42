/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileLinux.C,v 1.92 2005/08/22 22:35:09 dilma Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>

// FIXME GCC3 -JX. should not need this.
#if __GNUC__ >= 3
#ifndef __USE_GNU
#define __USE_GNU
#endif
#endif

#include "FileLinux.H"
#include <asm/ioctls.h>
#include <cobj/CObjRoot.H>
#include <sys/MountPointMgrClient.H>
#include <stub/StubNameTreeLinux.H>
#include "io/FileLinuxDir.H"
#include "io/FileLinuxFile.H"
#include "io/FileLinuxDevNull.H"
#include "io/FileLinuxDevZero.H"
#include "io/FileLinuxDevRandom.H"
#include "io/FileLinuxVirtFile.H"
#include <linux/major.h>
#include <io/FileLinuxStreamTTY.H>
#include <linux/termios.h>
#include <cobj/CObjRootSingleRep.H>
SysStatus
FileLinux::init(FileLinuxRef &useThis)
{
    waiters.init();
    (FileLinuxRef)CObjRootSingleRep::Create(this, useThis);
    return 0;
}

FileLinux::CurrentWorkingDirectory *FileLinux::cwd;
/* static */ uval FileLinux::UMaskValue = 0;

class FileLinux::CurrentWorkingDirectory {
    typedef BLock LockType;
    LockType  lock;
    PathNameDynamic<AllocGlobal> *cwd; // CWD for unix relative opens
    uval      cwdlen;
public:
    DEFINE_GLOBAL_NEW(CurrentWorkingDirectory);

    CurrentWorkingDirectory() {
	lock.init();
	cwd = 0;
	cwdlen = 0;
    }

    SysStatus
    getAbsPath(const char *nm, PathNameDynamic<AllocGlobal> *&pathName,
	       uval &pathLen, uval &maxPathLen) {
	if (unlikely(!nm)) {
	    return _SERROR(2406, 0, EINVAL);
	}
	if (strlen(nm) >= PATH_MAX) {
	    return _SERROR(2929, 0, ENAMETOOLONG);
	}

	// FIXME: fix pathname to be variable sized buffer
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	if (nm[0]=='/') {
	    pathLen = PathNameDynamic<AllocGlobal>::Create(nm, strlen(nm),
							   0, 0, pathName,
							   PATH_MAX+1);
	} else {
	    pathLen = PathNameDynamic<AllocGlobal>::Create(nm, strlen(nm),
							   cwd, cwdlen,
							   pathName,
							   PATH_MAX+1);
	}
	if (pathName == NULL) {
	    return _SERROR(2930, 0, ENAMETOOLONG);
	}
	maxPathLen = PATH_MAX+1;
	return 0;
    }

    void
    setCWD(PathNameDynamic<AllocGlobal> *pth, uval pthlen) {
	if (cwd) {
	    cwd->destroy(PATH_MAX+1);
	}
	cwd = pth;
	cwdlen = pthlen;
    }

    SysStatusUval
    getcwd(PathNameDynamic<AllocGlobal> *pathName, uval maxPathLen) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return pathName->appendPath(0,maxPathLen, cwd, cwdlen);
    }
};

// FIXME or CHECK  syscall creat cannot create special files (ie pipe, dev, sock)
/*static*/ SysStatus
FileLinux::Create(FileLinuxRef& newFile, const char* nm, uval oflag,
		  mode_t mode)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    SysStatus rc;
    TypeID type;
    ObjectHandle foh, doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    uval retry, numretry = 0;
    uval useType;

    /* fref is an argument for simplifying gathering traces of
     * file sharing information. This should go
     * away. */
    ObjRef fref;

    do {
	retry = 0;
#ifdef WARNING_ABOUT_UNSUPPORTED_FLAGS
	// warning about flags not yet supported
	tassertWrn(!(oflag & O_NOCTTY), "flag O_NOCTTY not supported yet\n");
	tassertWrn(!(oflag & O_NONBLOCK),
		   "flag O_NONBLOCK not supported yet\n");
	tassertWrn(!(oflag & O_NDELAY), "flag O_NDELAY not supported yet\n");
	tassertWrn(!(oflag & O_SYNC), "flag O_SYNC not supported yet\n");
	tassertWrn(!(oflag & O_NOFOLLOW),
		   "flag O_NOFOLLOW not supported yet\n");
#endif // WARNING_ABOUT_UNSUPPORTED_FLAGS

	rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
	if (_FAILURE(rc)) {
	    return rc;
	}

	rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen,
						   doh);
	if (_FAILURE(rc)) {
	    pth->destroy(maxpthlen);
	    return rc;
	}

	stubNT.setOH(doh);

	// Apply umask
	mode &= ~(UMaskValue);
	rc = stubNT._getObj(pth->getBuf(), pthlen, oflag, mode, foh,
			    type, useType, fref);
	pth->destroy(maxpthlen);
	if (_FAILURE(rc)) {
	    if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
		(_SGENCD(rc) == ECANCELED)) {
		if (numretry++ > 10) {
		    tassertWrn(0, "too many symbolic links\n");
		    return _SERROR(2676, 0, ELOOP);
		}
		retry = 1;		// retry operation

		rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
		_IF_FAILURE_RET(rc);
		DREFGOBJ(TheMountPointMgrRef)->resolveSymbolicLink(
		    pth, pthlen);
		pth->destroy(maxpthlen);
	    } else {
		return rc;
	    }
	}
    } while (retry != 0);

    switch (type) {
    case FileLinux_FILE:
	rc = FileLinuxFile::Create(newFile, foh, oflag, useType);
	break;
    case FileLinux_DIR:
	rc = FileLinuxDir::Create(newFile, foh, oflag, nm);
	break;
    case FileLinux_CHR_NULL:
	rc = FileLinuxDevNull::Create(newFile, foh, oflag);
	break;
    case FileLinux_CHR_ZERO:
	rc = FileLinuxDevZero::Create(newFile, foh, oflag);
	break;
    case FileLinux_CHR_RANDOM:
	rc = FileLinuxDevRandom::Create(newFile, foh, oflag);
	break;
    case FileLinux_CHR_TTY:
    case k42makedev(UNIX98_PTY_MASTER_MAJOR,0) ...
	k42makedev(UNIX98_PTY_MASTER_MAJOR + UNIX98_PTY_MAJOR_COUNT-1,255):
    case k42makedev(UNIX98_PTY_SLAVE_MAJOR,0) ...
	k42makedev(UNIX98_PTY_SLAVE_MAJOR + UNIX98_PTY_MAJOR_COUNT-1,255):
    case k42makedev(TTYAUX_MAJOR,2):
	rc = FileLinuxStreamTTY::Create(newFile, foh, oflag);
	break;
    case FileLinux_VIRT_FILE:
	rc = FileLinuxVirtFile::Create(newFile, foh, oflag);
	break;
    case FileLinux_STREAM:
	rc = FileLinuxStream::Create(newFile, foh, oflag);
	break;
    case FileLinux_PIPE:
	err_printf("Got a pipe\n");
	rc = FileLinuxStream::Create(newFile, foh, oflag);
	break;

	// Compiler generates: *** Warning ***: overflow in implicit
	// constant conversion for the FileLinux_UNKNOWN: case.
	// Since this goes to the default case anyway, we are
	// commenting it out.  FileLinux_UNKNOWN is ~0 which is (plus
	// or minus) huge and we don't want the compiler trying to lay
	// out a jump table for this anyway.

	// case FileLinux_UNKNOWN:
    default:
	err_printf("Unknown file type: %lx\n",type);
	rc = _SERROR(1432, 0, ENXIO);
	break;
    }
    return rc;
}

/* static */
SysStatus
FileLinux::Create(FileLinuxRef& newFile, TypeID clientType, 
		  ObjectHandle &withOH, char *data)
{
    uval oflag = 0;
    SysStatus rc = -1;

    switch (clientType) {
    case FileLinux_FILE:
	rc = FileLinuxFile::LazyReOpen(newFile, withOH, data,
				       sizeof(FileLinuxFile::LazyReOpenData));
	break;
    case FileLinux_DIR:
    case FileLinux_CHR_NULL:
    case FileLinux_CHR_ZERO:
    case FileLinux_CHR_RANDOM:
	passertMsg(false, "Creation of type not supported\n");
	break;

    case FileLinux_CHR_TTY:
    case k42makedev(UNIX98_PTY_MASTER_MAJOR,0) ...
	k42makedev(UNIX98_PTY_MASTER_MAJOR + UNIX98_PTY_MAJOR_COUNT-1,255):
    case k42makedev(UNIX98_PTY_SLAVE_MAJOR,0) ...
	k42makedev(UNIX98_PTY_SLAVE_MAJOR + UNIX98_PTY_MAJOR_COUNT-1,255):
    case k42makedev(TTYAUX_MAJOR,2):
	rc = FileLinuxStreamTTY::Create(newFile, withOH, oflag);
	break;
    case FileLinux_VIRT_FILE:
	rc = FileLinuxVirtFile::Create(newFile, withOH, oflag);
	break;
    case FileLinux_STREAM:
	rc = FileLinuxStream::LazyReOpen(newFile, withOH, data,
					 sizeof(FileLinuxStream::LazyReOpenData));
	break;

    default:
	err_printf("Unknown file type: %lx\n",clientType);
	rc = _SERROR(1432, 0, ENXIO);
	break;
    }
    return rc;
}

/* static */ SysStatus
FileLinux::Mknod(const char* nodeName, mode_t mode, dev_t dev)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;
    uval retry, numretry = 0;

    do {
	retry = 0;

	// resolve the path into k42 path object
	rc = FileLinux::GetAbsPath(nodeName, pth, pthlen, maxpthlen);
	_IF_FAILURE_RET(rc);

	// ipc across to the mount point 
	rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
	if (_FAILURE(rc)) {
	   pth->destroy(maxpthlen);
	   return rc;
	}

	stubNT.setOH(doh);

	mode &= ~(UMaskValue); // Apply umask

	rc = stubNT._mknod(pth->getBuf(), pthlen, mode, dev);
	pth->destroy(maxpthlen);

	if (_FAILURE(rc)) {
	    if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
		(_SGENCD(rc) == ECANCELED)) {
		if (numretry++ > 10) {
		    tassertWrn(0, "too many symbolic links\n");
		    return _SERROR(2884, 0, ELOOP);
		}
		retry = 1;		// retry operation

		rc = FileLinux::GetAbsPath(nodeName, pth, pthlen, maxpthlen);
		_IF_FAILURE_RET(rc);
		DREFGOBJ(TheMountPointMgrRef)->resolveSymbolicLink(pth, pthlen);
		pth->destroy(maxpthlen);
	    } else {
		return rc;
	    }
	}
    } while (retry!=0);

    return rc;
}

/* static */ SysStatus
FileLinux::Chown(const char *inPath, uid_t uid, gid_t gid)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(inPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);
    // get name and len from pth
    rc = stubNT._chown(pth->getBuf(), pthlen, uid, gid);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::Chmod(const char *inPath, mode_t mode)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(inPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);
    // get name and len from pth
    rc = stubNT._chmod(pth->getBuf(), pthlen, mode);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::Chdir(const char *nm)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    SysStatus rc;
    uval retry, numretry=0;
    struct stat status;

    do {
	retry = 0;
	rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
	_IF_FAILURE_RET(rc);

	ObjectHandle doh;
	StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);

	rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen,
						   doh);
	if (_FAILURE(rc)) {
	    pth->destroy(maxpthlen);
	    return rc;
	}

	//FIXME: Lookup should return a type so we can check it instead of..
	stubNT.setOH(doh);
	rc = stubNT._getStatus(pth->getBuf(), pthlen, status, 1);
	if ( (_SUCCESS(rc) && S_ISLNK(status.st_mode)) ||
	     (_FAILURE(rc) && (_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	      (_SGENCD(rc) == ECANCELED))) {
	    if (numretry++ > 10) {
		tassertWrn(0, "too many symbolic lins\n");
		return _SERROR(2676, 0, ELOOP);
	    }
	    FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
	    _IF_FAILURE_RET(rc);
	    DREFGOBJ(TheMountPointMgrRef)->resolveSymbolicLink(pth, pthlen);
	    retry = 1;		// retry operation
	    pth->destroy(maxpthlen);
	}
    } while (retry);

    if (unlikely(_FAILURE(rc))) return rc;

    if (S_ISDIR(status.st_mode)) {
	//FIXME: We just munched pth so restore from above
	rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
	tassert(_SUCCESS(rc),
		err_printf("second call to GetAbsPath has failed\n"));
	cwd->setCWD(pth, pthlen);
    } else {
	rc = _SERROR(1455, 1, ENOTDIR);
    }

    return rc;
}

/* static */ SysStatus
FileLinux::Truncate(const char *inPath, off_t length)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(inPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);
    // get name and len from pth
    rc = stubNT._truncate(pth->getBuf(), pthlen, length);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::Utime(const char *inPath, const struct utimbuf *utbuf)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(inPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);
    // get name and len from pth
    if (utbuf == NULL) {
	rc = stubNT._utime(pth->getBuf(), pthlen);
    } else {
	rc = stubNT._utime(pth->getBuf(), pthlen, *utbuf);
    }
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::Mkdir(const char *inPath, mode_t mode)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(inPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);
    // get name and len from pth
    rc = stubNT._mkdir(pth->getBuf(), pthlen, mode);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::Access(const char *pathname, int mode)
{
    SysStatus rc;
    FileLinux::Stat stat;
    // bogus implementation for now:
    rc = GetStatus(pathname, &stat);
    if (_FAILURE(rc)) {
	return rc;
    }

    // FIXME: should check mode, but since permission checking is all broken
    return 0;
}

/* static */ SysStatus
FileLinux::Rmdir(const char *inPath)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(inPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);
    // get name and len from pth
    rc = stubNT._rmdir(pth->getBuf(), pthlen);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::GetTreeForTwoPaths(const char *oldpath, const char* newpath,
			      uval &maxpthlen,
			      PathNameDynamic<AllocGlobal> *&oldp,
			      uval &oldlen,
			      PathNameDynamic<AllocGlobal> *&newp,
			      uval &newlen, ObjectHandle &toh)
{
    SysStatus rc;
    ObjectHandle oldNToh, newNToh;

    // get object handle for name tree corresponding to oldpath
    rc = FileLinux::GetAbsPath(oldpath, oldp, oldlen, maxpthlen);
    _IF_FAILURE_RET(rc);
    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(oldp, oldlen, maxpthlen,
					       oldNToh);
    if (_FAILURE(rc)) {
	oldp->destroy(maxpthlen);
	return rc;
    }

    // get object handle for name tree corresponding to newname. We
    // don't use Lookup since we want to extract from newpath only
    // the path part relative to the mount point
    rc = FileLinux::GetAbsPath(newpath, newp, newlen, maxpthlen);
    if (_FAILURE(rc)) {
	oldp->destroy(maxpthlen);
	return rc;
    }
    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(newp, newlen, maxpthlen,
					       newNToh);
    if (_FAILURE(rc)) {
	oldp->destroy(maxpthlen);
	newp->destroy(maxpthlen);
	return rc;
    }

    // need to check if both pathnames come from same file system.
    if ((oldNToh.pid() != newNToh.pid()) ||
	(oldNToh.xhandle() != newNToh.xhandle())) {
	oldp->destroy(maxpthlen);
	newp->destroy(maxpthlen);
	rc = _SERROR(1765, 1, EXDEV);
	return rc;
    }

    toh = oldNToh;

    return 0;
}

/* static */ SysStatus
FileLinux::Link (const char *oldpath, const char* newpath)
{
    PathNameDynamic<AllocGlobal> *oldp;
    PathNameDynamic<AllocGlobal> *newp;
    uval oldlen, newlen, maxpthlen;
    SysStatus rc;
    ObjectHandle toh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);

    rc = GetTreeForTwoPaths(oldpath, newpath, maxpthlen, oldp, oldlen,
			    newp, newlen, toh);
    _IF_FAILURE_RET(rc);

    stubNT.setOH(toh);
    rc = stubNT._link(oldp->getBuf(), oldlen, newp->getBuf(),
		      newlen);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    oldp->destroy(maxpthlen);
    newp->destroy(maxpthlen);

    return rc;
}

/* static */ SysStatus
FileLinux::Unlink(const char *inPath)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(inPath, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh, 0);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);
    // get name and len from pth
    rc = stubNT._unlink(pth->getBuf(), pthlen);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::Rename (const char *oldpath, const char* newpath)
{
    PathNameDynamic<AllocGlobal> *oldp;
    PathNameDynamic<AllocGlobal> *newp;
    uval oldlen, newlen, maxpthlen;
    SysStatus rc;
    ObjectHandle toh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);

    rc = GetTreeForTwoPaths(oldpath, newpath, maxpthlen, oldp, oldlen, newp,
			    newlen, toh);
    _IF_FAILURE_RET(rc);

    stubNT.setOH(toh);
    rc = stubNT._rename(oldp->getBuf(), oldlen, newp->getBuf(), newlen);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    oldp->destroy(maxpthlen);
    newp->destroy(maxpthlen);

    return rc;

}

/* static */ SysStatus
FileLinux::GetAbsPath(const char *nm, PathNameDynamic<AllocGlobal> *&pathName,
		      uval &pathLen, uval &maxPathLen)
{
    return cwd->getAbsPath(nm, pathName, pathLen, maxPathLen);
}

/* static */ SysStatusUval
FileLinux::Getcwd(PathNameDynamic<AllocGlobal> *pathName, uval maxPathLen)
{
    return cwd->getcwd(pathName, maxPathLen);
}

/* static */ SysStatus
FileLinux::Setcwd(PathNameDynamic<AllocGlobal> *pathName, uval pathLen)
{
    cwd->setCWD(pathName, pathLen);
    return 0;
}

/* static */ void
FileLinux::ClassInit()
{
    cwd = new CurrentWorkingDirectory();
}

/* static */ SysStatus
FileLinux::GetStatus(const char *nm, FileLinux::Stat *status,
		     uval followLink /* = 1*/)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;
    uval retry, numretry=0;

    do {
	retry = 0;

	rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
	if (_FAILURE(rc)) {
	    return rc;
	}

	rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh,
						   followLink);
	if (_FAILURE(rc)) {
	    pth->destroy(maxpthlen);
	    return rc;
	}

	stubNT.setOH(doh);
	// get name and len from pth
	rc = stubNT._getStatus(pth->getBuf(), pthlen, *status, followLink);
	if (_FAILURE(rc)) {
	    pth->destroy(maxpthlen);
	    if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
		(_SGENCD(rc) == ECANCELED)) {
		if (numretry++ > 10) {
		    tassertWrn(0, "too many symbolic links\n");
		    return _SERROR(2676, 0, ELOOP);
		}
		retry = 1;		// retry operation

		rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
		_IF_FAILURE_RET(rc);
		DREFGOBJ(TheMountPointMgrRef)->resolveSymbolicLink(
		    pth, pthlen);
		pth->destroy(maxpthlen);
	    } else {
		return rc;
	    }
	}
    } while (retry != 0);

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatus
FileLinux::Symlink(const char *oldpath, const char *nm)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);
    SysStatus rc;

    rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    if (_FAILURE(rc)) {
	pth->destroy(maxpthlen);
	return rc;
    }

    stubNT.setOH(doh);

    rc = stubNT._symlink(pth->getBuf(), pthlen, (char *)oldpath);
    if (_FAILURE(rc)) {
	if ((_SCLSCD(rc) == FileLinux::HIT_SYMBOLIC_LINK) &&
	    (_SGENCD(rc) == ECANCELED)) {
	    tassertMsg(0, "got a symbolic link, implement loop\n");
	}
    }

    pth->destroy(maxpthlen);
    return rc;
}

/* static */ SysStatusUval
FileLinux::Readlink(const char *nm, char *buf, size_t bufsize)
{
    if (bufsize < 0) {
	return _SERROR(2659, 0, EINVAL);
    }

    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    SysStatusUval rc;

    rc = FileLinux::GetAbsPath(nm, pth, pthlen, maxpthlen);
    if (_FAILURE(rc)) {
	return rc;
    }

    rc = DREFGOBJ(TheMountPointMgrRef)->readlink(pth, pthlen, maxpthlen,
						 buf, (uval) bufsize);
    pth->destroy(maxpthlen);
    return rc;
}

/* virtual */ void
FileLinux::setFlags(uval32 flags) {
    AtomicOr32(&openFlags, flags);
}

/* virtual */ void
FileLinux::modFlags(uval32 setBits, uval32 clrBits) {
    uval32 origFlags;
    uval32 tmpFlags;
    do {
	origFlags = tmpFlags = openFlags;
	tmpFlags |= setBits;
	tmpFlags &= ~clrBits;
    } while (!CompareAndStore32(&openFlags, origFlags, tmpFlags));
}

/* virtual */ void
FileLinux::clrFlags(uval32 flags) {
    AtomicAnd32(&openFlags, ~flags);
}

/* virtual */ SysStatus
FileLinux::ioctl(uval request, va_list args)
{
    switch (request) {
    case FIONBIO:
    {
	int* val = va_arg(args, int*);
	if (*val) {
	    setFlags(O_NONBLOCK);
	} else {
	    clrFlags(O_NONBLOCK);
	}
	return 0;
    }
    case TIOCSCTTY:
    case TIOCGPTN:
    case TIOCSPTLCK:
    case TCGETS:
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
    case TIOCGWINSZ:
    case TIOCSWINSZ:
    case TIOCGPGRP:
    case TIOCSPGRP:
	return _SERROR(1858, 0, ENOTTY);
	break;
    default:
	break;
    }
    return _SERROR(1965, 0, EOPNOTSUPP);
}

/* static */ uval
FileLinux::Stat::check(char expr, char *msg) {
    if (!expr) {
	tassertWrn(0, "%s\n", msg);
	return 1;
    }
    return 0;
}

SysStatus
FileLinux::Stat::Compare(FileLinux::Stat *tmpStatus)
{
    uval sum = 0;
    sum += check(st_ino == tmpStatus->st_ino,"st_ino differs");
    sum += check(st_mode == tmpStatus->st_mode, "st_mode differs");
    //tassert(st_mode == tmpStatus->st_mode, err_printf("ops"));
    sum += check(st_nlink == tmpStatus->st_nlink, "st_nlink differs");
    sum += check(st_uid == tmpStatus->st_uid, "st_uid differs");
    sum += check(st_gid == tmpStatus->st_gid, "st_gid differs");
    sum += check(st_size == tmpStatus->st_size, "st_size differs");
    /* for debugging */
    if (st_size != tmpStatus->st_size) {
	tassertWrn(0, "st_size differs: cached %ld FS %ld\n",
		   tmpStatus->st_size, st_size);
    }

    // FIXME dilma: compare time attributes (right now they are not
    // being updated consistently in the fslib, so they very likely
    // differ ...

    if (sum) {
	return _SERROR(1774, 0, 0);
    } else {
	return 0;
    }
}

/* virtual */ SysStatusUval
FileLinux::readAlloc(uval len, char * &buf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::readAlloc called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::readAllocAt(uval len, uval off, At at, char * &bf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::readAllocAt called\n"));
    return 0;
}
/* virtual */ SysStatus
FileLinux::readFree(char *ptr)
{
    passert(0, err_printf("FileLinux::readFree called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::writeAlloc(uval len, char * &buf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::writeAlloc called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::writeAllocAt(uval len, uval off, At at, char * &bf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::writeAllocAt called\n"));
    return 0;
}
/* virtual */ SysStatus
FileLinux::writeFree(char *ptr)
{
    passert(0, err_printf("FileLinux::writeFree called\n"));
    return 0;
}
/* virtual */ SysStatus
FileLinux::lock()
{
    passert(0, err_printf("FileLinux::lock called\n"));
    return 0;
}
/* virtual */ SysStatus
FileLinux::unLock()
{
    passert(0, err_printf("FileLinux::unLock called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::locked_readAlloc(uval len, char * &buf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::locked called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::locked_readAllocAt(uval len, uval off, At at, char *&bf,
			      ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::locked_readAllocAt called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::locked_readRealloc(char *prev, uval oldlen, uval newlen,
			      char * &buf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::locked_readRealloc called\n"));
    return 0;
}
/* virtual */ SysStatus
FileLinux::locked_readFree(char *ptr)
{
    passert(0, err_printf("FileLinux::locked_readFree called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::locked_writeAllocAt(uval len, uval off, At at,
			       char *&bf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::locked_writeAllocAt called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::locked_writeAlloc(uval len, char * &buf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::locked_writeAlloc called\n"));
    return 0;
}
/* virtual */ SysStatusUval
FileLinux::locked_writeRealloc(char *prev, uval oldlen, uval newlen,
			       char * &buf, ThreadWait **tw)
{
    passert(0, err_printf("FileLinux::locked_writeRealloc called\n"));
    return 0;
}
/* virtual */ SysStatus
FileLinux::locked_writeFree(char *ptr)
{
    passert(0, err_printf("FileLinux::locked_writeFree called\n"));
    return 0;
}
/* virtual */ SysStatus
FileLinux::locked_flush(uval release)
{
    passert(0, err_printf("FileLinux::locked_flush called\n"));
    return 0;
}

/* static */ SysStatus
FileLinux::Statfs(const char *path, struct statfs *buf)
{
    PathNameDynamic<AllocGlobal> *pth;
    uval pthlen, maxpthlen;
    SysStatus rc;
    rc = FileLinux::GetAbsPath(path, pth, pthlen, maxpthlen);
    _IF_FAILURE_RET(rc);

    ObjectHandle doh;
    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);

    rc = DREFGOBJ(TheMountPointMgrRef)->lookup(pth, pthlen, maxpthlen, doh);
    _IF_FAILURE_RET(rc); // FIXME free path  Ask Dilma.  Isnt' this freed below?

    stubNT.setOH(doh);
    // get name 4and len from pth
    rc = stubNT._statfs(*buf);

    pth->destroy(maxpthlen);

    return rc;
}

/* virtual */ SysStatus
FileLinux::attach()
{
    uval32 old;
    do {
	old = refCount;
	if (old == 0) {
	    return _SERROR(2790, 0, ENOENT);
	}
    } while (!CompareAndStore32(&refCount, old, old+1));
    return 0;
}

/* virtual */ SysStatusUval
FileLinux::detach()
{
    uval old = FetchAndAdd32(&refCount, (uval32)-1);
    if (old == 1) return destroy();
    tassertMsg(sval(old)>0,"Non detach destroy: %ld\n",old);

    return 1;
}

/* static */ SysStatus
FileLinux::Sync()
{
    SysStatus rc;

    ListSimple<ObjectHandle*, AllocGlobal> *list;
    rc = DREFGOBJ(TheMountPointMgrRef)->getNameTreeList(list);
    if (_FAILURE(rc)) {
	tassertWrn(0, "MountPointMgr method getNameTreeList returned "
		   "rc 0x%lx\n", rc);
	return 0;
    }

    StubNameTreeLinux stubNT(StubObj::UNINITIALIZED);

    void *curr = NULL;
    ObjectHandle *ohp;
    while ((curr = list->next(curr, ohp))) {
	stubNT.setOH(*ohp);
	rc = stubNT._sync();
	tassertWrn(_SUCCESS(rc), "stubNT._sync() call failed with rc 0x%lx\n",
		   rc);
    }

    // free the list
    while (list->removeHead(ohp)) {
	//err_printf("removed ophp %p\n", ohp);
    }
    delete list;

    return 0;
}
