/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockKFS.C,v 1.51 2004/11/02 19:41:26 dilma Exp $
 *****************************************************************************/

#include <kfsIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubFRPA.H>
#include <io/FileLinux.H>
#include <fslib/DirLinuxFS.H>

#include "FileSystemKFS.H"
#include "ServerFileBlockKFS.H"

#define INSTNAME ServerFileBlockKFS
#include <meta/TplMetaPAPageServer.H>
#include <xobj/TplXPAPageServer.H>
#include <tmpl/TplXPAPageServer.I>

typedef TplXPAPageServer<ServerFileBlockKFS> XPAPageServerSFKFS;
typedef TplMetaPAPageServer<ServerFileBlockKFS> MetaPAPageServerSFKFS;

SysStatus
ServerFileBlockKFS::locked_createFR()
{
    ObjectHandle oh, myOH;
    SysStatus rc;

    _ASSERT_HELD(stubDetachLock);

    KFS_DPRINTF(DebugMask::SERVER_FILE,
		"ServerFileBlockKFS::StubFRHolder::init() was called.\n");

    // create an object handle for upcall from kernel FR
    giveAccessByServer(myOH, _KERNEL_PID, MetaPAPageServerSFKFS::typeID());

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

    rc = stubKPT._createFRPA(oh, myOH, fileLength, (uval) getRef(),
			     name, namelen);
    tassertMsg(_SUCCESS(rc), "woops rc 0x%lx\n", rc);

    stubFR = new StubFRHolder();
    stubFR->stub.setOH(oh);
    // err_printf("constructing StubFRHolder %p\n", this);
    return 0;
}

/* static */ SysStatus
ServerFileBlockKFS::Create(ServerFileRef & fref, FSFile *fileInfo,
			   ObjectHandle kptoh)
{
    SysStatus rc;
    ServerFileBlockKFS *file = new ServerFileBlockKFS;

    KFS_DPRINTF(DebugMask::SERVER_FILE,
		"ServerFileBlockKFS::Create() was called.\n");

    tassertMsg(file != NULL, "failed allocate of ServerFileBlockKFS\n");

    rc = file->init(fileInfo, kptoh);
    if (_FAILURE(rc)) {
	delete file;
	fref = NULL;
	return rc;
    }

    fref = (ServerFileRef) file->getRef();

    return 0;
}

SysStatus
ServerFileBlockKFS::init(FSFile *finfo, ObjectHandle kptoh)
{
    SysStatus rc;
    FileLinux::Stat status;

    KFS_DPRINTF(DebugMask::SERVER_FILE,
		"ServerFileBlockKFS::init() was called.\n");

    rc = ServerFileBlock<StubFRPA>::init(kptoh);
    _IF_FAILURE_RET(rc);

    fileInfo = finfo;
    fsfKFS()->reValidateFSFile(&status);
    fileLength = status.st_size;

    CObjRootSingleRep::Create(this);

    MetaPAPageServerSFKFS::init();

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockKFS::startWrite(uval physAddr, uval objOffset, uval len,
			       XHandle xhandle)
{
#ifdef DILMA_DEBUG_RW
    err_printf("{W");
#endif // #ifdef DILMA_DEBUG_RW

    SysStatus rc = 0;

    KFS_DPRINTF(DebugMask::SERVER_FILE_W,
		"ServerFileBlockKFS::startWrite() was called.\n");

    //err_printf("startWrite() for addr 0x%lx\n", physAddr);
    rc = fsfKFS()->writeBlockPhys(physAddr, len, objOffset,
				  (ServerFileRef)getRef());

    lock.acquire();
    if (objOffset + len > fileLength) {
	fileLength = objOffset + len;
    }
    lock.release();

#ifdef DILMA_DEBUG_RW
    err_printf("}");
#endif // #ifdef DILMA_DEBUG_RW

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockKFS::startFillPage(uval physAddr, uval objOffset,
				  XHandle xhandle)
{
#ifdef DILMA_DEBUG_RW
    err_printf("{R");
#endif // #ifdef DILMA_DEBUG_RW

    SysStatus rc;

    KFS_DPRINTF(DebugMask::SERVER_FILE_R,
		"ServerFileBlockKFS::startFillPage() was called.\n");

#if 0
    char *name = "UNKNOWN";
#ifdef HACK_FOR_FR_FILENAMES
    name = nameAtCreation;
#endif // #ifdef HACK_FOR_FR_FILENAMES
    err_printf("<K reading offset %ld physAddr 0x%lx name %s>\n", objOffset,
	       physAddr, name);
#endif

    rc = fsfKFS()->readBlockPhys(physAddr, objOffset,
				 (ServerFileRef)getRef());

#ifdef DILMA_DEBUG_RW
    err_printf("}");
#endif // #ifdef DILMA_DEBUG_RW
    return 0;
}
