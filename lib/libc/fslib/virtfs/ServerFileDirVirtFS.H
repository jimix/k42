#ifndef __SERVER_FILE_DIR_VIRT_FS_H_
#define __SERVER_FILE_DIR_VIRT_FS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <fslib/DirLinuxFSVolatile.H>
#include <fslib/virtfs/FileInfoVirtFS.H>
class ServerFileDirVirtFS;
typedef ServerFileDirVirtFS **ServerFileDirVirtFSRef;

class ServerFileDirVirtFS : public DirLinuxFSVolatile {
    DEFINE_REFS(ServerFileDirVirtFS);
    DEFINE_GLOBALPADDED_NEW(ServerFileDirVirtFS);
protected:
    virtual SysStatus tryToDestroy();
    uval invalidCache;
    FileInfoVirtFSDir* fivfDir;
public:
    virtual SysStatus locked_revalidate() {
	if (!invalidCache) return 0;
	return locked_purge();
    }
    virtual uval shouldRevalidate() {
	return invalidCache;
    }
    virtual SysStatus markCacheInvalid() { invalidCache =1; return 0; };

    virtual SysStatus init(PathName *pathName, uval pathLen,
			   FileInfoVirtFSDir* theInfo,
			   DirLinuxFSRef par);

    static SysStatus Create(DirLinuxFSRef &rf,
			    PathName *pathName, uval pathLen,
			    FileInfoVirtFSDir* dinfo,
			    DirLinuxFSRef par) {
	ServerFileDirVirtFS *file = new ServerFileDirVirtFS;
	tassert((file != NULL),
		err_printf("failed allocate of ServerFileDirVirtFS\n"));

	SysStatus rc = file->init(pathName, pathLen, dinfo, par);
	if (_FAILURE(rc)) {
	    delete file;
	    rf = NULL;
	} else {
	    rf = (DirLinuxFSRef) file->getRef();
	}
	return rc;
    }
    // call from kernel to re-open this file, eventually change to giveAccess
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) {
	return Obj::_giveAccess(oh, toProcID, xhandle);
    }
};
#endif /* #ifndef __SERVER_FILE_DIR_VIRT_FS_H_ */
