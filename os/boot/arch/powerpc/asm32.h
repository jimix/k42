/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: asm32.h,v 1.1 2002/09/08 22:53:41 mostrows Exp $
 *****************************************************************************/
#ifndef _ASM32_H
#define _ASM32_H

/* defined for both asm and C */
#define REG_WIDTH     (4)
#define VEC_REG_WIDTH (16)

#define SPRN_CSRR0	0x03a
#define SPRN_CSRR1	0x03b
#define SPRN_DEAR	0x03d
#define SPRN_DEC	0x016
#define SPRN_DECAR	0x036
#define SPRN_ESR	0x03e
#define SPRN_IVPR	0x03f
#define SPRN_PID	0x030
#define SPRN_MMUCR	0x3b2	/* SPR definition, used in inline asm */
#define SPRN_TCR	0x154
#define SPRN_USPRG0	0x100

#define SPRN_TCR_WP	0xc0000000
#define SPRN_TCR_WRC	0x30000000
#define SPRN_TCR_WIE	0x08000000
#define SPRN_TCR_DIE	0x04000000
#define SPRN_TCR_FP	0x03000000
#define SPRN_TCR_FIE	0x00800000
#define SPRN_TCR_ARE	0x00400000


#ifdef __ASSEMBLY__
/* asm only */

#define DOT .
#define TEXT_ENTRY(x) \
	 .section ".text"; .align 2; GLBL_LABEL(x)
#define TEXT_END(x) \
	 .size	x,DOT-x
#define DATA_ENTRY(x) \
	.section ".data"; .align x;

#define PTESYNC	nop /* XXX check me */
#define SHIFT_R(a,r) ((a)>>(r))
#define SHIFT_L(a,l) ((a)<<(l))

#define GLBL_LABEL(x) .globl x; x:
#define C_TEXT(x) x
#define C_DATA(x) x

#define C_TEXT_ENTRY(x) \
	TEXT_ENTRY(C_TEXT(x))

#define C_TEXT_END(x) \
        TEXT_END(C_TEXT(x))

#define C_DATA_ENTRY(x) \
	DATA_ENTRY(3) \
	GLBL_LABEL(C_DATA(x))

#define CALL_CFUNC(rn) \
	mtctr	rn; \
	bctrl

#define LOADADDR(rn,name) \
	lis rn,name##@ha; \
	ori rn,rn,name##@l

/* 32-bit load/store */
#define LDR lwz
#define STR stw

#define	CMPI	cmpwi 0,
#define CMPL	cmplw 0,

#define MTMSR	mtmsr

#define RFI	rfi

#define HCALL_VEC_IDX_SHIFT	1
#define HCALL_VEC_MASK_NUM	(29-HCALL_VEC_IDX_SHIFT)
#define RLICR	extlwi	/* XXX check me! possibly off by 1 on the mask */

#else	/* __ASSEMBLY__ */
/* C only */

#define PTESYNC	"nop" /* XXX check me */
#define RFI	"rfi"

#endif	/* __ASSEMBLY__ */

#endif /* ! _ASM32_H */
