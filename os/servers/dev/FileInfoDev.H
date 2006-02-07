#ifndef __FILEINFODEV_H_
#define __FILEINFODEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileInfoDev.H,v 1.2 2004/02/27 17:14:20 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: File system specific interface to /dev
 * **************************************************************************/
#include <fslib/NameTreeLinuxFS.H>
#include "FileSystemDev.H"
#include "ServerFileCharDev.H"
#include <time.h>
#include <fslib/virtfs/FileInfoVirtFS.H>
#include <fslib/virtfs/FIVFAccessOH.H>

// For now /dev/null needs to use this because it uses a ServerFile object
class FileInfoDev: public FileInfoVirtFS{
public:
    char* name;
    DEFINE_GLOBAL_NEW(FileInfoDev);

    virtual void init(const char* n,
	      uval fsid, uval mode, uval rdev,
	      uval uid, uval gid) {
	lock.init();
	uval namelen = strlen(n)+1;
	name = (char*)allocGlobal(namelen);
	memcpy(name, n, namelen);
	name[namelen] = 0;
	status.st_mode = mode;
	fileNumber = GetFileNumber();
	status.st_ino = fileNumber;
	status.st_dev	= fsid;
	status.st_nlink	= 1;
	status.st_uid	= uid;
	status.st_gid	= gid;
	status.st_rdev	= rdev;
	status.st_size	= 0;
	status.st_blksize	= 4096;
	status.st_blocks	= 0;
	status.st_atime	= time(0);
	status.st_mtime	= time(0);
	status.st_ctime	= time(0);
    }
    FileInfoDev() {};
    FileInfoDev(const char* n,
		uval fsid, uval mode, uval rdev,
		uval uid, uval gid) {
	init(n, fsid, mode, rdev, uid, gid);
    }
    virtual ~FileInfoDev() {
        if (name) {
	    freeGlobal(name,strlen(name)+1);
	}
    };
    virtual SysStatus createServerFileChar(ServerFileRef &fref) {
	return ServerFileCharDev::Create(fref, this);
    }
    virtual SysStatus createServerFileBlock(ServerFileRef &fref) {
	return _SERROR(2581, 0, EOPNOTSUPP);
    }
    virtual SysStatusUval getServerFileType() {
	return VirtLocal;
    };
    virtual SysStatus deleteFile() {
	return _SERROR(2248, 0, EPERM);
    }

};

#endif // __FILEINFODEV_H_
