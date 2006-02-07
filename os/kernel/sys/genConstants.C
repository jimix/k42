/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: genConstants.C,v 1.79 2004/11/12 21:25:53 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     generation of the machine independent and dependent assembler constants
 * **************************************************************************/

#define VAR(name)		static unsigned long __TYPE_##name

#define SIZE(entity)		(unsigned long) (sizeof(entity))
#define ENUM(type,en)		(unsigned long) (type::en)
#define CONST(name)		(unsigned long) name

#define	FIELD_OFFSET1(abrev,str,field) \
    VAR(abrev##_##field) = OFFSETOF(str,field);
#define	FIELD_OFFSET2(abrev,str,field1,field2) \
    VAR(abrev##_##field2) = OFFSETOF(str,field1.field2);
#define	FIELD_OFFSET3(abrev,str,field1,field2,field3) \
    VAR(abrev##_##field3) = OFFSETOF(str,field1.field2.field3);
#define	STRUCT_SIZE(abrev,str) \
    VAR(abrev##_SIZE) = SIZE(str);
#define	ENUM_VALUE(abrev,str,en) \
    VAR(abrev##_##en) = ENUM(str,en);
#define CONSTANT(name) \
    VAR(name) = CONST(name);
#define DEFINED_CONSTANT(newname, name) \
    VAR(newname) = CONST(name);
#define VAR_SIZE(var) \
    VAR(var##_SIZE) = SIZE(var);

#include "kernIncs.H"
#include <sys/KernelInfo.H>
#include "sys/ppccore.H"
#include "init/genConstDefs.C"
#include "init/kernel.H"
#include "mem/SegmentTable.H"
#include "exception/ExceptionLocal.H"
#include "proc/Process.H"
#include "sys/Dispatcher.H"
#include <usr/ProgExec.H>
#include <scheduler/Scheduler.H>
#include <scheduler/DispatcherDefault.H>
#include <misc/expedient.H>
#include <misc/hardware.H>
#include <xobj/XBaseObj.H>
#include <cobj/XHandleTrans.H>
#include <cobj/sys/ActiveThrdCnt.H>
#include <defines/use_expedient.H>
#include <trace/trace.H>
#include <trace/traceException.h>
#include <trace/traceScheduler.h>
#include <sync/BLock.H>
#include <sync/Sem.H>
#include <sync/FairBLock.H>

void genConstants(void)
{
    DEFINED_CONSTANT(PG_SIZE, PAGE_SIZE);

    VAR_SIZE(allocLocal);
    VAR_SIZE(activeThrdCntLocal);
    VAR_SIZE(lTransTableLocal);
    VAR_SIZE(exceptionLocal);
    VAR_SIZE(kernelInfoLocal);
    VAR_SIZE(extRegsLocal);
    VAR_SIZE(BLock);
    VAR_SIZE(FairBLock);
    VAR_SIZE(Semaphore);
//    VAR_SIZE(FairBLockTraced);
    STRUCT_SIZE(TH, Thread);
    STRUCT_SIZE(VS, VolatileState);
    STRUCT_SIZE(ER, ExpRegs);
    STRUCT_SIZE(EPL, EntryPointLauncher);

    FIELD_OFFSET1(EL, ExceptionLocal, currentProcessAnnex);
    FIELD_OFFSET1(EL, ExceptionLocal, exceptionStack);
    FIELD_OFFSET1(EL, ExceptionLocal, currentDebugStack);

    DEFINED_CONSTANT(KERN_THREAD_SIZE, ExceptionLocal::KernThreadSize);
    DEFINED_CONSTANT(KERN_PGFLT_STK_SPACE, ExceptionLocal::KernPgfltStkSpace);

    FIELD_OFFSET1(PA, ProcessAnnex, launcher);
    FIELD_OFFSET1(PA, ProcessAnnex, dispatcher);
    FIELD_OFFSET1(PA, ProcessAnnex, dispatcherUser);
    FIELD_OFFSET1(PA, ProcessAnnex, userStateOffset);
    FIELD_OFFSET1(PA, ProcessAnnex, trapStateOffset);

    FIELD_OFFSET1(TH, Thread, startSP);
    FIELD_OFFSET1(TH, Thread, curSP);
    FIELD_OFFSET1(TH, Thread, bottomSP);
    FIELD_OFFSET1(TH, Thread, truebottomSP);
    FIELD_OFFSET1(TH, Thread, altStack);
    FIELD_OFFSET1(TH, Thread, upcallNeeded);

    FIELD_OFFSET1(KI, KernelInfo, onSim);
    FIELD_OFFSET1(KI, KernelInfo, onHV);

    FIELD_OFFSET1(XR, ExtRegs, disabled);
    FIELD_OFFSET1(XR, ExtRegs, dispatcher);

    FIELD_OFFSET2(D, Dispatcher, interrupts, flags);
    FIELD_OFFSET1(D, Dispatcher, trapDisabledSave);
    FIELD_OFFSET1(D, Dispatcher, _userStateOffset);
    FIELD_OFFSET1(D, Dispatcher, _trapStateOffset);
    FIELD_OFFSET1(DD, DispatcherDefault, dispatcherStack);
    FIELD_OFFSET1(DD, DispatcherDefault, rescheduleNeeded);
    FIELD_OFFSET1(DD, DispatcherDefault, allowPrimitivePPCFlag);
    FIELD_OFFSET1(DD, DispatcherDefault, currentDebugStack);
    FIELD_OFFSET1(DD, DispatcherDefault, sandboxShepherd);

    DEFINED_CONSTANT(SCHED_DISPATCHER_SPACE, Scheduler::DISPATCHER_SPACE);
    DEFINED_CONSTANT(ProgExec_BOOT_STACK_SIZE, ProgExec::BOOT_STACK_SIZE);
    DEFINED_CONSTANT(ProgExec_THR_STK_SIZE, ProgExec::THREAD_SIZE);
    DEFINED_CONSTANT(ProgExec_INIT_MEM_SIZE, ProgExec::INIT_MEM_SIZE);
    DEFINED_CONSTANT(ProgExec_USR_STK_SIZE, ProgExec::USR_STACK_SIZE);

    FIELD_OFFSET1(AC, AllocCell, next);

    FIELD_OFFSET1(LM, LMalloc, freeList);
    FIELD_OFFSET1(LM, LMalloc, nodeID);
    FIELD_OFFSET1(LM, LMalloc, maxCount);
    FIELD_OFFSET1(LM, LMalloc, pool);
    FIELD_OFFSET1(LM, LMalloc, mallocID);
#ifdef ALLOC_STATS
    FIELD_OFFSET1(LM, LMalloc, allocs);
    FIELD_OFFSET1(LM, LMalloc, frees);
    FIELD_OFFSET1(LM, LMalloc, remoteFrees);
#endif

    CONSTANT(RUN_ENTRY);
    CONSTANT(INTERRUPT_ENTRY);
    CONSTANT(TRAP_ENTRY);
    CONSTANT(PGFLT_ENTRY);
    CONSTANT(IPC_CALL_ENTRY);
    CONSTANT(IPC_RTN_ENTRY);
    CONSTANT(IPC_FAULT_ENTRY);
    CONSTANT(SVC_ENTRY);

#if !defined(USE_EXPEDIENT_USER_PGFLT) || \
    !defined(USE_EXPEDIENT_PPC) || \
    !defined(USE_EXPEDIENT_SCHEDULER) || \
    !defined(USE_EXPEDIENT_RESERVED_THREAD) || \
    !defined(USE_EXPEDIENT_USER_RESUME) || \
    !defined(USE_EXPEDIENT_INTERRUPT) || \
    !defined(USE_EXPEDIENT_SVC)

    FIELD_OFFSET1(EL, ExceptionLocal, kernelProcessAnnex);
    FIELD_OFFSET1(EL, ExceptionLocal, currentSegmentTable);
    FIELD_OFFSET1(EL, ExceptionLocal, trcInfoMask);
    FIELD_OFFSET1(EL, ExceptionLocal, trcInfoIndexMask);
    FIELD_OFFSET1(EL, ExceptionLocal, trcControl);
    FIELD_OFFSET1(EL, ExceptionLocal, trcArray);
    FIELD_OFFSET2(EL, ExceptionLocal, ipcTargetTable, _tableOffsetMask);
    FIELD_OFFSET2(EL, ExceptionLocal, ipcTargetTable, _table);
    DEFINED_CONSTANT(RD_HASH_OFFSET,
			IPCTargetTable::RD_HASH_OFFSET);
    FIELD_OFFSET2(EL, ExceptionLocal, dispatchQueue, cdaBorrowersTop);
    FIELD_OFFSET2(EL, ExceptionLocal, dispatchQueue, cdaBorrowers);
    DEFINED_CONSTANT(LOG_CDA_BORROWERS_SIZE,
			DispatchQueue::LOG_CDA_BORROWERS_SIZE);
    DEFINED_CONSTANT(CDA_BORROWERS_SIZE,
			DispatchQueue::CDA_BORROWERS_SIZE);
    FIELD_OFFSET1(PA, ProcessAnnex, reservedThread);
    FIELD_OFFSET1(PA, ProcessAnnex, excStateOffset);
    FIELD_OFFSET1(PA, ProcessAnnex, segmentTable);
    FIELD_OFFSET1(PA, ProcessAnnex, commID);
    FIELD_OFFSET1(PA, ProcessAnnex, ipcTargetNext);
    FIELD_OFFSET1(PA, ProcessAnnex, isKernel);
    FIELD_OFFSET1(PA, ProcessAnnex, ppcTargetID);
    FIELD_OFFSET1(PA, ProcessAnnex, ppcThreadID);
    FIELD_OFFSET1(PA, ProcessAnnex, cpuDomainNext);
    DEFINED_CONSTANT(PPC_PRMTV_MARKER, uval(PPC_PRIMITIVE_MARKER));
    ENUM_VALUE(SOFTINTR, SoftIntr, PREEMPT);
    FIELD_OFFSET1(TH, Thread, next);
    FIELD_OFFSET1(TH, Thread, targetID);
    FIELD_OFFSET1(TH, Thread, threadID);
    FIELD_OFFSET1(TH, Thread, activeCntP);
    FIELD_OFFSET1(TH, Thread, groups);
    FIELD_OFFSET1(ATC, ActiveThrdCnt, genIndexAndActivationCnt);
    FIELD_OFFSET1(ATC, ActiveThrdCnt, activeCnt);
    DEFINED_CONSTANT(ATC_COUNT_BITS, ActiveThrdCnt::COUNT_BITS);
    FIELD_OFFSET1(D, Dispatcher, hasWork);
    FIELD_OFFSET1(D, Dispatcher, ipcFaultReason);
    FIELD_OFFSET1(DD, DispatcherDefault, freeList);
    FIELD_OFFSET1(DD, DispatcherDefault, threadArraySize);
    FIELD_OFFSET1(DD, DispatcherDefault, threadArray);
    FIELD_OFFSET1(DD, DispatcherDefault, preemptRequested);
    FIELD_OFFSET1(DD, DispatcherDefault, barredGroups);
    FIELD_OFFSET1(DD, DispatcherDefault, barredList);
    FIELD_OFFSET2(DD, DispatcherDefault, published, xhandleTable);
    FIELD_OFFSET2(DD, DispatcherDefault, published, xhandleTableLimit);
    FIELD_OFFSET1(XR, ExtRegs, ppcPageLength);
    DEFINED_CONSTANT(VP_WILD, SysTypes::VP_WILD);
    DEFINED_CONSTANT(COMMID_VP_SHIFT, SysTypes::COMMID_VP_SHIFT);
    DEFINED_CONSTANT(PID_BITS, SysTypes::PID_BITS);
    DEFINED_CONSTANT(COMMID_PID_SHIFT, SysTypes::COMMID_PID_SHIFT);
    DEFINED_CONSTANT(COMMID_RD_SHIFT, SysTypes::COMMID_RD_SHIFT);
    DEFINED_CONSTANT(RD_MASK, SysTypes::RD_MASK);
    ENUM_VALUE(XBO, XBaseObj, LOG_SIZE_IN_UVALS);
    ENUM_VALUE(XBO, XBaseObj, FIRST_METHOD);
    FIELD_OFFSET1(XBO, XBaseObj, seqNo);
    FIELD_OFFSET1(XBO, XBaseObj, __nummeth);
    ENUM_VALUE(VTE, COVTableEntry, LOG_SIZE_IN_UVALS);
    FIELD_OFFSET1(VTE, COVTableEntry, _func);
    DEFINED_CONSTANT(SEQNO_SHIFT, _XHANDLE_SEQNO_SHIFT);
    DEFINED_CONSTANT(SEQNO_BITS, _XHANDLE_SEQNO_BITS);
    DEFINED_CONSTANT(IDX_SHIFT, _XHANDLE_IDX_SHIFT);
    DEFINED_CONSTANT(IDX_BITS, _XHANDLE_IDX_BITS);
    DEFINED_CONSTANT(ERRNO_INVAL, EINVAL);
    DEFINED_CONSTANT(ERRNO_NOMEM, ENOMEM);
    DEFINED_CONSTANT(ERRNO_PERM, EPERM);
    DEFINED_CONSTANT(ERRNO_AGAIN, EAGAIN);
    DEFINED_CONSTANT(ERRNO_SRCH, ESRCH);
    FIELD_OFFSET2(KI_TI, KernelInfo, traceInfo, mask);
    FIELD_OFFSET2(KI_TI, KernelInfo, traceInfo, indexMask);
    FIELD_OFFSET2(KI_TI, KernelInfo, traceInfo, traceControl);
    FIELD_OFFSET2(KI_TI, KernelInfo, traceInfo, traceArray);
    FIELD_OFFSET1(TC, TraceControl, index);
    FIELD_OFFSET1(TC, TraceControl, bufferCount);
    DEFINED_CONSTANT(TRC_BUFFER_NUMBER_BITS, TRACE_BUFFER_NUMBER_BITS);
    DEFINED_CONSTANT(TRC_BUFFER_OFFSET_BITS, TRACE_BUFFER_OFFSET_BITS);
    DEFINED_CONSTANT(TRC_BUFFER_OFFSET_MASK, TRACE_BUFFER_OFFSET_MASK);
    DEFINED_CONSTANT(TRC_TIMESTAMP_BITS, TRACE_TIMESTAMP_BITS);
    DEFINED_CONSTANT(TRC_TIMESTAMP_SHIFT, TRACE_TIMESTAMP_SHIFT);
    DEFINED_CONSTANT(TRC_LAYER_ID_BITS, TRACE_LAYER_ID_BITS);
    DEFINED_CONSTANT(TRC_LAYER_ID_SHIFT, TRACE_LAYER_ID_SHIFT);
    DEFINED_CONSTANT(TRC_MAJOR_ID_BITS, TRACE_MAJOR_ID_BITS);
    DEFINED_CONSTANT(TRC_MAJOR_ID_SHIFT, TRACE_MAJOR_ID_SHIFT);
    DEFINED_CONSTANT(TRC_LENGTH_BITS, TRACE_LENGTH_BITS);
    DEFINED_CONSTANT(TRC_LENGTH_SHIFT, TRACE_LENGTH_SHIFT);
    DEFINED_CONSTANT(TRC_DATA_BITS, TRACE_DATA_BITS);
    DEFINED_CONSTANT(TRC_DATA_SHIFT, TRACE_DATA_SHIFT);
    DEFINED_CONSTANT(TRC_K42_LAYER_ID, TRACE_K42_LAYER_ID);
    DEFINED_CONSTANT(TRC_EXCEPTION_MAJOR_ID, TRACE_EXCEPTION_MAJOR_ID);
    DEFINED_CONSTANT(TRC_EXCEPTION_PPC_CALL, TRACE_EXCEPTION_PPC_CALL);
    DEFINED_CONSTANT(TRC_EXCEPTION_PPC_RETURN, TRACE_EXCEPTION_PPC_RETURN);
    DEFINED_CONSTANT(TRC_SCHEDULER_MAJOR_ID, TRACE_SCHEDULER_MAJOR_ID);
    DEFINED_CONSTANT(TRC_SCHEDULER_CUR_THREAD, TRACE_SCHEDULER_CUR_THREAD);
    DEFINED_CONSTANT(TRC_SCHEDULER_PPC_XOBJ_FCT, TRACE_SCHEDULER_PPC_XOBJ_FCT);
#endif

#include __MINC(genConstantsArch.C)
}
