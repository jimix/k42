/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysFSAttrFile.C,v 1.2 2004/10/05 21:28:20 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: sysfs nodes
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "SysFSAttrFile.H"
#include "DevFSDir.H"
#include <fslib/ServerFileBlock.H>
#include <meta/MetaBlockDev.H>
#include "FileSystemDev.H"
#include "FileInfoDev.H"

class ServerFileBlk;
typedef ServerFileBlk** ServerFileBlkRef;

/* virtual */ void
SysFSAttrFile::__SysFSAttrFile::init(const char* n, dev_t dev,
				     uval mode, uval rdev,
				     uval uid, uval gid,
				     uval tok, ObjectHandle oh)
{
    token = tok;
    virtFileOH = oh;
    FileInfoDev::init(n, dev, mode, rdev, uid, gid);
}

/* virtual */ SysStatusUval
SysFSAttrFile::__SysFSAttrFile::getServerFileType() {
    return VirtRemote;
}

/* virtual */ SysStatus
SysFSAttrFile::__SysFSAttrFile::createServerFileBlock(ServerFileRef &fref) {
    return ServerFileVirtFS::Create(fref, this, 0, token);
}



/* static */ void
SysFSAttrFile::ClassInit() {
    MetaSysFSAttrFile::init();
}

/* virtual */ SysStatus
SysFSAttrFile::destroy() {
    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    sfaf->destroy();
    sfaf = NULL;

    return destroyUnchecked();
}


/* virtual */ SysStatus
SysFSAttrFile::init(char* fname, dev_t devID, mode_t mode,
		    ObjectHandle virtFileProvider, uval tok)
{
    SysStatus rc = 0;
    CObjRootSingleRep::Create(this);
    sfaf = new __SysFSAttrFile;
    sfaf->init(fname, devID, mode, FileLinux_FILE,
	       0, 0, tok, virtFileProvider);
    sfaf->status.st_dev  = 76; // who cares?
    return rc;
}


/* Static */ SysStatus
SysFSAttrFile::_CreateNode(__inbuf(*) char* name,
			   __in mode_t mode,
			   __in ObjectHandle parent,
			   __in ObjectHandle virtFileProvider,
  			   __in uval token,
			   __out ObjectHandle &oh,
			   __CALLER_PID pid) //StubSysFSAttrFile
{
    SysStatus rc;
    SysFSAttrFile *af = new SysFSAttrFile;
    rc = af->init(name, 0, mode, virtFileProvider, token);

    _IF_FAILURE_RET(rc);

    rc = FileSystemDev::RegisterNode(af->sfaf, name, mode, parent, pid);

    if (_SUCCESS(rc)) {
	rc = af->giveAccessByServer(oh,pid);
    }

    if (_FAILURE(rc)) {
	af->destroy();
    }


    return rc;
}
