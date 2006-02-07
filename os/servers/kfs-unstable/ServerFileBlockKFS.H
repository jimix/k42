#ifndef __SERVER_FILE_BLOCK_KFS_H_
#define __SERVER_FILE_BLOCK_KFS_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockKFS.H,v 1.2 2004/10/02 19:57:59 dilma Exp $
 *****************************************************************************/
/****************************************************************************
 * Module Description: The object that represents a server file for KFS
 ****************************************************************************/

#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <fslib/ServerFileBlock.H>
#include <stub/StubFRPA.H>
#include "FSFileKFS.H"

#include "KFSDebug.H"

class ServerFileBlockKFS;
class FileSystemKFS;
typedef ServerFileBlockKFS **ServerFileBlockKFSRef;
typedef FileSystemKFS **FileSystemKFSRef;

class ServerFileBlockKFS : public ServerFileBlock<StubFRPA>
{
private:
    FSFileKFS *fsfKFS() { 
	return (FSFileKFS *)fileInfo; 
    }
    DEFINE_REFS(ServerFileBlockKFS);
    DEFINE_GLOBALPADDED_NEW(ServerFileBlockKFS);
    DEFINE_ACCESS_SERVER(FileLinuxServer, MetaObj::controlAccess,
                         MetaObj::none);

    // functions with specialized implementations

    // invoked with stubDetachLock locked
    virtual SysStatus locked_createFR();
public:

    virtual SysStatus startWrite(uval physAddr, uval objOffset, uval len,
				 XHandle xhandle);
    virtual SysStatus startFillPage(uval physAddr, uval objOffset,
				    XHandle xhandle);

    virtual SysStatus _write(__in uval virtAddr, __in uval objOffset,
			     __in uval len, __XHANDLE xhandle);
protected:
    SysStatus init(FSFile *finfo);
public:
    static SysStatus Create(ServerFileRef &fref, FSFile *fileInfo);
};

#endif /* #ifndef __SERVER_FILE_BLOCK_KFS_H_ */
