/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU GPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: lmb.c,v 1.8 2004/02/27 17:14:30 mostrows Exp $
 *****************************************************************************/
/* Based on Linux file arch/ppc64/kernel/lmb.c */

/*
 *
 * Procedures for interfacing to Open Firmware.
 *
 * Peter Bergner, IBM Corp.	June 2001.
 * Copyright (C) 2001 Peter Bergner.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <stdarg.h>
#include <sys/types.H>

/* #include <asm/types.h>*/
/* #include <asm/page.h>*/
/* #include <asm/prom.h>*/

/* reach into the kernel to get the BootInfo structure */
#include "../../../kernel/bilge/arch/powerpc/BootInfo.H"
#include "lmb.h"
#define reloc_offset() 0

/* #include <asm/abs_addr.h> */
/* #include <asm/bitops.h> */

extern u64 klimit;
struct lmb lmb;


static u64
lmb_addrs_overlap(u64 base1, u64 size1,
                  u64 base2, u64 size2)
{
	return ((base1 < (base2+size2)) && (base2 < (base1+size1)));
}

static long
lmb_addrs_adjacent(u64 base1, u64 size1,
		   u64 base2, u64 size2)
{
	if (base2 == base1 + size1)
		return 1;
	else if (base1 == base2 + size2)
		return -1;

	return 0;
}

static long
lmb_regions_adjacent(struct lmb_region *rgn, u64 r1, u64 r2)
{
	u64 base1 = rgn->region[r1].base;
	u64 size1 = rgn->region[r1].size;
	u64 base2 = rgn->region[r2].base;
	u64 size2 = rgn->region[r2].size;

	return lmb_addrs_adjacent(base1, size1, base2, size2);
}

/* Assumption: base addr of region 1 < base addr of region 2 */
static void
lmb_coalesce_regions(struct lmb_region *rgn, u64 r1, u64 r2)
{
	u64 i;

	rgn->region[r1].size += rgn->region[r2].size;
	for (i=r2; i < rgn->cnt-1; i++) {
		rgn->region[i].base = rgn->region[i+1].base;
		rgn->region[i].physbase = rgn->region[i+1].physbase;
		rgn->region[i].size = rgn->region[i+1].size;
	}
	rgn->cnt--;
}

/* This routine called with relocation disabled. */
void
lmb_init(void)
{
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	memset(_lmb,0,sizeof(struct lmb));

	/* Create a dummy zero size LMB which will get coalesced away later.
	 * This simplifies the lmb_add() code below...
	 */
	_lmb->memory.region[0].base = 0;
	_lmb->memory.region[0].size = 0;
	_lmb->memory.cnt = 1;

	/* Ditto. */
	_lmb->reserved.region[0].base = 0;
	_lmb->reserved.region[0].size = 0;
	_lmb->reserved.cnt = 1;
}


/* This routine called with relocation disabled. */
void
lmb_analyze(void)
{
	u64 i;
	u64 mem_size = 0;
	u64 size_mask = 0;
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
#ifdef CONFIG_MSCHUNKS
	u64 physbase = 0;
#endif

	for (i=0; i < _lmb->memory.cnt; i++) {
		u64 lmb_size;

		lmb_size = _lmb->memory.region[i].size;

#ifdef CONFIG_MSCHUNKS
		_lmb->memory.region[i].physbase = physbase;
		physbase += lmb_size;
#else
		_lmb->memory.region[i].physbase = _lmb->memory.region[i].base;
#endif
		mem_size += lmb_size;
		size_mask |= lmb_size;
	}

	_lmb->memory.size = mem_size;
}

/* This routine called with relocation disabled. */
static long
lmb_add_region(struct lmb_region *rgn, u64 base, u64 size)
{
	u64 i, coalesced = 0;
	long adjacent;

	/* First try and coalesce this LMB with another. */
	for (i=0; i < rgn->cnt; i++) {
		u64 rgnbase = rgn->region[i].base;
		u64 rgnsize = rgn->region[i].size;

		adjacent = lmb_addrs_adjacent(base,size,rgnbase,rgnsize);
		if ( adjacent > 0 ) {
			rgn->region[i].base -= size;
			rgn->region[i].physbase -= size;
			rgn->region[i].size += size;
			coalesced++;
			break;
		}
		else if ( adjacent < 0 ) {
			rgn->region[i].size += size;
			coalesced++;
			break;
		}
	}

	if ((i < rgn->cnt-1) && lmb_regions_adjacent(rgn, i, i+1) ) {
		lmb_coalesce_regions(rgn, i, i+1);
		coalesced++;
	}

	if ( coalesced ) {
		return coalesced;
	} else if ( rgn->cnt >= MAX_LMB_REGIONS ) {
		return -1;
	}

	/* Couldn't coalesce the LMB, so add it to the sorted table. */
	for (i=rgn->cnt-1; i >= 0; i--) {
		if (base < rgn->region[i].base) {
			rgn->region[i+1].base = rgn->region[i].base;
			rgn->region[i+1].physbase = rgn->region[i].physbase;
			rgn->region[i+1].size = rgn->region[i].size;
		}  else {
			rgn->region[i+1].base = base;
			rgn->region[i+1].physbase = lmb_abs_to_phys(base);
			rgn->region[i+1].size = size;
			break;
		}
	}
	rgn->cnt++;

	return 0;
}

