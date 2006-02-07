#ifndef __FRCOMMON_H_
#define __FRCOMMON_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRCommon.H,v 1.24 2005/04/15 17:39:37 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interface to applications for files in memory manager
 * **************************************************************************/

#include "mem/FR.H"

#ifdef _IN_STUBGEN
class FCM;
class FCMDefault;
typedef FCM **FCMRef;
#else
#include "mem/FCMDefault.H"
#endif /* #ifndef _IN_STUBGEN */

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class FRCommon;
typedef FRCommon **FRCommonRef;
typedef FCMDefault **FCMDefaultRef;

class FRCommon : public FR {
protected:
    FCMRef fcmRef;
    uval   beingDestroyed;
    uval   pageSize;

    typedef BLock LockType;
    LockType      lock;

    DEFINE_GLOBAL_NEW(FRCommon);
    DEFINE_REFS(FRCommon);
    SysStatus init();

    virtual SysStatus getFCM(FCMRef &f) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return locked_getFCM(f);
    }

    virtual SysStatus locked_getFCM(FCMRef &f)=0;
    
    virtual SysStatus locked_attachRegion(
	FCMRef& fcmRef, RegionRef regRef, PMRef pmRef, AccessMode::mode accessMode);

public:
    virtual SysStatus detachFCM();

    virtual SysStatus fcmNotInUse();

    virtual SysStatus destroy();
	
    // if the FR does not already have an FCM use this else return error
    virtual SysStatus installFCM(FCMRef fcmRef);

    virtual SysStatus attachFCM(FCMRef &f, FCMRef attachingFCMRef) {
	SysStatus rc;
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	rc = locked_getFCM(f);
	if(_FAILURE(rc)) return rc;
#ifndef _IN_STUBGEN
	DREF(f)->addReference();
#endif /* #ifndef _IN_STUBGEN */
	return rc;
    }

    // returns 0 if fileOffset exists in file, 1 otherwise
    virtual SysStatus checkOffset(uval fileOffset){
	return 1;}

    virtual SysStatus setOffset(uval fileOffset, uval blockID){
	(void) fileOffset; (void) blockID;
	passert(0, err_printf("setOffset not allowed\n"));
	return 0;
    }

    virtual SysStatus freeOffset(uval fileOffset) {
	(void) fileOffset;
	return 0;
    }

    virtual SysStatus getBlockID(uval fileOffset, uval& blockID) {
	(void) fileOffset;
	blockID = uval(-1);
	return 0;
    }

    virtual SysStatus setFileLength(uval fileLength) {
	return 0;
    }

    virtual SysStatusUval getPageSize() {
	return _SRETUVAL(pageSize);
    }

    // attach to the FR's FCM to map (possibly) part of it
    virtual SysStatus attachRegion(
	FCMRef& _fcmRef, RegionRef regRef, PMRef pmRef,
	AccessMode::mode accessMode) {
	AutoLock<LockType> al(&lock); // locks now, unlocks on return
	return locked_attachRegion(_fcmRef, regRef, pmRef, accessMode);
    }
};
#endif /* #ifndef __FRCOMMON_H_ */
