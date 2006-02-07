/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ListArraySimple.C,v 1.2 2002/10/10 13:08:19 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple linked list supporting a single datum
 * **************************************************************************/

// Not really a ".C" file.  Don't include <sys/sysIncs.H>.
#include "ListArraySimple.H"

template<class T, class ALLOC>
void
ListArraySimple<T, ALLOC>::grow()
{
    if ( size ) {
	uval bsize=size*sizeof(T);
	T *newArray;
	newArray = (T*)
	    ALLOC::alloc(2*bsize);
	memcpy(newArray, array, bsize);
	tassert(newArray, err_printf("out of memory\n"));
	ALLOC::free(array,bsize);
	array = newArray;
	size *= 2;
    } else {
	array = (T *)
	    ALLOC::alloc(2*sizeof(T));
	tassert(array, err_printf("out of memory\n"));
	size = 2;
    }
}
    
template<class T, class ALLOC>
void
ListArraySimple<T, ALLOC>::add(T d)
{ 
    if (tail == size) {
	// array is full must grow
	grow();
    }
    array[tail++] = d;
}

template<class T, class ALLOC>
void *
ListArraySimple<T, ALLOC>::next(void *curr, T &d)
{
    if ( tail == 0  ) return 0;
			  
    T *node = (T *)curr; 
    if(!node)
	node = array;
    else {
	tassert(node >= array && node <= &array[size],
		err_printf("Invalid curr pointer\n"));
	
	if ( curr == &array[tail-1]  ) 
	    node = 0;
	else
	    node++;
    }
    if(node)
	d = *node;
    return node;
}    

template<class T, class ALLOC>
void 
ListArraySimple<T, ALLOC>::dump()
{
    err_printf("ListArraySimpleBase: this=%p array=%p"
	       " size=%ld tail=%ld:\n", this, array, size, tail);
    for (uval i=0; i<tail; i++) {
	err_printf("   &array[%ld]=%p\n", i, &array[i]);
    }
}


