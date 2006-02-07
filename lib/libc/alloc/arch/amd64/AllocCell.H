#ifndef __ALLOC_CELL_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __ALLOC_CELL_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocCell.H,v 1.7 2005/03/16 20:21:06 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: machine specific component of AllocCell.H
 * **************************************************************************/
/*
 * amd64 supports 48 bits of virtual address, with the left most sign extended
 * we support fewer numa nodes in this address configuration
 */

#ifdef INCLASS
    enum {
	LOG_MAX_NUMANODES=6,
	MAX_NUMANODES=1<<LOG_MAX_NUMANODES,
	//FIXME until we move vmapr away from c000... we need
	// to avoid the first two bits.
	SKIP_ON_LEFT=2,
	NUMANODE_SHIFT=48-LOG_MAX_NUMANODES,
	COUNT_SHIFT=NUMANODE_SHIFT,
	CELL_MAX_COUNT=MAX_NUMANODES-1,
	COUNT_MASK=CELL_MAX_COUNT
    };
#else

// FIXME: we could likely improve code if we knew that count was in upper-most
// bits; compiler might do this anyway though.

// VVV we probably have to revisit function of the "normalized" addressing of amd64 XXX

/*
 * called with the numa node number and the base of the
 * memory (USER_REGIONS_START for user, KERNEL_REGIONS_START for kernel)
 * returns the range of addresses used by AllocCell for that
 * numa node.
 *
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
    i.word = base+(1ul<<(NUMANODE_SHIFT-1));
    i.numaNode = node;
    base = i.word;
    // end of the region for this node
    top = base + (1ul<<(NUMANODE_SHIFT-1));
}

#endif
