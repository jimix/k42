#ifndef __EXCEPTION_EXP_H_
#define __EXCEPTION_EXP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionExp.H,v 1.20 2004/04/06 21:00:38 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Static functions are the C implementations of exception-level
 *    functionality that is expected to be coded in assembly language.
 * **************************************************************************/

#include __MINC(ExceptionExp.H)

class ExceptionExp {
public:
    static void NullGenericEntryException(ExceptionExpRegs *erp);
    static void NullRunEntryException(ExceptionExpRegs *erp);
    static void NullInterruptEntryException(ExceptionExpRegs *erp);
    static void NullTrapEntryException(ExceptionExpRegs *erp);
    static void NullPgfltEntryException(ExceptionExpRegs *erp);
    static void NullIPCCallEntryException(ExceptionExpRegs *erp);
    static void NullIPCReturnEntryException(ExceptionExpRegs *erp);
    static void NullIPCFaultEntryException(ExceptionExpRegs *erp);
    static void NullSVCEntryException(ExceptionExpRegs *erp);

    static void AcquireReservedThread(ExceptionExpRegs *erp);
    static void ReleaseReservedThread(ExceptionExpRegs *erp);

    static void ResumeUserProcess(ExceptionExpRegs *erp);

    static void TrapExceptionUser(ExceptionExpRegs *erp);

    static void HandleUserPgflt(ExceptionExpRegs *erp);
    static void HandleKernelPgflt(ExceptionExpRegs *erp);

    static void UserInterruptContinue(ExceptionExpRegs *erp);
    static void KernelInterruptContinue(ExceptionExpRegs *erp);

    static void NonnativeSyscall(ExceptionExpRegs *erp);

    static void IPCCallSyscall(ExceptionExpRegs *erp);
    static void IPCReturnSyscall(ExceptionExpRegs *erp);
    static void IPCSyscallRemoteOnThread(ExceptionExpRegs *erp);
    static void KernelReplyRemoteOnThread(ExceptionExpRegs *erp);
    static void AcceptRemoteIPC(ExceptionExpRegs *erp);
    static void PPCPrimitiveSyscall(ExceptionExpRegs *erp);
    static void IPCAsyncSyscall(ExceptionExpRegs *erp);
    static void IPCAsyncSyscallRemoteOnThread(ExceptionExpRegs *erp);
    static void InvalidSyscall(ExceptionExpRegs *erp);
};

#endif /* #ifndef __EXCEPTION_EXP_H_ */
