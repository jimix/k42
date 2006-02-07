#ifndef __EXCEPTION_LOCAL_H_
#define __EXCEPTION_LOCAL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocal.H,v 1.102 2005/02/09 18:45:43 mostrows Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description: A locally aliased structure containing objects
 * and pointers to objects that have to be accessed at exception level
 * **************************************************************************/


class SegmentTable;			// Segment.H causes .H loop
class ProcessAnnex;

#include <sys/KernelInfo.H>
#include "exception/DispatchQueue.H"
#include "exception/IPCTargetTable.H"
#include "exception/MPMsgMgrException.H"
#include "exception/KernelTimer.H"
#include "exception/IPCRetryManager.H"
#include "exception/HWInterrupt.H"
#include "bilge/HWPerfMon.H"
#include "init/SysFacts.H"
#include "init/MemoryMgrPrimitiveKern.H"
#include "trace/trace.H"
#include "defines/MLSStatistics.H"

#include __MINC(ExceptionLocal.H)

struct RemoteIPCBuffer {
    RemoteIPCBuffer *next;
    VPNum            sourceVP;
    uval             size;	// size of this struct plus ipcPage size
    uval             ipcType;	// SYSCALL_IPC_CALL/RTN
    CommID           callerID;
    IPCRegsArch      ipcRegs;
    uval             ipcPageLength;
};

struct ExceptionLocal : public ExceptionMachine {

#ifdef DO_MLS_STATS
    MLSStatistics      mls;
#endif /* #ifdef DO_MLS_STATS */
    VPNum              vp;			// this EL's VP number
    DispatchQueue      dispatchQueue;
    IPCTargetTable     ipcTargetTable;		// commID -> processAnnex map
    uval               exceptionStack;		// exc stack for this processor
    KernelInfo        *kernelInfoPtr;		// kernel-writable address for
						//     kernelInfoLocal
    ProcessAnnex      *currentProcessAnnex;
    ProcessAnnex      *kernelProcessAnnex;
    SysFacts           sysFacts;
    SegmentTable      *kernelSegmentTable;	// virtual addresses
    SegmentTable      *currentSegmentTable;	// used in low level pf code
    MPMsgMgrException *exceptionMsgMgr;		// local exc-level msg manager
    HWInterrupt       *hwInterrupt;		// hw interrupt management
    HWPerfMon         *hwPerfMonRep;
    KernelTimer        kernelTimer;		// anchor of timer services
    IPCRetryManager    ipcRetryManager;		// PAs awaiting retry notifs

    uval64             trcInfoMask;		// tracing control structures
    uval               trcInfoIndexMask;	//     shadowed here for easy
    TraceControl      *trcControl;		//     exception-level access
    uval64            *trcArray;

    uval               mainDebugStack;		// primary debugger stack
    uval               auxDebugStack;		// auxiliary debugger stack
    uval               currentDebugStack;	// current debugger stack

    CPUDomainAnnex    *serverCDA;		// CDA for servers on this pp

    /*
     * Array (indexed by kernel VP number) of lists of remote IPC buffers
     * that need to be returned to the pinned allocator.  The array itself
     * is dynamically allocated by VP 0.
     */
    static RemoteIPCBuffer **OldRemoteIPCBuffers;

    // FIXME - tighten up these sizes
    static const uval KernThreadSize = 20 * 1024;
    static const uval KernPgfltStkSpace = 8 * 1024;

    void print() {
	cprintf("------- some contents of Exception Local\n");
	cprintf("\tcurrentProcessAnnex = %p\n", currentProcessAnnex);
	cprintf("\tkernelProcessAnnex = %p\n", kernelProcessAnnex);
	cprintf("\tkernelSegmentTable = %p\n", kernelSegmentTable);
	cprintf("\tcurrentSegmentTable = %p\n", currentSegmentTable);
	cprintf("\tipcTargetTabl._tableSize = %lx\n",
					    ipcTargetTable._tableSize);
	cprintf("\tipcTargetTabl._tableIndexMask = %lx\n",
					    ipcTargetTable._tableIndexMask);
	cprintf("\tipcTargetTabl._tableOffsetMask = %lx\n",
					    ipcTargetTable._tableOffsetMask);
	cprintf("\tipcTargetTabl._table = %p\n", ipcTargetTable._table);
    }

    void init(VPNum vp, MemoryMgrPrimitiveKern *memory);
    void copyTraceInfo();

    static void SoftIntrKernel(SoftIntr::IntrType intVal);

    static void MakeReady(CommID commID);
    static void HandleInterprocessorInterrupt();
    static void FreeRemoteIPCBuffer(RemoteIPCBuffer *ipcBuf);
    static void AcceptRemoteIPC(ProcessAnnex *curProc);
    static SysStatus PPCAsyncRemote(ProcessAnnex *srcProc, CommID targetID,
				    XHandle xhandle, uval methnum,
				    uval length, uval *buf);
    static void PrintStatus(uval dumpThreads);

    void setExceptionMsgMgr(MPMsgMgrException *mgr) {
	exceptionMsgMgr = mgr;
    }
    MPMsgMgrException *getExceptionMsgMgr() {
	return exceptionMsgMgr;
    }

