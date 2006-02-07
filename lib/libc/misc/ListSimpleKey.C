/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListSimpleKey.C,v 1.17 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: simple list with key and value pair
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "ListSimpleKeyLocked.H"

template<class ALLOC>
uval
ListSimpleKeyBase<ALLOC>::find(uval k, uval &d)
{
    ListSimpleKeyNode *node;
    for(node = head; node != 0; node = node->next) {
	if(node->key == k) {
	    d = node->datum;
	    return 1;
	}
    }
    return 0;
}

template<class ALLOC>
uval
ListSimpleKeyBase<ALLOC>::has(uval d)
{
    ListSimpleKeyNode *node;
    for(node = head; node != 0; node = node->next) {
	if(node->datum == d) {
	    return 1;
	}
    }
    return 0;
}

template<class ALLOC>
void
ListSimpleKeyBase<ALLOC>::add(uval k, uval d)
{
    ListSimpleKeyNode *node;
    node = new ListSimpleKeyNode;
    tassert(node, err_printf("out of memory\n"));
    node->key	= k;
    node->datum	= d;
    node->next	= head;
    head = node;
    if (!tail)
	tail = node;
}

template<class ALLOC>
void
ListSimpleKeyBase<ALLOC>::addToEndOfList(uval k, uval d)
{
    ListSimpleKeyNode *node;
    node = new ListSimpleKeyNode;
    tassert(node, err_printf("out of memory\n"));
    node->key	= k;
    node->datum	= d;
    node->next	= NULL;
    if(!head)
	head = node;
    if(tail)
	tail->next = node;
    tail = node;
}

template<class ALLOC>
uval
ListSimpleKeyBase<ALLOC>::remove(uval k, uval &d)
{
    ListSimpleKeyNode *node, *prev;
    node = head;
    if(!node) {
	return 0;
    } else if(node->key == k) {
	d = node->datum;
	head = node->next;
	delete node;
	if(tail == node)
	    tail = NULL;
	return 1;

    } else for(prev = node, node = node->next;
	        node != 0;
		prev = node, node = node->next) {
	if(node->key == k) {
	    d = node->datum;
	    prev->next = node->next;
	    if(tail == node)
		tail = prev;
	    delete node;
	    return 1;
	}
    }
    return 0;
}

template<class ALLOC>
uval
ListSimpleKeyBase<ALLOC>::removeHead(uval &k, uval &d)
{
    ListSimpleKeyNode *node;
    node = head;
    if(node) {
	head = node->next;
	k = node->key;
	d = node->datum;
	if(tail == node)
	    tail = NULL;
	delete node;
	return 1;
    }
    return 0;
}

template<class ALLOC>
void *
ListSimpleKeyBase<ALLOC>::next(void *curr, uval &k, uval &d)
{
    ListSimpleKeyNode *node = (ListSimpleKeyNode *)curr;
    if(!node)
	node = head;
    else
	node = node->next;
    if(node) {
	k = node->key;
	d = node->datum;
    }
    return node;
}

template<class ALLOC>
uval
ListSimpleKeyLockedBase<ALLOC>::find(uval  key, uval &datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return ListSimpleKeyBase<ALLOC>::find(key,datum);
}

template<class ALLOC>
void
ListSimpleKeyLockedBase<ALLOC>::add(uval  key, uval  datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    ListSimpleKeyBase<ALLOC>::add(key,datum);
    return;
}

template<class ALLOC>
void
ListSimpleKeyLockedBase<ALLOC>::addToEndOfList(uval  key, uval  datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    ListSimpleKeyBase<ALLOC>::addToEndOfList(key,datum);
    return;
}

template<class ALLOC>
uval
ListSimpleKeyLockedBase<ALLOC>::addUnique(uval  key, uval &datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    if(! ListSimpleKeyBase<ALLOC>::find(key,datum)) {
	// not in list so add
	ListSimpleKeyBase<ALLOC>::add(key,datum);
	return 1;			// success
    }
    return 0;				// failure
}

template<class ALLOC>
uval
ListSimpleKeyLockedBase<ALLOC>::remove(uval  key, uval &datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return ListSimpleKeyBase<ALLOC>::remove(key,datum);
}

template<class ALLOC>
uval
ListSimpleKeyLockedBase<ALLOC>::removeHead(uval &key, uval &datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return ListSimpleKeyBase<ALLOC>::removeHead(key,datum);
}

// must acquire lock on the outside for this call
template<class ALLOC>
void *
ListSimpleKeyLockedBase<ALLOC>::next(void *curr, uval &key, uval &datum)
{
    /*tassert(lock.isLocked(), err_printf("Not locked\n"));*/
    return ListSimpleKeyBase<ALLOC>::next(curr,key,datum);
}

template<class ALLOC>
uval
ListSimpleKeyLockedBase<ALLOC>::getHead(uval &key, uval &datum)
{
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    if(this->head) {
	key   = this->head->key;
	datum = this->head->datum;
	return 1;
    }
    return 0;
}

// specific instantiations
template class ListSimpleKeyBase<AllocLocalStrict>;
template class ListSimpleKeyBase<AllocGlobal>;
template class ListSimpleKeyBase<AllocGlobalPadded>;
template class ListSimpleKeyBase<AllocPinnedLocalStrict>;
template class ListSimpleKeyBase<AllocPinnedGlobal>;
template class ListSimpleKeyBase<AllocPinnedGlobalPadded>;

template class ListSimpleKeyLockedBase<AllocLocalStrict>;
template class ListSimpleKeyLockedBase<AllocGlobal>;
template class ListSimpleKeyLockedBase<AllocGlobalPadded>;
template class ListSimpleKeyLockedBase<AllocPinnedLocalStrict>;
template class ListSimpleKeyLockedBase<AllocPinnedGlobal>;
template class ListSimpleKeyLockedBase<AllocPinnedGlobalPadded>;
