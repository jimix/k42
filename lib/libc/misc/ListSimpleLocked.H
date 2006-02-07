#ifndef __LIST_SIMPLE_LOCKED_H_
#define __LIST_SIMPLE_LOCKED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListSimpleLocked.H,v 1.24 2004/10/08 21:40:07 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Locked version of simplelist
 * **************************************************************************/

#include "ListSimple.H"


template<class ALLOC> class ListSimpleLockedBase :
    public ListSimpleBase<ALLOC> {
    BLock lock;

protected:
uval
ListSimpleLockedBase::find(uval datum)
{
    int rc;
    lock.acquire();
    rc = ListSimpleBase<ALLOC>::find(datum);
    lock.release();
    return rc;
}

void
ListSimpleLockedBase::add(uval datum)
{
    lock.acquire();
    ListSimpleBase<ALLOC>::add(datum);
    lock.release();
}

void
ListSimpleLockedBase::addToEndOfList(uval datum)
{
    lock.acquire();
    ListSimpleBase<ALLOC>::addToEndOfList(datum);
    lock.release();
}

uval
ListSimpleLockedBase::addUnique(uval datum)
{
    int rc = 0;				// assume failure
    lock.acquire();
    if (! ListSimpleBase<ALLOC>::find(datum)) {
	// not in list, so add
	ListSimpleBase<ALLOC>::add(datum);
	rc = 1;				// success
    }
    lock.release();
    return rc;
}

uval
ListSimpleLockedBase::remove(uval datum)
{
    int rc;
    lock.acquire();
    rc = ListSimpleBase<ALLOC>::remove(datum);
    lock.release();
    return rc;
}

uval
ListSimpleLockedBase::removeHead(uval & datum)
{
    int rc;
    lock.acquire();
    rc = ListSimpleBase<ALLOC>::removeHead(datum);
    lock.release();
    return rc;
}

uval
ListSimpleLockedBase::getHead(uval & datum)
{
    int rc = 0;
    lock.acquire();
    if (this->head) {
	datum = this->head->datum;
	rc = 1;
    }
    lock.release();
    return rc;
}

/* both the locks on this list and the parameter list must be held */
void
ListSimpleLockedBase::locked_transferTo(ListSimpleLockedBase<ALLOC>
                                        &locked_list)
{
    ListSimpleBase<ALLOC>::transferTo(locked_list);
}

public:
    void acquireLock()		{ lock.acquire(); }

    void releaseLock()		{ lock.release(); }

    // true if list is empty
    uval isEmpty() { if (this->head) return 0; return 1; }
};

template<class T, class ALLOC> class ListSimpleLocked :
    public ListSimpleLockedBase<ALLOC> {
public:
    DEFINE_ALLOC_NEW(ListSimpleLocked);

    inline uval find(T datum) {
	return ListSimpleLockedBase<ALLOC>::find((uval)datum);
    }

    inline void add(T datum) {
	ListSimpleLockedBase<ALLOC>::add((uval)datum);
    }

    inline void addToEndOfList(T datum) {
	ListSimpleLockedBase<ALLOC>::addToEndOfList((uval)datum);
    }

    inline void insertNext(void *prev, T datum) {
	ListSimpleLockedBase<ALLOC>::insertNext(prev,(uval)datum);
    }

    inline uval addUnique(T datum) {
	return ListSimpleLockedBase<ALLOC>::addUnique((uval)datum);
    }

    inline uval remove(T datum) {
	return ListSimpleLockedBase<ALLOC>::remove((uval)datum);
    }

    inline uval removeHead(T &datum) {
	return ListSimpleLockedBase<ALLOC>::removeHead((uval&)datum);
    }

    inline uval getHead(T &datum) {
        return ListSimpleLockedBase<ALLOC>::getHead((uval &)datum);
    }

    inline void locked_transferTo(ListSimpleLocked<T, ALLOC> &locked_list) {
        ListSimpleLockedBase<ALLOC>::locked_transferTo(locked_list);
    }

    /*
     * Iterate over the list.  Lock must be held externally.
     * void *iter = NULL
     * while ((iter=next(iter,datum)) {
     *     process datum;
     * }
     */
    inline void *next(void *curr, T &datum) {
	return ListSimpleLockedBase<ALLOC>::next(curr,(uval&)datum);
    }

};

#endif /* #ifndef __LIST_SIMPLE_LOCKED_H_ */
