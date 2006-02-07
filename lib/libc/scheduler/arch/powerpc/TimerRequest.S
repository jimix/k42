/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TimerRequest.S,v 1.14 2003/10/28 21:12:32 rosnbrg Exp $
 *****************************************************************************/

#include <sys/kinclude.H>
#include <sys/syscalls.H>
#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <sys/arch/powerpc/asmConstants.H>

#	extern "C" SysTime _TIMER_REQUEST(SysTime when,
#                                     TimerRequest::Kind kind)
#	assumes r0,r3-r12 volatile
#	syscall number passed in r0

#	sc resume at location in link register, r2 is lost

C_TEXT_ENTRY(_TIMER_REQUEST)
	LEAF_ENTER(r0)
	std	r2,STK_TOC(r1)
	bl	L0
	ld	r2,STK_TOC(r1)
	LEAF_RETURN(r0)
    L0:	lis	r0,SYSCALL_TIMER_REQUEST
	sc
C_TEXT_END(_TIMER_REQUEST)
