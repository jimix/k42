#ifndef __LMALLOC_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __LMALLOC_H_ */
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: LMalloc.H,v 1.3 2001/10/05 21:47:13 peterson Exp $
 *************************************************************************** */


/* This is stuff in LMalloc that depends on allocpool stuff and
 * hence must be included after allocpool definition.  */

#if !defined(INCLASS) && !defined(OUTOFCLASS)

inline AllocCell *
SyncedCellPtr::pop()
{
  return((AllocCell *)NULL);
}

inline uval
SyncedCellPtr::push(void *el, uval maxCount, AllocCellPtr &tmp)
{
  return(FAILURE);
}

inline void
SyncedCellPtr::getAndZero(AllocCellPtr &tmp)
{
}

inline uval
SyncedCellPtr::setIfZero(AllocCellPtr nval)
{
  return FAILURE;
}


/* ************************************************************ */
/*								*/
/*								*/
/* ************************************************************ */


inline void *
LMalloc::lMalloc()
{
  return ((void *)NULL);
}


inline void
LMalloc::lFree(void *tolfree)
{
}


inline void
LMalloc::lFreeNoCheckLocal(void *tolfree)
{
}

#endif /* #if !defined(INCLASS) && ... */
