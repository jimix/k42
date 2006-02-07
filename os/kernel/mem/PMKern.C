/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000-2003
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PMKern.C,v 1.17 2004/11/03 05:14:33 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Leaf PMs (generally for a given Process); has
 * no children PMs, only FCMs.  This grows and shrinks the physical
 * memory that backs the kenel, since we don't expect the kernel to be
 * changing much in size, not worth using the cell allocator in
 * PMRoot, we go diretly to the pinned allocator.
 * **************************************************************************/

#include "kernIncs.H"
#include <cobj/CObjRootSingleRep.H>
#include "mem/PMKern.H"
#include "mem/PageAllocatorKernPinned.H"


/* static */ SysStatus
PMKern::Create(PMRef &pmref)
{
    //err_printf("Creating PMKern\n");
    PMKern *pm;
    pm = new PMKern();
    tassert(pm!=NULL, err_printf("No mem for PMKern\n"));
    pmref = (PMRef)CObjRootSingleRepPinned::Create(pm);
    return 0;
}

SysStatus
PMKern::attachFCM(FCMRef fcm)
{
    // for now we do nothing
    return 0;
}

SysStatus
PMKern::detachFCM(FCMRef fcm)
{
    // for now we do nothing
    return 0;
}

SysStatus
PMKern::attachPM(PMRef pm)
{
    passert(0,err_printf("PMKern::attachPM should never be called\n"));
    return -1;
}

SysStatus
PMKern::detachPM(PMRef pm)
{
    passert(0,err_printf("PMKern::detachPM should never be called\n"));
    return -1;
}

SysStatus
PMKern::allocPages(FCMRef fcm, uval &ptr, uval size, uval pageable, uval flags, 
		   VPNum node)
{
    SysStatus retvalue;
    // allow kernel to use reserve to reduce deadlock if pageout daemon
    // causes kernel pagefault
    retvalue = DREFGOBJK(ThePinnedPageAllocatorRef)->
	allocPages(ptr, size, flags|PageAllocator::PAGEALLOC_USERESERVE, node);
    return (retvalue);
}

SysStatus
PMKern::getList(uval count,  FreeFrameList *ffl)
{
    SysStatus rc = 0;
    while(count--) {
	uval ptr;
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(ptr,PAGE_SIZE);
	if(_FAILURE(rc)) {
	    if (ffl->isNotEmpty())
		DREFGOBJK(ThePinnedPageAllocatorRef)->deallocListOfPages(ffl);
	    return rc;
	}
	ffl->freeFrame(ptr);
    }
    return rc;
}

SysStatus
PMKern::allocListOfPages(FCMRef fcm, uval count, FreeFrameList *ffl)
{
    return getList(count, ffl);
}

SysStatus
PMKern::deallocPages(FCMRef fcm, uval paddr, uval size)
{
    return DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(paddr, size);
}

SysStatus
PMKern::deallocListOfPages(FCMRef fcm, FreeFrameList *ffl)
{
    SysStatus retvalue;
    retvalue = DREFGOBJK(ThePinnedPageAllocatorRef)->deallocListOfPages(ffl);
    return (retvalue);
}

SysStatus
PMKern::allocPages(PMRef pm, uval &ptr, uval size, uval pageable, uval flags, 
		   VPNum node)
{
    SysStatus retvalue;
    // allow kernel to use reserve to reduce deadlock if pageout daemon
    // causes kernel pagefault
    retvalue = DREFGOBJK(ThePinnedPageAllocatorRef)->
	allocPages(ptr, size, flags|PageAllocator::PAGEALLOC_USERESERVE, node);
    return (retvalue);
}

SysStatus
PMKern::allocListOfPages(PMRef pm, uval count, FreeFrameList *ffl)
{
    return getList(count, ffl);
}

SysStatus
PMKern::deallocPages(PMRef pm, uval paddr, uval size)
{
    return DREFGOBJK(ThePinnedPageAllocatorRef)->deallocPages(paddr, size);
}

SysStatus
PMKern::deallocListOfPages(PMRef pm, FreeFrameList *ffl)
{
    SysStatus retvalue;
    retvalue = DREFGOBJK(ThePinnedPageAllocatorRef)->deallocListOfPages(ffl);
    return (retvalue);
}

SysStatus
PMKern::attachRef()
{
    // for now we do nothing
    return 0;
}

SysStatus
PMKern::detachRef()
{
    // for now we do nothing
    return 0;
}

SysStatus
PMKern::giveBack(PM::MemLevelState memLevelState)
{
    return 0;
}

SysStatus
PMKern::destroy()
{
    passert(0, err_printf("Destroying PMKern\n"));
    return -1;
}

SysStatus
PMKern::print()
{
    err_printf("PMKern maintains no status\n");
    return 0;
}
