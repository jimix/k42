/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU GPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lmb.h,v 1.3 2004/02/27 17:14:30 mostrows Exp $
 *****************************************************************************/
/* Based on Linux file include/asm-ppc64/lmb.h */

#ifndef _PPC64_LMB_H
#define _PPC64_LMB_H

/*
 * Definitions for talking to the Open Firmware PROM on
 * Power Macintosh computers.
 *
 * Copyright (C) 2001 Peter Bergner, IBM Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */


/* align addr on a size boundry - adjust address up/down if needed */
#define _ALIGN_UP(addr,size)	(((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))

#define MAX_LMB_REGIONS 128

union lmb_reg_property {
	struct reg_property32 addr32[MAX_LMB_REGIONS];
	struct reg_property64 addr64[MAX_LMB_REGIONS];
	struct reg_property_pmac addrPM[MAX_LMB_REGIONS];
};

#define LMB_MEMORY_AREA	1
#define LMB_IO_AREA	2

#define LMB_ALLOC_ANYWHERE	0
#define LMB_ALLOC_FIRST4GBYTE	(1UL<<32)
struct lmb_property {
	u64 base;
	u64 physbase;
	u64 size;
};

struct lmb_region {
	u64 cnt;
	u64 size;
	struct lmb_property region[MAX_LMB_REGIONS+1];
};

struct lmb {
	u64 debug;
	u64 rmo_size;
	struct lmb_region memory;
	struct lmb_region reserved;
};

extern struct lmb lmb;

extern void lmb_init(void);
extern void lmb_analyze(void);
extern long lmb_add(u64, u64);
extern long lmb_add_io(u64 base, u64 size);
extern long lmb_reserve(u64, u64);
extern u64 lmb_alloc(u64, u64);
extern u64 lmb_alloc_base(u64, u64, u64);
extern u64 lmb_phys_mem_size(void);
extern u64 lmb_end_of_DRAM(void);
extern u64 lmb_abs_to_phys(u64);
extern void lmb_dump(char *);



#endif /* _PPC64_LMB_H */
