#ifndef __ALLOC_CELL_H_
#define __ALLOC_CELL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocCell.H,v 1.21 2003/05/06 19:32:47 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

/*
 *
 * We reserve address bits for the numa allocator.
 * N.B. - the well known assigments in memoryMap.H depend on this
 * layout.
 *
 * An address, read left to right, contains:
 *
 * 1 bits user/kernel
 * LOG_MAX_NUMANODES bits for the node field.
 * LOG_CELL_MAX_COUNT bits for the AllocCell mechanism
 *
 * the numanode field must contain a value consistent with the
 * numa node this storage is allocated/freed on.
 *
 * the cell field must be CountValue for all addresses managed by the
 * AllocCell mechanism.  Addresses with CountValue in the cell field
 * should not be used for well known purposes.
 *
 */

class AllocCell;

struct AllocCellPtr : BitStructure {
    __BIT_FIELD(64, all, BIT_FIELD_START);
    __LOCK_BIT(all, 1);
    __WAIT_BIT(all, 0);

    AllocCellPtr() { /* empty body for NULL constructor */ }
    AllocCellPtr(uval cnt, void *p);
    void zero() { all(0); }
    AllocCell* pointer(uval numaNode);
    void pointerWithOldCount(AllocCell *ptr);
    uval count();
    void count(uval cnt);
    uval isEmpty() { return (all() == 0); };

    void printList(uval numaNode);
    void show_self(uval numaNode);

    void kosherMainList(uval numaNode);
};

// really like DataChunk; do we need two?
struct AllocCell {
    AllocCellPtr next;
    AllocCellPtr nextList;
#define INCLASS
#include __MINC(AllocCell.H)

    // struct used locally to manipulate numanode field of addresses
    union inserter{
	uval word;
	struct {
	    uval:AllocCell::SKIP_ON_LEFT;
	    uval numaNode:AllocCell::LOG_MAX_NUMANODES;
	};
    };

    // can't use unions, so we use methods to overlay mallocID over nextList
    // for list of remote cells (contains mallocID of each block)
    uval mallocID()		{ return *(uval *)&nextList; }
    void mallocID(uval mid)	{ *(uval *)&nextList = mid; }

    void print(uval numaNode);
    uval getNumBlocks(uval numaNode);
    static inline void NodeRegionRange(VPNum node, uval& base, uval& top);
    static inline VPNum AddrToNumaNode(uval ptr);
};

#undef INCLASS

inline
AllocCellPtr::AllocCellPtr(uval cnt, void *p)
{
    AllocCell::inserter i;
    i.word = uval(p);
    i.numaNode = cnt;
    all(i.word);
}

inline AllocCell*
AllocCellPtr::pointer(uval numaNode)
{
    AllocCell::inserter i;
    // replace count bits with 1 bits
    i.word = all();
    if(i.word != 0) i.numaNode = numaNode;
    return((AllocCell*)i.word);
}

inline void
AllocCellPtr::pointerWithOldCount(AllocCell* ptr)
{
    AllocCell::inserter o, n;
    o.word = all();
    n.word = uval(ptr);
    n.numaNode = o.numaNode;
    all(n.word);
}

inline uval
AllocCellPtr::count()
{
    AllocCell::inserter i;
    i.word = all();
    return(i.numaNode);
}

inline void
AllocCellPtr::count(uval cnt)
{
    AllocCell::inserter i;
    i.word = all();
    i.numaNode = cnt;
    all(i.word);
}

inline VPNum
AllocCell::AddrToNumaNode(uval ptr)
{
    inserter i;
    i.word = ptr;
    return i.numaNode;
}

#include __MINC(AllocCell.H)



#endif /* #ifndef __ALLOC_CELL_H_ */




