#ifndef __SERVER_FILE_DIR_DEV_H_
#define __SERVER_FILE_DIR_DEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileDirDev.H,v 1.11 2003/01/16 06:09:45 okrieg Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: The object that represents a server file for dev FS
 ****************************************************************************/

#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <fslib/DirLinuxFSVolatile.H>

class ServerFileDirDev;
typedef ServerFileDirDev          **ServerFileDirDevRef;

class ServerFileDirDev : public DirLinuxFSVolatile {
    DEFINE_REFS(ServerFileDirDev);
    DEFINE_GLOBALPADDED_NEW(ServerFileDirDev);
protected:
    virtual SysStatus destroy() {
	tassert(0, err_printf("NIY\n"));
	return 0;
    }

public:
    static SysStatus Create(DirLinuxFSRef &rf,
			    PathName *pathName, uval pathLen,
			    FSFile *dinfo, DirLinuxFSRef par) {
	ServerFileDirDev *file = new ServerFileDirDev;
	tassert((file != NULL),
		err_printf("failed allocate of ServerFileDirDev\n"));

	SysStatus rc = file->init(pathName, pathLen, dinfo, par);
	if (_FAILURE(rc)) {
	    delete file;
	    rf = NULL;
	} else {
	    rf = (DirLinuxFSRef) file->getRef();
	}
	return rc;
    }

    virtual SysStatus getStatus(FileLinux::Stat *status) {
	fileInfo->getStatus(status);
	return 0;
    }
};

#endif /* #ifndef __SERVER_FILE_DIR_DEV_H_ */

