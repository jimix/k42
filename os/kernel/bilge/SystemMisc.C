/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SystemMisc.C,v 1.60 2005/08/22 21:48:58 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A collection place for miscellaneous calls needed to
 *                     be made that are not directly related to a given
 *                     exported object.
 * **************************************************************************/

#include "kernIncs.H"
#include "SystemMisc.H"
#include "bilge/HWPerfMon.H"
#include <exception/ExceptionLocal.H>
#include <exception/KernelInfoMgr.H>
#include <proc/kernRunProcess.H>
#include <trace/traceBase.H>
#include <trace/traceCtr.H>
#include <trace/traceInfo.h>
#include <sys/ProcessSet.H>
#include <proc/Process.H>
#include <cobj/ObjectRefs.H>
#include "SystemControl.H"
#include "StreamServerConsole.H"
#include "BuildDate.H"
#include <mem/PMRoot.H>

struct SystemMisc::sysVPControlMsg : public MPMsgMgr::MsgAsync {
    VPNum vp;
    uval64 arg1;
    uval64 arg2;
    volatile uval *pVPSetCount;
    uval action;

    virtual void handle() {
	uval const myVP = vp;
	uval64 const myArg1 = arg1;
	uval64 const myArg2 = arg2;
	volatile uval *const myPVPSetCount = pVPSetCount;
	uval const myAction = action;
	free();
	sysVPControl(myAction, myVP, myArg1, myArg2, myPVPSetCount);
    }
};

/* static */ SystemMisc::SystemMiscRoot *SystemMisc::TheSystemMiscRoot;

SystemMisc::SystemMiscRoot::SystemMiscRoot()
{
  /* empty body */
}

SystemMisc::SystemMiscRoot::SystemMiscRoot(RepRef ref)
    : CObjRootMultiRep(ref)
    {
      /* empty body */
    }

extern void tracePrintBuffers(uval);

CObjRep *
SystemMisc::SystemMiscRoot::createRep(VPNum vp)
{
    CObjRep *rep=(CObjRep *)new SystemMisc;
    return rep;
}

