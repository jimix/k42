/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFRSwap.C,v 1.15 2004/10/29 16:30:21 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides trivially ram-backed swap file system
 * **************************************************************************/

#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include <scheduler/Scheduler.H>
#include "mem/PageAllocatorKernPinned.H"
#include "bilge/FSFRSwap.H"
#include "mem/FR.H"
#include "mem/PageCopy.H"
#include "meta/MetaFSFRSwap.H"
#include <cobj/XHandleTrans.H>

class FCMToSwap : public FCM {
public:
    FSFRSwap *swapFS;

    DEFINE_PINNEDGLOBALPADDED_NEW(FCMToSwap);
    static SysStatus Init(FRRef frRef, FSFRSwap *sFS) {
	FCMToSwap* fcm;
	FCMRef fcmRef;
	fcm = new FCMToSwap;
	fcmRef = (FCMRef)CObjRootSingleRepPinned::Create(fcm);
	DREF(frRef)->installFCM(fcmRef);
	fcm->swapFS = sFS;
	return 0;
    }
    virtual SysStatus ioComplete(uval offset, SysStatus rc) {
	return swapFS->ioComplete(offset, rc);
    }
    virtual SysStatus destroy() {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn) {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0) {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatusUval mapPage(uval fileOffset,
				  uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr,
				  AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn) {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode) {
	tassertMsg(0, "woops\n");
	return 0;
    }

    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   HATRef hatRef, AccessMode::mode accessMode,
				   uval vaddr, uval size) {
	tassertMsg(0, "woops\n");
	return 0;
    }

    virtual SysStatus attachFR(FRRef frRef) {
	// null function in this case, called when fr attaches back
	return 0;
    }

    virtual SysStatus getFRRef(FRRef& frRefArg) {
	passertMsg(0, "should not be called\n");
	return 0;
    }


    virtual SysStatus detachRegion(RegionRef regRef) {
	tassertMsg(0, "woops\n");
	return 0;
    }

