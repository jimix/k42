/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPANonPageableRamOnly.C,v 1.3 2004/10/05 21:28:19 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that supports ram only files.  New page
 *                     requests are never sent to the file system.
 * **************************************************************************/

#include "kernIncs.H"
#include "FRPANonPageableRamOnly.H"
#include "proc/Process.H"
#include <sys/ProcessSet.H>

/* static */ void
FRPANonPageableRamOnly::ClassInit(VPNum vp)
{
    if (vp!=0) return;
}


/* virtual */ SysStatusUval
FRPANonPageableRamOnly::startFillPage(uval physAddr, uval objOffset)
{
    /* all requests to fill a page should be rejected.  It is the FCM's
       job to zero fill the page */
    return PAGE_NOT_FOUND;
}

/* static */ SysStatus
FRPANonPageableRamOnly::Create(ObjectHandle &oh, ProcessID processID,
			       ObjectHandle file,
			       uval len,
			       uval fileToken,
			       char *name, uval namelen,
			       KernelPagingTransportRef ref)
{
    // This is the same thing as FRPA::_Create
    SysStatus rc;
    FRPANonPageableRamOnlyRef frref;
    ProcessRef pref;
    FRPANonPageableRamOnly *fr;

    // get process ref for calling file system
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(processID,
						   (BaseProcessRef&)pref);
    tassert(_SUCCESS(rc), err_printf("calling process can't go away??\n"));

    fr = new FRPANonPageableRamOnly;
    tassert( (fr!=NULL), err_printf("alloc should never fail\n"));

    rc = fr->init(file, len, fileToken, name, namelen, ref);

    tassert( _SUCCESS(rc), err_printf("woops\n"));

    frref = (FRPANonPageableRamOnlyRef)CObjRootSingleRep::Create(fr);

    // call giveAccessInternal here to provide fileSystemAccess to
    // the file systems OH
    // note that we set the client data to 1 to mark this as the fr
    // oh so we know if the fs goes away
    rc = DREF(frref)->giveAccessInternal(
	oh, processID,
	MetaFR::fileSystemAccess|MetaObj::controlAccess|MetaObj::attach,
	MetaObj::none, 0, 1);

    if (_FAILURE(rc)) return rc;

    return 0;
}
