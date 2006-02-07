/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultAsmUser.S,v 1.2 2003/10/28 21:12:32 rosnbrg Exp $
 *****************************************************************************/

#include <sys/kinclude.H>
#include <sys/syscalls.H>
#include <misc/asm.h>
#include <misc/arch/powerpc/asdef.h>
#include <sys/arch/powerpc/asmConstants.H>

/*
 * The kernel has fast-path implementations for the system calls that the
 * kernel itself uses.  User-mode invocations of these system calls are
 * collected here in a file that is not linked into the kernel.
 */

CODE_ENTRY(DispatcherDefault_SyscallIPCReturn)
	lis	r0,SYSCALL_IPC_RTN
	sc
CODE_END(DispatcherDefault_SyscallIPCReturn)

CODE_ENTRY(DispatcherDefault_SyscallYieldProcessor)
	lis	r0,SYSCALL_PROC_YIELD
	sc
CODE_END(DispatcherDefault_SyscallYieldProcessor)

CODE_ENTRY(DispatcherDefault_SyscallHandoffProcessor)
	lis	r0,SYSCALL_PROC_HANDOFF
	sc
CODE_END(DispatcherDefault_SyscallHandoffProcessor)
