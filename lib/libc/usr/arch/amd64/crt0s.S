/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: crt0s.S,v 1.9 2004/04/06 21:00:35 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: assembly code for crt0
 * **************************************************************************/

// VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV straight from x86 VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV FIXME pdb

#include <sys/kinclude.H>
#include <misc/asm.h>
#include <sys/syscalls.H>
#include <sys/arch/amd64/asmConstants.H>

#ifndef NDEBUG

#define BOOT_STACK()							\
	LOAD_C_DATA_UVAL(%rsp,extRegsLocal,0);				\
	leaq	XR_dispatcher(%rsp), %rsp;				\
	leaq	SCHED_DISPATCHER_SPACE+ProgExec_BOOT_STACK_SIZE(%rsp), %rsp;	\
        pushq   $0;							\
        pushq   $0;							\
        movq    %rsp,%rbp
#else

#define BOOT_STACK()							\
	LOAD_C_DATA_UVAL(%rsp,extRegsLocal,0);				\
	leaq	XR_dispatcher(%rsp), %rsp;				\
	leaq	SCHED_DISPATCHER_SPACE+ProgExec_BOOT_STACK_SIZE(%rsp), %rsp;	\
        pushq   $0;

#endif


//C_TEXT_ENTRY(__start)
C_TEXT_ENTRY(_start)
C_TEXT_LABEL(mainProgExecStartup)
        BOOT_STACK()
        call    C_TEXT(ProgExec_Init)	// do like powerpc, hopes it is OK pdb XXX
        // NOTREACHED
        int     $3

ENTRY_POINT_DESC(SecondaryStartDesc, ProgExec0_SecondaryStart)
CODE_ENTRY(ProgExec0_SecondaryStart)
        BOOT_STACK()
        call    C_TEXT(ProgExec_Init)
        // NOTREACHED
        int     $3

ENTRY_POINT_DESC(ForkChildDesc, ProgExec0_ForkChildStart)
CODE_ENTRY(ProgExec0_ForkChildStart)
	BOOT_STACK()
	call	C_TEXT(ProgExec_ForkChild)
	/* NOTREACHED */
        int     $3
