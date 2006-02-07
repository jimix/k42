#ifndef __LIST_SIMPLE_KEY_LOCKED_H_
#define __LIST_SIMPLE_KEY_LOCKED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListSimpleKeyLocked.H,v 1.21 2001/10/12 20:22:21 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: simple list with key and value pair, with locking
 * **************************************************************************/

#include "ListSimpleKey.H"

template<class ALLOC>
class ListSimpleKeyLockedBase : public ListSimpleKeyBase<ALLOC> {
    BLock lock;

protected:
    uval find(uval  key, uval &datum);

    void add(uval  key, uval  datum);

    void addToEndOfList(uval  key, uval  datum);

    uval addUnique(uval  key, uval &datum);

    uval remove(uval  key, uval &datum);

    uval removeHead(uval &key, uval &datum);

    // must acquire lock on the outside for this call
    void * next(void *curr, uval &key, uval &datum);

    uval getHead(uval &key, uval &datum);

public:

    void acquireLock()		{ lock.acquire(); }

    void releaseLock()		{ lock.release(); }
};

/* The Typed ListSimple Template */

template<class K,class T,class ALLOC> class ListSimpleKeyLocked :
    public ListSimpleKeyLockedBase<ALLOC> {
public:
    DEFINE_ALLOC_NEW(ListSimpleKeyLocked);

    inline uval find(K key, T  &datum) {
	return ListSimpleKeyLockedBase<ALLOC>::find((uval)key, (uval &)datum);
    }

    inline uval locked_find(K key, T  &datum) {
	return ListSimpleKeyBase<ALLOC>::find((uval)key, (uval &)datum);
    }

    inline void add(K key, T datum) {
	ListSimpleKeyLockedBase<ALLOC>::add((uval)key,(uval)datum);
    }

    inline void locked_add(K key, T datum) {
	ListSimpleKeyBase<ALLOC>::add((uval)key,(uval)datum);
    }

    inline void addToEndOfList(K key, T datum) {
	ListSimpleKeyLockedBase<ALLOC>::addToEndOfList((uval)key,(uval)datum);
    }

    inline uval addUnique(K key, T &datum) {
        uval retvalue;
	retvalue = ListSimpleKeyLockedBase<ALLOC>::addUnique((uval)key,
						  (uval&)datum);
	return(retvalue);
    }

    inline uval remove(K key, T &datum) {
        uval retvalue;
	retvalue = ListSimpleKeyLockedBase<ALLOC>::remove((uval)key,(uval&)datum);
	return(retvalue);
    }

    inline uval removeHead(K &key, T &datum) {
        uval retvalue;
	retvalue = ListSimpleKeyLockedBase<ALLOC>::removeHead((uval&)key,
						   (uval&)datum);
	return(retvalue);
    }

    inline void *next(void *curr, K &key, T &datum) {
        void *retvalue;
	retvalue = ListSimpleKeyLockedBase<ALLOC>::next(curr,(uval&)key,
					     (uval&)datum);
	return(retvalue);
    }

    inline uval getHead(K &key, T &datum) {
        uval retvalue;
	retvalue = ListSimpleKeyLockedBase<ALLOC>::getHead((uval&)key,
						       (uval&)datum);
	return(retvalue);
    }

};


#endif /* #ifndef __LIST_SIMPLE_KEY_LOCKED_H_ */
