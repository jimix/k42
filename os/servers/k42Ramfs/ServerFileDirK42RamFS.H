#ifndef __SERVER_FILE_DIR_K42_RAM_FS_H_
#define	__SERVER_FILE_DIR_K42_RAM_FS_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileDirK42RamFS.H,v 1.4 2003/03/12 20:18:47 dilma Exp $
 *****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <fslib/DirLinuxFS.H>

class ServerFileDirK42RamFS;
typedef ServerFileDirK42RamFS **ServerFileDirK42RamFSRef;

class ServerFileDirK42RamFS : public DirLinuxFS {
    // For now just using inherited destroy
    //virtual SysStatus destroy();
    DEFINE_REFS(ServerFileDirK42RamFS);
    DEFINE_GLOBALPADDED_NEW(ServerFileDirK42RamFS);
    DEFINE_ACCESS_SERVER(FileLinuxServer, MetaObj::controlAccess,
                         MetaObj::none);
public:
    static SysStatus Create(DirLinuxFSRef &rf, PathName *pathName,
			    uval pathLen, FSFile *fsFile, 
			    DirLinuxFSRef par);
};
#endif /* #ifndef __SERVER_FILE_DIR_K42_RAM_FS_H_ */
