/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DevFSDir.C,v 1.8 2004/10/05 21:28:20 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple devfs nodes.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "DevFSDir.H"
#include <fslib/ServerFileBlock.H>
#include <meta/MetaDevFSDir.H>
#include <meta/MetaBlockDev.H>
#include "FileSystemDev.H"
#include "ServerFileDirDev.H"
#include <fslib/virtfs/FileInfoVirtFS.H>

/* virtual */ SysStatus
DevFSDir::getType(TypeID &id)
{
    id = MetaDevFSDir::typeID();
    return 0;
}

/* static */ void
DevFSDir::ClassInit() {
    MetaDevFSDir::init();
}

/* virtual */
DevFSDir::__DevFSDir::~__DevFSDir() {
    if (name) {
	freeGlobal(name,strlen(name)+1);
    }
}

/* virtual */ SysStatus
DevFSDir::__DevFSDir::init(char* fname, mode_t mode)
{
    SysStatus rc;
    rc = FileInfoVirtFSDirStatic::init(mode);
    _IF_FAILURE_RET(rc);
    uval namelen = strlen(fname)+1;
    name = (char*)allocGlobal(namelen);
    memcpy(name, fname, namelen);
    name[namelen] = 0;

    status.st_mode = mode;
    status.st_rdev = FileLinux_DIR;
    status.st_dev  = 76;

    return 0;
}

/* virtual */ SysStatusUval
DevFSDir::__DevFSDir::getServerFileType()
{
    return FileInfoVirtFS::None;
}

/* virtual */ SysStatus
DevFSDir::__DevFSDir::createServerFileBlock(ServerFileRef &fref)
{
    return _SERROR(2364, 0, EOPNOTSUPP);
}


/* virtual */ SysStatus
DevFSDir::__DevFSDir::destroy() {
    SysStatus rc = removeAll();
    if (_FAILURE(rc)) {
	return rc;
    }
    rc = FileInfoVirtFSDirStatic::destroy();
    tassertRC(rc,"destroy failure\n");
    return rc;
}

/* virtual */ SysStatus
DevFSDir::destroy() {
    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    SysStatus rc = dir->destroy();
    if (_FAILURE(rc)) {
	return rc;
    }
    dir = NULL;

    return destroyUnchecked();
}


DevFSDir::DevFSDir() {
    CObjRootSingleRep::Create(this);
}

/* static */ SysStatus
DevFSDir::Create(DevFSDirRef &ref, char* name, mode_t mode,
		 ObjectHandle par, ProcessID pid)
{

    SysStatus rc;
    DevFSDir *here = new DevFSDir;
    here->dir = new __DevFSDir;
    rc = here->dir->init(name, mode);

    if (_SUCCESS(rc)) {
	rc = FileSystemDev::RegisterNode(here->dir, name, mode, par, pid);
    }

    if (_SUCCESS(rc)) {
	ref = here->getRef();
	FileInfoVirtFSDir * ref2;
	here->dir->getParent(ref2);
	tassertWrn(ref2, "dir '%s'/%p has no parent\n", name, here);
    } else {
	here->destroy();
    }
    return rc;
}

/* virtual */ SysStatus
DevFSDir::add(const char *nm, uval len, FileInfoVirtFS* finfo) {
    return dir->add(nm, len, finfo);
}

/* virtual */ SysStatus
DevFSDir::getDir(FileInfoVirtFSDir* &d)
{
    d = dir;
    return 0;
}

