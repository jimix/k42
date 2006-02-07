#ifndef _PROM_H
#define _PROM_H

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: prom.h,v 1.2 2002/09/08 22:53:43 mostrows Exp $
 *****************************************************************************/
/* Based on include/asm-ppc64/prom.h */
#define PTRRELOC(x)	x
#define PTRUNRELOC(x)	x
#define RELOC(x)	x

static unsigned long reloc_offset()
{
	return 0;
}

struct reg_property {
	u64 address;
	u64 size;
};

struct reg_property32 {
	u32 address;
	u32 size;
};

struct reg_property64 {
	u64 address;
	u64 size;
};

extern void * memset(void *s, int c, unsigned n);

#endif /* _PROM_H */
