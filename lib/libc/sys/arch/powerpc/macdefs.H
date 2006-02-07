#ifndef __MACDEFS_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __MACDEFS_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: macdefs.H,v 1.12 2003/03/11 20:48:49 marc Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/


#define LOG_PAGE_SIZE 12
#define LOG_MIN_PAGE_SIZE LOG_PAGE_SIZE	// different if multiple page sizes
#define LOG_MAX_PAGE_SIZE LOG_PAGE_SIZE	// different if multiple page sizes

#define LOG_SEGMENT_SIZE 28

#define HW_SEGTAB_SIZE 4096
