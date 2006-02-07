#ifndef __SYSCALLS_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __SYSCALLS_H_ */
/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: syscalls.H,v 1.6 2003/11/13 15:32:32 rosnbrg Exp $
 *************************************************************************** */

/* system call vector for generic64 design */
/*
 * System call numbers are shifted left 16 bits and passed in r0.  Linux
 * system calls use the low-order 16 bits; all higher-order bits are 0.
 */
#define SYSCALL_NONNATIVE        0	// must be 0
#define SYSCALL_SET_ENTRY_POINT  1
#define SYSCALL_PROC_YIELD       2
#define SYSCALL_PROC_HANDOFF     3
#define SYSCALL_IPC_CALL         4
#define SYSCALL_IPC_RTN          5
#define SYSCALL_PPC_PRIMITIVE    6
#define SYSCALL_IPC_ASYNC        7
#define SYSCALL_USER_RFI         8
#define SYSCALL_TRAP_RFI         9
#define SYSCALL_TIMER_REQUEST   10
#define LAST_SYSCALL            10

#define SYSCALL_LIMIT           16 // power-of-two, greater than LAST_SYSCALL
