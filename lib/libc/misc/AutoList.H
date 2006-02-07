#ifndef __AUTO_LIST_H_
#define __AUTO_LIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AutoList.H,v 1.17 2003/08/06 22:02:31 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: AutoListNode is a base class for objects that are to
 *                     contain storage for keeping themselves on a list.
 * **************************************************************************/

#include <sync/BLock.H>

class AutoListHead;

struct AutoListNode {
protected:
    AutoListNode *_next;
    AutoListNode *_prev;
    AutoListHead *_head;
    AutoListHead *lockHead();
public:
    AutoListNode() { _head = NULL; _next = _prev = this;};
    virtual void init() { _head = NULL; _next = _prev = this; };
    DEFINE_NOOP_NEW(AutoListNode);
    virtual void dup(AutoListNode &aln) { aln.init(); };
    // We must ensure that any objects derived from this class have been
    // removed from their lists before this dtor is called, since the
    // dtor sequence kills the virtual table
    virtual ~AutoListNode() {};

    AutoListNode *next();
    AutoListNode *prev();

    void append(AutoListNode* n);
    void prepend(AutoListNode* n);
    void detach(void);

    // All of these assume locks are held
    void lockedAppend(AutoListNode *n);
    void lockedPrepend(AutoListNode* n);
    void lockedDetach(void);
    AutoListHead* head();
};



class AutoListHead:public AutoListNode {
public:
    AutoListHead() {_head = this; _next = _prev = this;};
    virtual void init() { _head = this; _next = _prev = this; };
    virtual void forcedClear() {
	lock();
	_head = this;
	_next = _prev = this;
	unlock();
    }
    virtual void lock() { /* empty body */ }
    virtual void unlock() { /* empty body */ }
    virtual uval tryAcquire() { return 1; };
    virtual ~AutoListHead() {
	AutoListNode *node;
	lock();
	while ((node=next())) {
	    node->lockedDetach();
	}
    }
};

class LockedAutoListHead:public AutoListHead {
protected:
    BLock _lock;
public:
    virtual void init() { AutoListHead::init(); _lock.init(); };
    LockedAutoListHead():AutoListHead() {_lock.init();};
    virtual void lock() { _lock.acquire(); };
    virtual uval tryAcquire() { return _lock.tryAcquire(); };
    virtual void unlock() { _lock.release(); };
};

typedef LockedAutoListHead LockedAutoList;
typedef AutoListHead AutoList;


//There are no dependency issues, so the inlined implementation
//can be safely included in the header
#include <misc/AutoList.I>

#endif /* #ifndef __AUTO_LIST_H_ */
