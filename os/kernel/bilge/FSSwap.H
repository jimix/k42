#ifndef __FSSWAP_H_
#define __FSSWAP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FSSwap.H,v 1.10 2004/10/29 16:30:21 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides interface to swap file system
 * **************************************************************************/

#include "mem/FRComputation.H"

#define INIT_PAGER_CONTEXT (~0ULL)
typedef volatile uval* PagerContext;

class FSSwap : public Obj {
public:
    static SysStatus ClassInit(VPNum vp);

    // reads or writes a page from/to a paging block
    // only handles PAGE_SIZE requests
    // calls DREF(ref)->fillPageComplete(offset) when io is done
    virtual SysStatus startFillPage(uval physAddr, FRComputationRef ref,
				    uval offset, uval blockID,
				    PagerContext context)=0;

    // calls DREF(ref)->putPageComplete(offset) when io is done
    virtual SysStatus startPutPage(uval physAddr, FRComputationRef ref,
				   uval offset, uval& blockID,
				   PagerContext context, 
				   IORestartRequests *rr)=0;

    virtual SysStatus freePage(uval blockID, PagerContext context)=0;

    // returns _SUCCESS if swap is active
    virtual SysStatus swapActive()=0;

    virtual SysStatus printStats()=0;

};

#endif /* #ifndef __FSSWAP_H_ */
