/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageAllocatorDefault.C,v 1.64 2005/06/20 06:59:49 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: page allocator for user space
 *
 * This allocator is a version of Stephenson fast first fit.  In
 * Overview, it maintains a binary tree with each node representing a
 * free block.  The top node of the tree or any subtree represents
 * the largest block in the subtree.  The low subtree contains all
 * blocks whose address is lower than that of the top block.
 * Similarly, the high subtree contins the higher address blocks.
 *
 * This tree makes it possible to search for either a size or an
 * address efficiently if the tree stays balanced. Trickery to avoid
 * quadradic behavior when blocks of the same size are successively
 * allocated or freed is implemented.

 * This allocator maintains a disjoint free storage structure rather
 * than building the tree in the free blocks.  We judge that the space
 * used by this is worth the reduction in page (and possibly cache)
 * working set.
 *
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <defines/mem_debug.H>
#include "PageAllocatorDefault.H"
#include "sys/macdefs.H"
//#include <stdio.h>
#include <misc/BaseRandom.H>

inline PageAllocatorDefault::freePages *
PageAllocatorDefault::merge(freePages *low, freePages *high)
{
    _ASSERT_HELD(lock);
    freePages *res;
    freePages **top = &res;
    while (low && high) {
	if (low->size > high->size) {
	    *top = low;
	    top = &(low->high);
	    low = low->high;
	} else {
	    *top = high;
	    top = &(high->low);
	    high = high->low;
	}
    }
    *top = low ? low : high;
    return res;
}

/* virtual */void *
PageAllocatorDefault::getFreePagesMemory(uval& vaddr, uval& size)
{
    void *rtn=(void *)vaddr;
    vaddr+=PAGE_SIZE;
    size-=PAGE_SIZE;
    return rtn;
}

inline void
PageAllocatorDefault::extendFreeList(uval& vaddr, uval& size)
{
    _ASSERT_HELD(lock);
    // replenish freelist
    freePages *l;
    freePages *cur = anchor;
    freePages **top;
    top = &anchor;
    available -= PAGE_SIZE;
    if (cur) {
	while (cur->low) {
	    top = &(cur->low);
	    cur = cur->low;
	}
	l = (freePages*)(cur->start);
	if (cur->size == PAGE_SIZE) {
	    *top = cur->high;
	    freeList = cur;
	    // N.B. cur->low is already 0
	} else {
	    cur->start+=PAGE_SIZE;
	    cur->size-=PAGE_SIZE;
	    if ((cur->high) && (cur->high->size > cur->size)) {
		*top = cur->high;
		anchor = add(anchor,cur);
	    }
	}
    } else {
        l=(freePages *)getFreePagesMemory(vaddr,size);
    }
    uval i;
    for (i=0;i<PAGE_SIZE/sizeof(freePages);i++) {
	l[i].low = &(l[i+1]);
    }
    l[i-1].low = freeList;
    freeList = l;
    // we may have used all of this new block for freelist
}

SysStatus
PageAllocatorDefault::isNotFree(uval notfree, uval notfreesize)
{
    lock.acquire();
    sanity(notfree, notfreesize);
    lock.release();
    return 0;
}

#ifdef PAGEALLOCATOR_SANITY
void
PageAllocatorDefault::sanitybad()
{
    err_printf("sanity check failed\n");
    breakpoint();
}

uval marcavail,marcavailable;

void
PageAllocatorDefault::sanity(uval notfree=(uval)-1, uval notfreesize=0)
{
    _ASSERT_HELD(lock);
    freePages*cur = anchor;
    freePages *low,*high;
//    freePages *stack[4096];
    uval avail = 0;
    uval largest = 0;
    uval i = 0;
    while (cur) {
	//exists
	if ((!cur->start)||(!cur->size)) sanitybad();
	//alignment
	if ((cur->start|cur->size)&(PAGE_SIZE-1)) sanitybad();
	//check for notfree in this chunc
	if (((notfree<=cur->start) && (notfree+notfreesize)>cur->start)||
	   ((cur->start<=notfree) && ((cur->start+cur->size) > notfree)))
	   sanitybad();
	//sizes sorted correctly
	if (largest >= cur->start) sanitybad();
	avail += cur->size;
	if ((low = cur->low)) {
	    if (low->size > cur->size) sanitybad();
	    if (low->start > cur->start) sanitybad();
	}
	if ((high=cur->high)) {
	    if (high->size > cur->size) sanitybad();
	    if (high->start < cur->start) sanitybad();
	}
	if (low) {
	    stack[i++] = cur;
	    cur = low;
	} else {
	    largest = cur->start + cur->size;
	    cur = high;
	}
	while ((!cur)&&i) {
	    cur = stack[--i];
	    if (largest >= cur->start) sanitybad();
	    largest = cur->start + cur->size;
	    cur = cur->high;
	}
    }
    if (available==avail) return;
    marcavailable = available;
    marcavail = avail;
    sanitybad();
}
#endif /* #ifdef PAGEALLOCATOR_SANITY */

