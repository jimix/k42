#ifndef __SERVER_FILE_DIR_EXT2_H_
#define __SERVER_FILE_DIR_EXT2_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileDirExt2.H,v 1.3 2004/09/30 03:11:56 apw Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Basic common implementation of a file in server,
 * specific for directories.
 * **************************************************************************/

#include <fslib/DirLinuxFS.H>

class ServerFileDirExt2;
typedef ServerFileDirExt2 **ServerFileDirExt2Ref;

class ServerFileDirExt2 : public DirLinuxFS {
    virtual SysStatus destroy() { 
	// FIXME: we need to clean up Ext2 structures!!!!
	return DirLinuxFS::destroy();
    }

private:
    DEFINE_REFS(ServerFileDirExt2);

public:
    static SysStatus Create(DirLinuxFSRef &rf,
			    PathName *pathName, uval pathLen,
			    FSFile *dinfo, DirLinuxFSRef par) {
	ServerFileDirExt2 *file = new ServerFileDirExt2;
	tassertMsg(file != NULL, "failed allocate of ServerFileDirExt2\n");

	SysStatus rc = file->init(pathName, pathLen, dinfo, par);
	if(_FAILURE(rc)) {
	    delete file;
	    rf = NULL;
	} else {
	    rf = (DirLinuxFSRef) file->getRef();
	}

	return rc;
    }
};
#endif /* #ifndef __SERVER_FILE_DIR_EXT2_H_ */