    virtual SysStatus fsync() {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatusUval isNotInUse() {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatusUval isEmpty() {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatus discardCachedPages() {
	tassertMsg(0, "woops\n");
	return 0;
    }
    virtual SysStatus sharedSegments() {
	return 0;
    }
    virtual SysStatus printStatus(uval kind) {
	tassertMsg(0, "woops\n");
	return 0;
    }
};



FSFRSwap *FSFRSwap::obj;

/* static */ SysStatus
FSFRSwap::ClassInit(VPNum vp)
{
    SysStatus rc=0;

    if (vp != 0) return 0;

    obj = new FSFRSwap();
    obj->init();

    CObjRootSingleRep::Create(obj, (RepRef)GOBJK(TheFSSwapRef));
    MetaFSFRSwap::init();

    return rc;
}

void
FSFRSwap::init()
{
    nextPage = 0;
    frRef = 0;
    numSFill = numPut = numSPut = numComp = numFreed = 0;
}


SysStatus
FSFRSwap::ioComplete(uval offset, SysStatus rc)
{
    FetchAndAdd(&numComp, 1);
    Request *req;
    if ( requests.remove(offset, req) ) {
	// err_printf("FSFRSwap:ioComplete ref %p fileof %lx, swapoff %lx\n",
	// 	   req->ref, req->offset, offset);
	DREF(req->ref)->ioComplete(req->offset, rc);
	delete req;
	return 0;
    }
    tassertMsg(0, "completion for a request not on the list offset %ld\n",
	       offset);
    return 0;
}

/* virtual */ SysStatus
FSFRSwap::startFillPage(uval physAddr, FRComputationRef ref, uval offset,
			uval blockID, PagerContext context)
{
    SysStatus rc;
    tassertMsg(frRef, "woops\n");
    tassertMsg((blockID != uval(-1)), "request to fill undefined page\n");
    Request *req = new Request(ref, offset);
    requests.add(blockID, req);
    // err_printf("startfillpage, ref %p fileof %lx, swapoff %lx\n",
    //             ref, offset, blockID);
    FetchAndAdd(&numSFill, 1);
    //if ((numSFill % 100) == 0) err_printf("R");
    rc =  DREF(frRef)->startFillPage(physAddr, blockID);
    passertMsg(_SUCCESS(rc), "error on fill page to swap file\n");
    passertMsg((rc != FR::PAGE_NOT_FOUND), "page not found swap file?\n");
    return rc;
}

uval
FSFRSwap::allocOffset()
{
    uval blk;
    lock.acquire();
    // see if something on free list
    if (freeBlocks.removeHead(blk)==0) {
	blk = nextPage++;
	DREF(frRef)->setFileLength(nextPage*PAGE_SIZE);
	blk = blk*PAGE_SIZE;
    }
    lock.release();
    return blk;
}

/* virtual */ SysStatus
FSFRSwap::printStats()
{
    err_printf("numSFill %ld, numPut %ld, numSPut %ld, numComp %ld,"
	       " numFreed %ld\n",
	       numSFill, numPut, numSPut, numComp, numFreed);
    return 0;
}

/* virtual */ SysStatus
FSFRSwap::startPutPage(uval physAddr, FRComputationRef ref, uval offset,
		       uval& blockID, PagerContext context, 
		       IORestartRequests *rr)
{
    uval allocatedBlock = 0;
    SysStatus rc;
    tassertMsg(frRef, "woops, paging should not be enabled yet\n");
    if (blockID==uval(-1)) {
	allocatedBlock = blockID = allocOffset();
    }
    Request *req = new Request(ref, offset);
    requests.add(blockID, req);
    // err_printf("FSFRSwap:startputpage, ref %p fileof %lx, swapoff %lx\n",
    // 	       ref, offset, blockID);
    FetchAndAdd(&numSPut, 1);
    // if ((numSPut % 100) == 0) err_printf("W");
    rc = DREF(frRef)->startPutPage(physAddr, blockID, rr);
    if (_FAILURE(rc)) {
	// undo allocate of page, don't really need to do this, but
	// fits general strategy of undoing anything allocated on 
	// path where re-trying operation
	if (allocatedBlock) {
	    freePage(allocatedBlock, context);
	    blockID = uval(-1);
	}
	requests.remove(blockID, req);
	delete req;
    }
    return rc;
}

SysStatus
FSFRSwap::freePage(uval blockID, PagerContext context)
{
    // err_printf("FSFRSwap:freePage swapoff %lx\n", blockID);
    FetchAndAdd(&numFreed, 1);
    freeBlocks.add(blockID);
    return 0;
}

/* virtual */ SysStatus
FSFRSwap::swapActive()
{
    if (frRef != NULL) {
	return 1;
    }
    return 0;				// not active unless fr available
}

/* static */ SysStatus
FSFRSwap::_SetFR(__in ObjectHandle frOH, __CALLER_PID caller)
{
    ObjRef objRef;
    SysStatus rc;
    TypeID type;

    if (obj->frRef != NULL) {
	tassertWrn(0, "attempt to change swap FR ignored\n");
	return _SERROR(2235, 0, EEXIST);
    }
    // if this is an FR, allow tihs operation, i.e., no other checking
    // this is set once at boot time for now
    rc = XHandleTrans::XHToInternal(frOH.xhandle(), 0, 0, objRef, type);
    if (!_SUCCESS(rc)) {
	tassertWrn(_SUCCESS(rc), "FSFRSwap failed XHToInternal\n");
	return _SERROR(2486, 0, EINVAL);
    }

    // verify that type is of FR
    if (!MetaFR::isBaseOf(type)) {
	tassertWrn(0, "object handle <%lx,%lx> not of correct type\n",
		   frOH.commID(), frOH.xhandle());
	return _SERROR(2487, 0, EINVAL);
    }
    obj->frRef = (FRRef)objRef;
    FCMToSwap::Init(obj->frRef, obj);
    err_printf("initialized swap file system to FR\n");
    return 0;
}
