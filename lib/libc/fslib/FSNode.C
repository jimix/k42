/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSNode.C,v 1.6 2004/07/11 21:59:24 andrewb Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaNameTreeLinux.H>
#include <fcntl.h>
#include "FSNode.H"
#include <stub/StubMountPointMgr.H>

SysStatus
FSNode::Register(const char* name, TypeID type,struct stat *s)
{
    SysStatus rc;

    strncpy(entryName,name,16);
    devType = type;
    status = s;

    // Create cobj stuff

    ObjRef objRef;
    XHandle xHandle;
    objRef = (Obj **)CObjRootSingleRep::Create(this);

    // initialize xhandle, use the same for everyone
    xHandle = MetaNameTreeLinux::createXHandle(objRef, GOBJ(TheProcessRef),
					       MetaNameTreeLinux::none,
					       MetaNameTreeLinux::lookup);

    selfOH.initWithMyPID(xHandle);

    // register with mount point server
    char emptybuf[1];
    uval emptyLen = 0;



    PathName *emptyPath;
    PathNameDynamic<AllocGlobal> *pathToMount;
    uval pathLenToMount;
    rc = PathName::PathFromBuf(emptybuf, emptyLen, emptyPath);
    // register with mount point server
    pathLenToMount = PathNameDynamic<AllocGlobal>::Create(entryName,
		     strlen(entryName), 0, 0, pathToMount);


    rc = StubMountPointMgr::_RegisterMountPoint(pathToMount->getBuf(),
						pathLenToMount,
						selfOH,
						emptyPath->getBuf(),
						emptyLen, NULL, 0, 1);
    tassert(_SUCCESS(rc),
	    err_printf("register mount point failed: %016lx\n",rc));

    if (_FAILURE(rc)) {
	pathToMount->destroy(pathLenToMount);
    }

    return rc;
};


// call from client stub
/* virtual */ SysStatusUval
FSNode::_getStatus(char *name, uval namelen, struct stat &retStatus, 
		   ProcessID pid)
{
    memcpy(&retStatus, status, sizeof(struct stat));
    return 0;
}

