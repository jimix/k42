#ifndef __MEMORY_MAP_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: memoryMap.H,v 1.4 2003/03/12 14:13:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Declare well-known symbols.
 * **************************************************************************/

/* on amd64 addresses must be normalized, i.e. sign extended to 64 bit
 * lest GP fault.
 */

#ifdef notyet		// future XXX
#define USER_REGIONS_START		0x0000000000000000

// Pre-defined location for ld.so
#define LDSO_ADDR			0x00000000A0000000
#define USER_REGIONS_ALLOC_START	0x0000000100000000
// Location for K42 library, and it's referring library
#define K42LIB_ADDR			0x0000100070000000
#define K42ALIAS_ADDR			0x0000100050000000
//  Where to put workspace for transfer of data between parent/child in exec
#define K42WSPACE_ADDR			0x0000100000000000

#define USER_PSPECIFIC_BASE		0x0000700000000000
#define USER_REGIONS_END		0x0000800000000000
#define KERNEL_BOUNDARY			0xFFFF800000000000
#define KERNEL_REGIONS_START		0xFFFF900000000000
#define KERNEL_REGIONS_END		0xFFFFE00000000000
#define COMMON_PSPECIFIC_BASE		0xFFFFE00000000000
#define KERNEL_PSPECIFIC_BASE		0xFFFFE10000000000
#define TRACE_REGION_BASE		0xFFFFE20000000000
#else /* #ifdef notyet		// future XXX */
#define USER_REGIONS_START		0x0000000000000000

// Pre-defined location for ld.so
#define LDSO_ADDR			0x00000000A0000000
#define USER_REGIONS_ALLOC_START	0x0000000100000000
// Location for K42 library, and it's referring library
#define K42LIB_ADDR			0x0000100070000000
#define K42ALIAS_ADDR			0x0000100050000000
//  Where to put workspace for transfer of data between parent/child in exec
#define K42WSPACE_ADDR			0x0000100000000000

#define USER_PSPECIFIC_BASE		0x0000600000000000
#define USER_REGIONS_END		0x0000700000000000
#define KERNEL_BOUNDARY			0x0000700000000000
#define KERNEL_REGIONS_START		0x0000780000000000
#define KERNEL_REGIONS_END		0x00007E0000000000
#define COMMON_PSPECIFIC_BASE		0x00007E0000000000
#define KERNEL_PSPECIFIC_BASE		0x00007E1000000000
#define TRACE_REGION_BASE		0x00007E2000000000
#endif /* #ifdef notyet		// future XXX */
