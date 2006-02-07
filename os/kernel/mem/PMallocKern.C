/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMallocKern.C,v 1.4 2003/05/06 19:36:37 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Physical memory layer of the kernel memory allocator subsystem.  See
 * the header file for more details.
 * **************************************************************************/

#include "kernIncs.H"
#include <mem/PageAllocatorKernPinned.H>
#include <alloc/DataChunk.H>
#include "AllocRegionManagerKern.H"
#include "PMallocKern.H"
#include "VAllocServicesKern.H"
#include "MemDescKern.H"

PMallocKern::PMallocKern(uval mid, uval nid, AllocRegionManagerKern *regs)
{
    regions = regs;
    mallocID = mid;
    numaNode = nid;
}

/* return to lower layer numBlocks blocks.
 */
DataChunk*
PMallocKern::pMalloc(uval numBlocks)
{
    DataChunk *list;
    //err_printf("PMallocKern::alloc - id %ld, regions %lx, num %ld\n",
    //       mallocID, regions, numBlocks);
    list = regions->alloc(numBlocks, mallocID);
    tassert(list->getNumBlocks() == numBlocks,
	    err_printf("%ld != %ld\n", list->getNumBlocks(), numBlocks));
    return list;
}

/* listToFree is a list of lists of AllocCells, each element to be returned.
 */
void
PMallocKern::pFree(AllocCellPtr listToFree)
{
    AllocCellPtr nextLists;
    AllocCell   *el, *nextel;

    //err_printf("PMalloc::free - id %ld\n", mallocID);
    /* free all the lists in the list */
    while (!listToFree.isEmpty()) {
	el = listToFree.pointer(numaNode);
	nextLists = el->nextList;

	while (el) {
	    nextel = (el->next).pointer(numaNode);
	    AllocRegionManagerKern::free(uval(el), mallocID);
	    el = nextel;
	}

	listToFree = nextLists;
    }
}

/* virtual */ void
PMallocKern::checkMallocID(void *addr)
{
    MemDescKern *md;
    md = MemDescKern::AddrToMD(uval(addr));
    passert(md->mallocID() == mallocID,
	    err_printf("Bad free():  memory (address %p) with mallocID %ld\n"
		       "             freed to allocator with mallocID %ld.\n",
		       addr, md->mallocID(), mallocID));
}
