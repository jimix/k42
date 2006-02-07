/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockK42RamFS.C,v 1.22 2004/10/05 21:28:21 dilma Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubRegionDefault.H>
#include <io/FileLinux.H>
#include <fslib/DirLinuxFS.H>
#include <stdlib.h>
#include <stdio.h>

#include <trace/traceFS.h>

#include "FileSystemK42RamFS.H"
#include "ServerFileBlockK42RamFS.H"

#define INSTNAME ServerFileBlockK42RamFS
#include <meta/TplMetaPAPageServer.H>
#include <xobj/TplXPAPageServer.H>
#include <tmpl/TplXPAPageServer.I>

#define BRYAN_IS_BAD

typedef TplXPAPageServer<ServerFileBlockK42RamFS> XPAPageServerSFK42RamFS;
typedef TplMetaPAPageServer<ServerFileBlockK42RamFS>
MetaPAPageServerSFK42RamFS;

SysStatus
ServerFileBlockK42RamFS::locked_createFR()
{
    ObjectHandle oh, myOH;
    SysStatus rc;

    _ASSERT_HELD(stubDetachLock);

    // create an object handle for upcall from kernel FR
    giveAccessByServer(myOH, _KERNEL_PID,
		       MetaPAPageServerSFK42RamFS::typeID());

    char *name;
    uval namelen;
#ifdef HACK_FOR_FR_FILENAMES
    name = nameAtCreation;
    namelen = strlen(nameAtCreation);
#else
    /* FIXME: could we pass NULL through the PPC? Not time to check it now,
     * so lets fake some space */
    char foo;
    name = &foo;
    namelen = 0;
#endif // #ifdef HACK_FOR_FR_FILENAMES

    rc = stubKPT._createFRPANonPageableRamOnly(oh, myOH, fileLength,
					       (uval) getRef(),
			     name, namelen);
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    stubFR = new StubFRHolder();
    stubFR->stub.setOH(oh);

    return 0;
}

/* static */ SysStatus
ServerFileBlockK42RamFS::Create(ServerFileRef & fref, FSFile *fsFile,
				ObjectHandle toh)
{
    SysStatus rc;
    ServerFileBlockK42RamFS *file = new ServerFileBlockK42RamFS;

    passertMsg(file != NULL, "failed allocate of ServerFileBlockK42RamFS\n");

    rc = file->init(fsFile, toh);
    if (_FAILURE(rc)) {
	delete file;
	fref = NULL;
	return rc;
    }

    fref = (ServerFileRef) file->getRef();

    MetaPAPageServerSFK42RamFS::init();

    return 0;
}

SysStatus
ServerFileBlockK42RamFS::init(FSFile *fsFile, ObjectHandle toh)
{
    SysStatus rc;
    // interface to kernel

    rc = ServerFileBlock<StubFRPA>::init(toh);
    _IF_FAILURE_RET(rc);

    fileInfo = fsFile;

    fileLength = FileSystemK42RamFS::FINF(fsFile->getToken())->status.st_size;

    CObjRootSingleRep::Create(this);

    bufChunks = NULL;
    tassertMsg(fileLength == 0, "Non-zero file\n");
    return 0;
}

