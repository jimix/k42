/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemDev.C,v 1.44 2005/03/04 00:43:58 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: File system specific interface to /dev
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <fslib/NameTreeLinuxFS.H>
#include "FileSystemDev.H"
#include <meta/MetaFileSystemDev.H>
#include "FileInfoDev.H"
#include "ServerFileCharDev.H"
#include <time.h>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <fslib/virtfs/FIVFAccessOH.H>
#include "DevFSDir.H"
#include "DevFSRemote.H"
#include "DevFSBlk.H"
#include "SysFSAttrFile.H"

FileSystemDev::RootList* FileSystemDev::rootList = NULL;

/* virtual */ SysStatus
FileSystemDev::init(const char *mpath, mode_t mode)
{
    SysStatus rc;
    dir = new __DevFSDir;
    rc = dir->init("",mode);

    memcpy(rootEntry.name, mpath, 63);
    rootEntry.name[63] = 0;
    rootEntry.ref = getRef();
    return rc;
}

/* static */ SysStatus
FileSystemDev::Create(FileSystemDev* &fs, const char *mpath, mode_t mode)
{
    SysStatus rc;
    tassertMsg(strlen(mpath)<63, "mount path too long\n");
    fs = new FileSystemDev();
    rc = fs->init(mpath,mode);

    if (_SUCCESS(rc)) {
	fs->dir->mountToNameSpace(mpath);
    }

    if (_FAILURE(rc)) {
	delete fs;
    } else {
	rootList->add(&fs->rootEntry);
    }
    return rc;
}

/* static */ SysStatus
FileSystemDev::GetDir(ObjectHandle dirOH, ProcessID pid,
		      FileInfoVirtFSDirBase* &pdir)
{
    SysStatus rc = 0;
    if (!dirOH.valid()) {
	return _SERROR(2801, 0, EINVAL);
    }

    TypeID type;
    ObjRef ref;
    rc = XHandleTrans::XHToInternal(dirOH.xhandle(), pid, 0ULL, ref, type);

    _IF_FAILURE_RET(rc);

    if (!MetaDevFSDir::isBaseOf(type)) {
	tassertWrn(0, "Expecting DevFSDir OH, didn't get it\n");
	return _SERROR(2363, 0, EINVAL);
    }
    rc = DREF((DevFSDirRef) ref)->getDir(pdir);
    return rc;
}

/* static */ SysStatus
FileSystemDev::_GetRoot(__inbuf(*) char* name,
			__out ObjectHandle &oh, __CALLER_PID pid)
{
    SysStatus rc = _SERROR(2800, 0, ENOENT);
    rootList->acquireLock();
    void *curr = NULL;
    RootEntry *re;
    while ((curr=rootList->next(curr, re))) {
	if (strncmp(name, re->name, 64)==0) {
	    rc = DREF(re->ref)->giveAccessByServer(oh, pid);
	    break;
	}
    }
    rootList->releaseLock();
    return rc;
}

/* static */ SysStatus
FileSystemDev::_Create(__inbuf(*) char* name,
		  __in mode_t mode,
		  __in ObjectHandle par,
		  __out ObjectHandle &result,
		  __CALLER_PID pid) //StubDevFSDir
{
    SysStatus rc;
    DevFSDirRef dref = NULL;
    rc = DevFSDir::Create(dref, name, mode, par, pid);
    _IF_FAILURE_RET(rc);

    rc = DREF(dref)->giveAccessByServer(result,pid);

    if (_FAILURE(rc)) {
	DREF(dref)->destroy();
    }
    return rc;
}


/* static */ SysStatus
FileSystemDev::RegisterNode(FileInfoVirtFS* fivf,
			    const char* name, mode_t mode,
			    ObjectHandle parent, ProcessID pid)
{
    SysStatus rc;
    FileInfoVirtFSDirBase* dir = NULL;

    rc = GetDir(parent, pid, dir);

    _IF_FAILURE_RET(rc);

    rc = dir->add(name, strlen(name), fivf);

    return rc;
}

const char* FileSystemDev::Root="/dev";

/* static */ SysStatus
FileSystemDev::ClassInit(uval isCoverable /* = 0 */)
{
    SysStatus rc;
    MetaFileSystemDev::init();
    FileInfoVirtFSBase::ClassInit();
    DevFSDir::ClassInit();

    rootList = new RootList;

    FileSystemDev* devFS = NULL;
    rc = FileSystemDev::Create(devFS, "/dev", (mode_t)0755|S_IFDIR);
    passertMsg(_SUCCESS(rc), "FileSystem dev failure: %lx\n",rc);

    FileSystemDev* sysFS = NULL;
    rc = FileSystemDev::Create(sysFS, "/sys", (mode_t)0755|S_IFDIR);
    passertMsg(_SUCCESS(rc), "FileSystem sys failure: %lx\n",rc);

    DevFSBlk::ClassInit();
    DevFSRemote::ClassInit();
    DevFSDir::ClassInit();
    SysFSAttrFile::ClassInit();

    const uval mode = S_IFCHR|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;

    //Insert /dev/null entry
    rc = addFIDtoFS(devFS,
	    new FileInfoDev("null", 0, mode, FileLinux_CHR_NULL, 0, 0));
    _IF_FAILURE_RET(rc);

    //Insert /dev/zero entry
    rc = addFIDtoFS(devFS,
	    new FileInfoDev("zero", 0, mode, FileLinux_CHR_ZERO, 0, 0));
    _IF_FAILURE_RET(rc);

    //Insert /dev/random entry
    rc = addFIDtoFS(devFS,
	    new FileInfoDev("random", 0, mode, FileLinux_CHR_RANDOM, 0, 0));
    _IF_FAILURE_RET(rc);

    //Insert /dev/random entry
    rc = addFIDtoFS(devFS,
	    new FileInfoDev("urandom", 0, mode, FileLinux_CHR_RANDOM, 0, 0));
    _IF_FAILURE_RET(rc);
    return rc;
}

/* static */ SysStatus
FileSystemDev::addFIDtoFS(FileSystemDev *devFS, FileInfoDev *fid)
{
    SysStatus rc;
    rc = devFS->add(fid->name, strlen(fid->name), fid);

    if (_FAILURE(rc)) {
	if (_SGENCD(rc) == EEXIST) {
	    err_printf("devfs: File %s already exists\n", fid->name);
	} else if (_SGENCD(rc) == ENOTDIR) {
	    err_printf("devfs: a component used as a directory in "
		       "pathname %s is not, in fact, a directory\n",
		       fid->name);
	} else if (_SGENCD(rc) == ENOENT) {
	    err_printf("devfs: a directory component in %s does "
		       " not exist\n", fid->name);
	} else {
	    err_printf("devfs: FileLinuxVirtFile::CreateFile for %s "
		       "failed: (%ld, %ld, %ld)\n", fid->name, _SERRCD(rc),
		       _SCLSCD(rc), _SGENCD(rc));
	}
    }
    return rc;
}
