#ifndef __LINUX_VFS_HOOKS_H_
#define __LINUX_VFS_HOOKS_H_
/*
 * K42 File System
 *
 * This is a wrapper around K42's FileSystem Server's Objects,
 * so that they connect to Linux's VFS layer.
 *
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 * 
 * $Id: LinuxVFShooks.h,v 1.1 2004/02/11 23:03:59 lbsoares Exp $
 */

void Kfs_write_inode_hook(FSFileKFS *fsFile) { Kfs_fsync(fsFile); }
void Kfs_writeSuper_hook(void) { Kfs_writeSuper(); }
void Kfs_put_super_hook(void) {}
void Kfs_read_super_hook(void) {}
void Kfs_LinuxCleanPage_hook(struct buffer_head *bh) {}
void Kfs_LinuxDirtyPage_hook(struct page *p, void *sb) {}

#endif // #ifndef __LINUX_VFS_HOOKS_H_
