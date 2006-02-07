/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DataChunk.C,v 1.7 2000/05/11 11:48:05 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Just contains the print routine to print something out as a datachunk.
 * See header file for description of the class.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "alloc/DataChunk.H"

void
DataChunk::print()
{
    DataChunk *list = this;

    cprintf("[ ");
    while(list) {
	cprintf("%p ", list);
	list = list->next;
    }
    cprintf("]");
}

uval
DataChunk::getNumBlocks()
{
    DataChunk    *list = this;
    uval count = 0;

    while(list) {
	count++;
	list = list->next;
    }

    return count;
}
