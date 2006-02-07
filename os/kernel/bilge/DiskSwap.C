/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskSwap.C,v 1.9 2005/04/15 17:39:36 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides implementation of disk swap devices.
 * **************************************************************************/

#include "kernIncs.H"
#include "DiskSwap.H"
#include <misc/AtomicBitVec64.H>
#include <stub/StubBlockDev.H>
#include <cobj/XHandleTrans.H>
#include <meta/MetaBlockDev.H>
#include <misc/BlockSet.H>
#include <trace/trace.H>
#include <trace/traceSwap.h>


struct DiskSwap::DeviceDesc{
    BlockDevRef bd;
    BlockSet *usage;
    DeviceDesc(BlockDevRef ref, uval size);
    DEFINE_PINNEDGLOBAL_NEW(DeviceDesc);
};

DiskSwap::DeviceDesc::DeviceDesc(BlockDevRef bdr, uval bytes) {
    bd = bdr;
    usage = new BlockSet(bytes/SWAP_BLOCKSIZE);
}

DiskSwapRef DiskSwap::swapper = NULL;
/* static */ SysStatus
DiskSwap::ClassInit(VPNum vp)
{
    SysStatus rc;
    if (vp) return 0;
    MetaDiskSwap::init();

    DiskSwap* ds = new DiskSwap;
    rc = ds->init();
    if (_FAILURE(rc)) {
	delete ds;
    }
    return rc;
}


/* virtual */ SysStatus
DiskSwap::init()
{
    nextDevID = 0;
    numPagesAvail = 0;
    currentDev = NULL;
    CObjRootSingleRep::Create(this, (RepRef)GOBJK(TheFSSwapRef));
    swapper = getRef();
    return 0;
}


/* static */  SysStatusUval
DiskSwap::_attachDevice(ObjectHandle oh, uval bytes) {
    return DREF(swapper)->attachDevice(oh, bytes);
}

// Registers a block-device for paging, returns disk id
/* virtual */ SysStatusUval
DiskSwap::attachDevice(ObjectHandle oh, uval bytes) {
    //We get an oh to a BlockDev interface, want to use the ref instead
    ObjRef ref;
    TypeID type;
    SysStatus rc = XHandleTrans::XHToInternal(oh.xhandle(), 0, 0, ref, type);

    _IF_FAILURE_RET(rc);

    if (!MetaBlockDev::isBaseOf(type)) {
	tassertWrn(0, "object handle <%lx,%lx> not of correct type\n",
		   oh.commID(), oh.xhandle());
	return _SERROR(2491, 0, EINVAL);

    }

    BlockDevRef bd = (BlockDevRef)ref;
    DeviceDesc *dd = new DeviceDesc(bd, bytes);
    uval devID = FetchAndAdd(&nextDevID,1);
    devList.add(devID, (uval)dd);
    AtomicAdd(&numPagesAvail,bytes/PAGE_SIZE);
    return 0;
}

/* virtual */ SysStatusUval
DiskSwap::allocBlock(BlockDevRef &bdref, PagerContext context)
{
    //Note that we have to grab the devList lock, so we might
    // as well hold it during allocation to protect activeMap,
    // if it needs to be switched
    uval devID = ~0ULL;
    uval tmp;
    uval block = ~0ULL;
    BlockID bid;
    uval newBlockSet;
    AtomicBitVec64 *bv;
    DeviceDesc* dd;

    devList.acquireLock();
    if (*context!=INIT_PAGER_CONTEXT) {
	devID = GET_SWAP_DEVID(*context);
	uval id;
	void * searchDev = NULL;
	while ((searchDev = devList.next(searchDev, id, tmp))) {
	    if (id == devID) {
		break;
	    }
	}
	if (searchDev && id == devID) {
	    DeviceDesc * dd2 = (DeviceDesc*)tmp;
	    AtomicBitVec64* bv2 =
		    &dd2->usage->L1[GET_SWAP_BLOCKNUM(*context)/64];
	    block = bv2->setFindFirstUnSet();

	    if (block!=64) {
		bdref = dd2->bd;
		block += (*context & ~63ULL);
		goto end;
	    }
	}
    }

    //keep currentDev place holder between searches, so we round-robbin
    //among devices
    do {
	currentDev = devList.next(currentDev, devID, tmp);
    } while (!currentDev);

    dd = (DeviceDesc*)tmp;
    dd->usage->getBlockSet(newBlockSet, bv);
    block = bv->setFindFirstUnSet();
    tassertMsg(block<64,"New blockset is full\n");
    block += newBlockSet * 64;
    bdref = dd->bd;

  end:
    SET_SWAP_DEVID(bid, devID);
    SET_SWAP_BLOCKNUM(bid, block);
    *context = bid;
    AtomicAdd(&numPagesAvail,(uval)-1);
    devList.releaseLock();
    return bid;

}