// insert node into tree - note that this may return tree or node
// depending on sizes.  Works correctly if either or both are null.
PageAllocatorDefault::freePages*
PageAllocatorDefault::add(freePages *tree, freePages *node)
{
    _ASSERT_HELD(lock);
    freePages *next;
    freePages **top;
    if (!node) return tree;
    node->low = node->high = 0;
    if (!tree) {
	return node;
    }
    uval nodestart = node->start;
    freePages* cur = tree;
    top = &tree;
    //N.B. - if sizes are equal insert node immediately to avoid
    //quadradic behavior is a number of same size blocks
    //are returned. (Alternative is to wait till node is smaller)
    while (cur && (cur->size > node->size)) {
	if (cur->start<nodestart) {
	    top = &(cur->high);
	    cur = cur->high;
	} else {
	    top = &(cur->low);
	    cur = cur->low;
	}
    }
    *top = node;
    if (!cur) return tree;

    // now split the tree at cur below node.  We split the
    // tree "down the middle" so smaller address nodes are seperated from
    // higher address nodes to satisfy the tree invarient.
    if (cur->start < nodestart) {
	node->low = cur;
	top = &(node->high);
	goto findhigh;
    } else {
	node->high = cur;
	top = &(node->low);
    }
findlow:
    while ((next = cur->low)) {
	if (next->start > nodestart) {
	    cur = next;
	} else {
	    *top = next;
	    top = &(cur->low);
	    cur = next;
	    goto findhigh;
	}
    }
    *top = 0;
    return tree;
findhigh:
    while ((next = cur->high)) {
	if (next->start < nodestart) {
	    cur = next;
	} else {
	    *top = next;
	    top = &(cur->high);
	    cur = next;
	    goto findlow;
	}
    }
    *top = 0;
    return tree;
}

/*
 *
 * cur is a node containing the address range vaddr to vaddr+size.  *
 * top points to the pointer to cur in the tree (or possibly to the *
 * tree anchor).  This space is "removed" from the tree, and the tree
 * is repaired.  In the worse case, there are fragments both before
 * and after the range in the block, and a new block must be created.
*/

void
PageAllocatorDefault::allocFromBlock(freePages* cur,
				     freePages** top,
				     uval vaddr,
				     uval size)
{
    _ASSERT_HELD(lock);
    uval extraaddr;
    uval extrasize,rest;
    *top = merge(cur->low,cur->high);
    // any left over in front
    if (vaddr > cur->start) {
	extraaddr = cur->start;
	extrasize = vaddr-cur->start;
    } else {
	extraaddr = 0;
    }
    // any left over after
    if ((rest = ((cur->start+cur->size) - (vaddr+size)))) {
	cur->size = rest;
	cur->start = vaddr+size;
	anchor = add(anchor,cur);
	if (extraaddr) {
	    cur = freeList;			// freelist never empty
	    if (!(freeList = freeList->low)) {
		// we know there is space - we just freed some
		// so this call will never modify size
		extendFreeList(extraaddr,extrasize);
	    }
	    // back to the main story - cur is the new node
	    cur->start = extraaddr;
	    cur->size = extrasize;
	    anchor = add(anchor,cur);
	}
    } else if (extraaddr) {
	cur->start = extraaddr;
	cur->size = extrasize;
	anchor = add(anchor,cur);
    } else {
	cur->low = freeList;
	freeList = cur;
    }
}