/* static */ void
SystemMisc::sysVPControl(uval action, VPNum vp, uval64 arg1, uval64 arg2,
                         volatile uval *myPVPSetCount)
{
    uval64 actMask;

    switch (action) {

    case ACT_TRACE_SET_MASK:
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    if (arg1 == 0) {
		err_printf("vp %ld setting mask 0x%llx\n", vp, arg1);
	    } else {
		// make sure we do not miss heartbeat events
		actMask = arg1 | TRACE_CONTROL_MASK;
		err_printf("vp %ld setting mask 0x%llx req 0x%llx | CONTROL\n",
			   vp, actMask, arg1);
	    }
	}
	_TraceSetMask(arg1, 0);
	break;
    case ACT_TRACE_PRINT_MASK: {
	volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
	err_printf(
	    "vp %ld trace mask 0x%llx\n", Scheduler::GetVP(), trcInfo->mask);
	break;
    }
    case ACT_TRACE_GET_INFO:
	_TraceGetInfo(arg1);
	break;
    case ACT_TRACE_STOP:
	_TraceStopTraceD(0);
	break;
    case ACT_TRACE_RESET:
	_TraceReset(0);
	break;
    case ACT_HW_PERF_SET_COUNTING_MODE:
	DREFGOBJK(TheHWPerfMonRef)->setCountingMode((HWPerfMon::CountingMode)arg1);  // arg1: counting mode 
	break;
    case ACT_HW_PERF_SET_PERIOD_TYPE:
	DREFGOBJK(TheHWPerfMonRef)->setPeriodType((HWPerfMon::PeriodType)arg1);  // arg1: period type 
	break;
    case ACT_HW_PERF_SET_MUX_ROUND:
	DREFGOBJK(TheHWPerfMonRef)->setMultiplexingRound(arg1); // arg1 is "multiplexingRound" 
	break;
    case ACT_HW_PERF_SET_LOG_PERIOD:
	DREFGOBJK(TheHWPerfMonRef)->setLogPeriod(arg1); // arg1 is "logPeriod"
 	break;
    case ACT_HW_PERF_ADD_GROUP:
        {
	   uval32 groupId = arg1 >> 48;
	   uval32 share = (arg1 >> 32) & ((1 << 15) - 1);
	   uval32 samplingFreq = arg1 & ((1 << 31) - 1);
	   DREFGOBJK(TheHWPerfMonRef)->addGroup(groupId, share, samplingFreq); 
	}
 	break;
    case ACT_HW_PERF_REM_GROUP:
	DREFGOBJK(TheHWPerfMonRef)->removeGroup(arg1);   // arg1 is "groupNo"
 	break;
    case ACT_HW_PERF_START_SAMPLING:
	DREFGOBJK(TheHWPerfMonRef)->startSampling(arg1);        // arg1 is "delay"
	break;
    case ACT_HW_PERF_STOP_SAMPLING:
	DREFGOBJK(TheHWPerfMonRef)->stopSampling();
	break;

    case ACT_HW_PERF_START_CPI_BREAKDOWN:
	DREFGOBJK(TheHWPerfMonRef)->startCPIBreakdown(arg1);    // arg1 is "delay"
	break;
    case ACT_HW_PERF_STOP_CPI_BREAKDOWN:
	DREFGOBJK(TheHWPerfMonRef)->stopCPIBreakdown();
	break;

    case ACT_HW_PERF_START_WATCH:
	DREFGOBJK(TheHWPerfMonRef)->startWatch(); 
	break;
    case ACT_HW_PERF_LOG_AND_RESET_WATCH:
	DREFGOBJK(TheHWPerfMonRef)->logAndResetWatch(); 
	break;
    case ACT_HW_PERF_STOP_WATCH:
	DREFGOBJK(TheHWPerfMonRef)->stopWatch(); 
	break;

    case ACT_HW_PERF_ADD_CUSTOM_EVENTS:
	// not implemented yet
	break;
    case ACT_HW_PERF_REM_CUSTOM_EVENTS:
	// not implemented yet
	break;

    case ACT_TRACE_SET_OVERFLOW_BEHAVIOR:
	_TraceSetOverflowBehavior(arg1, 0);
	break;

    default:
	err_printf("error: unknown action code\n");
	return;
	break;
    }
    AtomicAdd(myPVPSetCount, 1);
}


uval _get_PVR() {
    uval pvr;
    asm volatile ("mfpvr %0" : "=&r" (pvr));
    return pvr;
}

/* static */ void
SystemMisc::ClassInit(VPNum vp, uval memorySize,
		      uval cpuClockFrequency)
{
    // all initialization on processor zero
    if (vp != 0) {
	SystemMisc::TheSystemMiscRoot->_cpuVersion[vp] = _get_PVR();
	return;
    }

    MetaSystemMisc::init();

    SystemMisc::TheSystemMiscRoot = new SystemMiscRoot;

    SystemMisc::TheSystemMiscRoot->testVal = 19;
    SystemMisc::TheSystemMiscRoot->_memorySize = memorySize;
    SystemMisc::TheSystemMiscRoot->_cpuVersion = (uval *)
		allocGlobal(sizeof(uval) * KernelInfo::CurPhysProcs());
    SystemMisc::TheSystemMiscRoot->_cpuVersion[vp] = _get_PVR();
    SystemMisc::TheSystemMiscRoot->_cpuClockFrequency = cpuClockFrequency;
}

/* static */ SysStatus
SystemMisc::_Create(ObjectHandle &ssOH, __CALLER_PID caller)
{
    SysStatus retvalue;
    retvalue = ((DREF((SystemMiscRef)(SystemMisc::TheSystemMiscRoot->getRef())))
	    ->giveAccessByServer(ssOH,caller));
    return (retvalue);
}

