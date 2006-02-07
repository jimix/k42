#ifndef __LIST_SIMPLE_KEY_H_
#define __LIST_SIMPLE_KEY_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListSimpleKey.H,v 1.18 2001/10/05 21:47:48 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: simple list with key and value pair
 * **************************************************************************/

template<class ALLOC> class ListSimpleKeyBase {
protected:
    struct ListSimpleKeyNode {
	ListSimpleKeyNode *next;	// pointer to next node
	uval datum;			// datum contained in node
	uval key;			// key which identifies the datum
	DEFINE_ALLOC_NEW(ListSimpleKeyNode);
    };

    class ListSimpleKeyNode *head, *tail;

    ListSimpleKeyBase() {reinit();}

    uval find(uval k, uval &d);

    uval has(uval d);

    void add(uval k, uval d);

    void addToEndOfList(uval k, uval d);

    uval remove(uval k, uval &d);

    uval removeHead(uval &k, uval &d);

    void * next(void *curr, uval &k, uval &d);

public:
    uval isEmpty() { return (head == 0); }

    void reinit() {head = 0; tail = 0;}
};

/*
 * The Typed ListSimple Template
 */

template<class K,class T,class ALLOC> class ListSimpleKey :
    public ListSimpleKeyBase<ALLOC> {
public:
    DEFINE_ALLOC_NEW(ListSimpleKey);

    inline uval find(K key, T  &datum) {
	return ListSimpleKeyBase<ALLOC>::find((uval)key, (uval &)datum);
    }

    inline void add(K key, T datum) {
	ListSimpleKeyBase<ALLOC>::add((uval)key,(uval)datum);
    }

    inline void addToEndOfList(K key, T datum) {
	ListSimpleKeyBase<ALLOC>::addToEndOfList((uval)key,(uval)datum);
    }

    inline uval remove(K key, T &datum) {
	return ListSimpleKeyBase<ALLOC>::remove((uval)key,(uval&)datum);
    }

    inline uval removeHead(K &key, T &datum) {
	return ListSimpleKeyBase<ALLOC>::removeHead((uval&)key,(uval&)datum);
    }

    inline void *next(void *curr, K &key, T &datum) {
	return ListSimpleKeyBase<ALLOC>::next(curr,(uval&)key,(uval&)datum);
    }

    inline uval has(T datum) {
	return ListSimpleKeyBase<ALLOC>::has((uval)datum);
    }
};


#endif /* #ifndef __LIST_SIMPLE_KEY_H_ */
