/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocCell.C,v 1.15 2003/05/06 19:32:47 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>

void AllocCellPtr::show_self(uval numaNode)
{
    cprintf("AllocCellPtr<%p> = %ld:%p\n",this,count(),pointer(numaNode));
}

void
AllocCell::print(uval numaNode)
{
    AllocCell *list = this;

    cprintf("[ ");
    while(list) {
	cprintf("%p ", list);
	list = (list->next).pointer(numaNode);
    }
    cprintf("]");
}

void
AllocCellPtr::kosherMainList(uval numaNode)
{
    AllocCellPtr p = *this;
    AllocCellPtr n;

    while( !(p.isEmpty()) ) {
	// passert((((uval)p.pointer()) > 0xd00000), err_printf("woops\n"));
	n = p.pointer(numaNode)->next;
	// passert( (n.count() == (p.count()-1)),  err_printf("woops\n"));
	p = n;
    }
    passert((p.count() == 0), err_printf("woops\n"));
}

void
AllocCellPtr::printList(uval numaNode)
{
    AllocCellPtr list = *this;

    cprintf("LL_%ld[ ",count());
    while(!list.isEmpty()) {
	AllocCellPtr ll = list;
//	cprintf("%lx[ ",list.pointer(numaNode));
	while(!ll.isEmpty()) {
	    cprintf("<%ld:%p> ", ll.count(),ll.pointer(numaNode));
	    ll = ll.pointer(numaNode)->next;
	}
	cprintf("]");
	list = list.pointer(numaNode)->nextList;
    }
    cprintf("]\n");
}

uval
AllocCell::getNumBlocks(uval numaNode)
{
    AllocCell *list = this;
    uval count = 0;

    while(list) {
	count++;
	list = (list->next).pointer(numaNode);
    }

    return count;
}

