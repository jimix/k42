#ifndef __EXCEPTION_LOCAL_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __EXCEPTION_LOCAL_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocal.H,v 1.20 2003/06/04 14:17:36 rosnbrg Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include __MINC(amd64_hw.H)
#include "init/kernel.H"
#include "sys/VolatileState.H"
#include <arch/amd64/segnum.h>              // NUM_GDT_ENTRIES, ...

/*
 * Structure for holding the registers that are passed through intact
 * on IPC, needed for remote IPC delivery.
 */
struct IPCRegsArch {
    uval64 rax;
    uval64 rbx;
    uval64 rcx;
    uval64 rdx;
    uval64 rbp;
    uval64 rsi;
    uval64 rdi;
    uval64 r8;
    uval64 r9;
    uval64 r13;
    uval64 r14;
    uval64 r15;
    uval64 float_savearea[FPR_SAVE_AREA_SIZE/sizeof(uval64)];
};

struct ExceptionMachine {
    VolatileState trapVolatileState;

    uval physCPU;				// physical cpu number
    uval logicalCPU;			// logical cpu number

    uval64 pendingIntrs;		// pending interrupts
    uval intrHandlerThr;		// interrupt handler thread

    X86Task x86Task;
    SegDesc Gdt[NUM_GDT_ENTRIES];

    void machineInit(VPNum vp, MemoryMgrPrimitiveKern *memory) {};
    void enterDebuggerArch() {}
    void exitDebuggerArch() {}

} __attribute__ ((aligned (16))); /* we must try to keep this 16-byte aligned */
/* see the comments in os/kernel/sys/arch/amd64/VolatileState.H */

extern code ExceptionLocal_LaunchUserOrKernel;

extern code ExceptionLocal_TrapException;
extern code ExceptionLocal_PgfltException;

extern code ExceptionLocal_IOInterrupt;
extern code ExceptionLocal_TimerInterrupt;
extern code ExceptionLocal_IPInterrupt;

extern code ExceptionLocal_SetEntryPointSyscall;
extern code ExceptionLocal_ProcessYieldSyscall;
extern code ExceptionLocal_ProcessHandoffSyscall;
extern code ExceptionLocal_IPCCallSyscall;
extern code ExceptionLocal_IPCReturnSyscall;
extern code ExceptionLocal_PPCPrimitiveSyscall;
extern code ExceptionLocal_IPCAsyncSyscall;
extern code ExceptionLocal_TimerRequestSyscall;
extern code ExceptionLocal_NullTimerInterrupt;
