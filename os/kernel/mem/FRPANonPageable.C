/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPANonPageable.C,v 1.12 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Primitive FR that obtains data via the thinwire
 * file system.
 * **************************************************************************/

#include "kernIncs.H"
#include "FRPANonPageable.H"
#include "mem/FCMFile.H"
#include "proc/Process.H"
#include <sys/ProcessSet.H>

/* static */ void
FRPANonPageable::ClassInit(VPNum vp)
{
    if (vp!=0) return;
}

/* virtual */ SysStatus
FRPANonPageable::locked_getFCM(FCMRef &r)
{
    /* This is the same thing as FRPA::locked_getFCM, except by
     * one argument being passaed to the creating of the FCM */
    _ASSERT_HELD(lock);

    if (beingDestroyed) {
	r = FCMRef(TheBPRef);
	return _SDELETED(2205);
    }

    if (fcmRef != 0) {
	r = fcmRef;
	return 0;
    }
    // okay, have to allocate a new fcm
    SysStatus rc = FCMFile::CreateDefault(fcmRef, (FRRef)getRef(), 0);

    tassertWrn(_SUCCESS(rc), "allocation of fcm failed\n");
    r = fcmRef;
    return rc;
}

/* static */ SysStatus
FRPANonPageable::Create(ObjectHandle &oh, ProcessID processID,
			ObjectHandle file,
			uval len,
			uval fileToken,
			char *name, uval namelen,
			KernelPagingTransportRef ref)
{
    // This is the same thing as FRPA::_Create
    SysStatus rc;
    FRPANonPageableRef frref;
    ProcessRef pref;
    FRPANonPageable *fr;

    // get process ref for calling file system
    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(processID,
						   (BaseProcessRef&)pref);
    tassert(_SUCCESS(rc), err_printf("calling process can't go away??\n"));

    fr = new FRPANonPageable;
    tassert( (fr!=NULL), err_printf("alloc should never fail\n"));

    rc = fr->init(file, len, fileToken, name, namelen, ref);

    tassert( _SUCCESS(rc), err_printf("woops\n"));

    frref = (FRPANonPageableRef)CObjRootSingleRep::Create(fr);

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

/*
 * for intrinsically non-pageable file, don't want to do anything when synced,
 * e.g., for ram file systems
 */
/* virtual */ SysStatus 
FRPANonPageable::_fsync() {
    return 0;
}; 
