/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FRComputation.C,v 1.61 2004/12/20 20:47:37 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Application interface to file, have one
 * per-open instance of file.
 * **************************************************************************/
#include "kernIncs.H"
#include "mem/FCMComputation.H"
#include "mem/FRComputation.H"
#include "mem/FCMPrimitiveKernel.H"
#include "mem/FRPlaceHolder.H"
#include "mem/PMRoot.H"
#include "bilge/FSSwap.H"
#include "meta/MetaFRComputation.H"
#include "stub/StubFRComputation.H"
#include <cobj/CObjRootSingleRep.H>
#include "defines/paging.H"
#include "defines/experimental.H"

#include "mem/FCMPrimitive.H"

/* virtual */ SysStatus
FRComputation::init(uval nnode)
{
    FRCommon::init();
    CObjRootSingleRep::Create(this);
    pageable = DREFGOBJK(TheFSSwapRef)->swapActive();
    numanode = nnode;
    pagerContext = INIT_PAGER_CONTEXT;
    return 0;
}

/* virtual */ SysStatus
FRComputation::getType(TypeID &id)
{
    id = StubFRComputation::typeID();
    return 0;
}

/* static */ SysStatus
FRComputation::InternalCreate(ObjectHandle &frOH, ProcessID caller,
			      uval pgSize, uval numanode)
{
    SysStatus rc;
    FRComputation *frcomp = new FRComputation;

    if (frcomp == NULL) {
	return -1;
    }

    // assume init doesn't need correct page size
    frcomp->init(numanode);

    frcomp->pageSize = pgSize;

    rc = frcomp->giveAccessByServer(frOH, caller);
    if (_FAILURE(rc)) {
	// if we can't return an object handle with object can never
	// be referenced.  Most likely cause is that caller has terminated
	// while this create was happening.
	frcomp->destroy();
    }
    return rc;
}

/* static */ SysStatus
FRComputation::Create(FRRef& frRef, uval pgSize)
{
    FRComputation *frcomp = new FRComputation;

    if (frcomp == NULL) {
	return -1;
    }

    // assume init doesn't need correct page size
    frcomp->init(PageAllocator::LOCAL_NUMANODE);

    frcomp->pageSize = pgSize;
    
    frRef = (FRRef)(frcomp->getRef());

    return 0;
}

/* static */ SysStatus
FRComputation::_Create(ObjectHandle &frOH, __CALLER_PID caller)
{
    return (InternalCreate(frOH, caller, PAGE_SIZE,
			   PageAllocator::LOCAL_NUMANODE));
}

/* static */ SysStatus
FRComputation::_CreateLargePage(ObjectHandle &frOH, uval pgSize,
				__CALLER_PID caller)
{
    if (0 == exceptionLocal.pageTable.getLogPageSize(pgSize))
	return _SERROR(2870, 0, EINVAL);
#ifdef LARGE_PAGES_FIXED_POOLS
    /* for now, we fail here if there are no large pages reserved
     * and large page reservations are in force
     */
    uval largePgSize, largePgReservCount, largePgFreeCount;
    ((PMRoot*)(DREFGOBJK(ThePMRootRef)))->getLargePageInfo(
	largePgSize, largePgReservCount, largePgFreeCount);
    if(largePgReservCount == 0)
	return _SERROR(2868, 0, ENOMEM);
    if(largePgSize != pgSize)
	return _SERROR(2869, 0, EINVAL);
#endif
    return (InternalCreate(frOH, caller, pgSize,
			   PageAllocator::LOCAL_NUMANODE));
}

/* static */ SysStatus
FRComputation::_CreateFixedNumaNode(ObjectHandle &frOH, uval numanode,
				    __CALLER_PID caller)
{
    if (numanode == PageAllocator::LOCAL_NUMANODE) {
	uval d1;
	DREFGOBJ(ThePageAllocatorRef)->getNumaInfo(
	    Scheduler::GetVP(), numanode, d1);
    }
    return (InternalCreate(frOH, caller, PAGE_SIZE, numanode));
}

