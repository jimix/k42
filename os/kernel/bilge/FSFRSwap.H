#ifndef __FSFR_SWAP_H_
#define __FSFR_SWAP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSFRSwap.H,v 1.5 2004/10/29 16:30:21 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides trivially ram-backed swap file system
 *
 * See FRSwap.H for swap strategy
 *
 * **************************************************************************/

#include <misc/ListSimpleLocked.H>
#include "mem/FRComputation.H"
#include "bilge/FSSwap.H"
typedef class FSFRSwap** FSFRSwapRef;

class FSFRSwap : public FSSwap __xbase(Obj)
{
    FRRef        frRef;
    uval nextPage;			// starts at 0

    uval numSFill;
    uval numPut;
    uval numSPut;
    uval numComp;
    uval numFreed;

    static FSFRSwap *obj;

    void init();
#ifndef _IN_STUBGEN
    BLock lock;				// just to protect allocation blockids
    // a list of blocks that were freed
    ListSimple<uval, AllocPinnedGlobal> freeBlocks;

    // state for asynchronous requests,
    class Request {
    public:
	FRComputationRef ref;
	uval offset;
	Request(FRComputationRef r, uval o) { ref = r; offset = o; }
	DEFINE_PINNEDGLOBALPADDED_NEW(Request);
    };

    // a list of pending io operations, key is offset in swap file
    ListSimpleKey<uval, Request *, AllocPinnedGlobal> requests;

    DEFINE_REFS(FSFRSwap);

    DEFINE_PINNEDGLOBALPADDED_NEW(FSFRSwap);
    friend class FCMToSwap;		// operations from fake swap FCM, on obj
#endif
protected:
    // just called within class on processor
    uval allocOffset();

    SysStatus ioComplete(uval offset, SysStatus rc);


public:
    virtual SysStatus printStats();

    static SysStatus ClassInit(VPNum vp);

    virtual SysStatus startFillPage(uval physAddr, FRComputationRef ref,
				    uval offset, uval blockID,
				    PagerContext context);

    virtual SysStatus startPutPage(uval physAddr, FRComputationRef ref,
				   uval offset, uval& blockID,
				   PagerContext context, 
				   IORestartRequests *rr);

    virtual SysStatus freePage(uval blockID, PagerContext context);

    // returns _SUCCESS if swap is active
    virtual SysStatus swapActive();
__xprotected:
    static SysStatus _SetFR(__in ObjectHandle frOH, __CALLER_PID caller);
};

#endif /* #ifndef __FSFR_SWAP_H_ */
