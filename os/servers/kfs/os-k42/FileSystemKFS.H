#ifndef __FILE_SYSTEM_KFS_H_
#define __FILE_SYSTEM_KFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2005.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemKFS.H,v 1.17 2005/07/10 16:08:45 dilma Exp $
 *****************************************************************************/

#include <io/FileLinux.H>
#include <fslib/FSFile.H>
#include <fslib/fs_defines.H>
#include <fslib/FileSystemList.H>

#include "defines.H"

#ifndef _IN_STUBGEN
#include <fslib/MultiLinkManager.H>
#endif
#include <fslib/FreeList.H>

#include "ObjToken.H"
#include "SuperBlock.H"

class KFSGlobals;
class FileSystemKFS;
typedef FileSystemKFS		**FileSystemKFSRef;

class FileSystemKFS :  public Obj __xbase(Obj) {
#ifndef _IN_STUBGEN
 private:
    static FileSystemList instances;
#endif // #ifndef _IN_STUBGEN    
 private:
    // all per-file system global state
    KFSGlobals *globals;

    DEFINE_GLOBALPADDED_NEW(FileSystemKFS);

    ~FileSystemKFS();

    SysStatus init(char *diskPath, uval flags, uval format = 0);

    static SysStatus CreateCommon(char *diskPath, char *mpath, uval flags,
				  uval format, uval isCoverable);

 protected:
    virtual SysStatus printStats();
    virtual SysStatus sync() {
	globals->super->sync();
	return 0;
    }

    virtual SysStatus setSyncMetaData(uval value);

 public:
    static SysStatus ClassInit();
    static SysStatus Create(char *diskPath, char *mpath, uval flags,
			    uval isCoverable = 1);
    static SysStatus CreateAndFormat(char *diskPath, char *mpath, uval flags,
				     uval isCoverable = 1);
    static void Block();

 private:
 __xprivate:
    static SysStatus _PrintStats() __xa(none);
    static SysStatus _PrintDebugClasses(__out uval &currentMask) __xa(none);
    static SysStatus _SetDebugMask(uval mask) __xa(none);

    static SysStatus _Mkfs(__inbuf(diskPathLen) char *diskPath,
			   __in uval diskPathLen,
			   __inbuf(mpathLen) char *mpath,
			   __in uval mpathLen,
			   __in uval format,
			   __in uval flags) __xa(none);

    static SysStatus _TestAlive(__inbuf(len) char *mpath,
				__in uval len);

    static SysStatus _SetSyncMetaData(__inbuf(len) char *path,
				      __in uval len,
				      __in uval value);

    // the following are not being used currently
    static SysStatus _SaveState();
};

#endif /* #ifndef __FILE_SYSTEM_KFS_H_ */

