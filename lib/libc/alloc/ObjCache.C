/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ObjCache.C,v 1.9 2002/10/30 17:35:57 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements simple allocator for use in SMT regions
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <misc/AutoList.I>
#include "ObjCache.H"

/* virtual */ void*
ObjCache::allocObj() {
    uval usedBits = PAGE_SIZE/ objSize;
    pages.lock();

    uval b = 0;

    PageDesc *pd;
    for (pd = (PageDesc*)pages.next();
	pd != NULL;
	pd = (PageDesc*)pd->next()) {
	b = pd->usage[0].findFirstUnSet();
	if (b== pd->usage[0].setSize() &&
	   usedBits > pd->usage[0].setSize()) {
	    // Check the second bitmap
	    b = pd->usage[1].findFirstUnSet() + pd->usage[0].setSize();
	    if (b < usedBits) {
		break;
	    // } else {
	    //No space on this page
	    }

	} else {
	    break;
	}
    }

    if (!pd) {
	uval newPage;
	SysStatus rc = ps->getPage(newPage);
	tassertMsg(_SUCCESS(rc),"getPage failed: %016lx\n",rc);
	pd = new PageDesc(newPage);

	pd->lockedAppend(&pages);

	b = 0;
    }

    tassertMsg(b<usedBits,"No space for object on page: %ld\n",b);

    if ( b >= pd->usage[0].setSize()) {
	pd->usage[1].setBit(b - pd->usage[0].setSize());
    } else {
	pd->usage[0].setBit(b);
    }
    void *ptr = (void*) (pd->pAddr + b * objSize);
    pages.unlock();

    return ptr;
}

/* virtual */ void
ObjCache::freeObj(void *ptr)
{
    uval page= ((uval)ptr) & (1-PAGE_SIZE);
    uval offset = ((uval)ptr) & (PAGE_SIZE-1);
    PageDesc *pd;
    for (pd = (PageDesc*)pages.next();
	pd != NULL;
	pd = (PageDesc*)pd->next()) {
	if (pd->pAddr == page) {
	    uval bit = offset / objSize;
	    if (bit>= pd->usage[0].setSize()) {
		pd->usage[1].clearBit( bit - pd->usage[0].setSize());
	    } else {
		pd->usage[0].clearBit( bit );
	    }
	    return;
	}
    }
    tassertMsg(0,"Can't free object: %p\n",ptr);
}
