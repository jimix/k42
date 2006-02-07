/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListSimple.C,v 1.23 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple linked list supporting a single datum
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "ListSimpleLocked.H"

template<class ALLOC>
uval
ListSimpleBase<ALLOC>::find(uval d)
{
    ListSimpleNode *node;
    for(node = head; node != 0; node = node->next) {
	if(node->datum == d) {
	    return 1;
	}
    }
    return 0;
}


template<class ALLOC>
void
ListSimpleBase<ALLOC>::add(uval d)
{
    ListSimpleNode *node;
    node = new ListSimpleNode;
    tassert(node, err_printf("out of memory\n"));
    node->datum = d;
    node->next = head;
    head = node;
    if (!tail) {
	tail = node;
    }
}

template<class ALLOC>
void
ListSimpleBase<ALLOC>::insertNext(void *prev, uval d)
{
   ListSimpleNode *node = (ListSimpleNode *)prev;
   if (prev) {
       ListSimpleNode *tmp = new ListSimpleNode;
       tmp->datum = d;
       tassert(tmp, err_printf("out of memory\n"));
       tmp->next = node->next;
       node->next = tmp;
   } else {
       addToEndOfList(d);
   }
}

template<class ALLOC>
void
ListSimpleBase<ALLOC>::addToEndOfList(uval d)
{
    ListSimpleNode *node;
    node = new ListSimpleNode;
    tassert(node, err_printf("out of memory\n"));
    node->datum	= d;
    node->next	= NULL;
    if(!head) {
	head = node;
    }
    if(tail) {
	tail->next = node;
    }
    tail = node;
}

template<class ALLOC>
uval
ListSimpleBase<ALLOC>::remove(uval d)
{
    ListSimpleNode *node, *prev;
    node = head;
    if(!node) {
	return 0;
    }

    if (node->datum == d) {
	head = node->next;
	delete node;
	if(tail == node) {
	    tail = NULL;
	}
	return 1;
    }
    for(prev = node, node = node->next; node != 0;
	prev = node, node = node->next) {
	if (node->datum == d) {
	    prev->next = node->next;
	    if(tail == node) {
		tail = prev;
	    }
	    delete node;
	    return 1;
	}
    }
    return 0;
}


// If prev is null deletes the first node if prev is non null then
// it is assumed to be a pointer to a node in the list and the node
// next to it is removed
template<class ALLOC>
uval
ListSimpleBase<ALLOC>::removeNext(void *prev)
{
    ListSimpleNode *node = (ListSimpleNode *)prev;
    ListSimpleNode *tmp = NULL;

    if (!head) return 0;

    if (node) {
	if (!node->next) return 0;
	tmp = node->next;
	node->next = tmp->next;
    } else {
	tmp = head;
	head = head->next;
    }
    delete tmp;
    return 1;
}

template<class ALLOC>
uval
ListSimpleBase<ALLOC>::removeHead(uval &d)
{
    ListSimpleNode *node;
    node = head;
    if(node) {
	head = node->next;
	d = node->datum;
	if(tail == node) {
	    tail = NULL;
	}
	delete node;
	return 1;
    }
    return 0;
}

template<class ALLOC>
void *
ListSimpleBase<ALLOC>::next(void *curr, uval &d)
{
    ListSimpleNode *node = (ListSimpleNode *)curr;
    if (!node) {
	node = head;
    } else {
	node = node->next;
    }
    if (node) {
	d = node->datum;
    }
    return node;
}

template<class ALLOC>
void
ListSimpleBase<ALLOC>::transferTo(ListSimpleBase<ALLOC> &list)
{
    if (isEmpty()) return;

    if (list.isEmpty()) {
	list.head = head;
	list.tail = tail;
    } else {
	list.tail->next = head;
	list.tail = tail;
    }
    reinit();
}

// specific instantiations
template class ListSimpleBase<AllocLocalStrict>;
template class ListSimpleBase<AllocGlobal>;
template class ListSimpleBase<AllocGlobalPadded>;
template class ListSimpleBase<AllocPinnedLocalStrict>;
template class ListSimpleBase<AllocPinnedGlobal>;
template class ListSimpleBase<AllocPinnedGlobalPadded>;
