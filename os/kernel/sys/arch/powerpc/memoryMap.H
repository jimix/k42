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
 * $Id: memoryMap.H,v 1.13 2004/03/05 21:20:47 mergen Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Declare well-known symbols.
 * **************************************************************************/

/*
 * This address layout assumes all 64 bits of virtual address are
 * implemented.  If not, we will need to convert to architecture
 * specific solutions.  See AllocCell for the address contraints
 * imposed by the numa memory allocator.
 */
#define USER_REGIONS_START		0x0000000000000000ULL

// Pre-defined location for ld.so
#define LDSO_ADDR			0x00000000A0000000ULL

#define USER_REGIONS_ALLOC_START	0x0000010000000000ULL
// Location for K42 library, and it's referring library
#define K42LIB_ADDR			0x0000100070000000ULL
#define K42ALIAS_ADDR			0x0000100050000000ULL

//  Where to put workspace for transfer of data between parent/child in exec
#define K42WSPACE_ADDR			0x0000100000000000ULL


//  This is the big chunk of memory after the allocPool area for
//  numaNode 4095.
#define USER_PSPECIFIC_BASE		0x7FFFF80000000000ULL
#define USER_REGIONS_END		0x8000000000000000ULL
#define KERNEL_BOUNDARY			0x8000000000000000ULL
#define KERNEL_REGIONS_START		0x8000000000000000ULL
// FIXME maa
// The kernel numa node layout is not respected by these fixed
// assignments.  We need to move all the fixed memory to the
// area starting at 0xFFFFF80000000000
/*
 * the kernel is linked at C00000010000000
 * the kernel must be linked outside of start-end
 * vmapsr access starts below the kernel's virtual
 * address by an offset which depends on where
 * the kernel is in real memory.
 * we set KERNEL_REGIONS_END to a small enough
 * number so that realToVirt(0) is always greater
 * then KERNEL_REGIONS_END
 */

#define KERNEL_REGIONS_END		0xB000000000000000ULL
#define KERNEL_PSPECIFIC_BASE		0xD000000000000000ULL
#define COMMON_PSPECIFIC_BASE		0xE000000000000000ULL
#define TRACE_REGION_BASE		0xE000000010000000ULL
#define IO_REGION_BASE			0xFFFF000000000000ULL
#define LOG_NUM_IOCHUNKS		9  // lolita.S assumes <= 13
#define LOG_IOCHUNK_SIZE		20
#define NUM_IOCHUNKS			((uval)1<<LOG_NUM_IOCHUNKS)
#define IOCHUNK_SIZE			((uval)1<<LOG_IOCHUNK_SIZE)
// must preserve (IO_REGION_BASE+(NUM_IOCHUNKS*IOCHUNK_SIZE)) <= 2**64-1

/*
 * Note:  The powerpc mapping-fault handler in lolita.S assumes that the
 *        KERNEL_PSPECIFIC_BASE, COMMON_PSPECIFIC_BASE, TRACE_REGION_BASE,
 *        and IO_REGION_BASE are in numerically increasing order and are
 *        above the VMapsR virtual address range.
 */

/*
 * We don't need to know the actual TLB set size for particular
 * implementations. We just need a power-of-two that is at least as large
 * as the TLB set size of current machines.
 */
#define TLB_SET_SIZE			0x100000ULL
