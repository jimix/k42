/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LMallocOpt.C,v 1.11 2003/05/06 19:32:59 marc Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
#include <sys/sysIncs.H>
#include <alloc/LMalloc.H>
#include <sys/arch/powerpc/asmConstants.H>

#define LMALLOCOPT_USE_ASM
AllocCell *
SyncedCellPtr::pop(uval numaNode)
{
#ifdef LMALLOCOPT_USE_ASM
    AllocCell *ptr;
    AllocCellPtr cellptr;

    __asm__ __volatile__("\n"
	"    pop_repeat:						\n"
	"	ldarx	%0,0,%2	# cellptr = freeList	[linked]	\n"
	"	cmpdi   %0,0 	        # ptr == NULL           	\n"
	"	beq-	pop_done	# if (ptr == NULL) goto done	\n"
	"       insrdi  %0,%4,%5,%6     # ptr = cellptr.pointer();	\n"
	"	ld	%1,%3(%0)	# cellptr = ptr->next		\n"
	"	stdcx.	%1,0,%2	# freeList = cellptr	[conditional]	\n"
	"	bne-	pop_repeat      # start over if			\n"
	"				#conditional store failed	\n"
	"    pop_done:							\n"
	: "=&b" (ptr), "=&r" (cellptr)
	: "b" (this), "i" (AC_next), "r" (numaNode),
	  "i" (AllocCell::LOG_MAX_NUMANODES),
	  "i" (64-(AllocCell::NUMANODE_SHIFT+AllocCell::LOG_MAX_NUMANODES))
	: "cr0"
    );
    return ptr;

#else
    AllocCellPtr tmp;
    AllocCell   *el;
    acquire(tmp);
    if (tmp.isEmpty()) {
	release(tmp);
	return NULL;
    }
    el = tmp.pointer(numaNode);
    release(el->next);
    return el;
#endif
}

uval
SyncedCellPtr::push(void *el, uval maxCount, AllocCellPtr &tmp)
{
#ifdef LMALLOCOPT_USE_ASM
    uval count, rc;
    AllocCellPtr newcellptr;

    __asm__ __volatile__("\n"
	"    pushncl_repeat:						\n"
	"	ldarx	%0,0,%4		# tmp = freeList	[linked]\n"
	"	mr	%1,%5		# newcellptr.pointer() = el	\n"
	"	extrdi	%2,%0,%8,%9 	# count = tmp.count()		\n"
	"	cmpld	%2,%6		# if (count >= maxCount)	\n"
	"	bge-	pushncl_full	#     goto full			\n"
	"	addi	%2,%2,1		# count++			\n"
	"	std	%0,%7(%5)	# el->next = tmp		\n"
	"	insrdi	%1,%2,%8,%9 	# newcellptr.count() = count	\n"
	"	stdcx.	%1,0,%4	# freeList = newcellptr	[conditional]	\n"
	"	bne-	pushncl_repeat	# start over if conditional	\n"
	"				# store failed			\n"
	"	li	%3,1		# rc = SUCCESS			\n"
	"	b	pushncl_done					\n"
	"    pushncl_full:						\n"
	"	li	%3,0		# rc = FAILURE			\n"
	"	std	%3,%7(%5)	# el->next.zero()		\n"
	"	li	%2,1		# count = 1			\n"
	"	insrdi	%1,%2,%8,%9 	# newcellptr.count() = count	\n"
	"	stdcx.	%1,0,%4		# freeList = newcellptr		\n"
	"	bne-	pushncl_repeat	# start over if conditional	\n"
	"				# store failed			\n"
	"    pushncl_done:						\n"
	: "=&b" (tmp), "=&r" (newcellptr), "=&b" (count), "=&r" (rc)
	: "b" (this), "b" (el), "r" (maxCount), "i" (AC_next),
	  "i" (AllocCell::LOG_MAX_NUMANODES),
	  "i" (64-(AllocCell::LOG_MAX_NUMANODES+AllocCell::NUMANODE_SHIFT))
	: "cr0"
    );
    return rc;

#else
    acquire(tmp);
    uval count = tmp.count();
    if (count != maxCount) {
	count++;
	((AllocCell *)el)->next = tmp;
	release(AllocCellPtr(count, el));
	return SUCCESS;
    }
    ((AllocCell *)el)->next.zero();
    release(AllocCellPtr(1,el));
    // list too big, returning tmp to move up to higher level
    return FAILURE;
#endif
}

void
SyncedCellPtr::getAndZero(AllocCellPtr &tmp)
{
#ifdef LMALLOCOPT_USE_ASM
    AllocCellPtr newcellptr;

    __asm__ __volatile__("\n"
	"    gAndZ_repeat:						\n"
	"	ldarx	%0,0,%2		# tmp = freeList	[linked]\n"
	"	li	%1,0		# newcellptr.zero()		\n"
	"	stdcx.	%1,0,%2	# freeList = newcellptr	[conditional]	\n"
	"	bne-	gAndZ_repeat	# start over if conditional	\n"
	"				# store failed			\n"
	: "=&b" (tmp), "=&r" (newcellptr)
	: "b" (this)
	: "cr0"
    );

#else
    acquire(tmp);
    release(AllocCellPtr(0,0));
#endif
}

uval
SyncedCellPtr::setIfZero(AllocCellPtr nval)
{
#ifdef LMALLOCOPT_USE_ASM
    uval rc;
    AllocCellPtr tmp;

    __asm__ __volatile__("\n"
	"    setIfZ_repeat:						\n"
	"	ldarx	%0,0,%2		# tmp = freeList	[linked]\n"
	"	cmpldi	%0,0		# if (!tmp.isEmpty)		\n"
	"	bne-	setIfZ_fail	#     goto fail			\n"
	"	stdcx.	%3,0,%2	# freeList = nval	[conditional]	\n"
	"	bne-	setIfZ_repeat	# start over if conditional	\n"
	"				# store failed			\n"
	"	li	%1,1		# rc = SUCCESS			\n"
	"	b	setIfZ_done					\n"
	"    setIfZ_fail:						\n"
	"	li	%1,0		# rc = FAILURE			\n"
	"    setIfZ_done:						\n"
	: "=&b" (tmp), "=&r" (rc)
	: "b" (this), "r" (nval)
	: "cr0"
    );
    return rc;

#else
    AllocCellPtr tmp;
    acquire(tmp);
    if((tmp.isEmpty())) {
	release(nval);
	return SUCCESS;
    } else {
	release(tmp);
	return FAILURE;
    }
#endif
}
