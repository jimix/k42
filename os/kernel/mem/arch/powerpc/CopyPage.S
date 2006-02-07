/************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CopyPage.S,v 1.1 2003/02/19 00:06:36 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Assembly page-copy routine cribbed from Linux
 * **************************************************************************/

#include <sys/kinclude.H>
#include <sys/syscalls.H>
#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <sys/arch/powerpc/asmConstants.H>

TOC_C_DATA_ENTRY(CopyPage)

/* based on copy_page from linux arch/ppc64/kernel/misc.S */
	
C_TEXT_ENTRY(CopyPage)
	clrrdi	r3,r3,12           	    /* Page align */
	clrrdi	r4,r4,12           	    /* Page align */
	mtctr	r5
	addi	r3,r3,-8
	addi	r4,r4,-8

1:      ld 	r6,8(r4)
        ldu	r7,16(r4)
	std     r6,8(r3) 
	stdu    r7,16(r3) 
	bdnz+	1b
	blr  
C_TEXT_END(CopyPage)
