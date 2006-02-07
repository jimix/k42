#ifndef __FILE_SYSTEM_EXT2_H_
#define __FILE_SYSTEM_EXT2_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemExt2.H,v 1.3 2004/11/04 03:54:05 dilma Exp $
 *****************************************************************************/


#include <fslib/fs_defines.H>
#include <io/FileLinux.H>
#include <fslib/PagingTransport.H>

class FileSystemExt2;
typedef FileSystemExt2		**FileSystemExt2Ref;

class FileSystemExt2 :  public Obj __xbase(Obj) {
 private:
    SysStatus init(char *diskPath, uval flags, void **mnt, void **entry);

    // for communication with MM
    PagingTransportRef tref;

 public:
    DEFINE_GLOBAL_NEW(FileSystemExt2);
    FileSystemExt2() {
	err_printf("In constructor for FileSystemExt2()\n");
    }
    ~FileSystemExt2() {
	err_printf("In destructor for FileSystemExt2()\n");
    }

    static SysStatus ClassInit(VPNum vp);
    static SysStatus Create(char *diskPath, char *mpath, uval flags);
};

#endif // #ifndef __FILE_SYSTEM_EXT2_H_