SysStatus
FRComputation::_giveAccess(__out ObjectHandle &oh,
			   __in ProcessID toProcID)
{
    return (giveAccessByServer(oh, toProcID));
}

SysStatus
FRComputation::locked_getFCM(FCMRef &r)
{
    _ASSERT_HELD(lock);

    SysStatus rc;

    if (beingDestroyed) {
	r = FCMRef(TheBPRef);
	return -1;
    }

    if (fcmRef != 0) {
	r = fcmRef;
	return 0;
    }

    // allocate a new fcm
    if (numanode == PageAllocator::LOCAL_NUMANODE) {
	rc = FCMComputation::Create(
	    fcmRef, (FRRef)getRef(), pageSize, pageable);
    } else {
	rc = FCMComputation::CreateFixedNumaNode(
	    fcmRef, (FRRef)getRef(), pageSize, pageable, numanode);
    }
    tassert(_SUCCESS(rc), err_printf("allocation of fcm failed\n"));
    r = fcmRef;
    return rc;
}


/*
 * this code depends on the FCM page lock logic to prevent
 * more than one operation on the same offset at the same time.
 * so, for example, we can safely release the lock after NOT finding
 * an entry, and add the entry later knowing its not been added in
 * the interum
 */
SysStatus
FRComputation::putPageInternal(uval physAddr, uval offset, uval async,
			       IORestartRequests *rr)
{
    uval blockID, oldBlockID;
    SysStatus rc;

    if (pageable) {
	lock.acquire();

	//returns -1 if not found
	if (blockHash.find(offset, blockID)) {
	    oldBlockID = blockID;
	} else {
	    blockID = oldBlockID = uval(-1);
	}

	lock.release();

	//allocates block if blockID is -1, otherwise uses blockID
	if (async) {
	    rc = DREFGOBJK(TheFSSwapRef)->
		startPutPage(physAddr, FRComputationRef(getRef()), offset,
			     blockID, &pagerContext, rr);
	} else {
	    passertMsg(0, "Not allowing synchronous call from FR to FS\n");
	    rc = _SERROR(2849, 0, ENOSYS);
	}

//	err_printf("putPageInternal %lx %lx %lx\n", this, offset, blockID);

	if (_FAILURE(rc)) {
	    // expecting would block error
	    tassertMsg((_SCLSCD(rc) == FR::WOULDBLOCK), "woops\n");
	    return rc;
	}

	if (oldBlockID == uval(-1)) {
	    //not found above, so add to hash table
	    lock.acquire();

	    blockHash.add(offset, blockID);

	    lock.release();
	}

	return 0;

    } else {
	tassert(0, err_printf("putPage on non-pageable fr\n"));
	return -1;
    }
}

SysStatus
FRComputation::putPage(uval physAddr, uval offset)
{
    return putPageInternal(physAddr, offset, 0, NULL);
}

SysStatus
FRComputation::startPutPage(uval physAddr, uval offset, IORestartRequests *rr)
{
    return putPageInternal(physAddr, offset, 1, rr);
}


SysStatus
FRComputation::exportedXObjectListEmpty()
{
    FCMRef tempfcmRef;
    if (beingDestroyed) return 0;

    // if isNotInUse returns error or true, destroy
    tempfcmRef = fcmRef;
    if (tempfcmRef == NULL ||(DREF(tempfcmRef)->isNotInUse())) {
	return destroy();
    }

    // when region list goes empty we will destroy
    return 0;
}



SysStatus
FRComputation::destroy()
{
    uval alreadyDestroyed = SwapVolatile(&beingDestroyed, 1);
    FCMRef tempfcmRef;

    if (alreadyDestroyed) return 0;

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    tempfcmRef = fcmRef;

    // first destroy FCM, which will free pages
    // we don't use locking here - but its safe because
    // any uses of fcmRef which beat this will get destroyed object
    // returns.

    fcmRef = 0;
    if (tempfcmRef) DREF(tempfcmRef)->destroy();

    // clean up swap pages
    uval restart = 0;
    uval blockID;
    uval offset;
    lock.acquire();
    while (blockHash.removeNext(offset, blockID, restart)) {
	DREFGOBJK(TheFSSwapRef)->freePage(blockID, &pagerContext);
    }

    blockHash.destroy();

    // schedule the object for deletion
    destroyUnchecked();

    return 0;
}

