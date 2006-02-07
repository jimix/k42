#ifndef __LEAK_PROOF_H_
#define __LEAK_PROOF_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LeakProof.H,v 1.13 2003/04/07 11:17:09 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: object which provides an allocation tracking
 * data base to detect memory leaks.
 * **************************************************************************/

#include "misc/linkage.H"
#include <sync/FairBLock.H>

class LeakProof{
public:
    // worse case, when rounding up hash table doubles its size so
    // we wind up with 4 Hash* per entry
    enum{callDepth=4};
    struct LeakData{
	uval callChain[callDepth];
	uval addr,size;
    };
protected:
    struct Hash{
	LeakData leakData;
	Hash*    next;
    };
    FairBLock hashLock;
    Hash**    hash;			// hash table (size is 2**n)
    uval      hashMask;			// (2**n) - 1
    Hash*     hashBlocks;		// array of hash blocks
    uval      numHashBlocks;		// total number of hash blocks
    uval      highWaterBlock;		// first never used block
    Hash*     freeList;			// chain of free hash blocks
    uval      skippedCount;		// number of allocs ignored
    uval      hashElements;		// debug see next
    uval      hashChains;		// debug

    Hash* getFree();
    inline uval addrHash(uval addr) {
	return (((addr>>3)^(addr>>12))&hashMask); };
    inline void allocRecord(uval addr, uval size, uval callChainSkip);
public:
    // worse case, when rounding up hash table doubles its size so
    // we wind up with 4 Hash* per entry
    enum {sizePerEntry=(sizeof(Hash)+4*sizeof(Hash*))};
    uval skipped() { return skippedCount; };
    void init(uval base, uval baseSize);
    void reset();
    void alloc(uval addr, uval size);
    void alloc1(uval addr, uval size);
    void alloc2(uval addr, uval size);
    void free(uval addr, uval size);
    SysStatus next(uval& handle, LeakData& leakData);
    void print();
};
#endif /* #ifndef __LEAK_PROOF_H_ */
