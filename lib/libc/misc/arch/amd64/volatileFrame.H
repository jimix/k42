#ifndef __VOLATILE_FRAME_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __VOLATILE_FRAME_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: volatileFrame.H,v 1.5 2001/12/06 23:05:07 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Assembler machinery for creating a stack frame that preserves all the
 *    volatile gpr's and fpr's.
 * **************************************************************************/

#ifdef __ASSEMBLER__

#include <misc/asm.h>
#include "misc/arch/amd64/fpr.h"          /* floating point register save area size */

/*
  This code saves and restores the volatile registers.  The
  structure of the saved registers is defined in VolatileState.H
  KEEP THIS CODE CONSISTENT with VolatileState.H


  We do these as macros because

  (a) they are not called very many places, so the overhead of
  replicated code is small, and

  (b) we are saving the info on the same stack (there is only
  one) that would have the return address if we made them
  subroutines. So it becomes painful to pull out the return
  address, and push/pop things under them.  (if you call a
  subroutine and then push stuff on the stack the return
  address is now under the stuff on the stack, so you have
  to remove the stuff from the stack to get to the return
  address.  if the reason for the subroutine is to put stuff
  on the stack, this is counter productive.)
*/

#define PART_VOLATILE_FRAME_SAVE()	        \
	pushq	%r9;				\
	pushq	%r8;				\
	pushq	%rdi;				\
	pushq	%rsi;				\
	pushq	%rbp;				\
	pushq	%rdx;				\
	pushq	%rcx;				\
	pushq	%rax;				\
	lea	-FPR_SAVE_AREA_SIZE(%rsp),%rsp;	\
	SAVE_FLOAT_(%rsp,%rax)

#define PART_VOLATILE_FRAME_RESTORE()	        \
	RESTORE_FLOAT_(%rsp,%rax);		\
	lea	FPR_SAVE_AREA_SIZE(%rsp),%rsp;	\
	popq	%rax;				\
	popq	%rcx;				\
	popq	%rdx;				\
	popq	%rbp;				\
	popq	%rsi;				\
	popq	%rdi;				\
	popq	%r8;				\
	popq	%r9



#define VOLATILE_FRAME_ENTER()		\
	FRAME_ENTER();			\
	pushq	%r11;			\
	pushq	%r10;			\
	PART_VOLATILE_FRAME_SAVE()


#define VOLATILE_FRAME_RETURN()		\
	PART_VOLATILE_FRAME_RESTORE();	\
	popq	%r10;			\
	popq	%r11;			\
	FRAME_RETURN()


#endif /* #ifdef __ASSEMBLER__ */
