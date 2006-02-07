#ifndef __LIST_ARRAY_SIMPLE_H_
#define __LIST_ARRAY_SIMPLE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListArraySimple.H,v 1.2 2001/10/05 21:47:48 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Very simple array based list
 * supporting a single pointer-sized datum.
 * The list itself, consisting of an array  pointer, tail
 * index plus the size of the array in elements.  The array is doubled
 * in size as needed.  It DOES NOT implement shrinking or deletion of items at
 * this time it is intended for simple cases of holding a set of items to be
 * processed as a group and disposed of as a group which can benifit from
 * locality in access.
 * **************************************************************************/

template<class T,class ALLOC> class ListArraySimple {
private:
    void grow();
protected:
    T *array;		// data contained in element
    uval  tail, size;

public:
    ListArraySimple() {tail = 0; size = 0;}

    void add(T datum);

    void *next(void *curr, T &d);

    uval isEmpty() { return (tail == 0); }

    void reinit() { tail = 0; }

    void dump();
//    void copy();

//    void transfer();
};


#endif /* #ifndef __LIST_ARRAY_SIMPLE_H_ */
