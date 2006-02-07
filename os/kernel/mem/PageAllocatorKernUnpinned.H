#ifndef __PAGE_ALLOCATOR_KERN_UNPINNED_H_
#define __PAGE_ALLOCATOR_KERN_UNPINNED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKernUnpinned.H,v 1.28 2003/05/08 18:59:12 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <cobj/CObjRootMultiRep.H>
#include <mem/PageAllocatorKern.H>

class PageAllocatorKernUnpinned : public PageAllocatorKern {

protected:
    uval numaNode;			// my numa node

    inline VPNum addrToNumaNode(uval vaddr) {
	return AllocCell::AddrToNumaNode(vaddr);
    }

    inline uval isLocalAddr(uval vaddr) {
	return addrToNumaNode(vaddr) == numaNode;
    }

    SysStatus init(VPNum vp, VPNum numaNodeArg);
    DEFINE_PINNEDGLOBALPADDED_NEW(PageAllocatorKernUnpinned);


    class MyRoot : public CObjRootMultiRepPinned {

    public:
	// we have to handle holes in the numanode numbering, at least
	// for startup issues
	PageAllocatorKernUnpinned *repByNumaNode[AllocCell::MAX_NUMANODES];
	VPNum maxNumaNodeNum;		// max numa node seen
	uval  cpusPerNumaNode;
	MyRoot(RepRef ref);

	virtual SysStatus handleMiss(COSTransObject * &co,
				     CORef ref, uval methodNum);
	virtual CObjRep *createRep(VPNum vp);
	virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize);
	DEFINE_PINNEDGLOBALPADDED_NEW(MyRoot);
    };

    friend class PageAllocatorKernUnpinned::MyRoot;
    MyRoot *root() { return (MyRoot *)myRoot; }

    virtual SysStatus getMoreMem(uval reqSize);

    virtual SysStatus bindRegionToNode(VPNum node, uval size, uval &vaddr);


public:
    static SysStatus ClassInit(VPNum vp);

    // allocates contiguous region of memory,
    virtual SysStatus allocPages(uval &ptr, uval size, uval flags=0,
				 VPNum node=LOCAL_NUMANODE);

    // allocates memory at specified physical address
    virtual SysStatus allocPagesAt(uval paddr, uval size, uval flags=0);

    // method to get aligned memory - ptr mod(align) = offset
    virtual SysStatus allocPagesAligned(uval &ptr, uval size,
					uval align, uval offset=0,
					uval flags=0,
					VPNum node=LOCAL_NUMANODE);

    // free memory at specified physical address
    virtual SysStatus deallocPages(uval paddr, uval size);

    virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize) {
	return COGLOBAL(getNumaInfo(vp, node, nodeSize));
    }
};



#endif /* #ifndef ... */