SysStatus
PageAllocatorDefault::init(uval start, uval size)
{

    lock.init();
    lock.acquire();
    anchor = 0;
    freeList = 0;
    tassert((PAGE_ROUND_UP(start)==start),
	    err_printf("page allocator does not support unaligned memory\n"));
#ifdef DEBUG_MEMORY
    uval leakSize;
    leakSize = LeakProof::sizePerEntry*(size/PAGE_SIZE);
    if (KernelInfo::OnSim()) {
	leakSize = 200*LeakProof::sizePerEntry;
    } else {
	leakSize = 20000*LeakProof::sizePerEntry;
    }
    leakSize = size>(16*leakSize)?leakSize:size/16;
    leakSize = PAGE_ROUND_UP(leakSize);
    leakProof.init(start+size-leakSize,leakSize);
    size-=leakSize;
#endif /* #ifdef DEBUG_MEMORY */
    available = size;
    extendFreeList(start,size);
    anchor = freeList;
    freeList = freeList->low;
    anchor->start = start;
    anchor->size = size;
    anchor->low = anchor->high = 0;
    lock.release();
    return 0;
}

/*
 *
 *Allocate the first region of size or return null
 */
SysStatus
PageAllocatorDefault::allocPages(uval &vaddr, uval size, uval f, VPNum n)
{
    (void)f; (void)n;			// flags and node parms not used here

    freePages **top;			// address of pointer to subtree
    freePages *cur, *next;

    // round up to a multiple of a page
    size = PAGE_ROUND_UP(size);

    lock.acquire();

#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */

 retry:

    top = &anchor;
    cur = anchor;
    // top node is (one of) the largest blocks
    if (!cur || cur->size < size) goto nospace;
    // search for lowest address block which is big enough
    while (1) {
	if ((next = cur->low) && (next->size == cur->size)) {
	    //To avoid quadradic behavior allocating a number of
	    //blocks of the same size, we reroot the subtree at the
	    //lower address node of the same size
	    *top = next;
	    cur->low = next->high;
	    next->high = cur;
	} else if (next && (next->size >= size)) {
	    top = &(cur->low);
	} else if ((next=cur->high) && (next->size >= size)) {
	    top = &(cur->high);
	} else break;
	cur = next;
    }
    // cur now points to the lowest address node which can provide size
    // top points to the pointer to cur in the tree

    vaddr = cur->start;
    tassert(((vaddr & (~PAGE_MASK)) == vaddr), err_printf("not aligned?\n"));
    cur->start += size;
    cur->size -= size;
    next = merge(cur->low,cur->high);

    if (cur->size) {
	next = add(next,cur);
    } else {
	cur->low = freeList;
	freeList = cur;
    }
    *top = next;
    available -= size;
    sanity(vaddr,size);
#ifdef DEBUG_MEMORY
    {
	leakProof.alloc(vaddr,size);
#if 0
	uval* p=(uval*)vaddr;
	//don't kill pages for now - simulator too slow
	//most unitialized bugs caught by clobber in alloc.H
	for (;p<(uval*)(vaddr+PAGE_SIZE);*(p++)=(uval)0xBFBFBFBFBFBFBFBFLL);
#endif /* #if 0 */
    }
#endif /* #ifdef DEBUG_MEMORY */
#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */
    lock.release();
    return 0;

 nospace:
    // call virtual function possibly overridden by subclass to get more space
    if (_SUCCESS(getMoreMem(size))) goto retry;

    tassertWrn(0, "warning allocator out of space: size %lx\n", size);

    lock.release();

    return _SERROR(1474, 0, ENOMEM);
}

/*
 *
 * Allocate the address range specified by vaddr and size if it is
 * free, otherwise fail.
 */
