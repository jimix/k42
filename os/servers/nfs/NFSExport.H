#ifndef __NFSEXPORT_H_
#define __NFSEXPORT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: NFSExport.H,v 1.12 2005/04/11 21:04:33 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: NFS Canonical form of file system data types.
 *	Favors default GNU/Linux.
 ****************************************************************************/

#include <sys/stat.h>
#include <io/FileLinux.H>

#include "nfs.h"

class NFSStat : public fattr {
public:
    void init(fattr *attributes);
    void toLinuxStat(FileLinux::Stat *status);

    uval isDir() {return (type == NFDIR);}
    uval isFile() {return (type == NFREG);}
};

inline void
NFSStat::init(fattr *attributes)
{
    type           = attributes->type;
    mode           = attributes->mode;
    rdev           = attributes->rdev;
    if (type == NFCHR && rdev == u_int(NFS2_FIFO_DEV)) {
	type = NFFIFO;
	mode = (mode & ~S_IFMT) | S_IFIFO;
	rdev = 0;
    }
    nlink          = attributes->nlink;
    uid            = attributes->uid;
    gid            = attributes->gid;
    size           = attributes->size;
    blocksize      = attributes->blocksize;
    blocks         = attributes->blocks;
    fsid           = attributes->fsid;
    fileid         = attributes->fileid;
    atime.seconds  = attributes->atime.seconds;
    atime.useconds = attributes->atime.useconds;
    mtime.seconds  = attributes->mtime.seconds;
    mtime.useconds = attributes->mtime.useconds;
    ctime.seconds  = attributes->ctime.seconds;
    ctime.useconds = attributes->ctime.useconds;
}

inline void
NFSStat::toLinuxStat(FileLinux::Stat *status)
{
    //                  = type;
    status->st_dev	= fsid;  //FIXME: is this valid?
    status->st_ino	= fileid;
    status->st_mode	= mode;
    status->st_nlink	= nlink;
    status->st_uid	= uid;
    status->st_gid	= gid;
    status->st_rdev	= rdev;
    status->st_size	= size;
    status->st_blksize	= blocksize;
    status->st_blocks	= blocks;
    status->st_atime	= atime.seconds;
    status->st_mtime	= mtime.seconds;
    status->st_ctime	= ctime.seconds;
    // some sanity checks
    tassert((isDir()==status->isDir()), err_printf("bad translation\n"));
    tassert((isFile()==status->isFile()), err_printf("bad translation\n"));
}

class NFSFlag {
public:
    uval32 flag;
    NFSFlag(uval32 f = 0) { flag = f;}
};

class NFSType {
public:
    enum {
	_IFSOCK = 0140000, _IFLNK = 0120000, _IFREG = 0100000,
	_IFBLK  = 0060000, _IFDIR = 0040000, _IFCHR = 0020000
    };

    uval32 type;
    NFSType(uval32 t = 0) { type = t;}
};


class NFSProt {
public:
    enum {
    _ISUID = 0004000, _ISGID = 0002000, _ISVTX = 0001000
    };

    uval32 prot;
    NFSProt(uval32 p = 0) { prot = p;}
};


class NFSMode {
public:
    enum {
    _IRUSR = 0000400, _IWUSR = 0000200, _IXUSR = 0000100,
    _IRGRP = 0000040, _IWGRP = 0000020, _IXGRP = 0000010,
    _IROTH = 0000004, _IWOTH = 0000002, _IXOTH = 0000001
    };
    uval32 mode;
    NFSMode(uval32 m = 0) { mode = m;}
};

#endif /* #ifndef __NFSEXPORT_H_ */
