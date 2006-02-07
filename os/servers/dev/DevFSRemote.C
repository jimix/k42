/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSRemote.C,v 1.8 2004/10/05 21:28:20 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple devfs nodes, for devices that are
 * identfied by remote ObjectHandle
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DevFSRemote.H"
#include "DevFSDir.H"
#include <fslib/ServerFileBlock.H>
#include <meta/MetaBlockDev.H>
#include "FileSystemDev.H"
#include <stub/StubDevOpener.H>

/* static */ void
DevFSRemote::ClassInit() {
    MetaDevFSRemote::init();
}

/* virtual */
DevFSRemote::__DevFSRemote::~__DevFSRemote() {
    if (name) {
	freeGlobal(name,strlen(name)+1);
	name = NULL;
    }
}

/* virtual */ SysStatus
DevFSRemote::__DevFSRemote::init(char* fname, mode_t mode,
				 ObjectHandle oh, TypeID ohType, uval tok)
{
    SysStatus rc = FIVFAccessOH::init(oh, ohType, mode);
    uval namelen = strlen(fname)+1;
    name = (char*)allocGlobal(namelen);
    memcpy(name, fname, namelen);
    name[namelen] = 0;
    token = tok;

    status.st_mode = mode;
    status.st_rdev = ohType;
    status.st_dev  = 76;
    return rc;
}

/* virtual */ SysStatus
DevFSRemote::init(char* fname, mode_t mode,
		  ObjectHandle oh, TypeID ohType,
		  ObjectHandle parent, ProcessID pid, uval token)
{
    CObjRootSingleRep::Create(this);
    dfsr = new __DevFSRemote;
    SysStatus rc = dfsr->init(fname, mode, oh, ohType, token);
    _IF_FAILURE_RET(rc);
    return FileSystemDev::RegisterNode(dfsr, fname, mode, parent, pid);
}


/* virtual */SysStatus
DevFSRemote::exportedXObjectListEmpty()
{
    return destroy();
}

/* virtual */ SysStatus
DevFSRemote::destroy() {
    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    dfsr->destroy();
    dfsr = NULL;

    return destroyUnchecked();
}

/* virtual */ SysStatus
DevFSRemote::__DevFSRemote::openCreateServerFile(ServerFileRef &fref,
						 uval oflag, ProcessID pid,
						 ObjectHandle &oh,
						 uval &useType, TypeID &ohType)
{
    SysStatus rc = 0;
    if (obj.valid()) {
	StubDevOpener device(StubObj::UNINITIALIZED);
	device.setOH(obj);
	rc = device._open(oh, ohType, pid, oflag, token);
	_IF_FAILURE_RET(rc);
    } else {
	oh = obj;
	ohType = type;
    }
    useType = FileLinux::NON_SHARED;
    return rc;
}

/* static */ SysStatus
DevFSRemote::_CreateNode(__inbuf(namelen) char* name,
			 __in mode_t mode,
			 __in ObjectHandle parent,
			 __in ObjectHandle obj,
			 __in TypeID objType,
			 __out ObjectHandle &nodeOH,
			 __in uval token,
			 __CALLER_PID pid) //StubDevFSRemote
{
    SysStatus rc;
    DevFSRemote *dfr = new DevFSRemote;
    rc = dfr->init(name, mode, obj, objType, parent, pid, token);

    _IF_FAILURE_RET(rc);

    DevFSRemoteRef newObj = (DevFSRemoteRef)dfr->getRef();
    if (_SUCCESS(rc)) {
	rc = DREF(newObj)->giveAccessByServer(nodeOH, pid,
					      MetaObj::controlAccess,
					      MetaObj::none);
    }
    if (_FAILURE(rc)) {
	DREF(newObj)->destroy();
    }
    return rc;
}
