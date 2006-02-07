#ifndef __FILE_SYSTEM_KFS_H_
#define __FILE_SYSTEM_KFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemKFS.H,v 1.11 2004/02/09 21:31:56 lbsoares Exp $
 *****************************************************************************/

#include <cobj/Obj.H>
#include <io/FileLinux.H>
#include <fslib/FSFile.H>
#include <fslib/fs_defines.H>
#include "defines.H"
#include <fslib/FreeList.H>

#include "ObjToken.H"
#include "SuperBlock.H"

class KFSGlobals;
class FileSystemKFS;
typedef FileSystemKFS		**FileSystemKFSRef;

class FileSystemKFS :  public Obj
{
 private:
    // padded because only one, so might as well
    DEFINE_GLOBALPADDED_NEW(FileSystemKFS);

    SysStatus init(char *diskPath, uval flags, uval format = 0);

 public:
    // all per-file system global state
    KFSGlobals *globals;

    virtual ~FileSystemKFS();

    static SysStatus Create(char *diskPath, uval flags,
			    FileSystemKFSRef fs, FSFile** root);

    void syncSuperBlock();

    uval releasePage(uval32 blkno);

    SysStatus destroy() {
	delete this;
	return 0;
    }
};

#endif /* #ifndef __FILE_SYSTEM_KFS_H_ */
