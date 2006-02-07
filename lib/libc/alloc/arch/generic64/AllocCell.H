#ifndef __ALLOC_CELL_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __ALLOC_CELL_H_ */
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AllocCell.H,v 1.3 2001/10/05 21:47:12 peterson Exp $
 *************************************************************************** */

inline
AllocCellPtr::AllocCellPtr(uval cnt, void *p)
{
}

inline AllocCell*
AllocCellPtr::pointer()
{
  return ((AllocCell *) NULL);
}

inline uval
AllocCellPtr::count()
{
  return ((uval)0);
}

inline void
AllocCellPtr::count(uval cnt)
{
}

