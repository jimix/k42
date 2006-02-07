#ifndef __LINUX_VFS_H_
#define __LINUX_VFS_H_

/*
 * K42 File System
 *
 * This is a wrapper around K42's FileSystem Server's Objects,
 * so that they connect to Linux's VFS layer.
 *
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: LinuxVFS.h,v 1.18 2004/02/09 21:31:57 lbsoares Exp $
 */

/* crt stuff */
extern void __do_global_ctors_aux(void);
extern void __do_global_dtors_aux(void);

#include <linux/utime.h>
#include <linux/dirent.h>

#include "sys/types.H"
#include "sys/SysStatus.H"

// FIXME: we only want to do this while compiling the cd file; when this
// header is included in a C++ file, we want it to keep the type so we
// can have type checking.

//typedef void FSFileKFS
#define FSFileKFS void
#define SuperBlock void

/* This is the "ugly-way" to access LinuxFileSystemKFS's methods.
 * This is the way I've managed to interoperate between C and C++.
 * This sucks because we have to copy all the parameters.
 * See LinuxFileSystemKFS.[CH].
 */
FSFileKFS * Kfs_init(char *diskPath, uval flags);
void *Kfs_getTokenOLD(FSFileKFS * fileInfo, const char *pathName, int length);
void *Kfs_getToken(ino_t ino, void *inode);
extern inline ino_t Kfs_lookup(FSFileKFS *dirFile, const char *pathName, int length);
SysStatus Kfs_getStatfs(FSFileKFS * fileInfo, struct statfs *buf);

SysStatus Kfs_getStatus(FSFileKFS * tok, struct stat64 *status);
SysStatus Kfs_fchown(FSFileKFS * fileInfo, uid_t uid, gid_t gid);
SysStatus Kfs_fchmod(FSFileKFS * fileInfo, mode_t mode);
SysStatus Kfs_ftruncate(FSFileKFS * fileInfo, off_t length);
SysStatus Kfs_link(FSFileKFS * oldFileInfo, FSFileKFS * newDirInfo,
		   char *newName,  uval newLen);

extern inline SysStatus Kfs_unlink(FSFileKFS * dirInfo, char *pathName, uval pathLen, FSFileKFS *fsFile);

extern inline SysStatus Kfs_deleteFile(FSFileKFS * fileInfo);
extern inline SysStatus Kfs_clearInode(FSFileKFS * fsFile);
extern inline SysStatus Kfs_rename(FSFileKFS * oldDirInfo, char *oldName, uval oldLen,
				   FSFileKFS * newDirInfo, char *newName, uval newLen,
				   FSFileKFS * renamedFinfo);

SysStatus Kfs_mkdir(FSFileKFS * dirInfo, char *compName, uval compLen,
		       mode_t mode, FSFileKFS * *newDirInfo);

SysStatus Kfs_rmdir(FSFileKFS * dirInfo, char *name, uval namelen);

// If utbuf == NULL, set actime and modtime to current time.
SysStatus Kfs_utime(FSFileKFS * fileInfo, const struct utimbuf *utbuf);
extern inline SysStatus Kfs_createFile(FSFileKFS * dirInfo, char *name, uval namelen,
				       mode_t mode, FSFileKFS * *fileInfo);
SysStatus Kfs_createDir(FSFileKFS * dirInfo, char *name, uval namelen,
			mode_t mode, FSFileKFS * *fileInfo);

/*
 *buf is a ptr to a buffer that will hold dirents
 *len = size of the array in bytes
 */
SysStatusUval Kfs_getDents(FSFileKFS * fileInfo, uval * cookie,
			   struct dirent64 *buf, uval len);

uval8 Kfs_getType(FSFileKFS * fsFile);
SysStatus Kfs_readBlockPhys(FSFileKFS * token, uval paddr, uval32 offset);
SysStatus Kfs_writeBlockPhys(FSFileKFS * token, uval paddr, uval32 length,
			     uval32 offset);
void Kfs_umount(void);
void Kfs_writeSuper(void);
SysStatus Kfs_fsync(FSFileKFS *fsFile);

void Kfs_markBufferClean(SuperBlock *sb, uval32 blkno);

uval Kfs_sizeofbce(void);

uval Kfs_releasePage(uval32 blkno);

uval Kfs_symlink(FSFileKFS *dirFile, char *name, uval namelen, char *value, FSFileKFS **fsFile);
uval Kfs_readlink(FSFileKFS *fsFile, char *buffer, uval buflen);

#define ALIGN_UP(addr,align)   (((uval)(addr) + ((align)-1)) & ~((align)-1))

#include "LinuxVFShooks.h"

#endif /* #ifndef __FILE_VFS_H_ */
