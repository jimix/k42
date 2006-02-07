#ifndef __ENTRY_POINTS_H_
#define __ENTRY_POINTS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: entryPoints.H,v 1.9 2003/11/13 15:32:30 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Define the entry points for processes
 * **************************************************************************/

enum EntryPointNumber {
    RUN_ENTRY = 0,
    INTERRUPT_ENTRY,
    TRAP_ENTRY,
    PGFLT_ENTRY,
    IPC_CALL_ENTRY,
    IPC_RTN_ENTRY,
    IPC_FAULT_ENTRY,
    SVC_ENTRY,
    NUM_ENTRY_POINTS
};

#endif /* #ifndef __ENTRY_POINTS_H_ */
