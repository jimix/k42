/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockKFS.C,v 1.3 2005/01/10 15:31:05 dilma Exp $
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

typedef TplXPAPageServer<ServerFileBlockKFS> XPAPageServerKFS;
typedef TplMetaPAPageServer<ServerFileBlockKFS> MetaPAPageServerKFS;


SysStatus
ServerFileBlockKFS::locked_createFR()
{
    ObjectHandle oh, myOH;
    SysStatus rc;

    _ASSERT_HELD(stubDetachLock);

    KFS_DPRINTF(DebugMask::SERVER_FILE,
		"ServerFileBlockKFS::StubFRHolder::init() was called.\n");

    // create an object handle for upcall from kernel FR
    giveAccessByServer(myOH, _KERNEL_PID, MetaPAPageServerKFS::typeID());

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

    rc = StubFRPA::_Create(oh, myOH, fileLength, name, namelen);

    tassertMsg(_SUCCESS(rc), "woops rc 0x%lx\n", rc);

    stubFR = new StubFRHolder();
    stubFR->stub.setOH(oh);
    // err_printf("constructing StubFRHolder %p\n", this);
    return 0;
}

/* static */ SysStatus
ServerFileBlockKFS::Create(ServerFileRef & fref, FSFile *fileInfo)
{
    SysStatus rc;
    ServerFileBlockKFS *file = new ServerFileBlockKFS;

    KFS_DPRINTF(DebugMask::SERVER_FILE,
		"ServerFileBlockKFS::Create() was called.\n");

    tassertMsg(file != NULL, "failed allocate of ServerFileBlockKFS\n");

    rc = file->init(fileInfo);
    if (_FAILURE(rc)) {
	delete file;
	fref = NULL;
	return rc;
    }

    fref = (ServerFileRef) file->getRef();

    return 0;
}

SysStatus
ServerFileBlockKFS::init(FSFile *finfo)
{
    SysStatus rc;
    FileLinux::Stat status;

    KFS_DPRINTF(DebugMask::SERVER_FILE,
		"ServerFileBlockKFS::init() was called.\n");

    rc = ServerFileBlock<StubFRPA>::init();
    _IF_FAILURE_RET(rc);

    fileInfo = finfo;
    fsfKFS()->reValidateFSFile(&status);
    fileLength = status.st_size;

    CObjRootSingleRep::Create(this);

    MetaPAPageServerKFS::init();

    return 0;
}

 /* virtual */ SysStatus
ServerFileBlockKFS::_write(uval physAddr, uval objOffset, uval len,
			   __XHANDLE xhandle)
{
    AutoLock<LockType> al(getLockPtr());  // locks now, unlocks on return

    KFS_DPRINTF(DebugMask::SERVER_FILE_W,
		"ServerFileBlockKFS::_write() was called.\n");

    tassert((len == PAGE_SIZE), err_printf("woops\n"));
    SysStatus rc;
    rc = fsfKFS()->writeBlockPhys(objOffset, physAddr, len);
    _IF_FAILURE_RET(rc);

    if (objOffset + len > fileLength) {
	fileLength = objOffset + len;
    }

    return 0;
}
