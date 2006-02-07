#ifndef __FCMREAL_H_
#define __FCMREAL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMReal.H,v 1.42 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FCM that mapps real memory 1-1 with offset
 * passed to it, used for mapping kernel on boot.
 * **************************************************************************/

#include "mem/FCM.H"

class FCMReal : public FCM {
protected:
    FRRef frRef;

    // called by FCMCRW, returns a locked page if it exists
    // if value returned > 0, then async == 0 done, < 0 error
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn);

    // unlocked previously gotton page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0);

public:
    static SysStatus ClassInit(VPNum vp);

    virtual SysStatusUval mapPage(uval fileOffset, uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);

    /*
     * Almost always used by kernel, so just always used pinned
     * implementation.
     */
    DEFINE_PINNEDGLOBALPADDED_NEW(FCMReal);

    // attach to the FCM to map (possibly) part of it
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode);

    virtual SysStatus detachRegion(RegionRef regRef);

    virtual SysStatus destroy();

    virtual SysStatusUval isNotInUse() { return 0; }

    virtual SysStatusUval isEmpty() { return 0; }

    // since the FCMReal lives forever, there's no need to remember its FR
    virtual SysStatus attachFR(FRRef frRefArg) { return 0; }

    virtual SysStatus getFRRef(FRRef& frRefArg) {
	frRefArg = frRef; return 0;}

    virtual SysStatus discardCachedPages() {
	passertMsg(0, "NYI\n");
	return 0;
    }

    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments() {
	return 0;
    }

    // used for debugging
    virtual SysStatus printStatus(uval kind);
};

#endif /* #ifndef __FCMREAL_H_ */
