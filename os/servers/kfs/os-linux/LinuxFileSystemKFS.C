/*
 * K42 File System
 *
 * This is a wrapper around K42's FileSystem Server's Objects,
 * so that they connect to Linux's VFS layer.
 *
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: LinuxFileSystemKFS.C,v 1.26 2004/03/07 00:42:40 lbsoares Exp $
 */

#include "defines.H"
#include "FSFileKFS.H"
#include "FileSystemKFS.H"
#include "LSOBasicDir.H"
#include "BlockCacheLinux.H"

FileSystemKFS *kfs;

/* __cxa_pure_virtual is G++ 3.2 new mangled name for pure_virtual */
extern "C" void
__cxa_pure_virtual()
{
	printk("cxa pure virtual method called\n");
}

extern "C" void
__pure_virtual()
{
	printk("pure virtual method called\n");
}

void
__no_builtins_allowed(const char *classname, const char *op,
                      const char *f, int l)
{
	printk("%s:%d: C++ builtin: %s; called in class: %s\n",
               f, l, op, classname);
}

void* operator new(size_t size)
{
    printk("NEW! allocating %d\n", size);
    return allocGlobal(size);
}

void* operator new[](size_t size)
{
    printk("NEW[]! allocating %d\n", size);
    return allocGlobal(size);
}

void operator delete(void *ptr)
{
    printk("delete! deallocating\n");
    freeGlobal(ptr);
}

void operator delete[](void *ptr)
{
    printk("delete[]! deallocating\n");
    freeGlobal(ptr);
}

extern "C" {
void *
Kfs_init(char *diskPath, uval flags)
{
    sval rc;
    FSFile *fsFile;
    if (!(rc = FileSystemKFS::Create(diskPath, flags, &kfs, &fsFile)))
	return fsFile;
    else
	return NULL;
}

ino_t
Kfs_lookup(FSFileKFS *dirFile, const char *pathName, int length)
{
    return dirFile->lookup(pathName, length).id;
}

void *
Kfs_getToken(ino_t ino, void *inode)
{
    ObjTokenID otokID;
    ObjToken *otok;
    FSFileKFS *fsFile;

    otokID.id = ino;

    otok = new ObjToken(otokID, kfs->globals);
    fsFile = new FSFileKFS(kfs->globals, otok);

    return fsFile;
}

SysStatus 
Kfs_getStatfs(FSFileKFS *fsFile, struct statfs *buf)
{
    return fsFile->statfs(buf);
}

/*
 * buf is a ptr to a buffer that will hold dirents
 * len = size of the array in bytes
 */
SysStatusUval
Kfs_getDents(FSFileKFS *fsFile, uval *cookie, struct dirent64 *buf, uval len)
{
    return _SGETUVAL(fsFile->getDents(*cookie, (struct direntk42 *)buf, len));
}

uval8
Kfs_getType(FSFileKFS *fsFile)
{
  //    return fsFile->linuxFileType();
    return 0;
}

SysStatus 
Kfs_getStatus(FSFileKFS *fsFile, struct stat *stat)
{
    return fsFile->getStatus((FileLinux::Stat *)stat);
}


SysStatus 
Kfs_fchown(FSFileKFS *fsFile, uid_t uid, gid_t gid)
{
    return fsFile->fchown(uid, gid);
}

SysStatus 
Kfs_fchmod(FSFileKFS *fsFile, mode_t mode)
{
    return fsFile->fchmod(mode);
}

SysStatus 
Kfs_ftruncate(FSFileKFS *fsFile, off_t length)
{
    return -(_SGENCD(fsFile->ftruncate(length)));
}

SysStatus 
Kfs_link(FSFileKFS *oldFsFile, FSFileKFS *newDirInfo,
     char *newName,  uval newLen)
{
    ServerFile *fref = NULL;
    return -(_SGENCD(oldFsFile->link(newDirInfo, newName, newLen, &fref)));
}

SysStatus
Kfs_unlink(FSFileKFS *dirFile, char *pathName, uval pathLen, FSFileKFS *fsFile)
{
    return -(_SGENCD(dirFile->unlink(pathName, pathLen, fsFile)));
}

SysStatus
Kfs_deleteFile(FSFileKFS *fsFile)
{
    return -(_SGENCD(fsFile->deleteFile()));
}

SysStatus
Kfs_clearInode(FSFileKFS *fsFile)
{
    sval rc;

    rc = fsFile->destroy();
    return -(_SGENCD(rc));
}

SysStatus
Kfs_rename(FSFileKFS *oldDirFile, char *oldName, uval oldLen,
       FSFileKFS *newDirFile, char *newName, uval newLen,
       FSFileKFS *renamedFsFile)
{
    return -(_SGENCD(oldDirFile->rename(oldName, oldLen, newDirFile, newName, newLen, renamedFsFile)));
}

SysStatus
Kfs_mkdir(FSFileKFS *dirFile, char *compName, uval compLen,
      mode_t mode, FSFile **newDirFile)
{
    return -(_SGENCD(dirFile->mkdir(compName, compLen, mode, newDirFile)));
}

SysStatus
Kfs_rmdir(FSFileKFS *dirFile, char *name, uval namelen)
{
    return -(_SGENCD(dirFile->rmdir(name, namelen)));
}

SysStatus
Kfs_utime(FSFileKFS *fsFile, const struct utimbuf *utbuf)
{
    return fsFile->utime(utbuf);
}

SysStatus
Kfs_createFile(FSFileKFS *dirFile, char *name, uval namelen,
	   mode_t mode, FSFile **fsFile)
{
    return -(_SGENCD(dirFile->createFile(name, namelen, mode, fsFile)));
}

SysStatus
Kfs_createDir(FSFileKFS *dirFile, char *name, uval namelen,
	  mode_t mode, FSFile **fsFile)
{
    return -(_SGENCD(dirFile->mkdir(name, namelen, mode, fsFile)));
}

SysStatus
Kfs_readBlockPhys(FSFileKFS *fsFile, uval paddr, uval32 offset)
{
    return -(_SGENCD(fsFile->readBlockPhys(paddr, offset)));
}

SysStatus 
Kfs_writeBlockPhys(FSFileKFS *fsFile, uval paddr, uval32 length,
		   uval32 offset)
{
    return -(_SGENCD(fsFile->writeBlockPhys(paddr, length, offset)));
}

void
Kfs_umount()
{
    kfs->destroy();
}

void
Kfs_writeSuper()
{
    kfs->syncSuperBlock();
}

SysStatus
Kfs_fsync(FSFileKFS *fsFile)
{
    return -(_SGENCD(fsFile->fsync()));
}

uval
Kfs_sizeofbce() 
{
	return sizeof(BlockCacheEntryLinux);
}

uval
Kfs_releasePage(uval32 blkno)
{
    return kfs->releasePage(blkno);
}

uval
Kfs_symlink(FSFileKFS *dirFile, char *name, uval namelen,
	    char *value, FSFile **fsFile)
{
    return -(_SGENCD(dirFile->symlink(name, namelen, value, fsFile)));
}

uval
Kfs_readlink(FSFileKFS *fsFile, char *buffer, uval buflen)
{
    return fsFile->readlink(buffer, buflen);
}
}
