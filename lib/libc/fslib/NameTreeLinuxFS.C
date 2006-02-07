/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NameTreeLinuxFS.C,v 1.60 2005/04/27 17:35:07 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaNameTreeLinux.H>
#include <fcntl.h>
#include "ServerFile.H"
#include "DirLinuxFS.H"
#include "DirLinuxFSVolatile.H"
#include "NameTreeLinuxFS.H"
#include "FSCreds.H"
#include <sys/MountPointMgrClient.H>

/* static */ void
NameTreeLinuxFS::Create(char *cPathToMount, DirLinuxFSRef dir,
			char *desc, uval descLen,
			uval isCoverable /* = 1 */)
{
    ObjRef objRef;
    XHandle xHandle;
    PathNameDynamic<AllocGlobal> *pathToMount;
    uval pathLenToMount;
    SysStatus rc;
    NameTreeLinuxFS *obj;

    obj = new NameTreeLinuxFS;
    objRef = (Obj **)CObjRootSingleRep::Create(obj);

    // initialize xhandle, use the same for everyone
    xHandle = MetaNameTreeLinux::createXHandle(objRef,
					       GOBJ(TheProcessRef),
					       MetaNameTreeLinux::none,
					       MetaNameTreeLinux::lookup);

    obj->oh.initWithMyPID(xHandle);
    obj->rootDirLinuxRef = dir;

    pathLenToMount = PathNameDynamic<AllocGlobal>::Create(
	cPathToMount, strlen(cPathToMount), 0, 0, pathToMount, PATH_MAX+1);

    // register with mount point server
    rc = DREFGOBJ(TheMountPointMgrRef)->registerMountPoint(
	pathToMount, pathLenToMount, obj->oh, NULL, 0, desc, descLen,
	isCoverable);
    tassert(_SUCCESS(rc), err_printf("register mount point failed\n"));

    // emptyPath doesn't need to be freed, but pathToMount needs to
    pathToMount->destroy(PATH_MAX+1);
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_getObj(char *name, uval namelen, uval oflag, uval mode,
			 ObjectHandle &obh, TypeID &type, uval &useType,
			 /* argument for simplifying gathering traces of
			  * file sharing information. This should go
			  * away. */
			 ObjRef &fref,
			 ProcessID pid)
{
    /* No lock - getObj locks */
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = DREF(dirLinuxRef)->open(oflag, pid, obh, useType, type);
    } else {
	rc = DREF(dirLinuxRef)->getObj(remainder, remainderLen,
				       oflag, (mode_t) mode, pid, obh,
				       useType, type,
				       /* argument for simplifying gathering
					* traces of file sharing information.
					* This should go away. */
				       fref);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    char buf[PATH_MAX+1];
    memcpy(buf, remainder, remainderLen);
    buf[remainderLen] = '\0';
    tassertWrn(_SUCCESS(rc), "getObj failed rc 0x%lx %s\n", rc, buf);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

return_rc:
    return rc;
}

// call from client stub
/*virtual */ SysStatus
NameTreeLinuxFS::_mknod(char *name, uval namelen, uval mode, uval dev,
			ProcessID processID)
{
    //  fixme locks
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(processID);
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = _SERROR(2878, 0, EINVAL);
    } else {
	rc = DREF(dirLinuxRef)->mknod(remainder, remainderLen,
				      mode, dev);
    }

    nhLock->releaseR();
    FSCreds::Release();

return_rc:
#ifdef MARIA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "%s failed rc 0x%lx\n", __PRETTY_FUNCTION__, rc);
#endif // ifdef MARIA_DEBUG_CALL_FAILURES

    return rc;
}

// call from client stub
/*virtual */ SysStatus
NameTreeLinuxFS::_bind(__inbuf(namelen)char *name, __in uval namelen,
                       __in uval mode, __in ObjectHandle serverSocketOH,
                       __CALLER_PID processID)
{
    //  fixme locks
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(processID);
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = _SERROR(2882, 0, EINVAL);
    } else {
	rc = DREF(dirLinuxRef)->bind(remainder, remainderLen, mode,
				     serverSocketOH);
    }

    nhLock->releaseR();
    FSCreds::Release();

