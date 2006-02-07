#ifndef __DISKSWAP_H_
#define __DISKSWAP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DiskSwap.H,v 1.5 2004/10/29 16:30:20 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides interface to swap devices.
 * **************************************************************************/

#include <sys/sysTypes.H>
#include <cobj/BaseObj.H>
#include <bilge/FSSwap.H>
#include <io/BlockDev.H>
#include <misc/ListSimpleKeyLocked.H>

#ifndef _IN_STUBGEN
#include <meta/MetaDiskSwap.H>


// A "BlockID" represents a block on backing-storage.  The BlockID
// encapsulates the identifier of the actual device and the block on
// the device.  This allows the DiskSwap object to determine how
// paging is done over multiple block devices.

typedef uval BlockID;
#define INVALID_BLOCKID ((BlockID)~0ULL)
#define SWAP_BLOCKSIZE_BITS 12ULL
#define SWAP_BLOCKSIZE (1<<SWAP_BLOCKSIZE_BITS)
#define SWAP_DEVID_SHIFT (64ULL-SWAP_BLOCKSIZE_BITS)
#define SWAP_BLOCKNUM_MASK ((1ULL<<SWAP_DEVID_SHIFT)-1)
#define SWAP_DEVID_MASK (~SWAP_BLOCKNUM_MASK)
inline uval GET_SWAP_DEVID(uval blockID) {
    return blockID>>SWAP_DEVID_SHIFT;
}
inline uval GET_SWAP_BLOCKNUM(uval blockID) {
    return blockID & SWAP_BLOCKNUM_MASK;
}
inline void SET_SWAP_DEVID(BlockID &blockID, uval val) {
    blockID=(val<<SWAP_DEVID_SHIFT)|GET_SWAP_BLOCKNUM(blockID);
}
inline void SET_SWAP_BLOCKNUM(BlockID &blockID, uval val) {
    blockID = (blockID & ~SWAP_BLOCKNUM_MASK) | (SWAP_BLOCKNUM_MASK & val);
}
class DiskSwap;
typedef DiskSwap **DiskSwapRef;

#endif /* #ifndef _IN_STUBGEN */


class DiskSwap : public FSSwap __xbase(Obj) {
    uval nextDevID;
    uval numPagesAvail;
    void* currentDev; //place holder for list walking
    typedef FairBLock LockType;
    LockType lock;
#ifndef _IN_STUBGEN
    virtual SysStatusUval allocBlock(BlockDevRef &bdref, PagerContext context);
    DEFINE_PINNEDGLOBAL_NEW(DiskSwap);
    DEFINE_REFS(DiskSwap);
    DEFINE_ACCESS_SERVER(DiskSwap, MetaObj::none, MetaObj::none);
    static DiskSwapRef swapper;
    struct DeviceDesc;
    ListSimpleKeyLocked<uval, uval,AllocPinnedGlobalPadded> devList;
#endif // #ifndef _IN_STUBGEN
    virtual SysStatus init();

public:
    static SysStatus ClassInit(VPNum vp);

    // reads or writes a page from/to a paging block
    // only handles PAGE_SIZE requests
    // calls DREF(ref)->fillPageComplete(offset) when io is done
    virtual SysStatus startFillPage(uval physAddr, FRComputationRef ref,
				    uval offset, uval blockID,
				    PagerContext context);

    // allocates a new block unless an existing block number is provided
    // blockID of uval(-1) requests new allocation
    virtual SysStatus putPage(uval physAddr, uval& blockID,
			      PagerContext context);

    // calls DREF(ref)->putPageComplete(offset) when io is done
    virtual SysStatus startPutPage(uval physAddr, FRComputationRef ref,
				   uval offset, uval& blockID,
				   PagerContext context,
				   IORestartRequests *rr);

    virtual SysStatus freePage(uval blockID, PagerContext context);

    // returns _SUCCESS if swap is active
    virtual SysStatus swapActive();

    virtual SysStatus printStats();

    // Registers a block-device for paging, returns disk id
    virtual SysStatusUval attachDevice(ObjectHandle oh, uval bytes);
__xpublic:
    // Registers a block-device for paging, returns disk id
    static SysStatusUval _attachDevice(__in ObjectHandle oh, __in uval bytes);
};

#endif /* #ifndef __DISKSWAP_H_ */