void
FRComputation::ClassInit(VPNum vp)
{
    if (vp!=0) return;
    MetaFRComputation::init();
}

/* virtual */ SysStatusUval
FRComputation::startFillPage(uval physAddr, uval offset)
{
    uval blockID;
    SysStatus rc;

    if (pageable) {
	lock.acquire();

	//returns -1 if not found
	if (!blockHash.find(offset, blockID)) {
	    blockID = uval(-1);
	}

	lock.release();

	if (blockID != uval(-1)) {
	    rc = DREFGOBJK(TheFSSwapRef)->
		startFillPage(physAddr, FRComputationRef(getRef()),
			      offset, blockID, &pagerContext);

	    passert(_SUCCESS(rc), err_printf("startFillPage failed\n"));
//	    err_printf("startFillPage %lx %lx %lx\n",
//		       this, offset, blockID);
	    return rc;
	}
//	err_printf("startFillPage %lx %lx Page_NOT_FOUND\n",
//		   this, offset);
    }

    return FR::PAGE_NOT_FOUND;
}

SysStatus
FRComputation::checkOffset(uval fileOffset)
{
    uval blockID;

    if (pageable) {
	lock.acquire();

	//returns -1 if not found
	if (!blockHash.find(fileOffset, blockID)) {
	    blockID = uval(-1);
	}


	lock.release();
	return blockID==uval(-1)?1:0;
    } else {
	return 1;
    }
}

/*
 * used for example in fork to move a disk block from
 * on fr to another
 * frees old block if any, sets new if any
 */
SysStatus
FRComputation::setOffset(uval fileOffset, uval blockID)
{
    SysStatus rc;
    lock.acquire();
    uval oldBlockID;
    if (blockHash.remove(fileOffset, oldBlockID)) {
	rc = DREFGOBJK(TheFSSwapRef)->freePage(oldBlockID, &pagerContext);
    }

    if (blockID != uval(-1)) {
	blockHash.add(fileOffset, blockID);
    }

    lock.release();
    return 0;
}

SysStatus
FRComputation::freeOffset(uval fileOffset)
{
    uval blockID, foundBlock;
    lock.acquire();

    foundBlock = blockHash.remove(fileOffset, blockID);

    lock.release();
    if (foundBlock) {
	DREFGOBJK(TheFSSwapRef)->freePage(blockID, &pagerContext);
    }
    return 0;
}

SysStatus
FRComputation::getBlockID(uval fileOffset, uval& blockID)
{
    lock.acquire();

    if (!blockHash.remove(fileOffset, blockID)) {
	blockID = uval(-1);
    }

    lock.release();
    return 0;
}


SysStatus
FRComputation::ioComplete(uval fileOffset, SysStatus rc)
{
    // copy the ref since we don't hold locks.
    FCMRef copyFcmRef = fcmRef;
    if (copyFcmRef == NULL) return 0;
    return DREF(copyFcmRef)->ioComplete(fileOffset, rc);
}

/* virtual */ SysStatus
FRComputation::_fsync()
{
    /* fsync is a nop for computational storage */
    return 0;
}

/* used by fork to transfer fr contents to new parent call is made on
 * old parent, passing ref of new parent assume caller has made sure
 * no IO is in progress call is made after frames (and there blocks)
 * have been transfered.
 */
/* virtual */ SysStatus
FRComputation::forkCopy(FCMComputationRef parentFCM)
{
    lock.acquire();
    uval offset, blockid, restart;
    restart = 0;
    while (blockHash.removeNext(offset, blockid, restart)) {
	DREF(parentFCM)->newBlockID(offset, blockid);
    }

    lock.release();
    return 0;
}
