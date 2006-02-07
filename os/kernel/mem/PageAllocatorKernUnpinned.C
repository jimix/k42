/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKernUnpinned.C,v 1.73 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    - unpinnedPageAllocator: allocates Pages in a virtual memory range
 *    - HAT: Hardware address Translation
 * **************************************************************************/

#include "kernIncs.H"
#include <alloc/VAllocServices.H>
#include "mem/PageAllocatorKernUnpinned.H"
#include "mem/RegionDefault.H"
#include "mem/RegionList.H"
#include "mem/FCMPrimitiveKernel.H"
#include "mem/FRPlaceHolder.H"
#include "mem/Access.H"
#include "proc/Process.H"
#include <cobj/CObjRootSingleRep.H>

#include __MINC(PageAllocatorKernUnpinned.H)


PageAllocatorKernUnpinned::MyRoot::MyRoot(RepRef ref)
     : CObjRootMultiRepPinned(ref)
{
    uval i;
    maxNumaNodeNum = 0;
    for (i = 0; i < AllocCell::MAX_NUMANODES; i++) {
	repByNumaNode[i] = NULL;
    }
    //err_printf("PAKU::MyRoot construct with ref %p\n", ref);
    if(KernelInfo::ControlFlagIsSet(KernelInfo::NO_NUMANODE_PER_VP)) {
	// treat whole machine as one numa node
	cpusPerNumaNode = KernelInfo::CurPhysProcs();
    } else {
	// for test purposes, lets us set the number of
	// cpus per numanode
	// beware - the very highest byte of control flags is used
	// to specify the page table size so stay away from it.
	uval flags = KernelInfo::GetControlFlags();
	flags = (flags >> KernelInfo::TEST_FLAG+1)&0xff;
	cpusPerNumaNode = flags?flags:1;
    }
}

CObjRep *
PageAllocatorKernUnpinned::MyRoot::createRep(VPNum vp)
{
    SysStatus rc;
    VPNum numaNode, d1;
    getNumaInfo(vp, numaNode, d1);

    PageAllocatorKernUnpinned *rep;

    rep = repByNumaNode[numaNode];
    if (rep == NULL) {
	// need to create allocator for this numa node
	rep = new PageAllocatorKernUnpinned();
	repByNumaNode[numaNode] = rep;
	if (numaNode > maxNumaNodeNum) maxNumaNodeNum = numaNode;

	rc = rep->init(vp, numaNode);
	tassert(_SUCCESS(rc), err_printf("oops\n"));
    }
    return rep;
}

SysStatus
PageAllocatorKernUnpinned::MyRoot::handleMiss(COSTransObject * &co,
					      CORef ref, uval methodNum)
{
    SysStatus rc;
    //err_printf("PageAllocatorKernUnpinned::MyRoot::handleMiss()\n");
    rc = CObjRootMultiRepPinned::handleMiss(co, ref, methodNum);
    //err_printf("PageAllocatorKernUnpinned::MyRoot::handleMiss() done\n");
    return rc;
}

SysStatus
PageAllocatorKernUnpinned::MyRoot::getNumaInfo(
    VPNum vp, VPNum& node, VPNum& nodeSize)
{
    node = vp / cpusPerNumaNode;
    nodeSize = cpusPerNumaNode;
    return 0;
}

SysStatus
PageAllocatorKernUnpinned::ClassInit(VPNum vp)
{
    SysStatus rc=0;
    VAllocServicesDefault *vas;
    void *place;

    if (vp==0) {
#ifdef UNPINNED_REP_PER_VP
//	err_printf("\n\nHack, testing: unpinned with numanode-per-vp\n\n\n");
#else /* #ifdef UNPINNED_REP_PER_VP */
#endif /* #ifdef UNPINNED_REP_PER_VP */
	(void)new MyRoot((RepRef)GOBJ(ThePageAllocatorRef));
    }

    place = allocPinnedGlobal(sizeof(VAllocServicesDefault));
    vas = new(place) VAllocServicesDefault(GOBJ(ThePageAllocatorRef));

    allocLocal[AllocPool::PAGED].init(vp, vas, AllocPool::PAGED);

    return rc;
}

