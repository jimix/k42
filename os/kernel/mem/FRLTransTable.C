/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRLTransTable.C,v 1.4 2003/03/30 18:30:55 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Application interface to file, have one
 * per-open instance of file.
 * **************************************************************************/
#include "kernIncs.H"
#include "FRLTransTable.H"
#include <stub/StubFRLTransTable.H>
#include <cobj/CObjRootSingleRep.H>
#include "FCMLTransTable.H"

/* virtual */ SysStatus
FRLTransTable::getType(TypeID &id)
{
    id = StubFRLTransTable::typeID();
    return 0;
}

/* static */ void
FRLTransTable::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaFRLTransTable::init();
}

/* static */ SysStatus
FRLTransTable::_Create(ObjectHandle &frOH, uval defaultObject,
                       __CALLER_PID caller)
{
    SysStatus rc;
    FRRef frRef;
    FRLTransTable *fr = new FRLTransTable;

    if (fr == NULL) {
	return -1;
    }

    frRef = (FRRef)(CObjRootSingleRep::Create(fr));

    fr->init();
    // okay, have to allocate a new fcm
    rc = FCMLTransTable<AllocGlobal>::Create(fr->fcmRef, frRef, defaultObject);
    tassertWrn(_SUCCESS(rc), "allocation of fcm failed\n");

    rc = fr->giveAccessByServer(frOH, caller);
    if (_FAILURE(rc)) {
	// if we can't return an object handle with object can never
	// be referenced.  Most likely cause is that caller has terminated
	// while this create was happening.
	fr->destroy();
    }
    return rc;
}

/* static */ SysStatus
FRLTransTablePinned::Create(FRRef &frRef, uval defaultObject)
{
    FRLTransTablePinned *fr = new FRLTransTablePinned;

    if (fr == NULL) {
	return -1;
    }

    frRef = (FRRef)(CObjRootSingleRepPinned::Create(fr));

    fr->init();
    // okay, have to allocate a new fcm
    SysStatus rc = FCMLTransTablePinned::Create(
	fr->fcmRef, frRef, defaultObject);
    tassertWrn(_SUCCESS(rc), "allocation of fcm failed\n");
    return 0;
}

SysStatus
FRLTransTable::locked_getFCM(FCMRef &r)
{
    _ASSERT_HELD(lock);

    if (beingDestroyed) {
	r = FCMRef(TheBPRef);
	return -1;
    }

    passertMsg(fcmRef!=0, "FRLTransTable no FCM???\n");

    r = fcmRef;
    return 0;
}

