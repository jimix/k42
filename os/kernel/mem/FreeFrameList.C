/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FreeFrameList.C,v 1.1 2003/11/21 04:04:07 okrieg Exp $
 *****************************************************************************/

#include "kernIncs.H"
#include "FreeFrameList.H"
    
//#define FULL_KOSHER
#ifndef FULL_KOSHER
void 
FreeFrameList::kosher() {};
#else
void 
FreeFrameList::kosher() 
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
FreeFrameList::getList(FreeFrameList *ffl)
{
    FrameInfo *tmp;
    uval amountMoved;
    tassertMsg((ffl->getCount()==0), "assumes a zero input list\n");
    kosher();
    ffl->kosher();

    if (!head) return;
    tmp = head;
    ffl->head = tmp;
    ffl->tail = tmp;
    amountMoved = tmp->frameCount + 1; // 1 is for list header
	
    ffl->count = amountMoved;
    count -= amountMoved;
	
    head = tmp->next;
    if (head) {
	head->prev = NULL;
    } else {
	tail = NULL;
    }
    tmp->next = NULL;
    kosher();
    ffl->kosher();
}

// returns some list that can be cheaply returned
void 
FreeFrameList::getListTail(FreeFrameList *ffl) 
{
    FrameInfo *tmp;
    uval amountMoved;
    tassertMsg((ffl->getCount()==0), "assumes a zero input list\n");
    kosher();
    ffl->kosher();

    if (!tail) return;
    tmp = tail;
    ffl->head = tmp;
    ffl->tail = tmp;
    amountMoved = tmp->frameCount + 1; // 1 is for list header
	
    ffl->count = amountMoved;
    count -= amountMoved;
	
    tail = tmp->prev;
    if (tail) {
	tail->next = NULL;
    } else {
	head = NULL;
    }
    tmp->prev = NULL;
    kosher();
    ffl->kosher();
}

// returns close to but perhaps less than amount requested
// for efficient movement of lists
uval 
FreeFrameList::getUpTo(uval reqCount,  FreeFrameList *ffl) 
{
    kosher();
    ffl->kosher();
    if (count == 0) return reqCount;
    
    while ((head) && (reqCount > head->frameCount)) {
	FrameInfo *tmp = head;
	uval amountMoved = tmp->frameCount + 1; // 1 is for list header
	
	reqCount -= amountMoved;
	ffl->count += amountMoved;
	count -= amountMoved;
	
	head = tmp->next;
	if (head) {
	    head->prev = NULL;
	} else {
	    tail = NULL;
	}
	tmp->next = ffl->head;
	if (ffl->head != NULL) {
	    ffl->head->prev = tmp;
	} else {  
	    ffl->tail = tmp;
	}
	ffl->head = tmp;
    }
    kosher();
    ffl->kosher();
    return reqCount;
}

// returns number of values not satisfied, i.e., 0 means got it all
uval 
FreeFrameList::getList(uval reqCount, FreeFrameList *ffl) 
{
    reqCount = getUpTo(reqCount, ffl);
    
    kosher();
    ffl->kosher();
    while ((reqCount > 0) && isNotEmpty()) {
	ffl->freeFrame(getFrame());
	reqCount--;
    }
    kosher();
    ffl->kosher();
    return reqCount;
}

void 
FreeFrameList::freeList(FreeFrameList *ffl) 
{
    kosher();
    ffl->kosher();
    tassertMsg((ffl->getCount() > 0), "woops\n");

    if (count) {
	count += ffl->count;
	ffl->tail->next = head;
	head->prev = ffl->tail;
	head = ffl->head;
	kosher();
    } else {
	count = ffl->count;
	tail = ffl->tail;
	head = ffl->head;
	kosher();
    }
    ffl->init();
    kosher();
    ffl->kosher();
}

	
