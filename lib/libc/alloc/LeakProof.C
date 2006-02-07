/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LeakProof.C,v 1.14 2003/04/07 11:17:08 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: leak detector - see Header
 * **************************************************************************/

#include "sys/sysIncs.H"
#include "LeakProof.H"

LeakProof::Hash*
LeakProof::getFree()
{
    Hash* h;
    if (freeList) {
	h = freeList;
	freeList = h->next;
    } else if (highWaterBlock<numHashBlocks) {
	h = hashBlocks+highWaterBlock;
	highWaterBlock++;
    } else {
	h = 0;
    }
    return h;
}

void
LeakProof::allocRecord(uval addr, uval size, uval callChainSkip)
{
    uval i;
    if (!hash) return;			// not running
    i = addrHash(addr);
    Hash* h;
    AutoLock<FairBLock> al(&hashLock);
    h = getFree();
    if (!h) {
	skippedCount++;
	return;
    }
    h->next = hash[i];
    hash[i] = h;
    h->leakData.addr = addr;
    h->leakData.size = size;
    GetCallChainSelf(callChainSkip, h->leakData.callChain, callDepth);
}

// versions of alloc differ in call chain recording
void
LeakProof::alloc(uval addr, uval size)
{
    // Skip one frame - it's just the call from the allocator.
    allocRecord(addr, size, 1);
}

void
LeakProof::alloc1(uval addr, uval size)
{
    // Skip one frame - it's just the call from the allocator.
#ifdef __OPTIMIZE__
    allocRecord(addr, size, 1);
#else
    // Skip one more - that's what alloc1 means.
    allocRecord(addr, size, 2);
#endif
}

void
LeakProof::alloc2(uval addr, uval size)
{
    // Skip one frame - it's just the call from the allocator.
#ifdef __OPTIMIZE__
    allocRecord(addr, size, 1);
#else
    // Skip two more - that's what alloc2 means.
    allocRecord(addr, size, 3);
#endif
}

void
LeakProof::free(uval addr, uval size)
{
    uval i;
    if (!hash) return;			// not running
    AutoLock<FairBLock> al(&hashLock);
    i = addrHash(addr);
    Hash* h = hash[i];
    Hash* prev = 0;
    while (h) {
	if (h->leakData.addr == addr) {
	    if (prev) {
		prev->next = h->next;
	    } else {
		hash[i] = h->next;
	    }
	    h->next = freeList;
	    freeList = h;
	    return;
	}
	prev = h;
	h = h->next;
    }
    return;
}

SysStatus
LeakProof::next(uval& handle, LeakData& leakData)
{
    uval index, position, i;
    Hash* h;
    AutoLock<FairBLock> al(&hashLock);

    if (!handle) {
	index=0;
	position=0;
	hashChains=0;
	hashElements=0;
    } else {
	index=handle&0xfffff;
	position=handle>>20;
    }
    while (index<=hashMask) {
	i = 0;
	h = hash[index];
	if (h && position==0) hashChains++; // count the chains
	while (h) {
	    if (i == position) {
		leakData = h->leakData;
		position++;
		hashElements++;		// count in use elements
		handle = index | (position<<20);
		return 0;
	    }
	    h = h->next;
	    i++;
	}
	index++;
	position = 0;
    }
    cprintf("Chains %ld Elements %ld\n",hashChains, hashElements);
    return -1;
}

void
LeakProof::init(uval base, uval baseSize)
{
    uval i,n,nestimate;
    if (baseSize == 0) {
	hash = 0;
	return;
    }
    hashLock.init();
    nestimate = baseSize/sizePerEntry;
    // compute size of hash table as next power of 2 gt 2*n
    n = 1;
    while (n<2*nestimate) n *= 2;
    hashMask = n-1;
    hash = (Hash**)base;
    // reserver actual size of hash table
    Hash* h;
    h = (Hash*)(base+n*sizeof(Hash*));
    hashBlocks = h;
    baseSize = baseSize-n*sizeof(Hash*);
    // clear hash table
    for (i=0;i<n;i++) hash[i] = 0;

    // recompute number of available hash blocks
    // may be smaller than nestimate since hash table was rounded to
    // power of two size
    n = baseSize/sizeof(Hash);
    numHashBlocks = n;
    freeList = 0;
    highWaterBlock = 0;
    skippedCount = 0;
}

void
LeakProof::reset()
{
    uval i;
    if (hash) {
	// clear hash table
	for (i=0;i<(hashMask+1);i++) hash[i] = 0;
	freeList = 0;
	highWaterBlock = 0;
	skippedCount = 0;
    } else {
	uval base, baseSize;
	// not running yet - initialize
	if (KernelInfo::OnSim()) {
	    baseSize = 200*LeakProof::sizePerEntry;
	} else {
	    baseSize = 20000*LeakProof::sizePerEntry;
	}
	baseSize = ALIGN_UP(baseSize,PAGE_SIZE);
	base = (uval)allocLocal[AllocPool::PINNED].allocGlobal(baseSize);
	if (base == 0) {
	    cprintf("no space for leakproof tables\n");
	    return;
	}
	init(base, baseSize);	// adjust size for LP stuff
    }

}

void
LeakProof::print()
{
    uval i,j;
    if (!hash) {
	uval base, baseSize;
	// not running yet - initialize
	if (KernelInfo::OnSim()) {
	    baseSize = 200*LeakProof::sizePerEntry;
	} else {
	    baseSize = 20000*LeakProof::sizePerEntry;
	}
	baseSize = ALIGN_UP(baseSize,PAGE_SIZE);
	base = (uval)allocLocal[AllocPool::PINNED].allocGlobal(baseSize);
	if (base == 0) {
	    cprintf("no space for leakproof tables\n");
	    return;
	}
	init(base, baseSize);	// adjust size for LP stuff
    }
    cprintf("HashTableSize %ld, numHashBlocks %ld, highWaterBlock %ld\n",
	    hashMask+1, numHashBlocks, highWaterBlock);
    cprintf("skipped %ld\n   addr     size     call chain\n",
	    skipped());
    LeakData leakData;
    i = 0;
    while (0==next(i,leakData)) {
	cprintf("%8lx %8lx ",
	       leakData.addr, leakData.size);
	for (j=0;(j<callDepth)&&(leakData.callChain[j]);j++) {
	    cprintf("%8lx ", leakData.callChain[j]);
	}
	cprintf("\n");
    }
}