SysStatus
PageAllocatorKernUnpinned::init(VPNum vp, VPNum numaNodeArg)
{
    SysStatus rc;
    uval vaddr;
    numaNode = numaNodeArg;

    //err_printf("PAKU::init: bp %lx, bn %lx, node %ld, size %lx, base %lx\n",
    //       numaBitPos, numaBitNum, numaNode, SIZE_UNPINNED,
    //       KERNEL_REGIONS_START);

    rc = bindRegionToNode(numaNode, SIZE_UNPINNED, vaddr);

    PageAllocatorDefault::init(vaddr, SIZE_UNPINNED);

    return rc;
}

SysStatus
PageAllocatorKernUnpinned::allocPages(uval &ptr, uval size, uval flags,
				      VPNum node)
{
    passertMsg(node == LOCAL_NUMANODE, "PAKU remote, NYI\n");
    return PageAllocatorKern::allocPages(ptr, size, flags, node);
}

SysStatus
PageAllocatorKernUnpinned::allocPagesAt(uval paddr, uval size, uval flags)
{
    return PageAllocatorKern::allocPagesAt(paddr, size, flags);
}

SysStatus
PageAllocatorKernUnpinned::allocPagesAligned(uval &ptr, uval size, uval align,
					     uval offset, uval flags,
					     VPNum node)
{
    SysStatus retvalue;
    passertMsg(node == LOCAL_NUMANODE, "PAKU remote, NYI\n");
    retvalue = PageAllocatorKern::allocPagesAligned(ptr, size, align, offset,
						flags, node);
    return (retvalue);
}


SysStatus
PageAllocatorKernUnpinned::deallocPages(uval vaddr, uval size)
{
    SysStatus rc;
    PageAllocatorKernUnpinned *rep;
    tassert(addrToNumaNode(vaddr) == addrToNumaNode(vaddr + size - 1),
	    err_printf("dealloc chunk spans numa nodes: a %lx s %lx\n",
		       vaddr, size));
    rep = this;
    if (!isLocalAddr(vaddr)) {
	//err_printf("Doing remote dealloc of %lx to %d\n", vaddr,
	//   addrToNumaNode(vaddr));
	tassert(addrToNumaNode(vaddr) <= COGLOBAL(maxNumaNodeNum),
		err_printf("Bad addr: %lx\n", vaddr));
	rep = COGLOBAL(repByNumaNode[addrToNumaNode(vaddr)]);
	tassert(rep != NULL, err_printf("Bad addr: %lx\n", vaddr));
    }
    rc = rep->PageAllocatorKern::deallocPages(vaddr, size);
    return rc;
}

SysStatus
PageAllocatorKernUnpinned::getMoreMem(uval reqSize)
{
    uval vaddr;
    SysStatus rc;

    _ASSERT_HELD(lock);			// called with lock held

    if (reqSize < SIZE_UNPINNED) {
	reqSize = SIZE_UNPINNED;
    }

    rc = bindRegionToNode(numaNode, reqSize, vaddr);

    //err_printf("PAKU::getMoreMem -> vaddr %lx, rc %lx \n", vaddr, rc);

    if (_SUCCESS(rc)) {
	rc = locked_deallocPages(vaddr, reqSize);
	passert(_SUCCESS(rc), err_printf("oops\n"));
    }

    return rc;
}


SysStatus
PageAllocatorKernUnpinned::bindRegionToNode(VPNum node, uval size, uval &vaddr)
{
    SysStatus rc;
    uval vaddr2;
    FCMRef fcmRef;
    RegionRef regionRef;

    rc = FCMPrimitiveKernel::Create(fcmRef);
    if (!_SUCCESS(rc)) return rc;

    FRRef frRef;
    FRPlaceHolderPinned::Create(frRef);
    DREF(frRef)->installFCM(fcmRef);

    vaddr = KERNEL_REGIONS_START;
    AllocCell::NodeRegionRange(node, vaddr, vaddr2);
    //err_printf("PAKU::BindRegion trying range <%lx -- %lx> size %lx\n",
    //       vaddr, vaddr2, size);
    rc = RegionDefaultKernel::CreateFixedLenWithinRange(
	regionRef, GOBJK(TheProcessRef), vaddr, vaddr2,
	size, 0, frRef, 1, 0, AccessMode::noUserWriteSup);

    if (!_SUCCESS(rc)) {
	err_printf("PAKU::BindRegion failed to get region\n");
	return rc;
    }

    //err_printf("PAKU::init got %lx size %lx for region\n", vaddr, size);

    return rc;
}
