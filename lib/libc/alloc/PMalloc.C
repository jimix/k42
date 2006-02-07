/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMalloc.C,v 1.17 2003/05/06 19:32:47 marc Exp $
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

#include <sys/sysIncs.H>
#include "DataChunk.H"
#include "AllocRegionManager.H"
#include "PMalloc.H"
#include "MemDesc.H"

PMallocDefault::PMallocDefault(uval mid, uval nid,
			       AllocRegionManager *regs, uval p)
{
    regions = regs;
    mallocID = mid;
    pool = p;
    numaNode = nid;
}

/* return to lower layer numBlocks blocks.
 */
DataChunk*
PMallocDefault::pMalloc(uval numBlocks)
{
    DataChunk *list;
    //err_printf("PMalloc::alloc - id %ld, regions %lx, num %ld\n",
    //       mallocID, regions, numBlocks);
    list = regions->alloc(numBlocks, mallocID);
    tassert(list->getNumBlocks() == numBlocks,
	    err_printf("%ld != %ld\n", list->getNumBlocks(), numBlocks));
    return list;
}

/* listToFree is a list of lists of AllocCells, each element to be returned.
 */
void
PMallocDefault::pFree(AllocCellPtr listToFree)
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
	    AllocRegionManager::free(uval(el), mallocID, pool);
	    el = nextel;
	}

	listToFree = nextLists;
    }
}

/* virtual */ void
PMallocDefault::checkMallocID(void *addr)
{
    MemDesc *md;
    md = MemDesc::FindMemDesc(uval(addr));
    passert(md->mallocID() == mallocID,
	    err_printf("Bad free():  memory (address %p) with mallocID %ld\n"
		       "             freed to allocator with mallocID %ld.\n",
		       addr, md->mallocID(), mallocID));
}
