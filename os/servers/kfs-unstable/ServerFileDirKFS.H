#ifndef __SERVER_FILE_DIR_KFS_H_
#define __SERVER_FILE_DIR_KFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileDirKFS.H,v 1.1 2004/02/11 23:03:56 lbsoares Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic common implementation of a file in server,
 * specific for directories.
 * **************************************************************************/

#include <fslib/DirLinuxFS.H>

#include "KFSDebug.H"

class ServerFileDirKFS;
typedef ServerFileDirKFS **ServerFileDirKFSRef;

class ServerFileDirKFS : public DirLinuxFS {
    virtual SysStatus destroy() { 
	// FIXME: we need to clean up KFS structures!!!!
	return DirLinuxFS::destroy();
    }

private:
    DEFINE_REFS(ServerFileDirKFS);

public:
    static SysStatus Create(DirLinuxFSRef &rf,
			    PathName *pathName, uval pathLen,
			    FSFile *dinfo, DirLinuxFSRef par) {
	KFS_DPRINTF(DebugMask::SERVER_FILE_DIR,
		    "ServerFileDirKFS::Create() IN\n");
	ServerFileDirKFS *file = new ServerFileDirKFS;
	tassertMsg(file != NULL, "failed allocate of ServerFileDirKFS\n");

	SysStatus rc = file->init(pathName, pathLen, dinfo, par);
	if(_FAILURE(rc)) {
	    delete file;
	    rf = NULL;
	} else {
	    rf = (DirLinuxFSRef) file->getRef();
	}

	KFS_DPRINTF(DebugMask::SERVER_FILE_DIR,
		    "ServerFileDirKFS::Create() OUT\n");
	return rc;
    }
};
#endif /* #ifndef __SERVER_FILE_DIR_KFS_H_ */
