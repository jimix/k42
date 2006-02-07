/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NameTreeLinuxUnion.C,v 1.4 2004/07/11 21:59:30 andrewb Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaNameTreeLinux.H>
#include "NameTreeLinuxUnion.H"
#include <sys/MountPointMgrClient.H>
#include <io/FileLinux.H>
#include <stub/StubFileLinuxServer.H>
#include <fslib/FSCreds.H>

/* static */ void
NameTreeLinuxUnion::Create(char *cPathToMount, ObjectHandle primOH,
			   ObjectHandle secOH,
			   char *desc, uval descLen,
			   uval isCoverable /* = 1 */)
{
    ObjRef objRef;
    XHandle xHandle;
    PathNameDynamic<AllocGlobal> *pathToMount;
    uval pathLenToMount;
    SysStatus rc;
    NameTreeLinuxUnion *obj;

    obj = new NameTreeLinuxUnion;
    objRef = (Obj **)CObjRootSingleRep::Create(obj);

    // initialize xhandle, use the same for everyone
    xHandle = MetaNameTreeLinux::createXHandle(objRef, GOBJ(TheProcessRef),
					       MetaNameTreeLinux::none,
					       MetaNameTreeLinux::lookup);

    obj->oh.initWithMyPID(xHandle);
    obj->primNT.setOH(primOH);
    obj->secNT.setOH(secOH);

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
NameTreeLinuxUnion::_getObj(char *name, uval namelen, uval oflag, uval mode,
			 ObjectHandle &oh, TypeID &type, uval &useType,
			 /* argument for simplifying gathering traces of
			  * file sharing information. This should go
			echo    * away. */
			 ObjRef &fref,
			 ProcessID pid)
{
    ObjectHandle poh;
    SysStatus rc = primNT._getObj(name, namelen, oflag, mode, poh, type,
				  useType, fref);

    if (_SUCCESS(rc)) {
	// need to generate oh for this client
	SysStatus rctmp = passAccess(poh, oh, pid, (AccessRights)-1,
				     MetaObj::none);
	tassertMsg(_SUCCESS(rctmp), "?");
    } else {
	if (oflag & O_CREAT) {
	    // file creation failed
	    if (_SGENCD(rc) == ENOENT) {
		// we need to create intermediate directories
		rc = createDirs(name, namelen, pid);
		if (_SUCCESS(rc)) {
		    rc = primNT._getObj(name, namelen, oflag, mode, poh, type,
					useType, fref);
		    tassertMsg(_SUCCESS(rc), "?");
		    
		    // need to generate oh for this client
		    SysStatus rctmp = passAccess(poh, oh, pid, (AccessRights)-1,
						 MetaObj::none);
		    tassertMsg(_SUCCESS(rctmp), "?");
		}
	    } else {
		//err_printf("_getObj returning error rc (%ld, %ld, %ld)\n",
		//	   _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
		return rc;
	    }
	} else { // not file creation, try to find it on secondary file system
	    rc = secNT._getObj(name, namelen, oflag, mode, poh, type,
			       useType, fref);
	    if (_SUCCESS(rc)) {
		// need to generate oh for this client
		SysStatus rctmp = passAccess(poh, oh, pid, (AccessRights)-1,
					     MetaObj::none);
		tassertMsg(_SUCCESS(rctmp), "?");

	    } else {
		//err_printf("_getObj on secNT also failed\n");
	    }
	}
    }
    
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxUnion::_mkdir(char *name, uval namelen, uval mode, ProcessID pid)
{
    SysStatus rc;

    rc = primNT._mkdir(name, namelen, mode);
    if (_FAILURE(rc)) {
	if (_SGENCD(rc) == ENOENT) {
	    // we need to create intermediate directories
	    rc = createDirs(name, namelen, pid);
	    if (_SUCCESS(rc)) {
		rc = primNT._mkdir(name, namelen, mode);
	    }
	} else {
	    err_printf("_mkdir returning error rc (%ld, %ld, %ld)\n",
		       _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	    return rc;
	}
    }
    return rc;
}

// call from client stub
/* virtual */ SysStatus
NameTreeLinuxUnion::_getStatus(char *name, uval namelen,
			       struct stat &retStatus, uval followLink,
			       ProcessID pid)
{
    SysStatus rc;
    rc = primNT._getStatus(name, namelen, retStatus, followLink);
    if (_FAILURE(rc)) { // try secondary file system
	rc = secNT._getStatus(name, namelen, retStatus, followLink);
    }

    //tassertWrn(_SUCCESS(rc), "_getStatus failed\n");
    return rc;
}

SysStatus
NameTreeLinuxUnion::createDirs(char *name, uval namelen, ProcessID pid)
{
    SysStatus rc;
    PathName *pathName;
    FileLinux::Stat stat_buf;

    rc = FSCreds::Acquire(pid); // init thread specific state to caller creds
    if (_FAILURE(rc)) {
	tassertWrn(0, "creds acquire failed\n");
	return rc;
    }

    uval pathLen = namelen;
    PathName *pathComp;  // Work with this one
    PathName *pathCompNext;
    uval compLen, prefixLen = 0;
    uval compNextLen = pathLen;

    rc = PathName::PathFromBuf(name, namelen, pathName);
    if (_FAILURE(rc)) {
	goto return_rc;
    }

    pathCompNext = pathName;

    if (pathName->isLastComp(pathLen) ||	// root is the container
	// root is it's own container
	(!(pathName->isComp(pathLen, pathName)))) {
	tassertMsg(0, "?? no intermediary directories to create?\n");
	goto return_rc;
    }

    do {
	pathComp = pathCompNext;
	compLen = compNextLen;
	prefixLen += sizeof(uval8) + pathComp->getCompLen(compLen);
	//err_printf("invoking _getStatus with prefixLen %ld\n", prefixLen);
	rc = secNT._getStatus(name, prefixLen, stat_buf, 1);
	if (_FAILURE(rc)) {
#if 0 // for debugging only
	    char buf[255];
	    uval len  = pathComp->getCompLen(compLen);
	    memcpy(buf, pathComp->getCompName(compLen), len);
	    buf[len] = '\0';
	    err_printf("Failure: dir %s not available on secNT\n", buf);
#endif
	    goto return_rc;
	}
	pathCompNext = pathComp->getNext(compLen, compNextLen);
    } while (!(pathCompNext->isLastComp(compNextLen)));

    // just create now
    pathCompNext = pathName;
    compNextLen = pathLen;
    prefixLen = 0;
    do {
	pathComp = pathCompNext;
	compLen = compNextLen;
	prefixLen += sizeof(uval8) + pathComp->getCompLen(compLen);
	rc = secNT._getStatus(name, prefixLen, stat_buf, 1);
	uval mode = stat_buf.st_mode;
	tassertMsg(_SUCCESS(rc), "impossible?\n");
	rc = primNT._getStatus(name, prefixLen, stat_buf, 1);
	if (_FAILURE(rc)) {
#if 0 // for debugging only
	    char buf[255];
	    uval len  = pathComp->getCompLen(compLen);
	    memcpy(buf, pathComp->getCompName(compLen), len);
	    buf[len] = '\0';
	    err_printf("we'll invoke mkdir for %s\n", buf);
#endif
	    // need to create this component
	    rc = primNT._mkdir(name, prefixLen, mode);
	    tassertMsg(_SUCCESS(rc), "why?");
	}
	pathCompNext = pathComp->getNext(compLen, compNextLen);
    } while (!(pathCompNext->isLastComp(compNextLen)));

 return_rc:
    FSCreds::Release();
    return rc;
}

SysStatus
NameTreeLinuxUnion::passAccess(ObjectHandle poh, ObjectHandle &oh, ProcessID pid,
			       AccessRights match, AccessRights nomatch)
{
    SysStatus rc;
    StubFileLinuxServer stub(StubObj::UNINITIALIZED);
    stub.setOH(poh);
    rc = stub._giveAccess(oh, pid, match, nomatch);
    tassertMsg(_SUCCESS(rc), "?");
    rc = stub._releaseAccess();
    tassertMsg(_SUCCESS(rc), "?");

    return rc;
}
