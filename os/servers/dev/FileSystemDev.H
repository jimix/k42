#ifndef __FILE_SYSTEM_DEV_H_
#define __FILE_SYSTEM_DEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemDev.H,v 1.32 2005/03/04 00:43:58 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: File system specific interface to /dev
 * **************************************************************************/
#include <io/DirBuf.H>
#include <io/FileLinux.H>

#include "DevFSDir.H"
#include "FileInfoDev.H"
#include <misc/ListSimpleLocked.H>
#ifndef EXPORT_FILESYSTEMDEV
#define EXPORT_FILESYSTEMDEV
#include <sys/types.h>
#endif //EXPORT_FILESYSTEMDEV

class FileSystemDev;

class FileSystemDev : public DevFSDir {
    // padded because only one, so might as well
    DEFINE_GLOBALPADDED_NEW(FileSystemDev);
#ifndef _IN_STUBGEN
    struct RootEntry{
	char name[64];
	DevFSDirRef ref;
    };
    RootEntry rootEntry;
    typedef ListSimpleLocked<RootEntry*, AllocGlobal> RootList;
    static RootList* rootList;
#endif // #ifndef _IN_STUBGEN
    virtual SysStatus init(const char *mpath, mode_t mode);
    static SysStatus addFIDtoFS(FileSystemDev *devFS, FileInfoDev *fid);
public:
    static const char* Root;
    static SysStatus ClassInit(uval isCoverable = 0);
    static SysStatus Create(FileSystemDev* &fs, const char *mpath,
			    mode_t mode);

    static SysStatus RegisterNode(FileInfoVirtFS* fivf,
				  const char* name, mode_t mode,
				  ObjectHandle parent, ProcessID pid);
    static SysStatus GetDir(ObjectHandle dirOH, ProcessID pid,
			    FileInfoVirtFSDirBase* &dir);
__xpublic:
    static SysStatus _Create(__inbuf(*) char* name,
			     __in mode_t mode,
			     __in ObjectHandle parent,
			     __out ObjectHandle &oh, //StubDevFSDir
			     __CALLER_PID pid);
    static SysStatus _GetRoot(__inbuf(*) char* name,
			      __out ObjectHandle &oh, __CALLER_PID pid);


};
#endif /* #ifndef __FILE_SYSTEM_DEV_H_ */
