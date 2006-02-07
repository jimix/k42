/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FreeFrameListChunk.C,v 1.1 2005/06/20 06:59:50 cyeoh Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "FreeFrameListChunk.H"
#include "FreeFrameList.H"
    
//#define FULL_KOSHER
#ifndef FULL_KOSHER
void 
FreeFrameListChunk::kosher() {};
#else
void 
FreeFrameListChunk::kosher() 
{
    uval countDiscovered=0;
    FrameInfo *cur=head, *realTail;
    
    if (!count) {
	tassertMsg((head == NULL), "woops\n");
	tassertMsg((tail == NULL), "woops\n");
	return;
    }
    
    tassertMsg((cur->prev == NULL), "woops\n");
    while(cur) {
	countDiscovered += cur->frameCount + 1;
	realTail = cur;
	cur = cur->next;
	if (cur) {
	    tassertMsg((cur->prev == realTail), "woops\n");
	}
    }
    tassertMsg((tail == realTail), "woops\n");
    tassertMsg((count == countDiscovered), "woops\n");
}
#endif // FULL_KOSHER

void 
FreeFrameListChunk::freeList(FreeFrameList *ffl) 
{
    kosher();
    tassertMsg((ffl->getCount() > 0), "woops\n");

    while (ffl->getCount()>0) {
	freeFrame(ffl->getFrame());
    }
    kosher();
}
