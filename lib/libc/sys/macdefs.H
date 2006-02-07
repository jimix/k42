#ifndef __MACDEFS_H_
#define __MACDEFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: macdefs.H,v 1.16 2005/05/24 02:55:44 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * Machine specific constants - e.g. page sizes
 * **************************************************************************/

#include __MINC(macdefs.H)

#define PAGE_SIZE ((uval)1<<LOG_PAGE_SIZE)
#define PAGE_MASK (PAGE_SIZE-1)
#define MIN_PAGE_SIZE ((uval)1<<LOG_MIN_PAGE_SIZE)
#define MIN_PAGE_MASK (MIN_PAGE_SIZE-1)
#define MAX_PAGE_SIZE ((uval)1<<LOG_MAX_PAGE_SIZE)
#define MAX_PAGE_MASK (MAX_PAGE_SIZE-1)
#define SEGMENT_SIZE ((uval)1<<(LOG_SEGMENT_SIZE))
#define SEGMENT_MASK (SEGMENT_SIZE-1)

#ifndef DFLT_ALIGNMENT
#define DFLT_ALIGNMENT PAGE_SIZE	// may be overridden in arch-dependent file
#endif /* #ifndef DFLT_ALIGNMENT */

#endif /* #ifndef __MACDEFS_H_ */
