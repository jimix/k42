/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRPlaceHolder.C,v 1.28 2004/10/29 16:30:32 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Application interface to file, have one
 * per-open instance of file.
 * **************************************************************************/
#include "kernIncs.H"
#include "mem/FCMStartup.H"
#include "mem/FRPlaceHolder.H"
#include <cobj/CObjRootSingleRep.H>

/* virtual */ SysStatus
FRPlaceHolder::setFileLength(uval fileLength)
{
    return 0;
}

/* static */ SysStatus
FRPlaceHolder::Create(FRRef &frRef, uval useRef)
{
    FRPlaceHolder *frstart = new FRPlaceHolder;

    if (frstart == NULL) {
	return -1;
    }

    frstart->init();

    if (useRef) {
	CObjRootSingleRepPinned::Create(
	    frstart, (RepRef)frRef);
    } else {
	frRef = (FRRef)(CObjRootSingleRep::Create(frstart));
    }

    return 0;
}

/* static */ SysStatus
FRPlaceHolderPinned::Create(FRRef &frRef, uval useRef)
{
    FRPlaceHolderPinned *frstart = new FRPlaceHolderPinned;

    if (frstart == NULL) {
	return -1;
    }

    frstart->init();

    if (useRef) {
	CObjRootSingleRepPinned::Create(
	    frstart, (RepRef)frRef);
    } else {
	frRef = (FRRef)(CObjRootSingleRepPinned::Create(frstart));
    }

    return 0;
}

/*
 * since Startup is special we need to hand create the FCM and attach
 * the created FCMStartup to the FRPlaceHolder we care about, thus if we
 * hit the assert below that means we have not created the FCMStartup
 * since this called is triggered from the region we no longer have the
 * information we need ot create the FCM at this point
 */
SysStatus
FRPlaceHolder::locked_getFCM(FCMRef &r)
{
    _ASSERT_HELD(lock);

    if (beingDestroyed) {
	r = FCMRef(TheBPRef);
	return -1;
    }

    if (fcmRef != 0) {
	r = fcmRef;
	return 0;
    }
    tassert(0, err_printf("FRPlaceHolder::we should already have an FCM see"
			  " comment\n"));
    return (-1);
}

SysStatus
FRPlaceHolder::putPage(uval physAddr,  uval objOffset)
{
    if (fcmRef) {
	// we can never write
	passertMsg(0,"FRPlaceHolder::putPage FIXME NYI\n");
	return 0;
    }
    return 0;
}


/* currently no destroy strategy for fr startup */
SysStatus
FRPlaceHolder::fcmNotInUse()
{
    passertMsg(0,"FRPlaceHolder::regionListIsEmpty FIXME NYI\n");
    return 0;
}

SysStatus
FRPlaceHolder::destroy()
{
    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    passertMsg(0,"FRPlaceHolder::destroy FIXME NYI\n");
    return 0;
}

/* virtual */ SysStatusUval
FRPlaceHolder::startFillPage(uval physAddr, uval objOffset)
{
    tassert(0,err_printf("FRPlaceHolder::startFillPage should not be "
			 "called\n"));
    return -1;
}

/* virtual */ SysStatus
FRPlaceHolder::_fsync()
{
    /* fsync is a nop for startup storage */
    return 0;
}

/* virtual */ SysStatus
FRPlaceHolder::startPutPage(uval physAddr, uval objOffset, 
			    IORestartRequests *rr)
{
    passertMsg(0, "FRPlaceHolder::startPutPage FIXME: NYI\n");
    return -1;
}
