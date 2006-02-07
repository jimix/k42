/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: align.C,v 1.4 2004/08/27 20:16:49 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *	Alignment exception handler.  Based on the file
 *	arch/ppc64/kernel/align.c borrowed from Linux 2.6 and modified
 *	for K42 by Bryan Rosenburg.
 * **************************************************************************/

/* align.c - handle alignment exceptions for the Power PC.
 *
 * Copyright (c) 1996 Paul Mackerras <paulus@cs.anu.edu.au>
 * Copyright (c) 1998-1999 TiVo, Inc.
 *   PowerPC 403GCX modifications.
 * Copyright (c) 1999 Grant Erickson <grant@lcse.umn.edu>
 *   PowerPC 403GCX/405GP modifications.
 * Copyright (c) 2001-2002 PPC64 team, IBM Corp
 *   64-bit and Power4 support
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <sys/VolatileState.H>
#include <scheduler/DispatcherDefault.H>
#include <scheduler/DispatcherDefaultExp.H>

#define INVALID	{ 0, 0 }

#define LD	0x01	/* load */
#define ST	0x02	/* store */
#define	SE	0x04	/* sign-extend value */
#define F	0x08	/* to/from fp regs */
#define U	0x10	/* update index register */
#define M	0x20	/* multiple load/store */
#define SW	0x40	/* byte swap */

#define DCBZ	0x5f	/* 8xx/82xx dcbz faults when cache not enabled */

/*
 * The PowerPC stores certain bits of the instruction that caused the
 * alignment exception in the DSISR register.  This array maps those
 * bits to information about the operand length and what the
 * instruction would do.
 */