/* virtual */ SysStatus
ServerFileBlockK42RamFS::startWrite(uval physAddr, uval objOffset, uval len,
				    XHandle xhandle)
{
    err_printf("startWrite with objOffset 0x%lx, len 0x%lx "
	       "(fileLength 0x%lx)\n", objOffset, len, fileLength);
    stubFR->stub._ioComplete(physAddr, objOffset, 0);

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockK42RamFS::startFillPage(uval physAddr, uval objOffset,
				       XHandle xhandle)
{
    passertMsg(0, "This should not happen (objOffset 0x%lx)\n",
	       objOffset);

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockK42RamFS::giveAccessSetClientData(ObjectHandle &oh,
						 ProcessID toProcID,
						 AccessRights match,
						 AccessRights nomatch,
						 TypeID type)
{
    SysStatus retvalue;
    ClientData *clientData = new ClientData();
    retvalue = giveAccessInternal(oh, toProcID, match, nomatch,
			      type, (uval)clientData);
    return (retvalue);
}

/* virtual */ SysStatus
ServerFileBlockK42RamFS::open(uval oflag, ProcessID pid, ObjectHandle &oh,
			      uval &useType, TypeID &type)
{
    return ServerFile::open(oflag, pid, oh, useType, type);
}

/* virtual*/ SysStatus
ServerFileBlockK42RamFS::exportedXObjectListEmpty()
{
    acquireLock();

    if (removalOnLastClose == 1) {
	SysStatus rc;
	if (bufChunks != NULL) {
	    locked_freeBufChunks();
	}
	rc = fileInfo->deleteFile();
	if (_FAILURE(rc)) {
	    tassertMsg(0, "let's look at this failure\n");
	    releaseLock();
	    return rc;
	}
	removalOnLastClose = 0;
	removed = 1;
	releaseLock();
	traceDestroy("k42RamFS", "exportedXObjectListEmpty");
	destroy();
	return 0;
    }

    releaseLock();
    return 0;
}

/* virtual */ SysStatus
ServerFileBlockK42RamFS::handleXObjFree(XHandle xhandle)
{
    return ServerFileBlock<StubFRPA>::handleXObjFree(xhandle);
}

/* virtual */ SysStatusUval
ServerFileBlockK42RamFS::_read(char *buf, uval len,
			       uval offset, __XHANDLE xhandle)
{
//    passertMsg(len <= FileLinux::SMALL_FILE_PPC_MAX_LOAD,
//	       "len %ld\n", len);

    AutoLock<LockType> al(getLockPtr()); // locks now, unlocks on return
    stubDetachLock.acquire();
    if (stubFR != NULL) {
	stubDetachLock.release();
	/* FIXME: return an approriate error (instead of EINVAL and
	 * change client to understand it */
	return _SERROR(2243, 0, ENOENT);
    }
    stubDetachLock.release();
    if (offset == uval(~0)) { /* hack: it means the client didn't
			       * really want to read, but check if it
			       * was ok for it to cache the small file
			       * in its own area (ie, check if there
			       * is already a FR for it*/
	return 0;
    }

    if (len > 0) {
	locked_bufChunkOperation(READ, buf, len, offset);
    }
#ifdef DILMA_DEBUG_SMALLFILES
    err_printf("Done ServerFileBlockK42RamFS::_read for len %ld\n", len);
#endif // #ifdef DILMA_DEBUG_SMALLFILES

    return _SRETUVAL(len);
}

/* virtual */ SysStatusUval
ServerFileBlockK42RamFS:: _write(const char *buf, uval len,
				 uval offset, __XHANDLE xhandle)
{
//    passertMsg(len <= FileLinux::SMALL_FILE_PPC_MAX_LOAD, "len %ld\n",
//	       len);

    acquireLock();

    // for debugging
//    stubDetachLock.acquire();
//    tassertMsg(stubFR == NULL, "?");

//    stubDetachLock.release();

    if (bufChunks == NULL) {
	bufChunks = (char**)
	    AllocGlobalPadded::alloc(NUM_CHUNKS*sizeof(char*));
	memset(bufChunks, 0, sizeof(char*)*NUM_CHUNKS);
	passertMsg(bufChunks != NULL, "out of memory\n");
    }

    if (len > 0) {
	locked_bufChunkOperation(WRITE, (char*) buf, len, offset);
    }

    releaseLock();

    return _SRETUVAL(len);
}

/* virtual */ SysStatus
ServerFileBlockK42RamFS::getFROH(ObjectHandle &oh, ProcessID pid)
{
    AutoLock<LockType> al(getLockPtr()); // locks now, unlocks on return

    SysStatus rc = ServerFileBlock<StubFRPA>::getFROH(oh, pid);
    _IF_FAILURE_RET(rc);
    if (bufChunks != NULL) {
	if (fileLength == 0) {
	    locked_freeBufChunks();
	} else {
	    passertMsg(fileLength <= FileLinux::MAX_SMALLFILE_SIZE, "?");
	    ObjectHandle tmpoh;
	    SysStatus rctmp = stubFR->stub._giveAccess(
		tmpoh, _SGETPID(DREFGOBJ(TheProcessRef)->getPID()));
	    StubFR stubTmp(StubObj::UNINITIALIZED);
	    stubTmp.setOH(tmpoh);
	    tassertMsg(_SUCCESS(rc), "ops");
	    passertMsg(fileLength <= FileLinux::MAX_SMALLFILE_SIZE,
		       "fileLength %ld\n", fileLength);
	    uval regadd;
	    rctmp = StubRegionDefault::_CreateFixedLenExt(
		regadd, fileLength, 0, tmpoh, 0,
		AccessMode::writeUserWriteSup, 0,
		RegionType::K42Region);
	    passertMsg(_SUCCESS(rctmp), "bind failed\n");

	    //locked_setFileLength will set fileLength
	    //This sequence truncates the file and re-fills it,
	    //in the process we must remember the real length.
	    uval length = fileLength;
	    rctmp = locked_setFileLength(0);

	    tassertMsg(_SUCCESS(rctmp), "ops");
	    locked_bufChunkOperation(COPY_TO_FR, (char*) regadd,
				     length, 0);
	    rctmp = locked_setFileLength(length);
	    tassertMsg(_SUCCESS(rctmp), "ops");

	    rctmp = DREFGOBJ(TheProcessRef)->regionDestroy(regadd);
	    passertMsg(_SUCCESS(rctmp), "regionDestroy failed?\n");
	    locked_freeBufChunks();
	    stubTmp._releaseAccess();
	}
    }
    return rc;
}

/* virtual */ SysStatus
ServerFileBlockK42RamFS::locked_setFileLength(uval len)
{
    _ASSERT_HELD_PTR(getLockPtr());
    AutoLock<BLock> al(&stubDetachLock); // locks now, unlocks on return
    // may not be attached, in which case don't care
    if (stubFR) {
	stubFR->stub._setFileLengthFromFS(len);
	// always return 0, since FR can go away
    } else if (len < fileLength) {
	locked_freeBufChunks(len);
    } else if (bufChunks == NULL) {
	bufChunks = (char**)
	    AllocGlobalPadded::alloc(NUM_CHUNKS*sizeof(char*));
	memset(bufChunks, 0, sizeof(char*)*NUM_CHUNKS);
	passertMsg(bufChunks != NULL, "out of memory\n");
    }
    fileLength = len;
    tassertMsg(fileLength==0 || stubFR || bufChunks,
	       "non-empty file without data\n");
    return 0;
}

/* virtual*/ SysStatus
ServerFileBlockK42RamFS::destroy()
{
    lock.acquire();
    if (bufChunks != NULL) {
	locked_freeBufChunks();
    }
    lock.release();
    return ServerFileBlock<StubFRPA>::destroy();
}

void
ServerFileBlockK42RamFS::locked_freeBufChunks(uval newlen)
{
    _ASSERT_HELD(lock);

    tassertMsg(bufChunks != NULL, "?");
    for (uval i = (newlen+CHUNK_SIZE-1)/CHUNK_SIZE; i < NUM_CHUNKS; i++) {
	if (bufChunks[i] != NULL) {
	    AllocGlobalPadded::free(bufChunks[i], CHUNK_SIZE);
	}
    }
    if (newlen==0) {
	AllocGlobalPadded::free(bufChunks, NUM_CHUNKS*sizeof(char*));
	bufChunks = NULL;
    }
}

void
ServerFileBlockK42RamFS::locked_bufChunkOperation(uval op, char *buf,
						  uval len, uval offset)
{
    _ASSERT_HELD(lock);

    uval first = offset / CHUNK_SIZE;
    uval last = (offset + len - 1) / CHUNK_SIZE;
    tassertMsg(first < NUM_CHUNKS && last < NUM_CHUNKS, "?");

    uval idx = 0;
    uval pos, nb;

    for (uval i = first; i <= last; i++) {
	tassertMsg((op == COPY_TO_FR && idx <= fileLength
		    && offset <= fileLength) ||
		   (idx <= FileLinux::SMALL_FILE_PPC_MAX_LOAD
		   && offset <= FileLinux::MAX_SMALLFILE_SIZE), "?");
	if (!bufChunks) {
	    tassertWrn(0,"no bufchunks: %lx\n", fileLength);
	    bufChunks = (char**)
		AllocGlobalPadded::alloc(NUM_CHUNKS*sizeof(char*));
	    memset(bufChunks, 0, sizeof(char*)*NUM_CHUNKS);
	}

	if (bufChunks[i] == NULL) {
	    bufChunks[i] = (char*) AllocGlobalPadded::alloc(CHUNK_SIZE);
	}
	passertMsg(bufChunks[i] != NULL, "no mem");

	pos = offset % CHUNK_SIZE;
	nb = MIN(len, CHUNK_SIZE-pos);
#if 0
	tassertWrn(0, "op %ld chunk %ld, pos %ld, nb %ld\n", op, i, pos,
		   nb);
#endif
	if (op == WRITE) {
	    memcpy(&bufChunks[i][pos], &buf[idx], nb);
	} else {
	    tassertMsg(op == READ || op == COPY_TO_FR, "?");
	    memcpy(&buf[idx], &bufChunks[i][pos], nb);
	}

	idx += nb;
	offset += nb;
	len-= nb;
    }
}
