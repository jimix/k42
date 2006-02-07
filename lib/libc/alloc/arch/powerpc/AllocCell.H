#ifndef __ALLOC_CELL_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __ALLOC_CELL_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocCell.H,v 1.12 2003/05/08 18:59:03 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine specific component of AllocCell.H
 * **************************************************************************/

#ifdef INCLASS
/*
 * We can support 2**12 numanodes.
 * N.B - the maximum lists sizes in the LMalloc/GMalloc system
 * are also bounded by the size of this field.
 */
    enum {
	LOG_MAX_NUMANODES=12,
	MAX_NUMANODES=1<<LOG_MAX_NUMANODES,
	//FIXME until we move vmapr away from c000... we need
	// to avoid the first two bits.
	SKIP_ON_LEFT=2,
	NUMANODE_SHIFT=64-SKIP_ON_LEFT-LOG_MAX_NUMANODES,
	COUNT_SHIFT=NUMANODE_SHIFT,
	CELL_MAX_COUNT=MAX_NUMANODES-1,
	COUNT_MASK=CELL_MAX_COUNT
    };
#else


/*
 * called with the numa node number and the base of the
 * memory (0 for user, 0x8000000000000000 for kernel)
 * returns the range of addresses used by AllocCell for that
 * numa node.
 *
 * The numa address stragety comsumes lots of segments.  In
 * powerpc, this can potentially swamp the segment table or
 * slb.  the segment table can hold 256 segment mappings
 * if they are uniformly distributed across the hash classes.
 * the hash classes are the low five bits of the segment part
 * of the address.
 *
 * The segment lookaside buffer may be as small as 64 entries.
 * So 64 may be the effective limit.
 *
 * This code is good enough for sensible numbers of processors,
 * particularly if we use several processors per numa node.
 * (We allocate an address range for each numa node).
 *
 * To do better, we would have to pack the address ranges for multiple
 * numa nodes in the same segment - which is messy.
 */
inline void
AllocCell::NodeRegionRange(VPNum node, uval& base, uval& top)
{
    inserter i;
    // fix up the base to account for segment table hash
    // in powerpc - need addresses to hash to different groups
    // so all the numa segments don't compete for the same
    // set table group
    // we use the upper half of the address space allocated
    // to a numa node for allocregion allocations
    i.word = base+((node&0x1f)<<LOG_SEGMENT_SIZE)+(1ul<<(NUMANODE_SHIFT-1));
    i.numaNode = node;
    base = i.word;
    // end of the region for this node
    top = (base&~(0x1ful<<LOG_SEGMENT_SIZE)) + (1ul<<(NUMANODE_SHIFT-1));
}

#endif
