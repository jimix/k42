#ifndef __FSRAM_SWAP_H_
#define __FSRAM_SWAP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSRamSwap.H,v 1.14 2004/10/29 16:30:21 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides trivially ram-backed swap file system
 *
 * See FRSwap.H for swap strategy
 *
 * Normally, the blockID is from a dense set of numbers which the swapper
 * maps to paging devices.  But in the scafold implementation, they
 * are just the addresses of pinned pages used to hold the data.
 *
 * **************************************************************************/

#include <misc/ListSimpleLocked.H>
#include "mem/FRComputation.H"
#include "bilge/FSSwap.H"

class IORestartRequests;
typedef class FSRamSwap** FSRamSwapRef;

class FSRamSwap : public FSSwap {

    uval          swapRunning;
    struct chain { chain* next; };
    PtrBLock<chain> freeListLock;
    uval          numFree;

    // state for asynchronous requests,
    class Request {
    public:
	enum ReqType { READ, WRITE };
	FRComputationRef ref;
	uval offset;
	uval paddr;
	uval blockID;
	ReqType reqType;
	Request(uval p, FRComputationRef r, uval o, uval b, ReqType req) {
	    paddr = p; ref = r; offset = o; blockID = b; reqType = req;
	}
	DEFINE_GLOBAL_NEW(Request);
    };
    ThreadID asyncID;

    // specify the pages of memory for computing
    // rest is ramdisk for paging - 0 for no paging
    void init(uval pages);
    void doAsyncOps();			// internal function
    static void DoAsyncOps(uval);	// perpetual function

    // queue of pending requests,
    // locked separately from object
    ListSimpleLocked<Request *, AllocGlobal> requests;


    DEFINE_REFS(FSRamSwap)

    DEFINE_GLOBALPADDED_NEW(FSRamSwap);

    SysStatusUval doFillPage(uval physAddr, uval blockID);

    SysStatusUval doPutPage(uval physAddr, uval blockID);

public:
    virtual SysStatus printStats() { return 0; }

    // if swap is 0, swapping is turned off completely
    static SysStatus ClassInit(VPNum vp, uval swap);

    virtual SysStatus startFillPage(uval physAddr, FRComputationRef ref,
				    uval offset, uval blockID,
				    PagerContext context);

    virtual SysStatus putPage(uval physAddr, uval& blockID,
			      PagerContext context);

    virtual SysStatus startPutPage(uval physAddr, FRComputationRef ref,
				   uval offset, uval& blockID,
				   PagerContext context, 
				   IORestartRequests *rr);

    virtual SysStatus freePage(uval blockID, PagerContext context);

    // returns _SUCCESS if swap is active
    virtual SysStatus swapActive();

};

#endif /* #ifndef __FSRAM_SWAP_H_ */
