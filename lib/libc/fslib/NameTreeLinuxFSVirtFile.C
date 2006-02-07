/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NameTreeLinuxFSVirtFile.C,v 1.13 2005/07/16 19:42:40 butrico Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: Entry for name lookups for Virtual Files
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "NameTreeLinuxFSVirtFile.H"
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaNameTreeLinux.H>
//#include <fcntl.h>
//#include "ServerFile.H"
#include "DirLinuxFS.H"
#include "DirLinuxFSVolatile.H"
#include "FSCreds.H"
#include <sys/MountPointMgrClient.H>

/* static */ void
NameTreeLinuxFSVirtFile::Create(const char *cPathToMount, DirLinuxFSRef dir,
				uval printFailures/*=0*/)
{
    ObjRef objRef;
    NameTreeLinuxFSVirtFile *obj;
    obj = new NameTreeLinuxFSVirtFile(printFailures);
    objRef = (Obj **) CObjRootSingleRep::Create(obj);

    XHandle xHandle;
    xHandle = MetaNameTreeLinux::createXHandle(objRef, GOBJ(TheProcessRef),
					       MetaNameTreeLinux::none,
					       MetaNameTreeLinux::lookup);
    obj->oh.initWithMyPID(xHandle);
    obj->rootDirLinuxRef = dir;

    PathNameDynamic<AllocGlobal> *pathToMount;
    uval pathToMountLen;
    pathToMountLen = PathNameDynamic<AllocGlobal>::Create(
	cPathToMount, strlen(cPathToMount), 0, 0, pathToMount, PATH_MAX+1);
    /* creates relPath (path relative to name tree root to be associated
     * with mount point). Since we are mounting the root of the name tree, it
     * is empty. Since length is zero, in the current implementation we don't
     * need to invoke PathfromBuf, since it does not perform any action.
     * But of course the PathName implementation can change ... */
    SysStatus rc;
    char emptybuf[1];
    uval emptyLen = 0;
    PathName *emptyPath;
    rc = PathName::PathFromBuf(emptybuf, emptyLen, emptyPath);
    // we don't need to check rc since routine does not fail for length==0,
    // but for now let's assert anyway ...
    tassert(_SUCCESS(rc), err_printf("PathFromBuf failed for len==0\n"));

    // register with mount point server (as not coverable, i.e., mount points
    // above it on the name space won't cover this one)
    rc = DREFGOBJ(TheMountPointMgrRef)->registerMountPoint(
	pathToMount, pathToMountLen, obj->oh, emptyPath, emptyLen,
	"virtfs", strlen("virtfs"), 0);
    tassert(_SUCCESS(rc), err_printf("register mount point failed\n"));

    // emptyPath doesn't need to be freed, but pathToMount needs to
    pathToMount->destroy(PATH_MAX+1);
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxFSVirtFile::_getObj(char *name, uval namelen, uval oflag, uval mode,
			 ObjectHandle &obh, TypeID &type, uval &useType,
			 /* argument for simplifying gathering traces of
			  * file sharing information. This should go
			  * away. */
			 ObjRef &fref,
			 ProcessID pid)
{
    SysStatus rc;

    #undef TRACE_PROCFS
    #ifdef TRACE_PROCFS
    err_printf("%s: name= %s namelen=%ld  print=%ld\n",
	       __PRETTY_FUNCTION__,
	       name, namelen, printFailures);
    #endif

    rc = NameTreeLinuxFS::_getObj(name, namelen, oflag, mode, obh, type,
				  useType, fref, pid);

    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT) &&
	_FAILURE(rc) && printFailures) {
	SysStatus pnrc;
	PathName *pathName;
	pnrc = PathName::PathFromBuf(name, namelen, pathName);
	/* should this show on a noDeb kernel? atm it does */
	if (_SUCCESS(pnrc)) {
	    char buf[1024];
	    pathName->getUPath(namelen, buf, 1024, 0);
	    passertWrn(_SUCCESS(rc),
                   "virtfs open failed on file '%s'(rc=0x%lx)\n", buf, rc);
	}
	else {
	    passertWrn(_SUCCESS(rc),
                   "virtfs open failed on file %s(rc=0x%lx)\n", name, rc);
	}
    }

    return rc;
}

/* virtual */ SysStatus
NameTreeLinuxFSVirtFile::_createVirtFile(char *name, uval namelen, uval mode,
					 ObjectHandle &vfoh, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    char *remainder;
    uval remainderLen;
    DirLinuxFSRef dirLinuxRef;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) return rc;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	return rc;
    }

    RWBLock *nhLock;
    rc = DREF(rootDirLinuxRef)->lookup(pathName, namelen,
				       remainder, remainderLen, 
				       dirLinuxRef, nhLock);
    if (_FAILURE(rc)) {
	FSCreds::Release();
	return rc;
    }

    if (remainderLen == 0) { // operation on root
	rc = _SERROR(2152, 0, EINVAL);
    } else {
	rc = DREF(dirLinuxRef)->createVirtFile(remainder, remainderLen,
					       (mode_t) mode, vfoh);
    }

    nhLock->releaseR();
    FSCreds::Release();
    return rc;
}