/* static */ void
SystemMisc::doAcrossAllProcs(uval action, uval64 arg1, uval64 arg2)
{
    VPNum numbVPs, vp;
    SysStatus rc=0;
    volatile uval vpSetCount;

    numbVPs = DREFGOBJ(TheProcessRef)->ppCount();

    vpSetCount = 0;

    for (vp = 0; vp < numbVPs; vp++) {
	if (vp == Scheduler::GetVP()) {
	    sysVPControl(action, vp, arg1, arg2, &vpSetCount);
	} else {
	    sysVPControlMsg *const msg =
	        new(Scheduler::GetEnabledMsgMgr()) sysVPControlMsg;
	    msg->vp = vp;
	    msg->arg1 = arg1;
	    msg->arg2 = arg2;
	    msg->pVPSetCount = &vpSetCount;
	    msg->action = action;
	    rc = msg->send(SysTypes::DSPID(0, vp));
	    tassert(_SUCCESS(rc), err_printf("send failed\n"));
	}
    }
    while (vpSetCount<numbVPs) ;  // spin until all processors have set mask
}

/* static */ SysStatus
SystemMisc::_TracePrintBuffers(__in uval arg, __CALLER_PID caller)
{
    tracePrintBuffers(arg);
    return 0;
}

/* static */ SysStatus
SystemMisc::_TracePrintBuffers(__CALLER_PID caller)
{
    tracePrintBuffers(0);
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceSetMask(uval newMask, __CALLER_PID caller)
{
    uval64 oldMask;
    char *buildInfo;
    SysStatus rc;
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);
    oldMask = trcInfo->mask;
    trcInfo->mask = newMask;
    exceptionLocal.copyTraceInfo();
    if ((oldMask == 0) && (newMask != 0)) {
	VPNum myvp;
	myvp = Scheduler::GetVP();
	TraceOSControlHeartbeat(Scheduler::SysTimeNow(), myvp);
	TraceOSInfoTicksPerSecond(Scheduler::TicksPerSecond());
 	TraceOSInfoNumberProcessors((uval64)(KernelInfo::CurPhysProcs()));
	TraceOSInfoCPUClockFrequency((uval64)
	    SystemMisc::TheSystemMiscRoot->_cpuClockFrequency);
	TraceOSInfoMemorySize(SystemMisc::TheSystemMiscRoot->_memorySize);
	// the 4+ is because each pte is 2 8byte = 16bytes quantities
 	TraceOSInfoPageTableSize((uval64)(exceptionLocal.pageTable.getLogNumPTEs()), 
		 (uval64)(1<<(4+(exceptionLocal.pageTable.getLogNumPTEs()))));
 	TraceOSInfoControlFlags((uval64)(KernelInfo::GetControlFlags()));
 	TraceOSUserSpawn(0, (uval64)-1, (uval64)-1, "kernel");
 	TraceOSUserSpawn(1, (uval64)-1, (uval64)-1, "baseServers");
	BuildDate::getLinkDate(buildInfo);
	TraceOSInfoLinkDate(buildInfo);
	BuildDate::getCVSCheckoutDate(buildInfo);
	TraceOSInfoCVSCheckoutDate(buildInfo);
	BuildDate::getBuiltBy(buildInfo);
	TraceOSInfoBuildUser(buildInfo);
	BuildDate::getDebugLevel(buildInfo);
	TraceOSInfoDebugLevel(buildInfo);
	rc = DREFGOBJK(TheHWPerfMonRef)->logCountersToTrace();
    }
    TraceOSControlTraceMask(newMask);
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceSetOverflowBehavior(uval newVal, __CALLER_PID caller)
{
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);

    trcInfo->overflowBehavior = newVal;

    return 0;
}


