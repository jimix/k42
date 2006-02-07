/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemKFSK42.C,v 1.38 2005/07/10 16:06:49 dilma Exp $
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

#include <fslib/PagingTransportPA.H>
#include <stub/StubKernelPagingTransportPA.H>

static ThreadID BlockedThread = Scheduler::NullThreadID;

/* static */ SysStatus
FileSystemKFS::ClassInit()
{
    DiskClient::ClassInit(0);
    MetaFileSystemKFS::init();
    PagingTransportPA::ClassInit(0);

    return 0;
}

/*
 * Create()
 *
 */
SysStatus
FileSystemKFS::Create(char *diskPath, char *mpath, uval flags,
		      uval isCoverable /* = 1*/)
{
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
    }

    return CreateCommon(diskPath, mpath, flags, 0/*format*/, isCoverable);
}



/*
 * CreateCommon()
 *
 */
SysStatus
FileSystemKFS::CreateCommon(char *diskPath, char *mpath,
			    uval flags, uval format, uval isCoverable)
{
    SysStatus rc;
    FileSystemKFSRef fsRef;
    DirLinuxFSRef dir;

    FileSystemKFS *obj = new FileSystemKFS();
    tassertMsg(obj != NULL, "ops");
    rc = obj->init(diskPath, flags, format);
    tassertWrn(_SUCCESS(rc), "FileSystemKFS::init() failed w/ rc 0x%lx\n", rc);
    _IF_FAILURE_RET(rc);

    // create the clustered object for the file system
    fsRef = (FileSystemKFSRef)CObjRootSingleRep::Create(obj);

    // Get the root directory.
    ObjToken *otok = new ObjToken(obj->globals->super->getRootLSO(),
				  obj->globals);
    FSFile *fi = new FSFileKFS(obj->globals, otok);
    DirLinuxFS::CreateTopDir(dir, ".", fi);

    // description of mountpoint
    char tbuf[256];
    const char *rw = (flags & MS_RDONLY ? "r" : "rw");
    sprintf(tbuf, "%s kfs %s pid 0x%lx", diskPath, rw,
	    _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));

    NameTreeLinuxFS::Create(mpath, dir, tbuf, strlen(tbuf), isCoverable);

    instances.add((ObjRef)fsRef, mpath);

    // create PagingTransport Object
    ObjectHandle fsptoh;
    rc = PagingTransportPA::Create(obj->globals->tref, fsptoh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    // asks the kernel to create a KernelPagingTransport

    ObjectHandle kptoh, sfroh;
    rc = StubKernelPagingTransportPA::_Create(fsptoh, kptoh, sfroh);
    tassertMsg(_SUCCESS(rc), "rc 0x%lx\n", rc);
    tassertMsg(kptoh.valid(), "ops");
    DREF(obj->globals->tref)->setKernelPagingOH(kptoh, sfroh);

    KFS_DPRINTF(DebugMask::FILE_SYSTEM_KFS, "FileSystemKFS::Create() OUT\n");

    uval db, dbs;
    rc = obj->globals->disk_ar->readCapacity(db, dbs);
    tassertMsg(_SUCCESS(rc), "readCapacity failed with rc 0x%lx", rc);

    err_printf("KFS mountpoint %s:\n"
	       "\tpid:   0x%lx\n"
	       "\tdisk:  %s (capacity %ld blocks,  %ld bytes)\n",mpath,
	       _SGETPID(DREFGOBJ(TheProcessRef)->getPID()),
	       diskPath, db, db*dbs);
    if (format) {
	err_printf("\t       Being formatted on creation.\n");
    }

    return 0;	// success
}


/*
 * CreateAndFormat()
 *
 */
SysStatus
FileSystemKFS::CreateAndFormat(char *diskPath, char *mpath, uval flags,
			       uval isCoverable/* = 1 */)
{
    KFS_DPRINTF(DebugMask::FILE_SYSTEM_KFS,
		"FileSystemKFS::CreateAndFormat() IN\n");

    if (KernelInfo::OnSim() == 2 /* SIM_MAMBO*/ ) {
	if (strncmp(diskPath, "/dev/mambobd/0", strlen("/dev/mambobd/0")) == 0) {
	    tassertWrn(0, "file disk image on /dev/mambobd/0 will be formatted.\n"
		       "Be aware that this is the same file created on build"
		       " with kitchroot image, so you better know what you're"
		       " doing (i.e., Make.paths should specify the build not "
		       " to generate a disk image\n");
	} else if (strncmp(diskPath, "/dev/mambobd/1",
			   strlen("/dev/mambobd/1")) == 0) {
	    passertMsg(0, "It's very unlikely you want to do this (/dev/mambobd/1 "
		       "is DISK0.0.1, which is either a link to a file you "
		       "don't want to change, or a dangling symlink). If you "
		       "know what you're doing, DISK0.0.1 is not a symlink "
		       "and it's ok to continue.\n");
	}
    }

    return CreateCommon(diskPath, mpath, flags, 1/*format*/, isCoverable);
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
    ObjRef oref = NULL;
    while ((curr = instances.next(curr, oref))) {
	FileSystemKFSRef fsref = (FileSystemKFSRef)oref;
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

/* static */ SysStatus
FileSystemKFS::_SetSyncMetaData(char *mpath, uval len, uval value)
{
    FileSystemKFSRef fsref = (FileSystemKFSRef) instances.find(mpath, len);
    if (fsref) {
	SysStatus rc;
	rc = DREF(fsref)->setSyncMetaData(value);
	return rc;
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
    ClassInit();
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
    ObjRef oref = NULL;
    while ((curr = instances.next(curr, oref))) {
	FileSystemKFSRef fsref = (FileSystemKFSRef)oref;
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

// value == 0 for turning syncMetaData off, 1 for turning it on
/* virtual */ SysStatus
FileSystemKFS:: setSyncMetaData(uval value)
{
    if (value == 1 && !globals->isSyncMetaDataOn()) {
	passertMsg(0, "Turning meta-data sync back NIY\n");
	return 0;
    }
    globals->setSyncMetaData(value);
    return 0;
}
