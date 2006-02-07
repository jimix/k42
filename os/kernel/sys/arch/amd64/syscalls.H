#ifndef __SYSCALLS_H_
<<<< include machine independant file - not this machine dependent file >>>>
#endif /* #ifndef __SYSCALLS_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: syscalls.H,v 1.5 2003/06/04 14:18:20 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

/* system call vector for amd64 design */
#define SYSCALL_SET_ENTRY_POINT 255
#define SYSCALL_PROC_YIELD      254
#define SYSCALL_PROC_HANDOFF    253
#define SYSCALL_IPC_CALL        252
#define SYSCALL_IPC_RTN         251
#define SYSCALL_PPC_PRIMITIVE   250
#define SYSCALL_IPC_ASYNC       249
#define SYSCALL_TIMER_REQUEST   248

