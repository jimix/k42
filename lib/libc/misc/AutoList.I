#ifndef _AUTOLIST_I_
#define _AUTOLIST_I_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AutoList.I,v 1.11 2003/08/06 22:02:31 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple linked list supporting a single datum
 * **************************************************************************/

#include "AutoList.H"

inline AutoListHead*
AutoListNode::lockHead()
{
    AutoListHead* h;
    while (1) {
	h = _head;
	if (h==NULL) {
	    return h;
	}
	h->lock();
	if (h==_head) {
	    break;
	}
	h->unlock();
    }
    return h;
}

inline
AutoListHead*
AutoListNode::head() {
    return _head;
};

inline
AutoListNode *
AutoListNode::next()
{
    if (_next == _head || _next == this) {
	return NULL;
    }
    return _next;
};

inline
AutoListNode *
AutoListNode::prev()
{
    if (_prev == _head || _prev == this) {
	return NULL;
    }
    return _prev;
};

inline
void
AutoListNode::append(AutoListNode* n)
{
    AutoListHead* h = lockHead();
    if (h == NULL) return;
    lockedAppend(n);
    h->unlock();
}

inline
void
AutoListNode::lockedAppend(AutoListNode* n)
{
    tassertMsg(n->_next == n && n->_prev == n , "already on list\n");
    _next->_prev = n;
    n->_next = _next;
    n->_prev = this;
    _next = n;
    n->_head = _head;
}

inline
void
AutoListNode::prepend(AutoListNode* n)
{
    AutoListHead* h = lockHead();
    if (h == NULL) return;
    lockedPrepend(n);
    h->unlock();
}

inline
void
AutoListNode::lockedPrepend(AutoListNode* n)
{
    _prev->_next = n;
    n->_prev = _prev;
    n->_next = this;
    n->_head = _head;
    _prev = n;
}

inline
void
AutoListNode::detach()
{
    AutoListHead* h = lockHead();
    if (h == NULL) return;

    lockedDetach();
    h->unlock();
}

inline
void
AutoListNode::lockedDetach()
{
    _next->_prev = _prev;
    _prev->_next = _next;
    _prev = _next = this;
    _head = NULL;
}


#endif /* #ifndef __AUTOLIST_I_ */
