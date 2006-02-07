#ifndef __LIST_SIMPLE_H_
#define __LIST_SIMPLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListSimple.H,v 1.27 2001/10/05 21:47:48 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Very simple linked list implementation
 * supporting a single pointer-sized datum.  A base class is provided
 * that is <typeless> of which one can derive <typed> list classes
 * very efficiently. The list itself, consisting of just a head
 * pointer to the first node objects of this class can not be defined
 * **************************************************************************/

template<class ALLOC> class ListSimpleBase {
protected:
    struct ListSimpleNode {
	ListSimpleNode *next;		// pointer to next node
	uval datum;			// data contained in node
	DEFINE_ALLOC_NEW(ListSimpleNode);
    };

    class ListSimpleNode *head, *tail;

    ListSimpleBase() {reinit();}

    uval find(uval  datum);

    void add(uval  datum);

    void addToEndOfList(uval  datum);

    void insertNext(void *prev, uval d);

    uval remove(uval  datum);

    uval removeHead(uval &datum);

    // return NULL if there is no next element, if curr is NULL, returns head
    void *next(void *curr, uval &d);

    void transferTo(ListSimpleBase<ALLOC> &list);
public:
    uval removeNext(void *prev);

    uval isEmpty() { return (head == 0); }

    void reinit() { head = 0; tail = 0; }

};

/* The Typed ListSimple Template */

template<class T, class ALLOC> class ListSimple :
    public ListSimpleBase<ALLOC> {
public:
    DEFINE_ALLOC_NEW(ListSimple);

    inline uval find(T  datum) {
	return ListSimpleBase<ALLOC>::find((uval)datum);
    }

    inline void add(T datum) {
	ListSimpleBase<ALLOC>::add((uval)datum);
    }

    inline void addToEndOfList(T datum) {
	ListSimpleBase<ALLOC>::addToEndOfList((uval)datum);
    }

    inline void insertNext(void *prev, T datum) {
	ListSimpleBase<ALLOC>::insertNext(prev,(uval)datum);
    }

    inline uval remove(T datum) {
	return ListSimpleBase<ALLOC>::remove((uval)datum);
    }

    inline uval removeHead(T &datum) {
	return ListSimpleBase<ALLOC>::removeHead((uval&)datum);
    }

    // if curr is 0, get head of list
    inline void *next(void *curr, T &datum) {
	return ListSimpleBase<ALLOC>::next(curr,(uval&)datum);
    }

    inline void transferTo(ListSimple<T,ALLOC> &list) {
        ListSimpleBase<ALLOC>::transferTo(list);
    }


};

#endif /* #ifndef __LIST_SIMPLE_H_ */
