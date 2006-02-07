/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemKFSLinux.C,v 1.4 2004/05/06 19:52:50 lbsoares Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include "FileSystemKFS.H"

#include <io/FileLinux.H>

#include "KFSGlobals.H"
#include "SuperBlock.H"
#include "KFSDisk.H"
#include "ObjToken.H"
#include "PSOTypes.H"
#include "KFSDebug.H"
#include "PSOBasicRW.H"
#include "RecordMap.H"
#include "LSOBasic.H"
#include "FSFileKFS.H"
#include "BlockCacheLinux.H"

/*
 * Create()
 *
 */
SysStatus
FileSystemKFS::Create(char *diskPath, uval flags,
		      FileSystemKFSRef fs, FSFile **root)
{
    SysStatus rc;

    KFS_DPRINTF(DebugMask::LINUX, "FileSystemKFS::Create() IN\n");

    FileSystemKFS *obj = new FileSystemKFS();
    tassertMsg(obj != NULL, "ops");
    rc = obj->init(diskPath, flags);
    tassertMsg(_SUCCESS(rc), "ops rc 0x%lx\n", rc);
    if (rc < 0) {
	DREF(fs) = NULL;
	root = NULL;
	return -1;
    }

    // Get the root directory.
    ObjTokenID otokID = obj->globals->super->getRootLSO();
    FSFile *fi = new FSFileKFS(obj->globals, &otokID, obj->globals->recordMap);

    *root = fi;
    DREF(fs) = obj;
    KFS_DPRINTF(DebugMask::LINUX, "FileSystemKFS::Create() OUT\n");
    return 0;	// success
}

FileSystemKFS::~FileSystemKFS()
{
    delete globals->soAlloc;
    delete globals->llpso;
    delete globals->recordMap;
    globals->super->freeBlockMap();
    delete globals->super;
    delete globals->blkCache;
    delete globals->disk_ar;
    //    globals->soHash.removeAll();
    //    globals->soHash.destroy();
    delete globals;
}

void
FileSystemKFS::syncSuperBlock()
{
#ifndef KFS_SNAPSHOT
    globals->recordMap->flush();
    globals->super->sync();
#else //#ifndef KFS_SNAPSHOT
    globals->super->sync();
#endif //#ifndef KFS_SNAPSHOT
}

uval
FileSystemKFS::releasePage(uval32 blkno)
{
    return ((BlockCacheLinux *)globals->blkCache)->releasePage(blkno);
}
