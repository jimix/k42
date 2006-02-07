#ifndef __FILE_SYSTEM_PROC_H_
#define __FILE_SYSTEM_PROC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FileSystemProc.H,v 1.3 2005/07/13 17:27:44 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: File system specific interface to the /proc file system
 * **************************************************************************/
#include <io/DirBuf.H>
#include <io/FileLinux.H>
#include <fslib/virtfs/FileInfoVirtFS.H>

class FileInfoVirtFSProcDir : public FileInfoVirtFSDirStatic {
public:
    virtual SysStatus locked_lookup(const char *name, uval namelen, 
				    DirEntry &entry);
};

class FileSystemProc : public FileInfoVirtFSProcDir {
protected:
    static FileSystemProc *theFileSystemProc;
    static const char *Root;

    static SysStatus Create(FileSystemProc* &fs, const char *mpath,
			    mode_t mode);
    static SysStatus init_linux_uts_version();
    static SysStatus init_linux_banner();
    static SysStatus init_linux_ngroups_max();
    static SysStatus mkStaticTextRO(FSFile *dir, char *fn, char *val);

public:
    DEFINE_GLOBALPADDED_NEW(FileSystemProc);
    static SysStatus ClassInit(uval isCoverable = 0);
    virtual SysStatus mountToNameSpace(const char *mpath);
};
#endif /* #ifndef __FILE_SYSTEM_PROC_H_ */
