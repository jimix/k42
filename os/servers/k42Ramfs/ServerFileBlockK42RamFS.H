#ifndef __SERVER_FILE_BLOCK_K42_RAM_FS_H_
#define __SERVER_FILE_BLOCK_K42_RAM_FS_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockK42RamFS.H,v 1.12 2004/10/14 15:12:42 dilma Exp $
 *****************************************************************************/
/****************************************************************************
 * Module Description:
 ****************************************************************************/

#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include <fslib/ServerFileBlock.H>
#include <stub/StubFR.H>
#include <stub/StubFRPA.H>
#include <mem/Access.H>

#include "FileSystemK42RamFS.H"
#include <io/FileLinux.H>

class ServerFileBlockK42RamFS;
class FileSystemK42RamFS;
typedef ServerFileBlockK42RamFS **ServerFileBlockK42RamFSRef;
typedef FileSystemK42RamFS **FileSystemK42RamFSRef;



class ServerFileBlockK42RamFS : public ServerFileBlock<StubFRPA>
{
private:
    DEFINE_REFS(ServerFileBlockK42RamFS);
    DEFINE_GLOBALPADDED_NEW(ServerFileBlockK42RamFS);
    DEFINE_ACCESS_SERVER(FileLinuxServer, MetaObj::controlAccess,
                         MetaObj::none);

    // invoked with stubDetachLock locked
    virtual SysStatus locked_createFR();

protected:
    enum {READ, WRITE, COPY_TO_FR};

#ifdef SMALL_FILE_ALLOC_ONE_BIG_CHUNK
    static const uval CHUNK_SIZE = FileLinux::MAX_SMALLFILE_SIZE;
    static const uval NUM_CHUNKS = 1;
#else
    static const uval CHUNK_SIZE = 0x800; // 2k
    static const uval NUM_CHUNKS =
                FileLinux::MAX_SMALLFILE_SIZE/CHUNK_SIZE + 1;
#endif
    char **bufChunks;

public:

    virtual SysStatus startWrite(uval physAddr, uval objOffset, uval len,
				 XHandle xhandle);
    virtual SysStatus startFillPage(uval physAddr, uval objOffset,
				    XHandle xhandle);

    virtual SysStatus exportedXObjectListEmpty(); // last client gone away
    virtual SysStatus handleXObjFree(XHandle xhandle);

protected:

    virtual SysStatus giveAccessSetClientData(
	ObjectHandle &oh, ProcessID toProcID, AccessRights match,
	AccessRights nomatch, TypeID type = 0);

    virtual SysStatus init(FSFile *fsFile, ObjectHandle toh);

    virtual SysStatus destroy();

    virtual SysStatus locked_setFileLength(uval len);

private:
    // A kludge to type cheat XBaseObj clientData to (protected struct)
    // ClientData
//    static ClientDataRamFS *Clnt(XHandle xhandle) {
//	return (ClientDataRamFS *) (XHandleTrans::GetClientData(xhandle));
//    }

    void locked_freeBufChunks(uval newlen=0);
    void locked_bufChunkOperation(uval op, char *buf,
				  uval len, uval offset);

public:
    static SysStatus Create(ServerFileRef & fref, FSFile *fsFile,
			    ObjectHandle toh);

    virtual SysStatus open(uval oflag, ProcessID pid, ObjectHandle &oh,
			   uval &useType, TypeID &type);

    virtual SysStatus getFROH(ObjectHandle &oh, ProcessID pid);

    virtual SysStatusUval _read(char *buf, uval len, uval offset, __XHANDLE xhandle);
    virtual SysStatusUval _write(const char *buf, uval len, uval offset,
				 __XHANDLE xhandle);
};

#endif /* #ifndef __SERVER_FILE_BLOCK_K42_RAM_FS_H_ */