SysStatus
PageAllocatorDefault::allocPagesAt(uval vaddr, uval size, uval f)
{
    (void)f;				// flags parm not used here

    tassert(((vaddr & (~PAGE_MASK)) == vaddr), err_printf("not aligned?\n"));

    lock.acquire();
#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */

 retry:

    freePages* cur = anchor;
    freePages** top = &anchor;
    while (cur) {
	if ((cur->start <= vaddr) && ((cur->start+cur->size) > vaddr)) {
	    if ((vaddr+size) <= cur->start+cur->size) {
		    allocFromBlock(cur,top,vaddr,size);
		    available -= size;
		    sanity(vaddr, size);
		    lock.release();
#ifdef DEBUG_MEMORY
		    {
			leakProof.alloc(vaddr,size);
#if 0
			uval* p=(uval*)vaddr;
			//don't kill pages for now - simulator too slow
			//most unitialized bugs caught by clobber in alloc.H
			for (;p<(uval*)(vaddr+PAGE_SIZE);
			    *(p++)=(uval)0xBFBFBFBFBFBFBFBFLL);
#endif /* #if 0 */
		    }
#endif /* #ifdef DEBUG_MEMORY */

		    return 0;
	    } else goto bad;
	}
	if (cur->start > vaddr) {
	    top = &(cur->low);
	    cur = cur->low;
	} else {
	    top = &(cur->high);
	    cur = cur->high;
	}
    }
bad:
    // call virtual function possibly overridden by subclass to get more space
    if (_SUCCESS(getMoreMem(size))) goto retry;

#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */
    lock.release();

#ifdef marcdebug
    tassertWrn(0,"warning allocator out of space: size %lx addr %lx\n",
	       size, vaddr);
#endif /* #ifdef marcdebug */

    vaddr = 0;
    return _SERROR(1475, 0, ENOMEM);
}


/*
 *
 * Allocate a region with an alignment or fail.
 * The aligment is specifed as an alignment and an offset.
 * The resultant vaddr will satisfy vaddr mod align = offset.
 * The old style power of two interface is gotten with a power of two
 * align value, and a zero offset value.
 * This routine does a brute force search and is relatively expensive.
 * Note the bounded stack size - this may cause the allocation to fail
 * when it could be done, or cause a larger than necessary block to be
 * fragmented.
 */
SysStatus
PageAllocatorDefault::allocPagesAligned(uval &vaddr, uval size,
					uval align, uval offset,
					uval f, VPNum n)
{
    (void)f; (void)n;			// flags and node parms not used here

    size = PAGE_ROUND_UP(size);

    lock.acquire();
#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */

 retry:

#define STACK_SIZE 64			// Made up value - should be OK
    freePages *stack[STACK_SIZE];
    uval sp = 0;
    freePages* cur = anchor;
    freePages** top = &anchor;
    freePages *found = 0;
    freePages **foundtop = 0;

    //search for first (smallest address) block which satisfies aligned request

    if (!cur) goto bad;

    while (1) {
	/* this test sees if the aligned request is within this block
	 * start by rounding block address up as required
	 * vaddr will always be ge cur->start
	 */
	vaddr = ((cur->start+align-offset-1)/align)*align+offset;
	if ((vaddr+size) <= (cur->start+cur->size)) {
	    found = cur;
	    foundtop = top;
	}
	// if block is completely too small or has no successors backtrack
	if ((cur->size < size) || (!(cur->low) && !(cur->high))) {
	    if (found) break;
	    if (sp) {
		cur = stack[--sp];
		top = &(cur->high);
		cur = cur->high;	// every stack entry has a high subtree
	    } else {
		goto bad;
	    }
	} else if (cur->low) {		// continue down the tree searching
	    if (cur->high && sp<STACK_SIZE) stack[sp++] = cur;
	    top = &(cur->low);
	    cur = cur->low;
	} else {
	    top = &(cur->high);
	    cur = cur->high;
	}
    }
    // we reach here with found pointing to first feasible block, foundtop
    // to anchor for that block in the tree
    // first remove that block
    vaddr = ((found->start+align-offset-1)/align)*align+offset;
    allocFromBlock(found,foundtop,vaddr,size);
    available -= size;
    tassertMsg((vaddr & PAGE_MASK) == 0, "%lx not page aligned?\n",vaddr);
    sanity(vaddr, size);
#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */
#ifdef DEBUG_MEMORY
    {
	leakProof.alloc(vaddr,size);
#if 0
	uval* p=(uval*)vaddr;
	//don't kill pages for now - simulator too slow
	//most unitialized bugs caught by clobber in alloc.H
	for (;p<(uval*)(vaddr+PAGE_SIZE);*(p++)=(uval)0xBFBFBFBFBFBFBFBFLL);
#endif /* #if 0 */
    }
#endif /* #ifdef DEBUG_MEMORY */
    lock.release();
    return 0;

 bad:
    // call virtual function possibly overridden by subclass to get more space
    if (_SUCCESS(getMoreMem(size))) goto retry;

    lock.release();

    tassertWrn(0, "warning allocator out of space: "
	       "size %lx align %lx offset %lx\n", size, align, offset);
    vaddr = 0;
    return _SERROR(1476, 0, ENOMEM);
}

