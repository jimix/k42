#ifndef PAGE_ALLOCATOR_KERN_PINNED_DEFH
#define PAGE_ALLOCATOR_KERN_PINNED_DEFH
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorKernPinned.H,v 1.42 2004/11/16 20:06:15 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <cobj/CObjRootMultiRep.H>
#include "mem/PageAllocatorKern.H"
#include "init/MemoryMgrPrimitiveKern.H"
#include "mem/PM.H"
#include "defines/paging.H"

class PageAllocatorKernPinned : public PageAllocatorKern {
public:
    /*
     * we have a class circularity problem - we can't
     * refer to MemDescKern here.  So we pay the price
     * of a method dispatch.  We instantiate a "real"
     * MemDescHash in PageAllocatorKernPinned.C
     */
public:
    class MemDescHash {
    public:
	virtual uval getMemDesc(uval addr) = 0;
	virtual void freeMemDesc(uval addr) = 0;
    };
  
protected:
    uval numaNode;			// my numa node; hw number in addr
	
    // use Base since can't dynamically allocate memory
protected:
    class MyRoot : public CObjRootMultiRepBase {
    public:
	// we have to handle holes in the numanode numbering, at least
	// for startup issues
	PageAllocatorKernPinned *repByNumaNode[AllocCell::MAX_NUMANODES];
	VPNum maxNumaNodeNum;		// max numa node seen
	uval pagingEnabled;

	MemDescHash *memDescHash;

	void vpInit(VPNum vp, MemoryMgrPrimitiveKern *memory);

	// own versions to deal with no dynamic allocation of memory
	virtual void replistAdd(uval vp, CObjRep * rep);
	virtual uval replistFind(uval vp, CObjRep *& rep);
	virtual void* replistNext(void *curr, uval& vp, CObjRep*& rep);
	virtual uval  replistRemove(uval vp, CObjRep *& rep);
	virtual uval  replistRemoveHead(CObjRep *& rep);
	virtual uval  replistIsEmpty();
	virtual uval  replistHasRep(CObjRep * rep);

	// all fields shared across reps are here
	// note, we use lock of parent class where needed


	// stuff to record all the chunks of memory in system
	enum { MAX_CHUNKS = 16 };
	struct {uval start; uval end;} physChunks[MAX_CHUNKS];
	uval numPhysChunks;
	uval totalPhysMem;			// total amount of memory
	// add new phys mem chunk, if not new will ignore; end is last byte
	void addPhysChunk(uval start, uval end);
	void printPhysChunks();
	uval validPhysAddr(uval paddr);

	static uval maxPhysMem;		// maximum possible memory
	static uval virtBase;		// virtual address for physical 0

	MyRoot(RepRef ref, uval vbase, uval maxMem);
	virtual SysStatus handleMiss(COSTransObject * &co,
				     CORef ref, uval methodNum);
	virtual CObjRep *createRep(VPNum vp);
	// numa structure used by this allocator
	virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize);
	DEFINE_PRIMITIVE_NEW(PageAllocatorKernPinned::MyRoot);
    };

    friend class PageAllocatorKernPinned::MyRoot;
    
    MyRoot *root() { return (MyRoot *)myRoot; }

    // returns true if paddr is a valid address in range
    virtual uval validPhysAddr(uval paddr) {
	return COGLOBAL(validPhysAddr)(paddr);
    }

    virtual SysStatus initVirtual(VPNum vp, MemoryMgrPrimitiveKern *memory);

    // test function to try page freeing stuff
    void doPageFreeingStuff();

    // init pinned subclass state
    void pinnedInit(VPNum numaNodeArg);

    DEFINE_PRIMITIVE_NEW(PageAllocatorKernPinned);

    // total of available memory across entire system
    uval totalAvailable();

    static PM::MemLevelState GetMemLevelState(uval numPages) {
	if (KernelInfo::OnSim()) {
	    // for now we don't page when running on simulator
	    return PM::HIGH;
	}
	if (numPages >= PAGEOUT_HIGH_NUM_PAGES) {
	    return PM::HIGH;
	} else if (numPages <= PAGEOUT_LOW_NUM_PAGES) {
	    if (numPages < PAGEOUT_CRIT_NUM_PAGES) {
		return PM::CRITICAL;
	    } else {
	       return PM::LOW;
	    }
	} else {
	    return PM::MID;
	}
    }

    SysStatus allocPagesSimple(uval &ptr, uval size,
			       uval align=0, uval off=0) {
	SysStatus rc;
	if (align == 0) rc = PageAllocatorKern::allocPages(ptr,size);
	else rc = PageAllocatorKern::allocPagesAligned(ptr,size,align,off);
	tassertWrn(!_SUCCESS(rc) || isLocalAddr(ptr),
		   "Ooops, allocated remote memory %lx\n", ptr);
	return rc;
    }
    SysStatus doAllocPagesAllNodes(uval &ptr, uval size, uval align,
				   uval offset, uval flags);
    SysStatus doAllocPages(uval &ptr, uval size, uval align,
			   uval offset, uval flags=0,
			   VPNum node=LOCAL_NUMANODE);

