/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
#include <sys/sysIncs.H>
#include <alloc/LMalloc.H>


// VVV may want to write it asm

AllocCell *
SyncedCellPtr::pop(uval numaNode)
{
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
}

uval
SyncedCellPtr::push(void *el, uval maxCount, AllocCellPtr &tmp)
{
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
}

void
SyncedCellPtr::getAndZero(AllocCellPtr &tmp)
{
    acquire(tmp);
    release(AllocCellPtr(0,0));
}

uval
SyncedCellPtr::setIfZero(AllocCellPtr nval)
{
    AllocCellPtr tmp;
    acquire(tmp);
    if((tmp.isEmpty())) {
	release(nval);
	return SUCCESS;
    } else {
	release(tmp);
	return FAILURE;
    }
}
