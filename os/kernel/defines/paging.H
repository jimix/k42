#ifndef __PAGING_H_
#define __PAGING_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: paging.H,v 1.21 2005/08/24 15:00:08 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Controls whether paging (pageout/swap) is on and
 *                      which daemons should be run
 * **************************************************************************/

#define OKRIEG_PAGING

// to stress paging, make max number of pages in an FCM huge
#ifdef OKRIEG_PAGING
#define FCM_MAX_NUMPAGES (32768*16)
#else
// standard mode today for K42
#define FCM_MAX_NUMPAGES (32768)
#endif

// more specific controls
#define FRSWAPFS   1			// do swapping using FR from a file
#define RAMSWAPFS  2			// do swapping to a ram disk
#define DISKSWAP   3			// do swapping to a block device

#ifdef OKRIEG_PAGING
#define SWAPFS  FRSWAPFS        	// actually makes the choice
#else
#define SWAPFS  DISKSWAP        	// actually makes the choice
#endif

#define PAGING_TO_SERVER		// paging to a server, so must pin
					// kernel and servers
#define NFS_PROVIDES_SWAPFR		// NFS provides the swapping FR

// if not FRSWAP, then assume RAMSWAP, and below says number of pages
// in ram disk for swap space if defined, note, just a debugging thing
// i.e., does no good to swap computation to a ram disk since it still
// consumes memory
//#define RAMSWAP 13000			// ram swap disks (only swap file now)
                                        // set to memory size - rest is
					// swap file
#undef PAGING_VERBOSE                  // testing printouts


#define PAGEOUT_CRIT_NUM_PAGES	       400 // restrict what can grab these
#ifdef OKRIEG_PAGING
#define PAGEOUT_LOW_NUM_PAGES	       20000 // start paging when less
#define PAGEOUT_HIGH_NUM_PAGES	       60000 // stop paging when we have free
#else
#define PAGEOUT_LOW_NUM_PAGES	       2000 // start paging when less
#define PAGEOUT_HIGH_NUM_PAGES	       5000 // stop paging when we have free
#endif

#define LARGE_PAGES_SUPPORTED
#ifdef LARGE_PAGES_SUPPORTED
// parameters that control large pages
// FIXME: for now only one large page size
#define LARGE_PAGES_SIZE  0x1000000
#define LARGE_PAGES_BRK_SIZE  LARGE_PAGES_SIZE
#define LARGE_PAGES_FIXED_POOLS

#define LARGE_PAGES_NON_PAGEABLE
#endif

/**
 * Enables the SyncService daemon, which flushes FCMFiles to disk every 30s
 */
/*
#define ENABLE_SYNCSERVICE
*/

/* For collecting some trace data by Dilma; this should go away soon!
 * This should be committed to the main repository commented out.
 */

//#define DILMA_DIRTY_PAGE_EVENTS

#ifdef DILMA_DIRTY_PAGE_EVENTS
#include <trace/traceDisk.h>
#define DILMA_TRACE_PAGE_DIRTY(thefcm,pg,inc) \
             if (inc==1 && !pg->dirty) { \
                dirtyCounter += inc; \
                lastCounterTrace++; \
             } else if (inc==-1 && pg->dirty) { \
                dirtyCounter += inc; \
                lastCounterTrace++; \
	     } \
             if (lastCounterTrace > 20) { \
                TraceOSDiskDirtyCounter((uval)thefcm, dirtyCounter); \
                lastCounterTrace = 0; \
             }
#else
#define DILMA_TRACE_PAGE_DIRTY(thefcm,pg,inc)
#endif // #ifdef DILMA_DIRTY_PAGE_EVENTS

#endif /* #ifndef __PAGING_H_ */
