#ifndef __IPC_H_
#define __IPC_H_
/*****************************************************************************s
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/

/* this copied from linux/ipc.h.  Cannot include directly */

/*
 * Version flags for semctl, msgctl, and shmctl commands
 * These are passed as bitflags or-ed with the actual command
 */
#define IPC_OLD 0       /* Old version (no 32-bit UID support on many
                           architectures) */
#define IPC_64  0x0100  /* New version (support 32-bit UIDs, bigger
                           message sizes, etc. */

#endif // __IPC_H_
