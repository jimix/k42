/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemKFS.C,v 1.4 2004/03/07 00:47:23 lbsoares Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <FileSystemKFS.H>
#include "KFSGlobals.H"
#include "SuperBlock.H"
#include "KFSDisk.H"
#include "ObjToken.H"
#include "PSOTypes.H"
#include "KFSDebug.H"
#include "PSODiskBlock.H"
#include "BlockCache.H"
#include <KFSFactory.H>

/*
 * init()
 *
 *   Mounts the file system and creates a clustered object for access
 *   to the filesystem.
 */
SysStatus
FileSystemKFS::init(char* diskPath, uval flags, uval format /* = 0 */)
{
    Disk *disk;
    SuperBlock *sb;
    uval32 rmapBlkno;

    SysStatus rc;

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FileSystemKFS::init() IN\n");
    KFS_DPRINTF(DebugMask::FS_FILE_KFS,
		"Mounting disk element onto blockserver.\n");

    // Will create a server thread for this disk
    disk = new KFSDisk;
    rc = ((KFSDisk *)disk)->init(diskPath, flags & MS_RDONLY);

    if (_FAILURE(rc)) {
	err_printf("Disk initialization failure: %s %016lx\n",
		   diskPath,rc);
	return rc;
    }
    //err_printf("For diskPath KFSDisk created is %p\n", disk);

    // Will read superblock info & block bit maps from disk
    globals = new KFSGlobals();

    // create a new BlockCache for KFS
    globals->blkCache = BlockCache::CreateBlockCache(disk, globals);
    passertMsg(globals->blkCache, "FileSystemKFS::init() something wrong "
	       "CreateBlockCache()");

    // Create a "low-level" PSO for the Superblock to access the disk
    // TODO: Make it possible, depending on the flag, to use other
    //       "low-level" PSOs, like, for example, a PSORamDisk.
    globals->llpso = new PSODiskBlock(globals, disk);

    // Set array element to point to this disk
    globals->disk_ar = disk;

    KFSFactory factory;
    rc = factory.allocSuperBlock(sb, globals, flags);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    sb->init(format);
    // set the superblock
    globals->super = sb;

    rc = factory.registerServerObjectTypes(globals);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    if (format == 1) {
	rc = sb->format("disk");
	_IF_FAILURE_RET(rc);
    }
    rc = sb->checkVersion();
    // FIXME: when things get more stable, clean up and return error instead of
    // panicing
    passertMsg(_SUCCESS(rc), "checkVersion() failed\n");

    // Get record oriented PSO for storing obj data
    rmapBlkno = sb->getRecMap();
    passertMsg(rmapBlkno != 0, "bad record PSO in superblock\n");

    rc = factory.allocRecordMap(globals->recordMap, rmapBlkno, globals);
    tassertMsg(_SUCCESS(rc), "? rc 0x%lx\n", rc);

    if (format == 1) {
	sb->createRootDirectory();
    }

    KFS_DPRINTF(DebugMask::FS_FILE_KFS, "FileSystemKFS::init() OUT\n");
    return 0;
}
