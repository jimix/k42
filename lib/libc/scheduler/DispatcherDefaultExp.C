/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: DispatcherDefaultExp.C,v 1.85 2004/09/16 21:05:17 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Static functions that are the C implementations of dispatcher
 *    functionality that is expected to be coded in assembly language.
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/BaseProcess.H>
#include <scheduler/DispatcherDefaultExp.H>
#include <scheduler/Scheduler.H>
#include <scheduler/DispatcherDefault.H>
#include <sys/ppccore.H>
#include <cobj/XHandleTrans.H>
#include <sys/syscalls.H>
#include <sys/Dispatcher.H>
#include <xobj/XBaseObj.H>
#include <misc/expedient.H>
#include <trace/traceScheduler.h>

/*
 * All the static functions in DispatcherDefaultExp are called from assembly
 * language via the C-linkage wrapper functions defined here.
 */

#define WRAPPER(cls, func) \
    extern "C" void cls##_##func(DispatcherDefaultExpRegs *erp) \
						    {cls::func(erp);}

WRAPPER(DispatcherDefaultExp, RunEntry)
WRAPPER(DispatcherDefaultExp, PPCClient)
WRAPPER(DispatcherDefaultExp, IPCCallEntry)
WRAPPER(DispatcherDefaultExp, PPCServerOnThread)
WRAPPER(DispatcherDefaultExp, IPCReturnEntry)
WRAPPER(DispatcherDefaultExp, IPCFaultEntry)
WRAPPER(DispatcherDefaultExp, IPCFaultOnThread)

static inline void
GotoAsm(DispatcherDefaultExpRegs *erp, code &func)
{
    GotoLegitimateAsm((ExpRegs *) erp, func);
}

static inline void
CallAsm(DispatcherDefaultExpRegs *erp, code &func)
{
    CallLegitimateAsm((ExpRegs *) erp, func);
}

/* From local::runEntry, disabled, on sched stack, no reg defined, no return */
/*static*/ void
DispatcherDefaultExp::RunEntry(DispatcherDefaultExpRegs *erp)
{
    if (erp->dispatcher->interrupts.pending()) {
	CallAsm(erp, DispatcherDefault_ProcessInterrupts);
    }

    if (erp->dispatcher->preemptRequested) {
	erp->dispatcher->preemptRequested = 0;
	GotoAsm(erp, DispatcherDefault_SyscallYieldProcessor);
    }

GetNextThread:
    CurrentThread = erp->dispatcher->getReadyThread();
    if (CurrentThread == NULL) {
	GotoAsm(erp, DispatcherDefault_SyscallYieldProcessor);
    }

    if ((CurrentThread->groups & erp->dispatcher->barredGroups) != 0) {
	CurrentThread->next = erp->dispatcher->barredList;
	erp->dispatcher->barredList = CurrentThread;
	goto GetNextThread;
    }

    TraceOSSchedulerCurThread(uval64(CurrentThread));

    GotoAsm(erp, DispatcherDefault_SuspendCore_Continue);
}

/* From local::PPCClient, erp has 3 ipc parms,
 * calling side of ppc call, returns with ppc call complete and returncode
 * in erp register.
 */
/*static*/ void
DispatcherDefaultExp::PPCClient(DispatcherDefaultExpRegs *erp)
{
    if (DispatcherDefault::IsDisabled()) {
	tassertSilent(erp->dispatcher->allowPrimitivePPCFlag, BREAKPOINT);
	Thread *const savedCurrentThread = CurrentThread;
	CallAsm(erp, DispatcherDefault_PPCPrimitiveClientCore);
	CurrentThread = savedCurrentThread;
    } else {
	DispatcherDefault::Disable();
	tassertSilent(CurrentThread->groups == 0, BREAKPOINT);
	CurrentThread->targetID = erp->PPC_calleeID;
	erp->PPC_threadID = CurrentThread->threadID;
	CallAsm(erp, DispatcherDefault_PPCClientCore);
	CurrentThread->targetID = SysTypes::COMMID_NULL;
	DispatcherDefault::Enable();
    }
}

/* From local::IPCCallEntry, disabled, on sched stack, ipc reg defined, handles
 * calls, no return
 */
/*static*/ void
DispatcherDefaultExp::IPCCallEntry(DispatcherDefaultExpRegs *erp)
{
    CurrentThread = erp->dispatcher->allocThread();
    if (CurrentThread == NULL) {
	erp->returnCode = _SERROR(1286, 0, ENOMEM);
	RESET_PPC_LENGTH();
	GotoAsm(erp, DispatcherDefault_SyscallIPCReturn);
    }

    GotoAsm(erp, DispatcherDefault_PPCServerOnThread);
}

/* From local::IPCCallEntry, disabled, on thread stack for ipc call handler,
 * func + methodnum + xobj in erp reg, no return.  Assumes calleeid is
 * set on return from Asm call, along with other ppc parameters and returncode
 * (most are just maintained in the registers, such as thread key and
 * callerid which becomes calleeid).
 */
