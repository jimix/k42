/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMStartup.C,v 1.25 2004/01/23 19:50:30 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * This FCM maps startup memory.
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKern.H"
#include "mem/FCMStartup.H"
#include <cobj/CObjRootSingleRep.H>
#include "PageDesc.H"

SysStatus
FCMStartup::Create(FCMRef &ref, uval imageOffset, uval imageSize)
{
    FCMStartup *fcm;

    fcm = new FCMStartup;
    if (fcm == NULL) return -1;

    ref = (FCMRef)CObjRootSingleRep::Create(fcm);
    fcm->frRef = NULL;

    fcm->imageOffset = imageOffset;
    fcm->imageSize = imageSize;
    return 0;
}

SysStatusUval
FCMStartup::getPage(uval fileOffset, void *&dataPtr, PageFaultNotification */*fn*/)
{
    uval paddr, virt;

    paddr = fileOffset + imageOffset;
    virt = PageAllocatorKernPinned::realToVirt(paddr);
    dataPtr = (void *)virt;
    return 0;
}

SysStatus
FCMStartup::releasePage(uval /*ptr*/,uval dirty)
{
    tassertMsg(dirty==0, "NYI");
    return 0;
}

SysStatusUval
FCMStartup::mapPage(uval offset, uval regionVaddr, uval regionSize,
		    AccessMode::pageFaultInfo pfinfo, uval vaddr,
		    AccessMode::mode access, HATRef hat, VPNum vp,
		    RegionRef /*reg*/, uval /*firstAccessOnPP*/,
		    PageFaultNotification */*fn*/)
{
    SysStatus rc;
    uval paddr;

    offset += vaddr - regionVaddr;

    if (offset >= imageSize) {
	return _SERROR(1439, 0, EFAULT);
    }

    paddr = offset + imageOffset;

    rc = DREF(hat)->mapPage(paddr, vaddr, PAGE_SIZE, pfinfo, access, vp, 1);
    
    return rc;
}

SysStatusUval
FCMStartup::getForkPage(
    PageDesc* callerPg, uval& returnUval, FCMComputationRef& childRef,
    PageFaultNotification *fn, uval copyOnWrite)
{
    callerPg->paddr = callerPg->fileOffset + imageOffset;
    return FRAMETOCOPY;
}


// attach to the FCM to map (possibly) part of it
SysStatus
FCMStartup::attachRegion(RegionRef regRef, PMRef pmRef,
			 AccessMode::mode accessMode)
{
    // FIXME implement this so we can correctly do destroy
    (void) regRef;
    (void) pmRef;
    (void) accessMode;
    return 0;
}

SysStatus
FCMStartup::detachRegion(RegionRef regRef)
{
    (void) regRef;
    return 0;
}

SysStatus
FCMStartup::destroy()
{
    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if(_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    //There is only one FCMStartup - behind TheFCMStartupRef in GOBJK
    //so panic if destroy is called
    passertMsg(0,"destroy for FCMStartup NYI\n");
    return 0;
}

SysStatus
FCMStartup::printStatus(uval kind)
{
    switch(kind) {
    case PendingFaults:

	break;
    default:
	err_printf("kind %ld not known in printStatus\n", kind);
    }
    return 0;
}

