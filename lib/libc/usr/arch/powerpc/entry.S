/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2002.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: entry.S,v 1.8 2004/06/28 17:01:37 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: assembly code for crt0
 * **************************************************************************/



#include <sys/kinclude.H>
#include <sys/syscalls.H>
#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <sys/arch/powerpc/asmConstants.H>

TOC_C_DATA_ENTRY(extRegsLocal)
TOC_C_DATA_ENTRY(forkStack)

	
.weak .ProgExec_Init

C_TEXT_ENTRY(_k42start)
	/* Locate the dispatcher address.  The boot stack is right below. */
	LOAD_C_DATA_UVAL(r1,extRegsLocal,XR_dispatcher)
	li	r4, 0
	stdu	r4,-STK_SIZE(r1)
	bl	C_TEXT(ProgExec_Init)
	blr
C_TEXT_END(_k42start)

ENTRY_POINT_DESC(SecondaryStartDesc, ProgExec0_SecondaryStart)
CODE_ENTRY(ProgExec0_SecondaryStart)
	/* Stack is at the very end of the dispatcher's memory area */
	LOAD_C_DATA_UVAL(r1,extRegsLocal,XR_dispatcher)
	addis	r1,r1,SHIFT_R(ProgExec_INIT_MEM_SIZE,16)
	li	r4, 0
	stdu	r4,-STK_SIZE(r1)
	bl	C_TEXT(ProgExec_Init)
	/* NOTREACHED */
	tw	4,0,0
CODE_END(ProgExec0_SecondaryStart)

ENTRY_POINT_DESC(ForkChildDesc, ProgExec0_ForkChildStart)
CODE_ENTRY(ProgExec0_ForkChildStart)
	/* Low-end of stack is pointed to by forkStack */
	LOAD_C_DATA_UVAL(r1,forkStack,0)
	addis	r1,r1,SHIFT_R(ProgExec_THR_STK_SIZE,16)
	li	r4, 0
	stdu	r4,-STK_SIZE(r1)
	bl	C_TEXT(ProgExec_ForkChild)
	/* NOTREACHED */
	tw	4,0,0
CODE_END(ProgExec0_ForkChildStart)