/* static */ SysStatus
SystemMisc::_TraceGetMask(__out uval& mask, __CALLER_PID caller)
{
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);

    mask = trcInfo->mask;

    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceGetInfo(__CALLER_PID caller)
{
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);
    TraceControl *const trcCtrl = trcInfo->traceControl;

    err_printf("vp %ld current trace index 0x%llx (buffer %lld, offset %lld)\n",
	       Scheduler::GetVP(),
	       trcCtrl->index,
	       TRACE_BUFFER_NUMBER_GET(trcCtrl->index),
	       TRACE_BUFFER_OFFSET_GET(trcCtrl->index));
    err_printf("    buffersProduced: %ld, buffersConsumed: %ld\n",
		trcCtrl->buffersProduced,
		trcCtrl->buffersConsumed);
    err_printf("    number of buffers: %lld, size: %lld bytes\n",
	       trcInfo->numberOfBuffers,
	       (uval64)(TRACE_BUFFER_SIZE*(sizeof(uval64))));
    if (trcInfo->overflowBehavior == TRACE_OVERFLOW_WRAP) {
	err_printf("    overflow behavior set to WRAP\n");
    } else if (trcInfo->overflowBehavior != TRACE_OVERFLOW_WRAP) {
	err_printf("    overflow behavior set to STOP\n");
    } else {
	err_printf("    overflow behavior set to undefined\n");
    }
    err_printf("    trace mask 0x%llx\n", trcInfo->mask);

    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceAndMask(uval andBits, __CALLER_PID caller)
{
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);
    trcInfo->mask &= andBits;
    TraceOSControlTraceMask(trcInfo->mask);
    exceptionLocal.copyTraceInfo();
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceOrMask(uval orBits, __CALLER_PID caller)
{
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);
    trcInfo->mask |= orBits;
    TraceOSControlTraceMask(trcInfo->mask);
    exceptionLocal.copyTraceInfo();
    return 0;
}


/* static */ SysStatus
SystemMisc::_TraceSetMaskAllProcs(uval newMask, __CALLER_PID caller)
{
    doAcrossAllProcs(ACT_TRACE_SET_MASK, newMask, 0);
    return 0;
}

/* static */ SysStatus
SystemMisc::_TracePrintMaskAllProcs(__CALLER_PID caller)
{
    doAcrossAllProcs(ACT_TRACE_PRINT_MASK, 0, 0);
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceSetOverflowBehaviorAllProcs(uval newVal, __CALLER_PID caller)
{
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);

    doAcrossAllProcs(ACT_TRACE_SET_OVERFLOW_BEHAVIOR, newVal, 0);

    if (trcInfo->overflowBehavior == TRACE_OVERFLOW_WRAP) {
	cprintf("Overflow Behavior set to WRAP\n");
    } else if (trcInfo->overflowBehavior != TRACE_OVERFLOW_WRAP) {
	cprintf("Overflow Behavior set to STOP\n");
    } else {
	cprintf("Overflow Behavior set to undefined\n");
    }

    return 0;
}


