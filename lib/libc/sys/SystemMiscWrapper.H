#ifndef __SYSTEM_MISC_WRAPPER_H_
#define __SYSTEM_MISC_WRAPPER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SystemMiscWrapper.H,v 1.30 2005/08/22 21:48:57 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for calling the resource manager
 * **************************************************************************/

#include <stub/StubSystemMisc.H>
#include <meta/MetaSystemMisc.H>

class SystemMiscWrapper : public Obj {

protected:
    DEFINE_GLOBAL_NEW(SystemMiscWrapper);

    SystemMiscWrapper() { /* empty body */ }

public:

    static SysStatus Create();

    virtual SysStatus tracePrintBuffers(uval arg);
    virtual SysStatus tracePrintBuffers();

    virtual SysStatus traceSetMask(uval newMask);
    virtual SysStatus traceGetMask(uval& mask);
    virtual SysStatus traceAndMask(uval andBits);
    virtual SysStatus traceOrMask(uval orBits);
    virtual SysStatus traceSetMaskAllProcs(uval newMask);
    virtual SysStatus tracePrintMaskAllProcs();
    virtual SysStatus traceAndMaskAllProcs(uval andBits);
    virtual SysStatus traceOrMaskAllProcs(uval orBits);
    virtual SysStatus traceInitCounters();
    virtual SysStatus traceDumpCounters();
    virtual SysStatusUval traceGetCounters(char *buf, uval len);

    virtual SysStatus traceStartTraceD(uval VPSet);
    virtual SysStatus traceStopTraceD();
    virtual SysStatus traceStopTraceDAllProcs();
    virtual SysStatus traceEnableTraceD();

    virtual SysStatus traceDump();
    virtual SysStatus traceReset();
    virtual SysStatus traceResetAllProcs();
    
    // hwPerfSetCountingModeAllProcs: specifies the scope where hardware events are counted. 
    //   "mode" could have one of the following values:
    //     CM_KERNEL (1): counting events only in kernel mode 
    //     CM_USER (2): counting evnets only in user mode  
    //     CM_BOTH (0): counting both in user and kernel modes
    virtual SysStatus hwPerfSetCountingModeAllProcs(uval mode);    
    
    // hwPerfSetPeriodTypeAllProcs: specifies the hardware event used as timer
    //   "type" could have one of the following values:
    //   PT_TIME (0): real time measured in cpu cycles 
    //   PT_INSTR (1): logical time in terms of the number of ISA instructions completed
    virtual SysStatus hwPerfSetPeriodTypeAllProcs(uval type); 

    // hwPerfSetMultiplexingRoundAllProcs: specifies multiplexing granularity.
    //   "round" in CPU cycles, is an interval in which all programmed HPC groups are counted once. 
    virtual SysStatus hwPerfSetMultiplexingRoundAllProcs(uval round); 

    // hwPerfSetLogPeriodAllProcs: specifies the granularity of periodic logging of programmed HPCs
    //    "period" in CPU cycles specifies the time between two consecutive HPC message in the
    //    log buffer
    virtual SysStatus hwPerfSetLogPeriodAllProcs(uval logPeriod); 

    // hwPerfAddGroupAllProcs: add an HPC group to the set of HPC groups to be counted
    //    "groupNo" is the group number
    //    "share" is the time share given to the group within a sampling 
    //    "samplingRate" specifies the rate of data sampling if the specified group
    //    has a data sampling counter, and will be unused otherwise
    virtual SysStatus hwPerfAddGroupAllProcs(uval groupNo, uval share, uval samplingRate);

    // hwPerfRemoveGroupAllProcs: removes an HPC group f
    virtual SysStatus hwPerfRemoveGroupAllProcs(uval groupNo);

    // hwPerfStartSamplingAllProcs: starts counting the added HPC groups and record
    // them into the log buffer
    //    "delay" in cycles specifies the warm-up time before HPC values start to get
    //    logged.
    virtual SysStatus hwPerfStartSamplingAllProcs(uval64 delay);
    virtual SysStatus hwPerfStopSamplingAllProcs();

    virtual SysStatus hwPerfStartCPIBreakdownAllProcs(uval64 delay);
    virtual SysStatus hwPerfStopCPIBreakdownAllProcs();

    // hwPerfStarWatchAllProcs: starts a "Watch", a vector of the events 
    // included in the programmed HPC groups. The watach vector is reset upon
    // calling this function. 
    virtual SysStatus hwPerfStartWatchAllProcs();

    // hwPerfLogAndResetWatchAllProcs: logs the current value of the watch
    // to the log buffer, and resets the watch vector.
    virtual SysStatus hwPerfLogAndResetWatchAllProcs();

    // hwPerfStopWatchAllProcs: disables the watch
    virtual SysStatus hwPerfStopWatchAllProcs();

    virtual SysStatus setControlFlags(uval flags);
    virtual SysStatus setControlFlagsBit(uval ctrlBit, uval value);

    virtual SysStatus systemControlInsert(char* buf, uval len);
    virtual SysStatus breakpointProc(ProcessID pid);
    virtual SysStatus launchProgram(__inbuf(*) char *name,
				    __inbuf(*) char *arg1,
				    __inbuf(*) char *arg2,
				    __in uval wait);

    virtual SysStatus getBasicMemoryInfo(uval& total, uval& free,
					 uval& largePageSize,
					 uval& totalReservedLargePages,
					 uval& freeReservedLargePages);
    virtual SysStatus getCpuInfo(uval cpuNumber, uval& cpuVersion,
				 uval& cpuClockFrequency);
};
#endif /* #ifndef __SYSTEM_MISC_WRAPPER_H_ */
