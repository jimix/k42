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
 * $Id: CacheSync.H,v 1.2 2001/10/05 21:50:07 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Cache sync stuff for mips
 * **************************************************************************/

#include <misc/hardware.H>

// staright form x86 yet XXX

static __inline__ void
CacheSync(PageDesc* pg)
{
    //you might thing that setting pg->cacheSynced would speed things
    //up.  NO.  This way, compiler can throw the whole test away!
}
