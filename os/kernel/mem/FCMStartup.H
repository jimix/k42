#ifndef __FCMSTARTUP_H_
#define __FCMSTARTUP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMStartup.H,v 1.22 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: FCM that mapps startup memory 1-1 with offset
 * passed to it, used for mapping kernel on boot.
 * **************************************************************************/

#include "mem/FCM.H"

class PageDesc;

class FCMStartup : public FCM {
protected:
    FRRef frRef;
    uval imageOffset; // offset from beginning of FCMReal where this image lives
    uval imageSize;

    // should not be called by anyone
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn);

    // should not be called by anyone
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0);

    virtual SysStatusUval getForkPage(
	PageDesc* callerPg, uval& returnUval,
	FCMComputationRef& childRef,
	PageFaultNotification *fn,
	uval copyOnWrite);

public:
    static SysStatus Create(FCMRef &ref, uval imageOffset, uval imageSize);

    // when an fr/fcm pair is build externally, the fr must connect
    virtual SysStatus attachFR(FRRef frRefArg){
	tassert(frRef == NULL, err_printf("already attached\n"));
	frRef = frRefArg;
	return 0;
    }

    virtual SysStatus getFRRef(FRRef& frRefArg) {
	frRefArg = frRef; return 0;}

    virtual SysStatusUval mapPage(uval fileOffset, uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);

    // always used by kernel, so us pinned implementation.
    DEFINE_PINNEDGLOBALPADDED_NEW(FCMStartup);

    // attach to the FCM to map (possibly) part of it
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode);

    virtual SysStatus detachRegion(RegionRef regRef);

    virtual SysStatus destroy();

    virtual SysStatusUval isNotInUse() { return 0; }

    virtual SysStatusUval isEmpty() { return 0; }

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

#endif /* #ifndef __FCMSTARTUP_H_ */