#ifdef marcdebug
uval marcaddr, marcsize;
uval marcstop=0;
#endif /* #ifdef marcdebug */

/*
 * frees - caller must get the size right
 * It is legal to free "new" storage that was never allocated
 */
SysStatus
PageAllocatorDefault::deallocPages(uval vaddr, uval size)
{
    AutoLock<LockType> al(&lock); // locks now, unlocks on return

    return locked_deallocPages(vaddr, size);
}

/* unlocked version exists to allow one to atomically expand space while
 * trying to allocate memory, so only one thread expands the space if
 * all try to allocate at the same time
 */
SysStatus
PageAllocatorDefault::locked_deallocPages(uval vaddr, uval size)
{
    _ASSERT_HELD(lock);			// must be called with lock held
#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */

    size = PAGE_ROUND_UP(size);
    tassertMsg((vaddr & PAGE_MASK) == 0, "%lx not page aligned?\n",vaddr);

#ifdef DEBUG_MEMORY
    leakProof.free(vaddr,size);
#if 0 /*DEBUG_MEMORY too slow on simulator*/
    {
	uval* p=(uval*)vaddr;
	//can't aford to clear monster allocations here
	for (;p<(uval*)(vaddr+PAGE_SIZE);*(p++)=(uval)0xDFDFDFDFDFDFDFDFLL);
    }
#endif /* #if 0 */
#endif /* #ifdef DEBUG_MEMORY */
#ifdef marcdebug
    marcaddr=vaddr;marcsize=size;
    if ((vaddr == marcstop) || (vaddr & (PAGE_SIZE-1))) breakpoint();
#endif /* #ifdef marcdebug */

    sanity(vaddr, size);

    // first check for coallesce
    tassert(((vaddr & (~PAGE_MASK)) == vaddr), err_printf("not aligned?\n"));

    available += size;
    freePages *cur = anchor;
    freePages **top= 0;
    freePages *cur1;
    freePages **top1;
    uval topSize = 0;			// size of node top is in
    // look for coalesce
    while (cur) {
	if (cur->start+cur->size == vaddr) {
	    cur->size += size;
	    // No find smallest bigger than this
	    if ((cur1=cur->high)) {
		top1 = &(cur->high);
		while (cur1->low) {
		    top1 = &(cur1->low);
		    cur1 = cur1->low;
		}
		if ((cur->start+cur->size) == cur1->start) {
		    //coalesce this as well
		    cur->size += cur1->size;
		    *top1 = cur1->high;
		    cur1->low = freeList;
		    freeList = cur1;
		}
	    }
	    goto enlarge;
	} else if (vaddr+size == cur->start) {
	    // similar searching to the left
	    cur->start = vaddr;
	    cur->size += size;
	    if ((cur1 = cur->low)) {
		top1 = &(cur->low);
		while (cur1->high) {
		    top1= &(cur1->high);
		    cur1 = cur1->high;
		}
		if ((cur1->start+cur1->size)==cur->start) {
		    //coalesce this as well
		    cur->start = cur1->start;
		    cur->size += cur1->size;
		    *top1 = cur1->low;
		    cur1->low = freeList;
		    freeList = cur1;
		}
	    }
	    goto enlarge;
	} else {
	    topSize = cur->size;
	    if (vaddr<cur->start) {
		top = &(cur->low);
		cur = cur->low;
	    } else {
		top = &(cur->high);
		cur = cur->high;
	    }
	}
    }

    // no coalesce found - so make new free block
    cur = freeList;			// freelist never empty
    if (!(freeList = freeList->low)) {
	// may modify vaddr and size if
	// space for freelist can't be found elsewhere
	extendFreeList(vaddr,size);
	if (size == 0) goto done;
    }
    // back to the main story - cur is the new node
    cur->start = vaddr;
    cur->size = size;
    anchor = add(anchor,cur);
done:
    sanity();
#ifdef marcdebug
    marcCheckAvail();
#endif /* #ifdef marcdebug */
    return 0;

enlarge:
    // cur points to enlarged block, topSize size of parent,
    // top points to parent
    if (top && (cur->size > topSize)) {
	// cur has gotten be enough to be in wrong place
	// remove it from current place
	*top = merge(cur->low,cur->high);
	// reinsert it
	anchor = add(anchor,cur);
    }
    goto done;
}

