#ifndef __AMD64_MACDEFS_H_
#define __AMD64_MACDEFS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: macdefs.H,v 1.6 2001/10/19 20:01:56 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/

// VVV

#define LOG_PAGE_SIZE 12
#define LOG_MIN_PAGE_SIZE LOG_PAGE_SIZE	// different if multiple page sizes
#define LOG_MAX_PAGE_SIZE LOG_PAGE_SIZE	// XXX

#define LOG_SEGMENT_SIZE 21		// one pde entry worth

#endif /* #ifndef __AMD64_MACDEFS_H_ */
