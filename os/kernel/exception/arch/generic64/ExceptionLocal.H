/* ****************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocal.H,v 1.7 2003/10/28 21:12:35 rosnbrg Exp $
 *************************************************************************** */

#ifndef __EXCEPTION_LOCAL_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __EXCEPTION_LOCAL_H_ */

/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "sys/syscalls.H"         // for SYSCALL_LIMIT

#define EXC_MAX_NUM	(32)

/*
 * Structure for holding the registers that are passed through intact
 * on IPC, needed for remote IPC delivery.
 */
struct IPCRegsArch {
    uval64 dummy;	// can't declare an empty structure
};

struct ExceptionMachine
{
    struct {uval64 r;} tmpState;	// temporary scratch space

    codeAddress handlers[EXC_MAX_NUM];	// phys jump addresses for exc handling
    codeAddress svc[SYSCALL_LIMIT];	// system call handlers

    // statistics
    uval num_ints;     			// counter for interrupts
    uval num_vdec;     			// virtual-coherency data exceptions
    uval num_viec;     			// virtual-coherency inst exceptions

    void machineInit(VPNum vp, MemoryMgrPrimitiveKern *memory) {};
    void enterDebuggerArch() {}
    void exitDebuggerArch() {}
};