/*virtual*/ SysStatus
PageAllocatorDefault::deallocAllPages(MemoryMgrPrimitive *memory)
{
    uval start, end, size;
    SysStatus rc;

    // Retrieve and make available any memory still available for allocation.
    memory->allocAll(start, size, PAGE_SIZE);
    rc = deallocPages(start, size);
    _IF_FAILURE_RET(rc);

    // Retrieve and make available any recorded chunks of memory.
    while (memory->retrieveChunk(start, end)) {
	rc = deallocPages(start, (end - start));
	_IF_FAILURE_RET(rc);
    }

    return 0;
}


SysStatus
PageAllocatorDefault::getMemoryFree(uval &avail)
{
    lock.acquire();
    avail = available;
    lock.release();
    return 0;
}

void
PageAllocatorDefault::printFreeMem(void)
{
    // FIXME not yet implemented
}

void 
PageAllocatorDefault::printMemoryFragmentation(void)
{
    int stackDepth = 0;

    lock.acquire();

    freePages *cur = anchor;
    freePages *low = NULL, *high = NULL;

    err_printf("__FragmentationFreePageChunks: ");
    while (cur) {
	err_printf("%li ", cur->size);
	
	low = cur->low;
	high = cur->high;

	if (low) {
	    stack[stackDepth++] = cur;
	    cur = low;
	} else {
	    cur = high;
	}

	while ( (!cur) && stackDepth ) {
	    cur = stack[--stackDepth];
	    cur = cur->high;
	}
    }

    lock.release();
}

SysStatus
PageAllocatorDefault::getNumaInfo(VPNum vp, VPNum& node, VPNum& nodeSize)
{
    // should never be called
    passert(0, err_printf("This should never be called\n"));
    return -1;
}

SysStatus
PageAllocatorDefault::bindRegionToNode(VPNum node, uval size, uval &vaddr)
{
    // should never be called
    passert(0, err_printf("This should never be called\n"));
    return -1;
}

#ifndef STANDALONE
// #include <cobj/GOBJ.H>
#endif /* #ifndef STANDALONE */

#ifdef STANDALONE
#define HEAPSIZE 0x1000000
#ifdef PERFTEST
#define NUMTESTS 10000000
#else /* #ifdef PERFTEST */
#define NUMTESTS 10000
#endif /* #ifdef PERFTEST */

#else /* #ifdef STANDALONE */
#define HEAPSIZE 0x800000
#ifdef PERFTEST
#define NUMTESTS 10000
#else /* #ifdef PERFTEST */
#define NUMTESTS 1000
#endif /* #ifdef PERFTEST */

#endif /* #ifdef STANDALONE */

#if 0
#define ARRAY 1024
#else /* #if 0 */
#define ARRAY 32			// for running on simulator
#endif /* #if 0 */

static uval vals[ARRAY];
static uval sizes[ARRAY];
static uval stopval = 155;
static uval curval = 0;
static uval dummy = 1;
static uval stopn = (uval)-1;
uval noalloc = 0;