return_rc:
#ifdef MARIA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "%s failed\n", __PRETTY_FUNCTION__);
    _SERROR_EPRINT(rc);
#endif // ifdef MARIA_DEBUG_CALL_FAILURES

    return rc;
}

// call from client stub
/*virtual */ SysStatus
NameTreeLinuxFS::_getSocketObj(__inbuf(namelen)char *name, __in uval namelen,
                               __out ObjectHandle &serverSocketOH,
			       __CALLER_PID processID)
{
    //  fixme locks
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(processID);
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = _SERROR(2901, 0, EINVAL);
    } else {
	rc = DREF(dirLinuxRef)->getSocketObj(remainder, remainderLen, 
					     serverSocketOH, processID);
    }

    nhLock->releaseR();
    FSCreds::Release();

return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_chown(char *name, uval namelen, uval uid, uval gid,
			ProcessID pid)
{
    SysStatus rc;
    DirLinuxFSRef dirLinuxRef;
    char *remainder;
    uval remainderLen;
    PathName *pathName;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = DREF(dirLinuxRef)->fchown((uid_t) uid, (gid_t)gid);
    } else {
	rc = DREF(dirLinuxRef)->chown(remainder, remainderLen,
				      (uid_t) uid, (gid_t)gid);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "chown failed rc 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES
 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_utime(char *name, uval namelen, const struct utimbuf& utbuf,
			ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = DREF(dirLinuxRef)->utime(&utbuf);
    } else {
	rc =  DREF(dirLinuxRef)->utime(remainder,
				       remainderLen, &utbuf);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "utime failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatusUval
NameTreeLinuxFS::_readlink(char *name, uval namelen, char *buf, uval bufsize,
			   ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) {
	rc = _SERROR(2650, 0, EINVAL);
	goto return_rc;
    }

    rc =  DREF(dirLinuxRef)->readlink(remainder, remainderLen, buf,
				      bufsize);
    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "utime failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_symlink(char *name, uval namelen, char *oldpath,
			  ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) {
	rc = _SERROR(2877, 0, EINVAL);
	goto return_rc;
    }

    rc =  DREF(dirLinuxRef)->symlink(remainder, remainderLen, oldpath);

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "utime failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_utime(char *name, uval namelen, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = DREF(dirLinuxRef)->utime(NULL);
    } else {
	rc = DREF(dirLinuxRef)->utime(remainder, remainderLen);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "second utime failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_mkdir(char *name, uval namelen, uval mode, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	// already exists
	rc = _SERROR(1783, 0, EEXIST);
    } else {
	rc = DREF(dirLinuxRef)->mkdir(remainder, remainderLen,
				      (mode_t) mode);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "mkdir failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_rmdir(char *name, uval namelen, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	// can't remove root directory
	rc = _SERROR(1856, 0, ENOTEMPTY);
    } else {
	rc =  DREF(dirLinuxRef)->rmdir(remainder, remainderLen);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "rmdir failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_chmod(char *name, uval namelen, uval mode, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = DREF(dirLinuxRef)->fchmod((mode_t) mode);
    } else {
	rc = DREF(dirLinuxRef)->chmod(remainder, remainderLen,
				      (mode_t) mode);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "chmod failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_truncate(char *name, uval namelen, uval length, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = _SERROR(1727, 0, EISDIR);
    } else {
	rc = DREF(dirLinuxRef)->truncate(remainder, remainderLen,
					 (off_t) length);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "truncate failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_getStatus(char *name, uval namelen, struct stat &retStatus,
			    uval followLink, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = DREF(dirLinuxRef)->getStatus(FileLinux::Stat::FromStruc(
	    &retStatus));
    } else {
	rc = DREF(dirLinuxRef)->getStatus(remainder, remainderLen,
					  FileLinux::Stat::FromStruc(
					      &retStatus), followLink);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    char buf[PATH_MAX+1];
    memcpy(buf, remainder, remainderLen);
    buf[remainderLen] = '\0';
    tassertWrn(_SUCCESS(rc), "getStatus failed 0x%lx %s\n", rc, buf);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_link(char *name, uval namelen, char *newName, uval newlen,
		       ProcessID pid)
{
    /* No lock - link locks */
    SysStatus rc;
    PathName *oldPathName, *newPathName;
    char *remainderOld, *remainderNew;
    uval remainderLenOld, remainderLenNew;
    DirLinuxFSRef olddirLinuxRef, newdirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, oldPathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    rc = PathName::PathFromBuf(newName, newlen, newPathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *oldnhLock;
    rc = DREF(rootDirLinuxRef)->lookup(oldPathName, namelen,
				       remainderOld, remainderLenOld,
				       olddirLinuxRef, oldnhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *newnhLock;
    rc = DREF(rootDirLinuxRef)->lookup(newPathName, newlen,
				       remainderNew, remainderLenNew,
				       newdirLinuxRef, newnhLock);
    if (_FAILURE(rc)) {
	oldnhLock->releaseR();
	FSCreds::Release();
	goto return_rc;
    }

    if (oldPathName->getCompLen(namelen) == 0) { // operation on root
	rc = _SERROR(2562, 0, EPERM);
    } else {
	rc = DREF(olddirLinuxRef)->link(remainderOld, remainderLenOld,
					newdirLinuxRef,
					remainderNew, remainderLenNew);
    }

    oldnhLock->releaseR();
    newnhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "link failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_unlink(char *name, uval namelen, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen,
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    if (remainderLen == 0) { // operation on root
	// can't remove root directory
	rc = _SERROR(1794, 0, EISDIR);
    } else {
	rc = DREF(dirLinuxRef)->unlink(remainder, remainderLen);
    }

    nhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    tassertWrn(_SUCCESS(rc), "unlink failed 0x%lx\n", rc);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_rename(char *name, uval namelen, char *newName, uval newLen,
			 ProcessID pid)
{
    SysStatus rc;
    char *remainderOld, *remainderNew;
    uval remainderLenOld, remainderLenNew;
    PathName *oldPathName, *newPathName;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) goto return_rc;

    rc = PathName::PathFromBuf(name, namelen, oldPathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    rc = PathName::PathFromBuf(newName, newLen, newPathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *oldNhLock;
    DirLinuxFSRef oldDirLinuxRef;
    rc = DREF(rootDirLinuxRef)->lookup(oldPathName, namelen,
				       remainderOld, remainderLenOld,
				       oldDirLinuxRef, oldNhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	goto return_rc;
    }

    RWBLock *newNhLock;
    DirLinuxFSRef newDirLinuxRef;
    rc = DREF(rootDirLinuxRef)->lookup(newPathName, newLen,
				       remainderNew, remainderLenNew,
				       newDirLinuxRef, newNhLock);
    if (_FAILURE(rc)) {
	oldNhLock->releaseR();
	FSCreds::Release();
	goto return_rc;
    }

    if (oldPathName->getCompLen(namelen) == 0) { // operation on root
	rc = _SERROR(1767, 0, EPERM);
	goto unlock_return_rc;
    }

    rc = DREF(oldDirLinuxRef)->rename(remainderOld, remainderLenOld,
				      newDirLinuxRef,
				      remainderNew, remainderLenNew);

unlock_return_rc:
    oldNhLock->releaseR();
    newNhLock->releaseR();
    FSCreds::Release();

#ifdef DILMA_DEBUG_CALL_FAILURES
    char buf[PATH_MAX+1];
    memcpy(buf, remainderOld, remainderLenOld);
    buf[remainderLenOld] = '\0';
    tassertWrn(_SUCCESS(rc), "rename failed 0x%lx for %s\n", rc, buf);
#endif // ifdef DILMA_DEBUG_CALL_FAILURES

 return_rc:
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFS::_statfs(struct statfs &buf, ProcessID pid)
{
    SysStatus rc;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    _IF_FAILURE_RET(rc);

    rc = DREF(rootDirLinuxRef)->statfs(&buf);
    FSCreds::Release();
    return rc;
}

/* virtual */ SysStatus
NameTreeLinuxFS::_createVirtFile(char *name, uval namelen, uval mode,
				 ObjectHandle& vfoh, ProcessID pid)
{
	return _SERROR(2146, 0, EOPNOTSUPP);
}

/* virtual */ SysStatus
NameTreeLinuxFS::_sync()
{
    return DREF(rootDirLinuxRef)->sync();
}