struct SwapIOCompletion : public BlockDev::IOCompletion{
    uval offset;
    BlockID blockID;
    FRComputationRef ref;
    DiskSwapRef dsr;
    DEFINE_PINNEDGLOBAL_NEW(SwapIOCompletion);
    SwapIOCompletion(uval off, BlockID bid, FRComputationRef frcRef,
		     DiskSwapRef _dsr)
	:offset(off),blockID(bid), ref(frcRef), dsr(_dsr) {};
    virtual void complete(BlockDevRef bdRef, SysStatus err) {
	TraceOSSwapIOComplete(offset, (uval64)ref, err);

	if (_FAILURE(err) && blockID != INVALID_BLOCKID) {
	    uval fakeContext = INIT_PAGER_CONTEXT;

	    DREF(dsr)->freePage(blockID,&fakeContext);

	    blockID = INVALID_BLOCKID;
	}

	DREF(ref)->ioComplete(offset, err);
	delete this;
    }
    virtual ~SwapIOCompletion() {}
};

// allocates a new block unless an existing block number is provided
// blockID of uval(-1) requests new allocation
/* virtual */ SysStatus
DiskSwap::putPage(uval physAddr, uval& blockID, PagerContext context)
{
    SysStatusUval rc;
    BlockDevRef bd;
//    AutoLock<LockType> al(&lock);
    if (blockID == ~0ULL) {
	//Allocate a new block
	rc = allocBlock(bd, context);
	_IF_FAILURE_RET(rc);
	blockID = _SGETUVAL(rc);
    } else {
	// Need to get the block dev ref
	uval devID = GET_SWAP_DEVID(blockID);
	uval tmp;
	devList.find(devID,tmp);
	DeviceDesc * dd = (DeviceDesc*)tmp;
	bd = dd->bd;
    }

    TraceOSSwapPut(physAddr, blockID);

    uval blockNum = GET_SWAP_BLOCKNUM(blockID);
    rc = DREF(bd)->putBlock(physAddr, SWAP_BLOCKSIZE,
			    blockNum * SWAP_BLOCKSIZE);
    if (_FAILURE(rc)) {
	freePage(blockID, context);
	blockID = ~0ULL;
    }
    return rc;
}

