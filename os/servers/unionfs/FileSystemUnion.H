#ifndef __FILE_SYSTEM_UNION_H_
#define __FILE_SYSTEM_UNION_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemUnion.H,v 1.1 2003/09/17 21:24:28 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: unionfs is a file system that works as the union
 * of two existing file systems (primaryFS and secondaryFS) in the following
 * way:
 * - open of a file first goes to primaryFS, if not found there the operation
 * goes to the secondaryFS)
 * - creation of a file always goes to the primaryFS. If the pathname for the
 * file involves subdirectories, they will be created if necessary on primaryFS
 * if they already exist on secondaryFS)
 * - operations such as stat and readlink first go to the primaryFS; if it fails
 * there we try the secondaryFS
 * - operations that change a file (chown, utime, symlink, link, etc) always
 * go to the primaryFS.
 ****************************************************************************/

#include <io/FileLinux.H>

class FileSystemUnion : public Obj __xbase(Obj) {
private:
    // padded because only one, so might as well
    DEFINE_GLOBALPADDED_NEW(FileSystemUnion);

public:
    static void Block();


    static SysStatus ClassInit(VPNum vp, char *primPath, char *secPath,
			       char *mpath, uval isCoverable = 1);

__xprotected:
    static SysStatus _TestAlive(__inbuf(len) char *mpath, __in uval len)
	__xa(none);
    static SysStatus _Mkfs(char *primPath, char *secPath, char *mpath,
			   uval isCoverable) __xa(none);
};
#endif /* #ifndef __FILE_SYSTEM_UNION_H_ */

