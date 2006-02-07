#ifndef __FS_H_
#define __FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: fs.H,v 1.4 2003/12/22 20:08:05 dilma Exp $
 *****************************************************************************/

// this is a hack to remove the kernel allocation from the tools
// note: KFS_TOOLS set by the -D flag at compile time
#include "../kfsIncs.H"

// fs.H is included in FSDisk.H
class Disk;
class FSDisk;
class KFSGlobals;
class SuperBlock;

// formats kfs on the given disk
SysStatus formatKFS(Disk *);

SuperBlock *initFS(Disk *disk, KFSGlobals *g, uval format=0);

SysStatus createFileKFS(char *prog, Disk *d, int fd, char *newPath,
			uval mode, uval uid, uval gid);
SysStatus createDirKFS(char *prog, Disk *d, char *dir, uval uid, uval gid);
SysStatus createSymLinkKFS(char *prog, Disk *d, char *oldPath, char *newPath,
			   uval uid, uval gid);
sval linkFileKFS(char *prog, Disk *d, char *oldPath, char *newPath);
sval validateDiskKFS(Disk *);

#endif /* #ifndef __FS_H_ */
