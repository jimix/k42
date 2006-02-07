/*
 * K42 File System
 *
 * This is a wrapper around K42's FileSystem Server's Objects,
 * so that they connect to Linux's VFS layer.
 *
 * Copyright (C) 2003 Livio B. Soares (livio@ime.usp.br)
 * Licensed under the LGPL
 *
 * $Id: LinuxBufferKFS.C,v 1.1 2003/09/23 15:22:55 dilma Exp $
 */

#include "defines.H"
#include "BlockCacheLinux.H"

extern "C" {
uval
Kfs_sizeofbce() {
	return sizeof(BlockCacheEntryLinux);
}
}
