/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemKFSK42.C,v 1.8 2004/11/01 19:37:36 dilma Exp $
 *****************************************************************************/

#include <kfsIncs.H>

#include "FileSystemKFS.H"
#include "ServerFileBlockKFS.H"
#include "ServerFileDirKFS.H"
#include "KFSGlobals.H"

#include <fslib/NameTreeLinuxFS.H>

#include "SuperBlock.H"
#include "KFSDisk.H"
#include "ObjToken.H"

#include <sys/KernelInfo.H>
#include <io/FileLinux.H>
#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaFileSystemKFS.H>
#include <sys/time.h>

#include <stdio.h>
#include <trace/traceFS.h>

#ifdef GATHER_BC_STATS
#include "BlockCacheK42.H"
#endif // #ifdef GATHER_BC_STATS

#include <fslib/FileSystemList.H>
/* static */ FileSystemList FileSystemKFS::instances;

static ThreadID BlockedThread = Scheduler::NullThreadID;

/* static */ SysStatus
FileSystemKFS::ClassInit()
{
    DiskClient::ClassInit(0);
    MetaFileSystemKFS::init();
    return 0;
}

/*
 * Create()
 *
 */
SysStatus
FileSystemKFS::Create(char *diskPath, char *mpath, uval flags)
{
    SysStatus rc;
    FileSystemKFSRef fsRef;
    DirLinuxFSRef dir;

    KFS_DPRINTF(DebugMask::FILE_SYSTEM_KFS,
		"FileSystemKFS::Create() IN\n");

    if (KernelInfo::OnSim() == 1 /* SIM_SIMOSPPC*/ ) {
	// FIXME:use proper symbol
	const char *expectedName[2] = {"/dev/tda", "/dev/tdb"};
	tassertMsg(
	    (strncmp(diskPath, expectedName[0], strlen(expectedName[0])) == 0||
	     strncmp(diskPath, expectedName[1], strlen(expectedName[1])) == 0),
	    "diskPath is %s\n", diskPath);
    } else if (KernelInfo::OnSim() == 2 /* SIM_MAMBO*/ ) { // FIXME:use symbol
	const char *expectedName[3] = {"/dev/mambobd/0", "/dev/mambobd/1",
				       "/dev/mambobd/2"};
	tassertMsg(
	    (strncmp(diskPath, expectedName[0], strlen(expectedName[0])) == 0 ||
	     strncmp(diskPath, expectedName[1], strlen(expectedName[1])) == 0 ||
	     strncmp(diskPath, expectedName[2], strlen(expectedName[2])) == 0),
	    "diskPath is %s\n", diskPath);
    } else {
	err_printf("KFS being initialized with device %s\n", diskPath);
    }

    FileSystemKFS *obj = new FileSystemKFS();
    tassertMsg(obj != NULL, "ops");
    rc = obj->init(diskPath, flags);
    tassertWrn(_SUCCESS(rc), "FileSystemKFS::init failed w/ rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    // create the clustered object for the file system
    fsRef = (FileSystemKFSRef)CObjRootSingleRep::Create(obj);

    // Get the root directory.
    ObjTokenID otokID = obj->globals->super->getRootLSO();
    FSFile *fi = new FSFileKFS(obj->globals, &otokID, obj->globals->recordMap);
    DirLinuxFS::CreateTopDir(dir, ".", fi);

    // description of mountpoint
    char tbuf[256];
    const char *rw = (flags & MS_RDONLY ? "r" : "rw");
    sprintf(tbuf, "%s kfs %s pid 0x%lx", diskPath, rw,
	    _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

    NameTreeLinuxFS::Create(mpath, dir, tbuf, strlen(tbuf));

    instances.add((ObjRef)fsRef, mpath);

    KFS_DPRINTF(DebugMask::FILE_SYSTEM_KFS, "FileSystemKFS::Create() OUT\n");
    return 0;	// success
}

/*
 * CreateAndFormat()
 *
 */
SysStatus
FileSystemKFS::CreateAndFormat(char *diskPath, char *mpath, uval flags)
{
    SysStatus rc;
    FileSystemKFSRef fsRef;
    DirLinuxFSRef dir;

    KFS_DPRINTF(DebugMask::FILE_SYSTEM_KFS,
		"FileSystemKFS::CreateAndFormat() IN\n");

    if (KernelInfo::OnSim() == 1 /* SIM_SIMOSPPC*/ ) {
	if (strncmp(diskPath, "/dev/tda", strlen("/dev/tda")) == 0) {
	    tassertWrn(0, "file disk image on /dev/tda will be formatted.\n"
		       "Be aware that this is the same file created on build"
		       " with kitchroot image, so you better know what you're"
		       " doing (i.e., Make.paths should specify the build not "
		       " to generate a disk image\n");
	} else if (strncmp(diskPath, "/dev/tdb", strlen("/dev/tdb")) == 0) {
	    passertMsg(0, "It's very unlikely you want to do this (/dev/tdb "
		       "is DISK0.0.1, which is either a link to a file you "
		       "don't want to change, or a dangling symlink). If you "
		       "know what you're doing, DISK0.0.1 is not a symlink "
		       "and it's ok to continue.\n");
	}
    } else if (KernelInfo::OnSim() == 2 /* SIM_MAMBO*/ ) { // FIXME:use symbol
	passertMsg(0, "NIY\n"); /* need to check if diskPath is valid, trivial
				 * to do when someone needs it */
    } else {
	err_printf("KFS being initialized with device %s\n", diskPath);
    }

    // MetaFileSystemKFS::init();
    FileSystemKFS *obj = new FileSystemKFS();
    tassertMsg(obj != NULL, "ops");
    rc = obj->init(diskPath, flags, 1);
    tassertWrn(_SUCCESS(rc), "FileSystemKFS::init() failed w/ rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    MetaFileSystemKFS::init();
    // create the clustered object for the file system
    fsRef = (FileSystemKFSRef)CObjRootSingleRep::Create(obj);

    // Get the root directory.
    ObjTokenID otokID = obj->globals->super->getRootLSO();
    FSFile *fi = new FSFileKFS(obj->globals, &otokID, obj->globals->recordMap);
    DirLinuxFS::CreateTopDir(dir, ".", fi);

    // description of mountpoint
    char tbuf[256];
    const char *rw = (flags & MS_RDONLY ? "r" : "rw");
    sprintf(tbuf, "%s kfs %s pid 0x%lx", diskPath, rw,
	    _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

    NameTreeLinuxFS::Create(mpath, dir, tbuf, strlen(tbuf));

    instances.add((ObjRef)fsRef, mpath);

    KFS_DPRINTF(DebugMask::FILE_SYSTEM_KFS, "FileSystemKFS::Create() OUT\n");
    return 0;	// success
}

/*
 * _SaveState()
 *
 *   Performs a sync() of all dirty metadata to the disk, for
 *   every FileSystemKFS instance currenty available.
 */
SysStatus
FileSystemKFS::_SaveState()
{
    KFS_DPRINTF(DebugMask::FILE_SYSTEM_KFS,
		"FileSystemKFS::_SaveState() Saving State\n");

    SysStatus rc;
    void *curr = NULL;
    FileSystemKFSRef fsref = NULL;
    while ((curr = instances.next(curr, (ObjRef)fsref))) {
	rc = DREF(fsref)->sync();
	_IF_FAILURE_RET(rc);
    }

    return 0;
}

/* static */ SysStatus
FileSystemKFS::_TestAlive(char *mpath, uval len)
{
    FileSystemKFSRef fsref = (FileSystemKFSRef) instances.find(mpath, len);
    if (fsref) {
	return 0;
    } else {
	return _SERROR(2774, 0, ENOENT);
    }
}

/* static */ void
FileSystemKFS::Block()
{
    BlockedThread = Scheduler::GetCurThread();
    while (BlockedThread != Scheduler::NullThreadID) {
	// NOTE: this object better not go away while deactivated
	Scheduler::DeactivateSelf();
	Scheduler::Block();
	Scheduler::ActivateSelf();
    }
}

/* static */ SysStatus
FileSystemKFS::_Mkfs(char *diskPath, uval diskPathLen,
		     char *mpath, uval mpathLen, uval format, uval flags)
{
    diskPath[diskPathLen] = '\0';
    mpath[mpathLen] = '\0';
    if (format) {
	return CreateAndFormat(diskPath, mpath, flags);
    } else {
	return Create(diskPath, mpath, flags);
    }
}

/* static */ SysStatus
FileSystemKFS::_PrintStats()
{
    if (instances.isEmpty()) {
	err_printf("No kfs file system mounted\n");
	return 0;
    }

    SysStatus rc;
    void *curr = NULL;
    FileSystemKFSRef fsref = NULL;
    while ((curr = instances.next(curr, (ObjRef)fsref))) {
	rc = DREF(fsref)->printStats();
	_IF_FAILURE_RET(rc);
    }
    return 0;
}

/* static */ SysStatus
FileSystemKFS:: _PrintDebugClasses(uval &currentDebugMask)
{
#if defined(KFS_DEBUG) && !defined(NDEBUG)
    DebugMask::PrintDebugClasses(currentDebugMask);
#else
    err_printf("You need defined(KFS_DEBUG) && !defined(NDEBUG)\n");
#endif

    return 0;
}

/* static */ SysStatus
FileSystemKFS:: _SetDebugMask(uval mask)
{
#if defined(KFS_DEBUG) && !defined(NDEBUG)
    uval m = DebugMask::GetDebugMask();
    DebugMask::SetDebugMask(mask);
    err_printf("DebugMask was 0x%lx, now is 0x%lx", m, mask);
#else
    err_printf("You need defined(KFS_DEBUG) && !defined(NDEBUG)\n");
#endif
    return 0;
}

/* virtual */ SysStatus
FileSystemKFS::printStats()
{
#ifdef GATHERING_STATS
    globals->st.printStats();
#endif // #ifdef GATHERING_STATS

#ifdef GATHER_BC_STATS
    BlockCacheK42 *b = (BlockCacheK42*) globals->blkCache;
    b->printStats();
#endif //#ifdef GATHER_BC_STATS

#if !defined(GATHERING_STATS) && !defined(GATHER_BC_STATS)
    err_printf("No stats available\n");
#endif // #if !defined(GATHERING_STATS) && !defined(GATHER_BC_STATS)

    return 0;
}
