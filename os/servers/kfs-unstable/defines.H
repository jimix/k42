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
 * $Id: defines.H,v 1.2 2004/05/06 19:52:50 lbsoares Exp $
 *****************************************************************************/

#ifdef KFS_TOOLS
#define _LARGEFILE64_SOURCE
#endif

//#define KFS_SNAPSHOT

//#define KFS_USE_GLOBAL_RECORDMAP

// For gathering statistics for BlockCache on k42
//#define GATHER_BC_STATS

#endif // #ifndef __DEFINES_H_
