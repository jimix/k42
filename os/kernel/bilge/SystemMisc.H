#ifndef __SYSTEM_MISC_H_
#define __SYSTEM_MISC_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SystemMisc.H,v 1.34 2005/08/22 21:48:58 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A collection place for miscellaneous calls needed to
 *                     be made that are not directly related to a given
 *                     exported object.
 * **************************************************************************/

#include <meta/MetaSystemMisc.H>
#include <cobj/CObjRootMultiRep.H>

// FIXME: fix stub-compiler grammar so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class SystemMisc;

typedef SystemMisc **SystemMiscRef;

class SystemMisc : public Obj {
    enum {
	 ACT_TRACE_SET_MASK, 
         ACT_TRACE_PRINT_MASK, 
	 ACT_TRACE_STOP,
	 ACT_TRACE_RESET, 
	 ACT_TRACE_DUMP,
	 ACT_TRACE_GET_INFO, 
	 ACT_TRACE_SET_OVERFLOW_BEHAVIOR,
	 ACT_HW_PERF_SET_COUNTING_MODE,
	 ACT_HW_PERF_SET_PERIOD_TYPE,
	 ACT_HW_PERF_SET_MUX_ROUND,
	 ACT_HW_PERF_SET_LOG_PERIOD,
	 ACT_HW_PERF_ADD_GROUP, 
	 ACT_HW_PERF_REM_GROUP, 
	 ACT_HW_PERF_ADD_CUSTOM_EVENTS,
	 ACT_HW_PERF_REM_CUSTOM_EVENTS,
	 ACT_HW_PERF_START_SAMPLING, 
	 ACT_HW_PERF_STOP_SAMPLING, 
	 ACT_HW_PERF_START_CPI_BREAKDOWN, 
	 ACT_HW_PERF_STOP_CPI_BREAKDOWN, 
	 ACT_HW_PERF_START_WATCH, 
	 ACT_HW_PERF_LOG_AND_RESET_WATCH, 
	 ACT_HW_PERF_STOP_WATCH 
    };
    static uval createCount;
protected:
#ifndef _IN_STUBGEN
    class SystemMiscRoot : public CObjRootMultiRep {
    public:
	uval testVal;

	/* memory available in the machine at boot time, minus a little bit
	 * for machine tables, etc.  This value is used by /proc/memory.
	 * This is set, once, during kernel initialization.  Should the 
	 * amount of memory given to the system change after the system has
	 * booted, then interfaces need to be added to keep this value
	 * up-to-date.
	 */
	uval _memorySize;	
	uval * _cpuVersion;
	uval _cpuClockFrequency;

	virtual CObjRep * createRep(VPNum vp);
	SystemMiscRoot();
	SystemMiscRoot(RepRef ref);

	DEFINE_GLOBALPADDED_NEW(SystemMiscRoot);
    };
    friend class SystemMisc::SystemMiscRoot;
    SystemMiscRoot *root() { return (SystemMiscRoot *)myRoot; }
#else /* #ifndef _IN_STUBGEN */
    struct SystemMiscRoot;  // needed for declarations later in this file
#endif /* #ifndef _IN_STUBGEN */

    static SystemMiscRoot *TheSystemMiscRoot;

public:
#ifndef _IN_STUBGEN

    static void sysVPControl(uval action, VPNum vp, uval64 arg1, uval64 arg2,
                             volatile uval *myPVPSetCount);
#endif

    struct sysVPControlMsg;

    DEFINE_GLOBALPADDED_NEW(SystemMisc);

