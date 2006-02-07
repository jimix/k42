/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/

#include <sys/kinclude.H>

// VVV 

#include <misc/asm.h>
#include <sys/syscalls.H>
#include <sys/arch/amd64/asmConstants.H>
#include <misc/expedient.H>

#	extern "C" SysTime _TIMER_REQUEST(SysTime when,
#                                     TimerRequest::Kind kind)
#	on entry rdi contains when, rsi kind
C_TEXT_ENTRY(_TIMER_REQUEST)	
#ifndef NDEBUG
	pushq	%rbp
	movq	%rsp,%rbp
#endif
	int $(SYSCALL_TIMER_REQUEST)
#ifndef NDEBUG
	movq	%rbp,%rsp
	popq	%rbp
#endif
	ret