static struct {
    uval8 len;
    uval8 flags;
} aligninfo[128] = {
	{ 4, LD },		/* 00 0 0000: lwz / lwarx */
	INVALID,		/* 00 0 0001 */
	{ 4, ST },		/* 00 0 0010: stw */
	INVALID,		/* 00 0 0011 */
	{ 2, LD },		/* 00 0 0100: lhz */
	{ 2, LD+SE },		/* 00 0 0101: lha */
	{ 2, ST },		/* 00 0 0110: sth */
	{ 4, LD+M },		/* 00 0 0111: lmw */
	{ 4, LD+F },		/* 00 0 1000: lfs */
	{ 8, LD+F },		/* 00 0 1001: lfd */
	{ 4, ST+F },		/* 00 0 1010: stfs */
	{ 8, ST+F },		/* 00 0 1011: stfd */
	INVALID,		/* 00 0 1100 */
	{ 8, LD },		/* 00 0 1101: ld */
	INVALID,		/* 00 0 1110 */
	{ 8, ST },		/* 00 0 1111: std */
	{ 4, LD+U },		/* 00 1 0000: lwzu */
	INVALID,		/* 00 1 0001 */
	{ 4, ST+U },		/* 00 1 0010: stwu */
	INVALID,		/* 00 1 0011 */
	{ 2, LD+U },		/* 00 1 0100: lhzu */
	{ 2, LD+SE+U },		/* 00 1 0101: lhau */
	{ 2, ST+U },		/* 00 1 0110: sthu */
	{ 4, ST+M },		/* 00 1 0111: stmw */
	{ 4, LD+F+U },		/* 00 1 1000: lfsu */
	{ 8, LD+F+U },		/* 00 1 1001: lfdu */
	{ 4, ST+F+U },		/* 00 1 1010: stfsu */
	{ 8, ST+F+U },		/* 00 1 1011: stfdu */
	INVALID,		/* 00 1 1100 */
	INVALID,		/* 00 1 1101 */
	INVALID,		/* 00 1 1110 */
	INVALID,		/* 00 1 1111 */
	{ 8, LD },		/* 01 0 0000: ldx */
	INVALID,		/* 01 0 0001 */
	{ 8, ST },		/* 01 0 0010: stdx */
	INVALID,		/* 01 0 0011 */
	INVALID,		/* 01 0 0100 */
	{ 4, LD+SE },		/* 01 0 0101: lwax */
	INVALID,		/* 01 0 0110 */
	INVALID,		/* 01 0 0111 */
	{ 0, LD },		/* 01 0 1000: lswx */
	{ 0, LD },		/* 01 0 1001: lswi */
	{ 0, ST },		/* 01 0 1010: stswx */
	{ 0, ST },		/* 01 0 1011: stswi */
	INVALID,		/* 01 0 1100 */
	{ 8, LD+U },		/* 01 0 1101: ldu */
	INVALID,		/* 01 0 1110 */
	{ 8, ST+U },		/* 01 0 1111: stdu */
	{ 8, LD+U },		/* 01 1 0000: ldux */
	INVALID,		/* 01 1 0001 */
	{ 8, ST+U },		/* 01 1 0010: stdux */
	INVALID,		/* 01 1 0011 */
	INVALID,		/* 01 1 0100 */
	{ 4, LD+SE+U },		/* 01 1 0101: lwaux */
	INVALID,		/* 01 1 0110 */
	INVALID,		/* 01 1 0111 */
	INVALID,		/* 01 1 1000 */
	INVALID,		/* 01 1 1001 */
	INVALID,		/* 01 1 1010 */
	INVALID,		/* 01 1 1011 */
	INVALID,		/* 01 1 1100 */
	INVALID,		/* 01 1 1101 */
	INVALID,		/* 01 1 1110 */
	INVALID,		/* 01 1 1111 */
	INVALID,		/* 10 0 0000 */
	INVALID,		/* 10 0 0001 */
	{ 0, ST },		/* 10 0 0010: stwcx. */
	INVALID,		/* 10 0 0011 */
	INVALID,		/* 10 0 0100 */
	INVALID,		/* 10 0 0101 */
	INVALID,		/* 10 0 0110 */
	INVALID,		/* 10 0 0111 */
	{ 4, LD+SW },		/* 10 0 1000: lwbrx */
	INVALID,		/* 10 0 1001 */
	{ 4, ST+SW },		/* 10 0 1010: stwbrx */
	INVALID,		/* 10 0 1011 */
	{ 2, LD+SW },		/* 10 0 1100: lhbrx */
	{ 4, LD+SE },		/* 10 0 1101  lwa */
	{ 2, ST+SW },		/* 10 0 1110: sthbrx */
	INVALID,		/* 10 0 1111 */
	INVALID,		/* 10 1 0000 */
	INVALID,		/* 10 1 0001 */
	INVALID,		/* 10 1 0010 */
	INVALID,		/* 10 1 0011 */
	INVALID,		/* 10 1 0100 */
	INVALID,		/* 10 1 0101 */
	INVALID,		/* 10 1 0110 */
	INVALID,		/* 10 1 0111 */
	INVALID,		/* 10 1 1000 */
	INVALID,		/* 10 1 1001 */
	INVALID,		/* 10 1 1010 */
	INVALID,		/* 10 1 1011 */
	INVALID,		/* 10 1 1100 */
	INVALID,		/* 10 1 1101 */
	INVALID,		/* 10 1 1110 */
	INVALID,		/* 10 1 1111: dcbz - handled as special case */
	{ 4, LD },		/* 11 0 0000: lwzx */
	INVALID,		/* 11 0 0001 */
	{ 4, ST },		/* 11 0 0010: stwx */
	INVALID,		/* 11 0 0011 */
	{ 2, LD },		/* 11 0 0100: lhzx */
	{ 2, LD+SE },		/* 11 0 0101: lhax */
	{ 2, ST },		/* 11 0 0110: sthx */
	INVALID,		/* 11 0 0111 */
	{ 4, LD+F },		/* 11 0 1000: lfsx */
	{ 8, LD+F },		/* 11 0 1001: lfdx */
	{ 4, ST+F },		/* 11 0 1010: stfsx */
	{ 8, ST+F },		/* 11 0 1011: stfdx */
	INVALID,		/* 11 0 1100 */
	{ 8, LD+M },		/* 11 0 1101: lmd */
	INVALID,		/* 11 0 1110 */
	{ 8, ST+M },		/* 11 0 1111: stmd */
	{ 4, LD+U },		/* 11 1 0000: lwzux */
	INVALID,		/* 11 1 0001 */
	{ 4, ST+U },		/* 11 1 0010: stwux */
	INVALID,		/* 11 1 0011 */
	{ 2, LD+U },		/* 11 1 0100: lhzux */
	{ 2, LD+SE+U },		/* 11 1 0101: lhaux */
	{ 2, ST+U },		/* 11 1 0110: sthux */
	INVALID,		/* 11 1 0111 */
	{ 4, LD+F+U },		/* 11 1 1000: lfsux */
	{ 8, LD+F+U },		/* 11 1 1001: lfdux */
	{ 4, ST+F+U },		/* 11 1 1010: stfsux */
	{ 8, ST+F+U },		/* 11 1 1011: stfdux */
	INVALID,		/* 11 1 1100 */
	INVALID,		/* 11 1 1101 */
	INVALID,		/* 11 1 1110 */
	INVALID,		/* 11 1 1111 */
};

#define SWAP(a, b, t)	((t) = (a), (a) = (b), (b) = (t))

