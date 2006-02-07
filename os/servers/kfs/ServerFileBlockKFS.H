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
 * $Id: ServerFileBlockKFS.H,v 1.29 2004/11/02 19:41:26 dilma Exp $
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
#include "defines.H"

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

    virtual SysStatus completeIORequest(uval rc, uval addr, uval offset) {
	stubFR->stub._ioComplete(addr, offset, rc);
	return 0;
    }
    static void CompleteIORequest(void *obj, uval addr, uval offset,
				  uval rc) {
	SysStatus r = DREF((ServerFileBlockKFSRef)obj)
	    ->completeIORequest(rc, addr, offset);
	tassertMsg(_SUCCESS(r), "rc 0x%lx\n", rc);
    }
protected:
    SysStatus init(FSFile *finfo, ObjectHandle toh);
public:
    static SysStatus Create(ServerFileRef &fref, FSFile *fileInfo,
			    ObjectHandle toh);
};

#endif /* #ifndef __SERVER_FILE_BLOCK_KFS_H_ */
