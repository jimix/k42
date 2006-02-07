#ifndef __SERVER_FILE_DIR_NFS_H_
#define __SERVER_FILE_DIR_NFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileDirNFS.H,v 1.52 2005/07/14 21:40:19 dilma Exp $
 ****************************************************************************/
/****************************************************************************
 * Module Description: The root object for a NFS file system dir
 ****************************************************************************/

#include "NFSHandle.H"
#include <meta/MetaFileLinuxServer.H>
#include <cobj/XHandleTrans.H>
#include <fslib/DirLinuxFSVolatile.H>
#include "FileSystemNFS.H"

class PathName;
class DirLinuxFS;
class ServerFileDirNFS;
typedef ServerFileDirNFS  **ServerFileDirNFSRef;
typedef DirLinuxFS        **DirLinuxFSRef;

class ServerFileDirNFS : public DirLinuxFSVolatile {
    DEFINE_GLOBALPADDED_NEW(ServerFileDirNFS);
    FileSystemNFS::FSFileNFS *fsfNFS() { 
	return (FileSystemNFS::FSFileNFS *)fileInfo; 
    }
protected:
    virtual SysStatus init(PathName *pathName, uval pathLen, FSFile *theInfo, 
			   DirLinuxFSRef par);

    // defines mininum and maximum values used for timeout
    static SysTime TIMEOUT_MIN, TIMEOUT_MAX;
    // defines neurotic threashold for directories (if the directory has
    // changed recently (last hour, for example), we should be very strict
    static SysTime STRICT_THRESHOLD;

    /* defines timeout for cached information. The policy is that it
     * starts from TIMEOUT_MIN and doubles each time we revalidate the
     * cached information successfully, up to TIMEOUT_MAX. */
    SysTime timeout;
    // timestamp for last time cached information has been retrieved
    SysTime timestamp;
    // timestamp for last time we updated timeout in terms of doubling it
    SysTime timeoutUpdateStamp;
    // timestamp for last discrepancy from mtime in remote server
    SysTime mtimeChangeStamp;

    virtual uval shouldRevalidate();
    virtual SysStatus locked_revalidate();

public:
    static SysStatus Create(DirLinuxFSRef &rf,
			    PathName *pathName, uval pathLen,
			    FSFile *dinfo, DirLinuxFSRef par) {
	ServerFileDirNFS *file = new ServerFileDirNFS;
	tassert((file != NULL),
		err_printf("failed allocate of ServerFileDirNFS\n"));

	SysStatus rc = file->init(pathName, pathLen, dinfo, par);
	if(_FAILURE(rc)) {
	    delete file;
	    rf = NULL;
	} else {
	    rf = (DirLinuxFSRef) file->getRef();
	}

	return rc;
    }
    // call from kernel to re-open this file
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh, 
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) {
	passertMsg(0, "NYI\n");
	return 0;
    }

    virtual SysStatusUval _getDents(char *buf, uval len, __XHANDLE xhandle,
				    __CALLER_PID pid) {
	// init thread specific state to caller creds
	SysStatus rc = FSCreds::Acquire(pid); 
	_IF_FAILURE_RET(rc);
	return DirLinuxFS::_getDents(buf, len, xhandle, pid);
    }
    virtual SysStatus sync();
};

#endif /* #ifndef __SERVER_FILE_DIR_NFS_H_ */
