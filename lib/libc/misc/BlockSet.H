#ifndef _BLOCK_SET_H
#define _BLOCK_SET_H
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BlockSet.H,v 1.3 2002/10/10 13:08:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides a hierachical bitmap for disk-block
 * allocation management.
 **************************************************************************/

#include <misc/AtomicBitVec64.H>
#include <sync/FairBLock.H>
#include <alloc/alloc.H>

//Represents set of blocks represented by device
class BlockSet{
    // 1 -> used , 0->free
    AtomicBitVec64 L4[1];
    AtomicBitVec64 L3[64];
    AtomicBitVec64 L2[64*64];
    AtomicBitVec64 *mapLevel[4];

    //Get the bitmap in upper level
    AtomicBitVec64 *parent(uval level, uval id) {
	passertMsg(level<3,"Can't get parent at root\n");
	return &mapLevel[level+1][id/64];
    }

    //The intent was to make this all lock-free.
    //But, I don't trust this code to do that yet.
    //Besides, we're going to disk as it is, so performance is
    //shot and the hold times are going to be small anyways.
    typedef FairBLock LockType;
    LockType lock;
public:
    AtomicBitVec64 L1[64*64*64];
    DEFINE_PINNEDGLOBAL_NEW(BlockSet);

    BlockSet(uval numBlocks);
    uval blockSetID(AtomicBitVec64* bv) {
	uval x = (((uval)bv) - (uval)&L1[0])/sizeof(AtomicBitVec64);
	tassertMsg(x < 64*64*64,"Bad block set: %p\n",bv);
	return x;
    }

    //Retrieve a set of 64 blocks represented by the given bitmap.
    //SetID identifies the set of 64 represented by "bv"
    // -- setID == 1 -> bv represents blocks 64-127
    uval getBlockSet(uval &setID, AtomicBitVec64* &bv);

    //clearSet indicates if the specified blockSetID is to be cleared after
    // being released.  This allows the caller to set all bits in the set
    // to mark it as busy, while it is being freed.
    void putBlockSet(AtomicBitVec64* bv, uval clearSet=0);
};

#endif /* _BLOCK_SET_H */