extern "C" sval
fix_alignment(uval trapNumber, uval trapInfo, uval trapAuxInfo,
	      VolatileState *vsp, NonvolatileState *nvsp)
{
    uval instr, nb, flags, tmp, reg, areg, i, dsisr, linesize;
    uval8 *addr, *p;
    uval64 *lp;

    union {
	sval64 ll;
	double dd;
	uval8 v[8];
	struct {
	    uval8 hi32[4];
	    float low32;
	} f32;
	struct {
	    uval8 hi32[4];
	    sval32 low32;
	} x32;
	struct {
	    uval8 hi48[6];
	    sval16 low16;
	} x16;
    } data;

    /*
     * Return 0 on success
     * Return -1 if unable to handle the interrupt
     */

    dsisr = trapInfo;

    /* extract the operation and registers from the dsisr */
    reg = (dsisr >> 5) & 0x1f;		/* source/dest register */
    areg = dsisr & 0x1f;		/* register to update */
    instr = (dsisr >> 10) & 0x7f;
    instr |= (dsisr >> 13) & 0x60;

    /* DAR has the operand effective address */
    addr = (uval8 *) trapAuxInfo;

    /*
     * Special handling for dcbz
     * dcbz may give an alignment exception for accesses to caching inhibited
     * storage
     */
    if (instr == DCBZ) {
	linesize = KernelInfo::PCacheLineSize();
	lp = (uval64 *) (((uval64) addr) & ~(linesize - 1));
	for (i = 0; i < linesize / sizeof(uval64); i++) {
	    *lp++ = 0;
	}
	vsp->iar += 4;	// skip trapping instruction
	return 0;
    }

    /* Lookup the operation in our table */
    nb = aligninfo[instr].len;
    flags = aligninfo[instr].flags;

    /* A size of 0 indicates an instruction we don't support */
    /* we also don't support the multiples (lmw, stmw, lmd, stmd) */
    if ((nb == 0) || (flags & M)) {
	return -1;			/* too hard or invalid instruction */
    }
    
    /* If we are loading, get the data */
    if (flags & LD) {
	data.ll = 0;
	p = addr;
	switch (nb) {
	case 8:
	    data.v[0] = *p++;
	    data.v[1] = *p++;
	    data.v[2] = *p++;
	    data.v[3] = *p++;
	case 4:
	    data.v[4] = *p++;
	    data.v[5] = *p++;
	case 2:
	    data.v[6] = *p++;
	    data.v[7] = *p++;
	}
    }
    
    /* If we are storing, get the data from the saved gpr or fpr */
    if (flags & ST) {
	if (flags & F) {
	    if     (reg <= 13)   { data.ll = (&vsp->f0)[reg - 0]; }
	    else /* reg >= 14 */ { data.ll = (&nvsp->f14)[reg - 14]; }
	    if (nb == 4) {
		/* Doing stfs, have to convert to single */
		data.f32.low32 = data.dd;
	    }
	} else {
	    if      (reg <= 13)   { data.ll = (&vsp->r0)[reg - 0]; }
	    else  /* reg >= 14 */ { data.ll = (&nvsp->r14)[reg - 14]; }
	}
    }
    
    /* Swap bytes as needed */
    if (flags & SW) {
	if (nb == 2) {
	    SWAP(data.v[6], data.v[7], tmp);
	} else {	/* nb must be 4 */
	    SWAP(data.v[4], data.v[7], tmp);
	    SWAP(data.v[5], data.v[6], tmp);
	}
    }
    
    /* Sign extend as needed */
    if (flags & SE) {
	if (nb == 2) {
	    data.ll = data.x16.low16;
	} else {	/* nb must be 4 */
	    data.ll = data.x32.low32;
	}
    }
    
    /* If we are loading, move the data to the gpr or fpr */
    if (flags & LD) {
	if (flags & F) {
	    if (nb == 4) {
		/* Doing lfs, have to convert to double */
		data.dd = data.f32.low32;
	    }
	    if     (reg <= 13)   { (&vsp->f0)[reg - 0] = data.ll; }
	    else /* reg >= 14 */ { (&nvsp->f14)[reg - 14] = data.ll; }
	} else {
	    if      (reg <= 13)   { (&vsp->r0)[reg - 0] = data.ll; }
	    else  /* reg >= 14 */ { (&nvsp->r14)[reg - 14] = data.ll; }
	}
    }
    
    /* If we are storing, copy the data to the user */
    if (flags & ST) {
	p = addr;
	switch (nb) {
	case 8:
	    *p++ = data.v[0];
	    *p++ = data.v[1];
	    *p++ = data.v[2];
	    *p++ = data.v[3];
	case 4:
	    *p++ = data.v[4];
	    *p++ = data.v[5];
	case 2:
	    *p++ = data.v[6];
	    *p++ = data.v[7];
	}
    }
    
    /* Update RA as needed */
    if (flags & U) {
	if      (areg <= 13)   { (&vsp->r0)[areg - 0] = (uval64) addr; }
	else  /* areg >= 14 */ { (&nvsp->r14)[areg - 14] = (uval64) addr; }
    }

    vsp->iar += 4;	// skip trapping instruction
    return 0;
}