    void setHWPerfMonRep(HWPerfMon *rep) {
        hwPerfMonRep = rep;
    }

    HWPerfMon *getHWPerfMonRep() {
        return hwPerfMonRep;
    }

    static void EnterDebugger();
    static void ExitDebugger();
    static uval InDebugger();
};

#define PROC_INDEX(pid)   (pid)

/*
 * Implemented in ExceptionLocal.C, called from ExceptionLocalAsm.S.
 */
extern "C" void ExceptionLocal_MakeCurrentProcessAnnexReady();
extern "C" SysStatus ExceptionLocal_SetEntryPoint(EntryPointNumber entryPoint,
						  EntryPointDesc entry);
extern "C" ProcessAnnex *ExceptionLocal_GetNextProcess();
extern "C" ProcessAnnex *ExceptionLocal_GetHandoffProcess(CommID targetID);

extern "C" SysStatus ExceptionLocal_PgfltHandler(ProcessAnnex *srcProc,
						 uval faultInfo,
						 uval faultAddr,
						 uval noReflection);

extern "C" void ExceptionLocal_AwaitDispatch(ProcessAnnex *srcProc);
extern "C" void ExceptionLocal_RequestRetryNotification(ProcessAnnex *target);
extern "C" void ExceptionLocal_PPCPrimitiveAwaitRetry(ProcessAnnex *srcProc,
						      CommID targetID);
extern "C" SysStatus ExceptionLocal_IPCRemote(IPCRegsArch *ipcRegsP,
					      CommID targetID,
					      uval ipcType,
					      ProcessAnnex *srcProc);
extern "C" SysStatus ExceptionLocal_PPCAsyncRemote(ProcessAnnex *srcProc,
						   CommID targetID,
						   XHandle xhandle,
						   uval methnum,
						   uval length, uval *buf);
extern "C" void ExceptionLocal_TracePPCCall(CommID newCommID);
extern "C" void ExceptionLocal_TracePPCReturn(CommID newCommID);

/*
 * Implemented in ExceptionLocalAsm.S, called from ExceptionLocal.C.
 */
extern "C" void ExceptionLocal_AcceptRemoteIPC(IPCRegsArch *ipcRegsP,
					       CommID targetID,
					       uval ipcType,
					       ProcessAnnex *srcProc);

extern code ExceptionLocal_NullGenericEntryException;
extern code ExceptionLocal_NullRunEntryException;
extern code ExceptionLocal_NullInterruptEntryException;
extern code ExceptionLocal_NullTrapEntryException;
extern code ExceptionLocal_NullPgfltEntryException;
extern code ExceptionLocal_NullIPCCallEntryException;
extern code ExceptionLocal_NullIPCReturnEntryException;
extern code ExceptionLocal_NullIPCFaultEntryException;
extern code ExceptionLocal_NullSVCEntryException;

extern code ExceptionLocal_Launch_RUN_ENTRY;
extern code ExceptionLocal_Launch_INTERRUPT_ENTRY;
extern code ExceptionLocal_Launch_TRAP_ENTRY;
extern code ExceptionLocal_Launch_PGFLT_ENTRY;
extern code ExceptionLocal_Launch_IPC_CALL_ENTRY;
extern code ExceptionLocal_Launch_IPC_RTN_ENTRY;
extern code ExceptionLocal_Launch_IPC_FAULT_ENTRY;
extern code ExceptionLocal_Launch_SVC_ENTRY;

extern code ExceptionLocal_CreateThread;
extern code ExceptionLocal_AcquireReservedThread;
extern code ExceptionLocal_ReleaseReservedThread;

extern code ExceptionLocal_MakeCurProcReady;

extern code ExceptionLocal_UserResume;
extern code ExceptionLocal_ResumeUserProcess;

extern code ExceptionLocal_KernelPgfltResume;
extern code ExceptionLocal_KernelPgfltToTrap;

extern code ExceptionLocal_UserInterruptContinue;
extern code ExceptionLocal_UserInterruptAwaitDispatch;
extern code ExceptionLocal_KernelInterruptContinue;
extern code ExceptionLocal_KernelInterruptResume;
extern code ExceptionLocal_KernelReflectInterrupt;
extern code ExceptionLocal_IdleLoopYield;

extern code ExceptionLocal_ReqRetryNotif;
extern code ExceptionLocal_AwaitDispatch_Launch_IPC_RTN_ENTRY;
extern code ExceptionLocal_AwaitDispatch_PPCPrimitiveResume;
extern code ExceptionLocal_IPCSyscallRemote;
extern code ExceptionLocal_KernelReplyRemote;
extern code ExceptionLocal_PPCPrimitiveResume;
extern code ExceptionLocal_PPCPrimitiveSyscallRetry;
extern code ExceptionLocal_IPCAsyncSyscallRemote;
extern code ExceptionLocal_IPCAsyncSyscallInterruptReturn;

extern EntryPointDesc IdleLoopDesc;

extern ExceptionLocal exceptionLocal;

#endif /* #ifndef __EXCEPTION_LOCAL_H_ */