void
testPageAllocator(uval doDefault)
{
    PageAllocatorDefault it;
    uval i,j,k,km,n,nn,o;
    uval32 *ip;
    SysStatus rc;
    uval heap;
    uval numTests;
    sval c = '1';
#if 0
    if (!doDefault) {
	cprintf("choose number loops(default) NUMBTEST, (1) - 10, (2) - 100:");

	c = getchar();

	cprintf("<%c>\n", (char) c);
    }
#endif
    numTests = NUMTESTS;
    if (c=='1') numTests = 10;
    if (c=='2') numTests = 100;
    cprintf("running allocator test with numTests = %ld\n", numTests);

    for (i=0;i<ARRAY;i++) {
	vals[i] = sizes[i] = 0;
    }

#ifndef STANDALONE
    rc = DREFGOBJ(ThePageAllocatorRef)->allocPages(heap, HEAPSIZE);
    if (_FAILURE(rc)) breakpoint();

#else /* #ifndef STANDALONE */
    heap = (uval)malloc(HEAPSIZE+PAGE_SIZE);
#endif /* #ifndef STANDALONE */

    it.init((heap+PAGE_SIZE-1)&(~(PAGE_SIZE-1)),HEAPSIZE);

    BaseRandom random;

    for (i=0;i<numTests;i++) {
	if (numTests != NUMTESTS) {
	    // only print out when doing low number of loops on slow simulator
	    cprintf("loop iteration is %ld\n", i);
	}
#ifndef PERFTEST
	curval = i;			// so its easy to see in debugger
	if ((i%100)==0) {
	    cprintf("case %ld\n",i);
	}
#endif /* #ifndef PERFTEST */
	if (i == stopval)
	    dummy = 0;

	j = random.getVal();
	n = random.getVal() & (ARRAY-1);
	if (n==stopn)
	    dummy = 1;
	if (sizes[n]) {
#ifndef PERFTEST
	    km = sizes[n]/4;
	    ip = (uval32 *) (vals[n]);
	    for (k=0;k<km;k++) {
		if (ip[k] != n+1)
		    breakpoint();
	    }
	    ip[0] = 0;
#endif /* #ifndef PERFTEST */
#ifdef MALLOCPERF
	    free((void*)(vals[n]));
	    rc = 0;
#else /* #ifdef MALLOCPERF */
	    rc = it.deallocPages(vals[n],sizes[n]);
#endif /* #ifdef MALLOCPERF */
	    if (_FAILURE(rc))
		breakpoint();
	    sizes[n] = 0;
	}
	else if (j & 32) {
	    o = j*0x1000;		// offset
	    j = 1<<((j&7)+12);		// alignment and size
	    o &= (j-1);			// round down

#ifdef MALLOCPERF
	    vals[n] = (uval)malloc(j); rc = !(vals[n]);
#else /* #ifdef MALLOCPERF */
	    rc = it.allocPagesAligned(vals[n], j, j, o);
#endif /* #ifdef MALLOCPERF */
	    /* if no memory, just go on, but don't record this */
	    if (_SUCCESS(rc)) {
#ifndef PERFTEST
		km = j/4;
		ip = (uval32 *) (vals[n]);
		if (ip[0]) {
		    nn = ip[0]-1;
		    if ((nn < ARRAY)&& sizes[nn]
			&& (vals[n]>=vals[nn])
			&& (vals[n]<(vals[nn]+sizes[nn])))
			breakpoint();
		}
		for (k=0;k<km;k++) {
		    ip[k] = n+1;
		}
#endif /* #ifndef PERFTEST */
		sizes[n] = j;
	    } else noalloc += 1;

	} else {
	    /* allocate */
	    j = j & (31<<12);
#ifdef MALLOCPERF
	    vals[n] = (uval)malloc(j);rc = !(vals[n]);
#else /* #ifdef MALLOCPERF */
	    rc = it.allocPages(vals[n], j);
#endif /* #ifdef MALLOCPERF */
	    /* if no memory, just go on, but don't record this */
	    if (_SUCCESS(rc)) {
#ifndef PERFTEST
		km = j/4;
		ip = (uval32 *) (vals[n]);
		if (ip[0]) {
		    nn = ip[0]-1;
		    if ((nn < ARRAY)&& sizes[nn]
			&& (vals[n]>=vals[nn])
			&& (vals[n]<(vals[nn]+sizes[nn])))
			breakpoint();
		}
		for (k=0;k<km;k++) {
		    ip[k] = n+1;
		}
#endif /* #ifndef PERFTEST */
		sizes[n] = j;
	    } else noalloc += 1;
	}
    }

    for (n=0;n<ARRAY;n++) {
	if (sizes[n]) {
#ifdef MALLOCPERF
	    free((void*)(vals[n]));
#else /* #ifdef MALLOCPERF */
	    rc = it.deallocPages(vals[n],sizes[n]);
	    if (_FAILURE(rc)) breakpoint();
#endif /* #ifdef MALLOCPERF */
	}
    }

// all checking of internal is done by sanity calls.  There is no
// final check here since the last dealloc above was followed
// by a call to sanity.

#ifndef STANDALONE
    rc = DREFGOBJ(ThePageAllocatorRef)->deallocPages(heap,HEAPSIZE);
    if (_FAILURE(rc)) breakpoint();
#else /* #ifndef STANDALONE */
    free((void*)heap);
#endif /* #ifndef STANDALONE */

    cprintf("Success: %d cycles, %ld skipped-no space\n",NUMTESTS,noalloc);
}
