#ifndef __DEFINES_H_
#define __DEFINES_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *
 * $Id: defines.H,v 1.8 2004/11/16 11:50:12 dilma Exp $
 *****************************************************************************/

#ifndef NDEBUG
//#define DILMA_DEBUG_RW
#endif

#ifdef KFS_TOOLS
#define _LARGEFILE64_SOURCE
#endif

//#define KFS_SNAPSHOT

// For gathering statistics for BlockCache on k42
//#define GATHER_BC_STATS

#endif // #ifndef __DEFINES_H_
