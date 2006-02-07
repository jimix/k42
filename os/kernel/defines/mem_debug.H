#ifndef __MEM_DEBUG_H_
#define __MEM_DEBUG_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mem_debug.H,v 1.15 2003/03/14 23:13:27 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Enables debugging options for memory
 * **************************************************************************/

//#define HACK_FOR_FR_FILENAMES

#ifndef NDEBUG
//#define DEBUG_MEMORY

// fill allocated memory with 0xBF
//#define DEBUG_CORRUPTMEM
// use LeakProof
//#define DEBUG_LEAK
#endif /* #ifndef NDEBUG */

// track number of allocations at different levels
#undef ALLOC_STATS
// track total instantaneous memory requested across system
#undef ALLOC_TRACK

// check allocator IDs when freeing memory
#undef CHECK_MALLOC_IDS

#endif /* #ifndef __MEM_DEBUG_H_ */
