/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRCommon.C,v 1.19 2004/01/13 15:40:49 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Application interface to file, have one
 * per-open instance of file.
 * **************************************************************************/
#include "kernIncs.H"
#include "mem/FCMDefault.H"
#include "mem/FRCommon.H"

/* virtual */ SysStatus
FRCommon::init()
{
    fcmRef = NULL;
    beingDestroyed = 0;
    pageSize = PAGE_SIZE;
    lock.init();
    return 0;
}

SysStatus
FRCommon::detachFCM()
{
    if (beingDestroyed) return 0;

    FetchAndClearVolatile((uval *)&fcmRef);
    destroy();
    return 0;
}


SysStatus
FRCommon::installFCM(FCMRef locFCMRef)
{
    // FIXME we ought to do some sanity checking to make sure this
    //       ref is reasonable
    if(fcmRef != 0 || beingDestroyed) {
	// FIXME we want ! ENOENT and we want it to be class specific name
	return _SERROR(1269, ENOENT, 0);
    } else {
	fcmRef = locFCMRef;
	DREF(fcmRef)->attachFR((FRRef)getRef());
	return 0;
    }
}

/* virtual */
SysStatus FRCommon::locked_attachRegion(
    FCMRef& fcmRef, RegionRef regRef, PMRef pmRef, AccessMode::mode accessMode)
{
    _ASSERT_HELD(lock);
    SysStatus rc;
    rc = locked_getFCM(fcmRef);
    if(_FAILURE(rc)) return rc;
    return DREF(fcmRef)->attachRegion(regRef, pmRef, accessMode);
}

/* The default destroy strategy is to destroy the pair when there are
 * no object refs to the FR and no regions attached to the FCM.
 * Either may happen last!  We assume that there is NO way to get an
 * object ref to an FR once no refs are outstanding.  If this is not
 * true the FR most override fcmNotInUse
 *
 * There is a potential sync race here - we must make sure that if the
 * last releaseAccess happens on one processor and the
 * regionListIsEmpty call on another, that one of the two decides to
 * destroy.
 */
SysStatus
FRCommon::fcmNotInUse()
{
    if (beingDestroyed) return 0;

    // if no xobjects connected, destroy
    if(isEmptyExportedXObjectList()) {
	destroy();
    }

    // when exportedXObjList goes empty, we will destroy
    return 0;
}

SysStatus
FRCommon::destroy()
{
    uval alreadyDestroyed = SwapVolatile(&beingDestroyed, 1);
    FCMRef tempfcmRef;

    if (alreadyDestroyed) return 0;

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if(_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    tempfcmRef = fcmRef;

    // first destroy FCM, which will free pages
    // we don't use locking here - but its safe because
    // any uses of fcmRef which beat this will get destroyed object
    // returns.

    fcmRef = 0;
    if (tempfcmRef) DREF(tempfcmRef)->destroy();

    // schedule the object for deletion
    destroyUnchecked();
    return 0;
}