/* This routine called with relocation disabled. */
long
lmb_add(u64 base, u64 size)
{
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_rgn = &(_lmb->memory);

	/* On pSeries LPAR systems, the first LMB is our RMO region. */
	if ( base == 0 )
		_lmb->rmo_size = size;

	return lmb_add_region(_rgn, base, size);

}

long
lmb_reserve(u64 base, u64 size)
{
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_rgn = &(_lmb->reserved);

	return lmb_add_region(_rgn, base, size);
}

long
lmb_overlaps_region(struct lmb_region *rgn, u64 base, u64 size)
{
	u64 i;

	for (i=0; i < rgn->cnt; i++) {
		u64 rgnbase = rgn->region[i].base;
		u64 rgnsize = rgn->region[i].size;
		if ( lmb_addrs_overlap(base,size,rgnbase,rgnsize) ) {
			break;
		}
	}

	return (i < rgn->cnt) ? i : -1;
}

u64
lmb_alloc(u64 size, u64 align)
{
	return lmb_alloc_base(size, align, LMB_ALLOC_ANYWHERE);
}

u64
lmb_alloc_base(u64 size, u64 align, u64 max_addr)
{
	long i, j;
	u64 base = 0;
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_mem = &(_lmb->memory);
	struct lmb_region *_rsv = &(_lmb->reserved);

	for (i=_mem->cnt-1; i >= 0; i--) {
		u64 lmbbase = _mem->region[i].base;
		u64 lmbsize = _mem->region[i].size;

		if ( max_addr == LMB_ALLOC_ANYWHERE )
			base = _ALIGN_DOWN(lmbbase+lmbsize-size, align);
		else if ( lmbbase < max_addr )
			base = _ALIGN_DOWN(min(lmbbase+lmbsize,max_addr)-size, align);
		else
			continue;

		while ( (lmbbase <= base) &&
			((j = lmb_overlaps_region(_rsv,base,size)) >= 0) ) {
			base = _ALIGN_DOWN(_rsv->region[j].base-size, align);
		}

		if ( (base != 0) && (lmbbase <= base) )
			break;
	}

	if ( i < 0 )
		return 0;

	lmb_add_region(_rsv, base, size);

	return base;
}

u64
lmb_phys_mem_size(void)
{
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
#ifdef CONFIG_MSCHUNKS
	return _lmb->memory.size;
#else
	struct lmb_region *_mem = &(_lmb->memory);
	u64 idx = _mem->cnt-1;
	u64 lastbase = _mem->region[idx].physbase;
	u64 lastsize = _mem->region[idx].size;

	return (lastbase + lastsize);
#endif /* CONFIG_MSCHUNKS */
}

u64
lmb_end_of_DRAM(void)
{
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_mem = &(_lmb->memory);
	u64 idx;

	for (idx=_mem->cnt-1; idx >= 0; idx--) {
#ifdef CONFIG_MSCHUNKS
		return (_mem->region[idx].physbase + _mem->region[idx].size);
#else
		return (_mem->region[idx].base + _mem->region[idx].size);
#endif /* CONFIG_MSCHUNKS */
	}

	return 0;
}

u64
lmb_abs_to_phys(u64 aa)
{
	u64 i, pa = aa;
	u64 offset = reloc_offset();
	struct lmb *_lmb = PTRRELOC(&lmb);
	struct lmb_region *_mem = &(_lmb->memory);

	for (i=0; i < _mem->cnt; i++) {
		u64 lmbbase = _mem->region[i].base;
		u64 lmbsize = _mem->region[i].size;
		if ( lmb_addrs_overlap(aa,1,lmbbase,lmbsize) ) {
			pa = _mem->region[i].physbase + (aa - lmbbase);
			break;
		}
	}

	return pa;
}



static long cnt_trailing_zeros(u64 mask)
{
	if (mask == 0) return 0;
        long cnt = 0;
	while (!(mask & 1)) {
		mask= mask>>1;
		++cnt;
	}
	return cnt;
}



void
lmb_dump(char *str)
{
	u64 i;

	printf("\n\rlmb_dump: %s\n\r", str);
	printf("    debug                       = %s\n\r",
		(lmb.debug) ? "TRUE" : "FALSE");
	printf("    memory.cnt                  = %d\n\r",
		lmb.memory.cnt);
	printf("    memory.size                 = 0x%llx\n\r",
		lmb.memory.size);
	for (i=0; i < lmb.memory.cnt ;i++) {
		printf("    memory.region[%d].base       = 0x%llx\n\r",
			i, lmb.memory.region[i].base);
		printf("                      .physbase = 0x%llx\n\r",
			lmb.memory.region[i].physbase);
		printf("                      .size     = 0x%llx\n\r",
			lmb.memory.region[i].size);
	}

	printf("\n\r");
	printf("    reserved.cnt                = %d\n\r",
		lmb.reserved.cnt);
	printf("    reserved.size               = 0x%llx\n\r",
		lmb.reserved.size);
	for (i=0; i < lmb.reserved.cnt ;i++) {
		printf("    reserved.region[%d].base     = 0x%llx\n\r",
			i, lmb.reserved.region[i].base);
		printf("                      .physbase = 0x%llx\n\r",
			lmb.reserved.region[i].physbase);
		printf("                      .size     = 0x%llx\n\r",
			lmb.reserved.region[i].size);
	}
}
