#ifndef __SERVER_FILE_CHAR_DEV_H_
#define __SERVER_FILE_CHAR_DEV_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileCharDev.H,v 1.14 2005/04/15 17:39:37 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: The object that represents a server file for dev FS
 ****************************************************************************/

#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <fslib/virtfs/ServerFileVirtFS.H>

class ServerFileCharDev;
typedef ServerFileCharDev          **ServerFileCharDevRef;

class ServerFileCharDev : public ServerFileVirtFSLocal {
    DEFINE_REFS(ServerFileCharDev);
    DEFINE_GLOBALPADDED_NEW(ServerFileCharDev);

    // These are meningless to directories
    // functions specific to file system
    virtual SysStatus ftruncate(off_t length) {
	return _SERROR(2027, 0, EISDIR);
    }
    virtual SysStatus getFROH(ObjectHandle &oh, ProcessID pid) {
	return _SERROR(2028, 0, EISDIR);
    }

    SysStatus init(FSFile *dinfo) {
	SysStatus rc = ServerFile::init();
	_IF_FAILURE_RET(rc);

	useType = FileLinux::FIXED_SHARED;

        fileInfo = dinfo;

	CObjRootSingleRep::Create(this);
	return 0;
    }

protected:
    virtual SysStatus destroy() {
	tassert(0, err_printf("NIY\n"));
	return 0;
    }

public:
    static SysStatus Create(ServerFileRef &cref, FSFile *cinfo) {
	ServerFileCharDev *file = new ServerFileCharDev;
	tassert((file != NULL),
		err_printf("failed allocate of ServerFileCharDev\n"));

	SysStatus rc = file->init(cinfo);
	if (_FAILURE(rc)) {
	    delete file;
	    cref = NULL;
	} else {
	    cref = (ServerFileRef) file->getRef();
	}
	return rc;
    }

    virtual SysStatus getStatus (FileLinux::Stat *status) {
	fileInfo->getStatus(status);
	return 0;
    }
    virtual SysStatus open(uval oflag, ProcessID pid, ObjectHandle &oh,
			   uval &uType, TypeID &type) {
	SysStatus rc = ServerFile::open(oflag, pid, oh, uType, type);
	FileLinux::Stat stat;
	getStatus(&stat);
	// seems a bit bogus, hiddent way to get type of this file
	type = stat.st_rdev;
	return rc;
    }

    virtual SysStatus handleXObjFree(XHandle xhandle) { return 0; };
};

#endif /* #ifndef __SERVER_FILE_CHAR_DEV_H_ */

