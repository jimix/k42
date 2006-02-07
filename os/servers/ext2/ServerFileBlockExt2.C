/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ServerFileBlockExt2.C,v 1.6 2004/11/04 03:54:05 dilma Exp $
 *****************************************************************************/

//  All the code here is very very similar to the code in other ServerFileBlock
//  such as ServerFileBlockKFS

#include <sys/sysIncs.H>
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubFRPA.H>
#include <io/FileLinux.H>
#include <fslib/DirLinuxFS.H>

#include "FileSystemExt2.H"
#include "ServerFileBlockExt2.H"

#define INSTNAME ServerFileBlockExt2
#include <meta/TplMetaPAPageServer.H>
#include <xobj/TplXPAPageServer.H>
#include <tmpl/TplXPAPageServer.I>

typedef TplXPAPageServer<ServerFileBlockExt2> XPAPageServerExt2;
typedef TplMetaPAPageServer<ServerFileBlockExt2> MetaPAPageServerExt2;


SysStatus
ServerFileBlockExt2::locked_createFR()
{
    ObjectHandle oh, myOH;
    SysStatus rc;

    _ASSERT_HELD(stubDetachLock);

    // create an object handle for upcall from kernel FR
    giveAccessByServer(myOH, _KERNEL_PID, MetaPAPageServerExt2::typeID());

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

    return 0;
}

/* static */ SysStatus
ServerFileBlockExt2::Create(ServerFileRef & fref, FSFile *fileInfo,
			    ObjectHandle kptoh)
{
    SysStatus rc;
    ServerFileBlockExt2 *file = new ServerFileBlockExt2;

    tassertMsg(file != NULL, "failed allocate of ServerFileBlockExt2\n");

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
ServerFileBlockExt2::init(FSFile *finfo, ObjectHandle kptoh)
{
    SysStatus rc;
    FileLinux::Stat status;

    rc = ServerFileBlock<StubFRPA>::init(kptoh);
    _IF_FAILURE_RET(rc);

    fileInfo = finfo;
    ((FSFileExt2*)finfo)->getStatus(&status);
    fileLength = status.st_size;

    CObjRootSingleRep::Create(this);

    MetaPAPageServerExt2::init();

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockExt2::startWrite(uval physAddr, uval objOffset, uval len,
				XHandle xhandle)
{
    AutoLock < LockType > al(getLockPtr());	// locks now, unlocks on return

    SysStatus rc = 0;

    rc = ((FSFileExt2*)fileInfo)->writeBlockPhys(physAddr, len, objOffset);
    if (rc) goto out;

    if (objOffset + len > fileLength) {
	fileLength = objOffset + len;
    }

    rc = ((FSFileExt2*)fileInfo)->updateFileLength(fileLength);
    if (rc) goto out;

    stubFR->stub._ioComplete(physAddr, objOffset, rc);

 out:
    return rc;
}

/* virtual */ SysStatus
ServerFileBlockExt2::startFillPage(uval physAddr, uval objOffset,
				   XHandle xhandle)
{
    AutoLock<LockType> al(getLockPtr());  // locks now, unlocks on return
    SysStatus rc;

#if 0
    char *name = "UNKNOWN";
#ifdef HACK_FOR_FR_FILENAMES
    name = nameAtCreation;
#endif // #ifdef HACK_FOR_FR_FILENAMES
    err_printf("<K reading offset %ld physAddr 0x%lx name %s>\n", objOffset,
	       physAddr, name);
#endif
    rc = ((FSFileExt2*)fileInfo)->readBlockPhys(physAddr, objOffset);
    stubFR->stub._ioComplete(physAddr, objOffset, rc);

#ifndef NDEBUG
    err_printf("E");
#endif // #ifndef NDEBUG

    return 0;
}

/* virtual */ SysStatus
ServerFileBlockExt2::_write(uval physAddr, uval objOffset, uval len,
			    __XHANDLE xhandle)
{
    AutoLock<LockType> al(getLockPtr());  // locks now, unlocks on return

    passertMsg(len == PAGE_SIZE, "woops\n");
    SysStatus rc;
    rc = ((FSFileExt2*)fileInfo)->writeBlockPhys(objOffset, physAddr, len);
    _IF_FAILURE_RET(rc);

    if (objOffset + len > fileLength) {
	fileLength = objOffset + len;
    }

    return 0;
}
