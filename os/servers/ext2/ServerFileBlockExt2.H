#ifndef __SERVER_FILE_BLOCK_EXT2_H_
#define __SERVER_FILE_BLOCK_EXT2_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockExt2.H,v 1.3 2004/11/04 03:54:05 dilma Exp $
 *****************************************************************************/
/****************************************************************************
 * Module Description: 
 ****************************************************************************/

#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <fslib/ServerFileBlock.H>
#include <stub/StubFRPA.H>
#include "FSFileExt2.H"

class ServerFileBlockExt2;
class FileSystemExt2;
typedef ServerFileBlockExt2 **ServerFileBlockExt2Ref;
typedef FileSystemExt2 **FileSystemExt2Ref;

class ServerFileBlockExt2 : public ServerFileBlock<StubFRPA>
{
private:
    DEFINE_REFS(ServerFileBlockExt2);
    DEFINE_GLOBALPADDED_NEW(ServerFileBlockExt2);
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
    SysStatus init(FSFile *finfo, ObjectHandle kptoh);
public:
    static SysStatus Create(ServerFileRef &fref, FSFile *fileInfo,
			    ObjectHandle kptoh);
};

#endif /* #ifndef __SERVER_FILE_BLOCK_KFS_H_ */