/* static */ SysStatus
SystemMisc::_TraceGetInfoAllProcs(__CALLER_PID caller)
{
    doAcrossAllProcs(ACT_TRACE_GET_INFO, 0, 0);
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceAndMaskAllProcs(uval andBits, __CALLER_PID caller)
{
    tassert(0, err_printf("TraceAndMaskAllProcs NYI\n"));
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceOrMaskAllProcs(uval orBits, __CALLER_PID caller)
{
    tassert(0, err_printf("TraceOrMaskAllProcs NYI\n"));
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceInitCounters(__CALLER_PID caller)
{
    TraceAutoCount::initCounters();
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceDumpCounters(__CALLER_PID caller)
{
    for (uval numb = 0; numb < TraceAutoCount::MAX; numb++ ) {
	cprintf("%ld: ", numb);
	TraceAutoCount::dumpCounterPair(TraceAutoCount::CtrIdx(numb));
    }
    return 0;
}


/* static */ SysStatusUval
SystemMisc::_TraceGetCounters(__outbuf(__rc:len) char *buf,
			      __in uval len,
			      __CALLER_PID caller)
{
    uval ctrIdx;
    uval64 *counters=(uval64 *)buf;
    if (len < (TraceAutoCount::MAX)*2*sizeof(uval64)) return 0;

    for (uval i = 0; i < TraceAutoCount::MAX; i++ ) {
	ctrIdx = 2*i;
	TraceAutoCount::fetchCounterPair(TraceAutoCount::CtrIdx(i),
					 &(counters[ctrIdx]),
					 &(counters[ctrIdx+1]));
    }
    return (TraceAutoCount::MAX)*2*sizeof(uval64);
}

/* static */ SysStatusUval
SystemMisc::_IncrementMountVersionNumber()
{
    KernelInfo::SystemGlobal* sgp;
    DREFGOBJK(TheKernelInfoMgrRef)->lockAndGetPtr(sgp);
    sgp->mountVersionNumber++;
    SysStatusUval ret = _SRETUVAL(sgp->mountVersionNumber);
    DREFGOBJK(TheKernelInfoMgrRef)->publishAndUnlock();
    return ret;
}

/* static */ SysStatus
SystemMisc::_InitMountVersionNumber()
{
    KernelInfo::SystemGlobal* sgp;
    DREFGOBJK(TheKernelInfoMgrRef)->lockAndGetPtr(sgp);
    sgp->mountVersionNumber = 0;
    DREFGOBJK(TheKernelInfoMgrRef)->publishAndUnlock();
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceStartTraceD(uval VPSet, __CALLER_PID caller)
{
    uval wait = 0; // do not block for traced
    char vps[8];
    char vpsN[8];

    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);

    strcpy(vps, "--vp");

    if (VPSet == 0) {
	strcpy(vpsN, "0");
    } else {
	strcpy(vpsN, "1");
    }

    if (!(trcInfo->tracedRunning)) {
	//FIXME we could use a lock or some sychronization here
	trcInfo->tracedRunning = 1;
	exceptionLocal.copyTraceInfo();
	kernRunInternalProcess("tracedServer", vps, vpsN, wait);
	return 0;
    } else {
	return _SERROR(1964, 0, EEXIST);
    }
}

/* static */ SysStatus
SystemMisc::_TraceStopTraceD(__CALLER_PID caller)
{
    //FIXME we could use a lock or some sychronization here
    //cprintf("sending stop request to trace daemon vp %ld\n",Scheduler::GetVP());
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);
    trcInfo->tracedRunning = 0;
    exceptionLocal.copyTraceInfo();
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceStopTraceDAllProcs(__CALLER_PID caller)
{
    doAcrossAllProcs(ACT_TRACE_STOP, 0, 0);
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceEnableTraceD(__CALLER_PID caller)
{
    //FIXME we could use a lock or some sychronization here
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);
    trcInfo->tracedRunning = 1;
    exceptionLocal.copyTraceInfo();
    return 0;
}

//FIXME: Kludge alert please see KernelInit.C dumpTraceBuffers
extern void dumpTraceBuffers();

/* static */ SysStatus
SystemMisc::_TraceDump(__CALLER_PID caller)
{
    dumpTraceBuffers();
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceReset(__CALLER_PID caller)
{
    TraceReset(Scheduler::GetVP());
    return 0;
}

/* static */ SysStatus
SystemMisc::_TraceResetAllProcs(__CALLER_PID caller)
{
    doAcrossAllProcs(ACT_TRACE_RESET, 0, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfSetCountingModeAllProcs(__CALLER_PID caller, __in uval mode) 
{
    doAcrossAllProcs(ACT_HW_PERF_SET_COUNTING_MODE, mode, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfSetPeriodTypeAllProcs(__CALLER_PID caller, __in uval type) 
{
    doAcrossAllProcs(ACT_HW_PERF_SET_PERIOD_TYPE, type, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfSetMultiplexingRoundAllProcs(__CALLER_PID caller, __in uval round) 
{
    doAcrossAllProcs(ACT_HW_PERF_SET_MUX_ROUND, round, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfSetLogPeriodAllProcs(__CALLER_PID caller, __in uval period) 
{
    doAcrossAllProcs(ACT_HW_PERF_SET_LOG_PERIOD, period, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfAddGroupAllProcs(__CALLER_PID caller, 
		                    __in uval groupNo, 
				    __in uval share, 
				    __in uval samplingFreq) 
{
    uval64 arg1 = (uval64)(groupNo << 48) + (uval64)(share << 32) + (uval64)samplingFreq;
    doAcrossAllProcs(ACT_HW_PERF_ADD_GROUP, arg1, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfRemoveGroupAllProcs(__CALLER_PID caller, __in uval groupNo) 
{
    doAcrossAllProcs(ACT_HW_PERF_REM_GROUP, groupNo, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfStartSamplingAllProcs(__CALLER_PID caller, __in uval64 delay) 
{
    doAcrossAllProcs(ACT_HW_PERF_START_SAMPLING, delay, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfStopSamplingAllProcs(__CALLER_PID caller) 
{
    doAcrossAllProcs(ACT_HW_PERF_STOP_SAMPLING, 0, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfStartCPIBreakdownAllProcs(__CALLER_PID caller, __in uval64 delay) 
{
    doAcrossAllProcs(ACT_HW_PERF_START_CPI_BREAKDOWN, delay, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfStopCPIBreakdownAllProcs(__CALLER_PID caller) 
{
    doAcrossAllProcs(ACT_HW_PERF_STOP_CPI_BREAKDOWN, 0, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfStartWatchAllProcs(__CALLER_PID caller) 
{
    doAcrossAllProcs(ACT_HW_PERF_START_WATCH, 0, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfLogAndResetWatchAllProcs(__CALLER_PID caller) 
{
    doAcrossAllProcs(ACT_HW_PERF_LOG_AND_RESET_WATCH, 0, 0);
    return 0;
}

/* static */ SysStatus 
SystemMisc::_HWPerfStopWatchAllProcs(__CALLER_PID caller) 
{
    doAcrossAllProcs(ACT_HW_PERF_STOP_WATCH, 0, 0);
    return 0;
}


/* static */ SysStatus
SystemMisc::_SetControlFlags(__in uval ctrlFlags)
{
    KernelInfoMgr::SetControl(ctrlFlags);
    return 0;
}
/* static */ SysStatus
SystemMisc::_SetControlFlagsBit(__in uval ctrlBit, __in uval value)
{
    KernelInfoMgr::SetControlBit(ctrlBit, value);
    return 0;
}

/* static */ SysStatus
SystemMisc::_SystemControlInsert(__inbuf(len) char* buf, __in uval len)
{
    SystemControl::Insert(buf,len);
    return 0;
}

/* static */ SysStatus
SystemMisc::_BreakpointProc(__in ProcessID pid)
{
    BaseProcessRef pref;
    DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, pref);
    DREF((ProcessRef)pref)->callBreakpoint();
    return 0;
}

/* static */ SysStatus
SystemMisc::_LaunchProgram(__inbuf(*) char *name,
			   __inbuf(*) char *arg1,
			   __inbuf(*) char *arg2,
			   __in uval wait)
{
    return kernRunInternalProcess(name, arg1, arg2, wait);
}

/* static */ SysStatus
SystemMisc::_TakeOverConsoleForLogin(__in ProcessID pid,
				     __out ObjectHandle &oh)
{
    err_printf("taking over console pid %lx %lx\n", pid, _SGETPID(pid));
    SystemControl::DetachFromConsole();
    StreamServerConsole::StartLoginShell(oh, _SGETPID(pid));
    return 0;
}

/* static */ SysStatus
SystemMisc::_GetBasicMemoryInfo(__out uval& total, __out uval& free,
				__out uval& largePageSize,
				__out uval& largePageReservCount,
				__out uval& largePageFreeCount)
{
    SysStatus rc;

    total = SystemMisc::TheSystemMiscRoot->_memorySize;
    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(free);
    _IF_FAILURE_RET(rc);
    rc = ((PMRoot *) DREFGOBJK(ThePMRootRef))->getLargePageInfo(largePageSize,
		   	largePageReservCount, largePageFreeCount);
    return rc;
}

/* static */ SysStatus
SystemMisc::_GetCpuInfo(__in uval cpuNumber, __out uval& cpuVersion,
	    __out uval& cpuClockFrequency)
{
    if (cpuNumber >= KernelInfo::CurPhysProcs() ) {
	return _SERROR(2808, 0, EINVAL);
    } else {
	cpuVersion = SystemMisc::TheSystemMiscRoot->_cpuVersion[cpuNumber];
	cpuClockFrequency = SystemMisc::TheSystemMiscRoot->_cpuClockFrequency;
	return 0;
    }
}
