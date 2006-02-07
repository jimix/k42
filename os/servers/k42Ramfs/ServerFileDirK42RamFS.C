/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileDirK42RamFS.C,v 1.4 2003/01/16 06:09:46 okrieg Exp $
 *****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <sys/sysIncs.H>
#include "ServerFileDirK42RamFS.H"
#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>

/* static */ SysStatus
ServerFileDirK42RamFS::Create(DirLinuxFSRef &rf, PathName *pathName, 
			      uval pathLen, FSFile *fsFile, DirLinuxFSRef par)
{
    ServerFileDirK42RamFS *file = new ServerFileDirK42RamFS;
    passertMsg(file != NULL, "failed allocation of ServerFileDirK42RamFS\n");

    SysStatus rc = file->init(pathName, pathLen, fsFile, par);
    if(_FAILURE(rc)) {
	delete file;
	rf = NULL;
    } else {
	rf = (DirLinuxFSRef) file->getRef();
    }
    return rc;
}

