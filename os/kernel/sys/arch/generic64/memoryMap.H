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
 * $Id: memoryMap.H,v 1.5 2003/03/12 14:13:37 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Declare well-known symbols.
 * **************************************************************************/

#define USER_REGIONS_START		0x0000000000000000

// Pre-defined location for ld.so
#define LDSO_ADDR			0x00000000A0000000

#define USER_REGIONS_ALLOC_START	0x0000000100000000
// Location for K42 library, and it's referring library
#define K42LIB_ADDR			0x0000100070000000
#define K42ALIAS_ADDR			0x0000100050000000
//  Where to put workspace for transfer of data between parent/child in exec
#define K42WSPACE_ADDR			0x0000100000000000

#define USER_PSPECIFIC_BASE		0x7000000000000000
#define USER_REGIONS_END		0x8000000000000000
#define KERNEL_BOUNDARY			0x8000000000000000
#define KERNEL_REGIONS_START		0x8000000000000000
#define KERNEL_REGIONS_END		0xC000000000000000
#define KERNEL_PSPECIFIC_BASE		0xD000000000000000
#define COMMON_PSPECIFIC_BASE		0xE000000000000000
#define TRACE_REGION_BASE		0xE000000010000000
