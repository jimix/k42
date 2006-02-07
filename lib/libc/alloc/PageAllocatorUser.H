#ifndef __PAGE_ALLOCATOR_USER_H_
#define __PAGE_ALLOCATOR_USER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorUser.H,v 1.23 2003/05/08 18:58:54 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: user-level memory allocator
 * **************************************************************************/

// FIXME: make arch-dep
// should minimum be segment size?
#define PAGE_ALLOCATOR_USER_LOG_SIZE 	28 // minimum/default region size
#define PAGE_ALLOCATOR_USER_SIZE	(uval(1)<<PAGE_ALLOCATOR_USER_LOG_SIZE)

#include <cobj/CObjRootMultiRep.H>
#include <alloc/PageAllocatorDefault.H>

/*
 * This is the implementation page allocator in user space. To get more
 * memory it will have a pointer to a region alloc that will provide
 * more memory of the right type (regular, pinned, aliased, etc)
 * Eventually, we expect it to be an extension of PageAllocatorDefault
 */
class MemoryMgrPrimitive;

class PageAllocatorUser : public PageAllocatorDefault {
    DEFINE_PRIMITIVE_NEW(PageAllocatorUser);

protected:

    uval numaBitPos;			// first bit in addr identifying node
    uval numaBitNum;			// number of bits in identifying node
    uval numaNode;			// my numa node

    inline VPNum addrToNumaNode(uval vaddr) {
	return AllocCell::AddrToNumaNode(vaddr);
    }

    inline uval isLocalAddr(uval vaddr) {
	return addrToNumaNode(vaddr) == numaNode;
    }

    // use Base since can't dynamically allocate memory
    class MyRoot : public CObjRootMultiRepBase {

    public:

	// we have to handle holes in the numanode numbering, at least
	// for startup issues
	PageAllocatorUser **repByNumaNode;
	// make it big enough for now
	PageAllocatorUser *initialArray[32];
	VPNum maxNumaNodeNum;		// max numa node seen
	VPNum repByNumaNodeMax;		// current array size
	uval cpusPerNumaNode;		// simple assignment of cpus to nodes

	// own versions to deal with no dynamic allocation of memory
	virtual void replistAdd(uval vp, CObjRep * rep);
	virtual uval replistFind(uval vp, CObjRep *& rep);
	virtual void* replistNext(void *curr, uval& vp, CObjRep*& rep);
	virtual uval  replistRemove(uval vp, CObjRep *& rep);
	virtual uval  replistRemoveHead(CObjRep *& rep);
	virtual uval  replistIsEmpty();
	virtual uval  replistHasRep(CObjRep * rep);

	void vpInit(VPNum vp, MemoryMgrPrimitive *pa);

	MyRoot(RepRef ref);

	virtual SysStatus handleMiss(COSTransObject * &co,
				     CORef ref, uval methodNum);
	virtual CObjRep *createRep(VPNum vp);
	virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize);
	DEFINE_PRIMITIVE_NEW(PageAllocatorUser::MyRoot);
    };

    friend class PageAllocatorUser::MyRoot;
    MyRoot *root() { return (MyRoot *)myRoot; }

    virtual SysStatus getMoreMem(uval reqSize);

public:

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

    static SysStatus ClassInit(DispatcherID dspid, MemoryMgrPrimitive *memory);

    // allocate virtual region that will be in local range for given numa node
    virtual SysStatus bindRegionToNode(VPNum numaNode, uval size, uval &vaddr);

    virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize) {
	return COGLOBAL(getNumaInfo(vp, node, nodeSize));
    }
};


#endif /* #ifndef __PAGE_ALLOCATOR_USER_H_ */