#ifdef marcdebug
    virtual void marcCheckAvail();
#endif /* #ifdef marcdebug */

public:
    // support for Kernel Pinned Allocators MemDesc.  We use a hash table
    // since we can't bound the range of frame addresses and we don't expect
    // lots of traffic.
    // GetMemDesc finds or creates a memDesc for the frame containing addr
    // All work done using virtual addresses.
    virtual uval getMemDesc(uval addr) {
	return COGLOBAL(memDescHash->getMemDesc(addr));
    }
    
    virtual void freeMemDesc(uval addr) {
	COGLOBAL(memDescHash->freeMemDesc(addr));
    }
    
    /*
     * we have a memdesc allocator on each numanode, in the hope that
     * on average, the memdesc associated with a frame will be on
     * the same node as the frame.  However, at least until its proven
     * to be a problem, there is a global hash table for memdesc lookup.
     * Note that most memdesc traversal occurs without going through
     * the hash table.
     */
    BLock nextMemDescLock;
    uval nextMemDesc;

    virtual void* getNextForMDH(uval size) {
	AutoLock<BLock> al(&nextMemDescLock); // locks now, unlocks on return
	uval ptr;
	ptr = nextMemDesc;
	// if ptr+size-1 (the last byte)
	// is in a different page from ptr, we need a new page
	if((ptr ^ (ptr+size-1)) & -PAGE_SIZE) {
	    // not enough room this page
	    allocPages(ptr, PAGE_SIZE);
	}
	nextMemDesc = ptr + size;
	return (void*)ptr;
    }
    
    // when asking an FCM to give back a page, describes degree of flexibility
    // in terms of which page to give back
    enum PageConstraints { ANY=0, FIXED=1, SAME_COLOR=2, SAME_NUMANODE=3 };
    enum PageCommand { GIVE_BACK_OR_UNMAP=0, GIVE_BACK=1 };

    // return codes an FCM can use on call to giveBack
    enum GiveBackRC { GAVE_BACK=0, UNMAPPED=1, PINNED=2, NOT_OWNER=3,
		      DOING_IO=4, NO_RESOURCES=5 };

    // called after boot-time memory has been allocated to properly dispose
    // of the memory remaining in the primitive memory manager
    static SysStatus ClassInit(VPNum vp, MemoryMgrPrimitiveKern *memory);

    // called later to initialize virtual components of allocator
    static SysStatus ClassInitVirtual(VPNum vp,MemoryMgrPrimitiveKern *memory);

    virtual SysStatus startPaging();

    // we front-end the following alloc/dealloc calls to support paging
    // Blocking calls may block if memory is low to leave memory for last-
    // chance code that may need more memory, as well as for numa support

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

    // get an idea of how consumed is memory
    virtual SysStatus getMemLevelState(PM::MemLevelState &state);

    // amount of memory available (across all reps)
    virtual SysStatus getMemoryFree(uval &avail);

    // numa structure used by this allocator
    virtual SysStatus getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize) {
	return COGLOBAL(getNumaInfo(vp, node, nodeSize));
    }
		       

    // converts virtual address to real
    static uval virtToReal(uval virt) { return virt - MyRoot::virtBase; }

    // converts real address to virtual
    static uval realToVirt(uval real) { return real + MyRoot::virtBase; }


    // maps a pinned address to its owning numa node
    inline VPNum addrToNumaNode(uval paddr) {
	//FIXME
	return 0;
    }

    // returns true if addr is from local numa node
    inline uval isLocalAddr(uval vaddr) {
	return addrToNumaNode(vaddr) == numaNode;
    }

    // misc. functions for triggering some debug action
    virtual SysStatus countStuff(uval &valid, uval &accessed, uval &total,
				 uval &avail);


    // free memory at addresses specified in a list
    virtual SysStatus deallocListOfPages(FreeFrameList *ffl);

};

#endif /* #ifndef PAGE_ALLOCATOR_KERN_PINNED_DEFH */
