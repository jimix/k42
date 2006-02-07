/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMReal.C,v 1.52 2004/07/11 21:59:27 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * This FCM maps real memory.
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/FCMReal.H"
#include "mem/FRPlaceHolder.H"
#include <cobj/CObjRootSingleRep.H>


SysStatusUval
FCMReal::getPage(uval fileOffset, void *&dataPtr, PageFaultNotification */*fn*/)
{
    uval paddr, virt;

    paddr = fileOffset;
    virt = PageAllocatorKernPinned::realToVirt(paddr);
    dataPtr = (void *)virt;
    return 0;
}

SysStatus
FCMReal::releasePage(uval /*ptr*/, uval dirty)
{
    tassertMsg(dirty==0, "NYI");            
    return 0;
}


SysStatus
FCMReal::ClassInit(VPNum vp)
{
    if (vp==0) {
	FCMReal* fcm;
	FCMRef fcmRef;
	fcm = new FCMReal;
	fcmRef = (FCMRef)CObjRootSingleRepPinned::Create(fcm);
	fcm->frRef = GOBJK(TheFRRealRef);
	FRPlaceHolderPinned::Create(fcm->frRef, 1);
	DREF(fcm->frRef)->installFCM(fcmRef);
    }
    return 0;
}

SysStatusUval
FCMReal::mapPage(uval offset,
		 uval regionVaddr,
		 uval regionSize,
		 AccessMode::pageFaultInfo pfinfo,
		 uval vaddr, AccessMode::mode access,
		 HATRef hat, VPNum vp,
		 RegionRef /*reg*/, uval /*firstAccessOnPP*/,
		 PageFaultNotification */*fn*/)
{
    SysStatus rc;
    uval paddr;
    
    offset += vaddr - regionVaddr;

    //FIXME - should this check for real memory available to this
    //        application manager???

    // FIXME debugging next line lines
    // uval tmp  = *(uval *) (0xc0000000 | offset);


    paddr = offset;

    rc = DREF(hat)->mapPage(paddr, vaddr, PAGE_SIZE, pfinfo, access, vp, 1);
    
    return rc;
}

// attach to the FCM to map (possibly) part of it
SysStatus
FCMReal::attachRegion(RegionRef regRef, PMRef pmRef,
		      AccessMode::mode accessMode)
{
    (void) regRef;
    (void) pmRef;
    (void) accessMode;
    return 0;
}

SysStatus
FCMReal::detachRegion(RegionRef regRef)
{
    (void) regRef;
    return 0;
}

SysStatus
FCMReal::destroy()
{
    //There is only one FCMReal - behind TheFCMRealRef in GOBJK
    //so panic if destroy is called
    passert(0,err_printf("Attempt to destroy the FCMReal\n"));
    return -1;				// not reached
}

SysStatus
FCMReal::printStatus(uval kind)
{
    switch(kind) {
    case PendingFaults:

	break;
    default:
	err_printf("kind %ld not known in printStatus\n", kind);
    }
    return 0;
}
