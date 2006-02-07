#ifndef __CACHE_SYNC_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __CACHE_SYNC_H_ */

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CacheSync.H,v 1.2 2001/10/05 21:50:09 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Cache sync stuff for mips
 * **************************************************************************/

#include <misc/hardware.H>
#include "mem/PageAllocatorKern.H"

static __inline__ void
CacheSync(PageDesc* pg)
{
}

static __inline__ void
CacheSync(uval paddr, uval len)
{
}