/* virtual */ SysStatus
DiskSwap::startPutPage(uval physAddr, FRComputationRef ref,
		       uval offset, uval& blockID, PagerContext context,
		       IORestartRequests *rr)
{
    // for now, assume we can't run out of resources on disk swap,
    // eventually we will ahve to limit, then start using IORestartRequests
    SysStatus rc = 0;
    int ret;
    BlockDevRef bd;
//    AutoLock<LockType> al(&lock);
    uval vmrAddr;
    if (blockID == ~0ULL) {
	//Allocate a new block
	rc = allocBlock(bd, context);
	_IF_FAILURE_RET(rc);
	blockID = _SGETUVAL(rc);
    } else {
	// Need to get the block dev ref
	uval devID = GET_SWAP_DEVID(blockID);
	uval tmp;
	devList.find(devID,tmp);
	DeviceDesc * dd = (DeviceDesc*)tmp;
	bd = dd->bd;
    }
    uval blockNum = GET_SWAP_BLOCKNUM(blockID);

    SwapIOCompletion *sic = new SwapIOCompletion(offset, blockID,
						 ref, getRef());

    TraceOSSwapPutAsync(offset, (uval64)ref, blockID);

    vmrAddr = PageAllocatorKernPinned::realToVirt(physAddr);

    ret = DREF(bd)->asyncOp(1, vmrAddr, SWAP_BLOCKSIZE,
 			   blockNum * SWAP_BLOCKSIZE, sic);
    tassertMsg(ret == 0,"async op failed: %d\n",ret);
    if (ret<0) {
	rc = _SERROR(2492, 0, -ret);
    }
    return rc;
}

/* virtual */ SysStatus
DiskSwap::startFillPage(uval physAddr, FRComputationRef ref,
			uval offset, uval blockID, PagerContext context) {
    // Need to get the block dev ref
    SysStatus rc = 0;
    int ret;
//    AutoLock<LockType> al(&lock);
    uval vmrAddr;
    uval devID = GET_SWAP_DEVID(blockID);
    uval tmp;
    devList.find(devID,tmp);
    DeviceDesc * dd = (DeviceDesc*)tmp;
    BlockDevRef bd = dd->bd;

    TraceOSSwapFillAsync(offset, (uval64)ref, blockID);

    uval blockNum = GET_SWAP_BLOCKNUM(blockID);
    SwapIOCompletion *sic = new SwapIOCompletion(offset, (BlockID)~0ULL,
						 ref, getRef());

    vmrAddr = PageAllocatorKernPinned::realToVirt(physAddr);
    ret = DREF(bd)->asyncOp(0, vmrAddr, SWAP_BLOCKSIZE,
			    blockNum * SWAP_BLOCKSIZE, sic);

    tassertMsg(ret == 0,"async op failed: %d\n",ret);
    if (ret<0) {
	rc = _SERROR(2296, 0, -ret);
    }
    return rc;
}


/* virtual */ SysStatus
DiskSwap::freePage(uval blockID, PagerContext context) {
    uval devID = GET_SWAP_DEVID(blockID);
    uval block = GET_SWAP_BLOCKNUM(blockID);

    //Do not use blockID from here on in --- it contains block & device number
    //Once we have the device identified we want to use block number only,
    //---> use block, not blockID
    (void) blockID;

    uval tmp;
    void * searchDev = NULL;
    devList.acquireLock();
    uval id;
    while ((searchDev = devList.next(searchDev, id, tmp))) {
	if (id == devID) {
	    break;
	}
    }
    if (searchDev == NULL) {
	devList.releaseLock();
	return _SERROR(2272, 0, ENODEV);
    }

    DeviceDesc* dd = (DeviceDesc*)tmp;
    AtomicBitVec64 *bv = &dd->usage->L1[block/64];

    bv->clearBit(block %64);

    AtomicAdd(&numPagesAvail,1);

    //If bitmap is all clear, set all bits to prevent any allocations
    // from it.  In the meantime, we try to let go of this block set.

    if (bv->flipAllIfZero()) {
	//We're freeing the block, so erase the context so that
	//nobody tries to use it anymore
	if (block/64 == (*context)/64) {
	    *context = INIT_PAGER_CONTEXT;
	}
	//putBlockSet will clear all the bits
	dd->usage->putBlockSet(bv,1);
//	err_printf("freePage: %lx %lx\n", block, block/64);
    } else {
//	err_printf("freePage: %lx (%lx)\n", block,
//		   (uval)dd->usage->L1[block/64].getBits());
    }
    devList.releaseLock();
    return 0;
}

/* virtual */ SysStatus
DiskSwap::swapActive()
{
    return numPagesAvail;
}

/* virtual */ SysStatus
DiskSwap::printStats()
{
    return 0;
}