/*static*/ void
DispatcherDefaultExp::PPCServerOnThread(DispatcherDefaultExpRegs *erp)
{
    DispatcherDefault::Enable();

    CurrentThread->activate();

    uval seqNo = XHANDLE_SEQNO(erp->PPC_xHandle);
    uval idx   = XHANDLE_IDX(erp->PPC_xHandle);

    if (idx >= erp->dispatcher->published.xhandleTableLimit) {
	erp->returnCode = _SERROR(1414, 0, EPERM);
	goto ErrorReturn;
    }

    erp->PPC_xObj = &erp->dispatcher->published.xhandleTable[idx];

    // make sure the method number is valid
    if ((erp->PPC_methodNum < XBaseObj::FIRST_METHOD) ||
	    (erp->PPC_methodNum >= erp->PPC_xObj->__nummeth)) {
	erp->returnCode = _SERROR(1111, erp->PPC_methodNum, EPERM);
	goto ErrorReturn;
    }

    // nummeth is the "lock" - must not fetch any other xobject values
    // until after the sync.
    SyncAfterAcquire();

    if (seqNo != erp->PPC_xObj->seqNo) {
	erp->returnCode = _SERROR(1415, 0, EPERM);
	goto ErrorReturn;
    }

    // we have a method that looks ok => call it
    erp->PPC_function =
	(XBaseObj::GetFTable(erp->PPC_xObj))[erp->PPC_methodNum].getFunc();

    TraceOSSchedulerPPCXobjFCT(
		    uval64(CurrentThread), uval64(erp->PPC_function));

    CallAsm(erp, DispatcherDefault_InvokeXObjMethod);

NormalReturn:
    CurrentThread->deactivate();
    DispatcherDefault::Disable();
    erp->dispatcher = DISPATCHER;
    erp->dispatcher->freeThread(CurrentThread);
    GotoAsm(erp, DispatcherDefault_SyscallIPCReturn);
ErrorReturn:
    RESET_PPC_LENGTH();
    goto NormalReturn;
}

/* From local::IPCReturnEntry, disabled, on sched stack, ipc reg defined,
 * handles returns, no return
 */
/*static*/ void
DispatcherDefaultExp::IPCReturnEntry(DispatcherDefaultExpRegs *erp)
{
    uval key = Thread::GetKey(erp->PPC_threadID);
    if (key < erp->dispatcher->threadArraySize) {
	CurrentThread = erp->dispatcher->threadArray[key];
	if ((CurrentThread != NULL) &&
	    SysTypes::COMMID_PID_MATCH(CurrentThread->targetID,
				       erp->PPC_callerID))
	{
	    TraceOSSchedulerCurThread(uval64(CurrentThread));
	    GotoAsm(erp, DispatcherDefault_PPCClientCore_Continue);
	}
    }

    // drop this request and pretend nothing happened
    tassertWrn(0, "Illegal ppc reply <key=%ld, callerID=%lx>\n",
	       key, erp->PPC_callerID);
    RESET_PPC_LENGTH();
    GotoAsm(erp, DispatcherDefault_RunEntry);
}

/* From local::IPCFaultEntry, disabled, on sched stack, ipc reg defined,
 * handles ipc failures and refusals, no return
 */
/*static*/ void
DispatcherDefaultExp::IPCFaultEntry(DispatcherDefaultExpRegs *erp)
{
    TraceOSSchedulerIPCFault(erp->dispatcher->ipcFaultReason);
    if (_SGENCD(erp->dispatcher->ipcFaultReason) == EAGAIN) {
	/*
	 * Warning, PPC page is likely in use here, better not print.
	 */
	CurrentThread = erp->dispatcher->allocThread();
	tassertSilent(CurrentThread != NULL, BREAKPOINT);
	TraceOSSchedulerCurThread(uval64(CurrentThread));
	GotoAsm(erp, DispatcherDefault_IPCFaultOnThread);
    }

    RESET_PPC();
    if (_SCLSCD(erp->dispatcher->ipcFaultReason) == SYSCALL_IPC_CALL) {
	tassertWrn(0, "IPC call failed, rc %lx, resuming caller.\n",
		   erp->dispatcher->ipcFaultReason);
	uval key = Thread::GetKey(erp->PPC_threadID);
	tassert(key < erp->dispatcher->threadArraySize,
		err_printf("bad thread key %ld.\n", key));
	CurrentThread = erp->dispatcher->threadArray[key];
	tassert(CurrentThread != NULL,
		err_printf("bad thread (key %ld).\n", key));
	TraceOSSchedulerCurThread(uval64(CurrentThread));
	tassert(SysTypes::COMMID_PID_MATCH(CurrentThread->targetID,
					   erp->PPC_callerID),
		err_printf("bad callerID %lx.\n", erp->PPC_callerID));
	erp->returnCode = erp->dispatcher->ipcFaultReason;
	GotoAsm(erp, DispatcherDefault_PPCClientCore_Continue);
    } else {
	tassertWrn(0, "IPC reply failed, rc %lx, continuing.\n",
		   erp->dispatcher->ipcFaultReason);
	GotoAsm(erp, DispatcherDefault_RunEntry);
    }
}

/* From local::IPCFaultEntry, disabled, on thread stack, all ipc parameters
 * erp regs, no return.
 */
/*static*/ void
DispatcherDefaultExp::IPCFaultOnThread(DispatcherDefaultExpRegs *erp)
{
    // Preserve ipcFaultReason across Suspend().
    SysStatus reason = erp->dispatcher->ipcFaultReason;

    CurrentThread->next =
	erp->dispatcher->ipcRetryList[erp->dispatcher->ipcRetryID];
    erp->dispatcher->ipcRetryList[erp->dispatcher->ipcRetryID] = CurrentThread;

    PRESERVE_PPC_PAGE();

    CurrentThread->blocking();
    DispatcherDefault_Suspend(erp->dispatcher);

    RESTORE_PPC_PAGE();

    erp->dispatcher->freeThread(CurrentThread);

    if (_SCLSCD(reason) == SYSCALL_IPC_CALL) {
	GotoAsm(erp, DispatcherDefault_SyscallIPCCall);
    } else {
	GotoAsm(erp, DispatcherDefault_SyscallIPCReturn);
    }
}
