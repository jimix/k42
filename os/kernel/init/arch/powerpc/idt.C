/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: idt.C,v 1.25 2003/11/13 15:32:29 rosnbrg Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include "exception/ExceptionLocal.H"
#include "sys/syscalls.H"
#include "mem/PageAllocatorKernPinned.H"

extern code exc_exi_handler;
extern code exc_dec_handler;
extern code exc_perf_handler;
extern code exc_null_handler;

void
initExceptionHandlers(VPNum vp)
{
    exceptionLocal.svc[SYSCALL_NONNATIVE] =
					&ExceptionLocal_NonnativeSyscall;
    exceptionLocal.svc[SYSCALL_SET_ENTRY_POINT] =
					&ExceptionLocal_SetEntryPointSyscall;
    exceptionLocal.svc[SYSCALL_PROC_YIELD] =
					&ExceptionLocal_ProcessYieldSyscall;
    exceptionLocal.svc[SYSCALL_PROC_HANDOFF] =
					&ExceptionLocal_ProcessHandoffSyscall;
    exceptionLocal.svc[SYSCALL_IPC_CALL] =
					&ExceptionLocal_IPCCallSyscall;
    exceptionLocal.svc[SYSCALL_IPC_RTN] =
					&ExceptionLocal_IPCReturnSyscall;
    exceptionLocal.svc[SYSCALL_PPC_PRIMITIVE] =
					&ExceptionLocal_PPCPrimitiveSyscall;
    exceptionLocal.svc[SYSCALL_IPC_ASYNC] =
					&ExceptionLocal_IPCAsyncSyscall;
    exceptionLocal.svc[SYSCALL_USER_RFI] =
					&ExceptionLocal_UserRFISyscall;
    exceptionLocal.svc[SYSCALL_TRAP_RFI] =
	                                &ExceptionLocal_TrapRFISyscall;
    exceptionLocal.svc[SYSCALL_TIMER_REQUEST] =
					&ExceptionLocal_TimerRequestSyscall;
}

void
fixupExceptionHandlers(VPNum vp)
{
    uval physAddr;

    physAddr = PageAllocatorKernPinned::virtToReal(uval(&exc_exi_handler));
    exceptionLocal.handlers[EXC_IDX(EXC_EXI)] = (codeAddress) physAddr;

    physAddr = PageAllocatorKernPinned::virtToReal(uval(&exc_dec_handler));
    exceptionLocal.handlers[EXC_IDX(EXC_DEC)] = (codeAddress) physAddr;

    physAddr = PageAllocatorKernPinned::virtToReal(uval(&exc_perf_handler));
    exceptionLocal.handlers[EXC_IDX(EXC_PERF)] = (codeAddress)physAddr;
}

void
killExceptionHandlers()
{
    //Wipe out everything but I/O interrupts
    uval physAddr;

    physAddr = PageAllocatorKernPinned::virtToReal(uval(&exc_null_handler));
    exceptionLocal.handlers[EXC_IDX(EXC_DEC)] = (codeAddress) physAddr;
    exceptionLocal.handlers[EXC_IDX(EXC_PERF)] = (codeAddress)physAddr;
}