    DEFINE_ACCESS_SERVER(
	SystemMisc,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    static void ClassInit(VPNum vp, uval memorySize, uval cpuClockFrequency);

    static void doAcrossAllProcs(uval action, uval64 arg1, uval64 arg2);

__xpublic:
    static SysStatus _Create(__out ObjectHandle &ssOH, __CALLER_PID caller);

    static SysStatus _TracePrintBuffers(__in uval arg, __CALLER_PID caller);
    static SysStatus _TracePrintBuffers(__CALLER_PID caller);

    static SysStatus _TraceGetMask(__out uval& mask, __CALLER_PID caller);
    static SysStatus _TraceSetMask(uval newMask, __CALLER_PID caller);
    static SysStatus _TraceAndMask(uval andBits, __CALLER_PID caller);
    static SysStatus _TraceOrMask(uval orBits, __CALLER_PID caller);
    static SysStatus _TraceSetOverflowBehavior(uval newVal, __CALLER_PID caller);
    static SysStatus _TraceSetMaskAllProcs(uval newMask, __CALLER_PID caller);
    static SysStatus _TracePrintMaskAllProcs(__CALLER_PID caller);
    static SysStatus _TraceAndMaskAllProcs(uval andBits, __CALLER_PID caller);
    static SysStatus _TraceOrMaskAllProcs(uval orBits, __CALLER_PID caller);
    static SysStatus _TraceSetOverflowBehaviorAllProcs(uval newVal, 
						       __CALLER_PID caller);
    static SysStatus _TraceInitCounters(__CALLER_PID caller);
    static SysStatus _TraceDumpCounters(__CALLER_PID caller);
    static SysStatusUval _TraceGetCounters(__outbuf(__rc:len) char *buf,
					   __in uval len,
					   __CALLER_PID caller);
    static SysStatus _TraceGetInfo(__CALLER_PID caller);
    static SysStatus _TraceGetInfoAllProcs(__CALLER_PID caller);

    // FIXME: these should only be called by mount server, move into
    //        kernel
    static SysStatus _InitMountVersionNumber();
    static SysStatus _IncrementMountVersionNumber();

    static SysStatus _TraceStartTraceD(__in uval VPSet, __CALLER_PID caller);
    static SysStatus _TraceStopTraceD(__CALLER_PID caller);
    static SysStatus _TraceStopTraceDAllProcs(__CALLER_PID caller);
    static SysStatus _TraceEnableTraceD(__CALLER_PID caller);

    static SysStatus _TraceDump(__CALLER_PID caller);
    static SysStatus _TraceReset(__CALLER_PID caller);
    static SysStatus _TraceResetAllProcs(__CALLER_PID caller);

    static SysStatus _HWPerfSetCountingModeAllProcs(__CALLER_PID caller, __in uval mode);
    static SysStatus _HWPerfSetPeriodTypeAllProcs(__CALLER_PID caller, __in uval type);
    static SysStatus _HWPerfSetMultiplexingRoundAllProcs(__CALLER_PID caller, __in uval round);
    static SysStatus _HWPerfSetLogPeriodAllProcs(__CALLER_PID caller, __in uval period);

    static SysStatus _HWPerfAddGroupAllProcs(__CALLER_PID caller, __in uval groupNo, __in uval share, __in uval samplingFreq);
    static SysStatus _HWPerfRemoveGroupAllProcs(__CALLER_PID caller, __in uval groupNo);

    static SysStatus _HWPerfStartSamplingAllProcs(__CALLER_PID caller, __in uval64 delay);
    static SysStatus _HWPerfStopSamplingAllProcs(__CALLER_PID caller);

    static SysStatus _HWPerfStartWatchAllProcs(__CALLER_PID caller);
    static SysStatus _HWPerfLogAndResetWatchAllProcs(__CALLER_PID caller);
    static SysStatus _HWPerfStopWatchAllProcs(__CALLER_PID caller);

    static SysStatus _HWPerfStartCPIBreakdownAllProcs(__CALLER_PID caller, __in uval64 delay);
    static SysStatus _HWPerfStopCPIBreakdownAllProcs(__CALLER_PID caller);

    static SysStatus _SetControlFlags(__in uval ctrlFlags);
    static SysStatus _SetControlFlagsBit(__in uval ctrlBit, __in uval value);

    static SysStatus _SystemControlInsert(__inbuf(len) char* buf, 
					  __in uval len);
    static SysStatus _BreakpointProc(__in ProcessID pid);
    static SysStatus _LaunchProgram(__inbuf(*) char *name,
				    __inbuf(*) char *arg1,
				    __inbuf(*) char *arg2,
				    __in uval wait);

    static SysStatus _TakeOverConsoleForLogin(__in ProcessID pid, 
					      __out ObjectHandle &oh);

    static SysStatus _GetBasicMemoryInfo(__out uval& total, __out uval& free,
					 __out uval& largePageSize,
					 __out uval& totalReservedLargePages,
					 __out uval& freeReservedLargePages);
    static SysStatus _GetCpuInfo(__in uval cpuNumber, __out uval& cpuVersion,
			         __out uval& cpuClockFrequency);
};
#endif /* #ifndef __SYSTEM_MISC_H_ */
