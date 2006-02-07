/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SystemMiscWrapper.C,v 1.28 2005/08/22 21:48:57 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Wrapper object for the system services object
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/SystemMiscWrapper.H>
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubSystemMisc.H>

/*static*/ SysStatus
SystemMiscWrapper::Create()
{
    SystemMiscWrapper *wrapper = new SystemMiscWrapper;
    tassert(wrapper != NULL,
	    err_printf("failed to create sys services wrapper\n"));

    new CObjRootSingleRep(wrapper, (RepRef)GOBJ(TheSystemMiscRef));

    return 0;
}

/* virtual */ SysStatus
SystemMiscWrapper::tracePrintBuffers(uval arg)
{
    return (StubSystemMisc::_TracePrintBuffers(arg));
}

/* virtual */ SysStatus
SystemMiscWrapper::tracePrintBuffers()
{
    return (StubSystemMisc::_TracePrintBuffers());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceSetMask(uval newMask)
{
    return (StubSystemMisc::_TraceSetMask(newMask));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceGetMask(uval& mask)
{
    return (StubSystemMisc::_TraceGetMask(mask));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceAndMask(uval andBits)
{
    return (StubSystemMisc::_TraceAndMask(andBits));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceOrMask(uval orBits)
{
    return (StubSystemMisc::_TraceOrMask(orBits));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceSetMaskAllProcs(uval newMask)
{
    return (StubSystemMisc::_TraceSetMaskAllProcs(newMask));
}

/* virtual */ SysStatus
SystemMiscWrapper::tracePrintMaskAllProcs()
{
    return (StubSystemMisc::_TracePrintMaskAllProcs());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceAndMaskAllProcs(uval andBits)
{
    return (StubSystemMisc::_TraceAndMaskAllProcs(andBits));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceOrMaskAllProcs(uval orBits)
{
    return (StubSystemMisc::_TraceOrMaskAllProcs(orBits));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceInitCounters()
{
    return (StubSystemMisc::_TraceInitCounters());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceDumpCounters()
{
    return (StubSystemMisc::_TraceDumpCounters());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceGetCounters(char *buf, uval len)
{
    return (StubSystemMisc::_TraceGetCounters(buf,len));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceStartTraceD(uval VPSet)
{
    return (StubSystemMisc::_TraceStartTraceD(VPSet));
}

/* virtual */ SysStatus
SystemMiscWrapper::traceStopTraceD()
{
    return (StubSystemMisc::_TraceStopTraceD());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceStopTraceDAllProcs()
{
    return (StubSystemMisc::_TraceStopTraceDAllProcs());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceEnableTraceD()
{
    return (StubSystemMisc::_TraceEnableTraceD());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceReset()
{
    return (StubSystemMisc::_TraceReset());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceDump()
{
    return (StubSystemMisc::_TraceDump());
}

/* virtual */ SysStatus
SystemMiscWrapper::traceResetAllProcs()
{
    return (StubSystemMisc::_TraceResetAllProcs());
}
    
/* virtual */ SysStatus
SystemMiscWrapper::hwPerfSetCountingModeAllProcs(uval mode)
{
    return (StubSystemMisc::_HWPerfSetCountingModeAllProcs(mode));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfSetPeriodTypeAllProcs(uval type)
{
    return (StubSystemMisc::_HWPerfSetPeriodTypeAllProcs(type));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfSetMultiplexingRoundAllProcs(uval round)
{
    return (StubSystemMisc::_HWPerfSetMultiplexingRoundAllProcs(round));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfSetLogPeriodAllProcs(uval period)
{
    return (StubSystemMisc::_HWPerfSetLogPeriodAllProcs(period));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfAddGroupAllProcs(uval groupNo, uval share, uval samplingFreq)
{
    return (StubSystemMisc::_HWPerfAddGroupAllProcs(groupNo, share, samplingFreq));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfRemoveGroupAllProcs(uval groupNo)
{
    return (StubSystemMisc::_HWPerfRemoveGroupAllProcs(groupNo));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfStartSamplingAllProcs(uval64 delay)
{
    return (StubSystemMisc::_HWPerfStartSamplingAllProcs(delay));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfStopSamplingAllProcs()
{
    return (StubSystemMisc::_HWPerfStopSamplingAllProcs());
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfStartCPIBreakdownAllProcs(uval64 delay)
{
    return (StubSystemMisc::_HWPerfStartCPIBreakdownAllProcs(delay));
}

/* virtual */ SysStatus
SystemMiscWrapper::hwPerfStopCPIBreakdownAllProcs()
{
    return (StubSystemMisc::_HWPerfStopCPIBreakdownAllProcs());
}

/*virtual*/ SysStatus 
SystemMiscWrapper::hwPerfStartWatchAllProcs()
{
    return (StubSystemMisc::_HWPerfStartWatchAllProcs());
}

/*virtual*/ SysStatus 
SystemMiscWrapper::hwPerfLogAndResetWatchAllProcs()
{
    return (StubSystemMisc::_HWPerfLogAndResetWatchAllProcs());
}

/*virtual*/ SysStatus 
SystemMiscWrapper::hwPerfStopWatchAllProcs()
{
    return (StubSystemMisc::_HWPerfStopWatchAllProcs());
}

/* virtual */ SysStatus
SystemMiscWrapper::setControlFlags(uval flags)
{
    return (StubSystemMisc::_SetControlFlags(flags));
}


/* virtual */ SysStatus
SystemMiscWrapper::setControlFlagsBit(uval ctrlBit, uval value)
{
    return (StubSystemMisc::_SetControlFlagsBit(ctrlBit, value));
}

/* virtual */ SysStatus
SystemMiscWrapper::systemControlInsert(char* buf, uval len)
{
    return (StubSystemMisc::_SystemControlInsert(buf,len));
}

/* virtual */ SysStatus
SystemMiscWrapper::breakpointProc(ProcessID pid)
{
    return StubSystemMisc::_BreakpointProc(pid);
}

/* virtual */ SysStatus
SystemMiscWrapper::launchProgram(char *name, char *arg1,
				 char *arg2, uval wait)
{
    return StubSystemMisc::_LaunchProgram(name, arg1, arg2, wait);
}

/* virtual */ SysStatus
SystemMiscWrapper::getBasicMemoryInfo(uval& total, uval& free,
				      uval& largePageSize,
				      uval& totalReservedLargePages,
				      uval& freeReservedLargePages)
{
    return StubSystemMisc::_GetBasicMemoryInfo(total, free, largePageSize,
					       totalReservedLargePages,
					       freeReservedLargePages);
}

/* virtual */ SysStatus
SystemMiscWrapper::getCpuInfo(uval cpuNumber, uval& cpuVersion,
			uval& cpuClockFrequency)
{
    return StubSystemMisc::_GetCpuInfo(cpuNumber, cpuVersion, cpuClockFrequency);
}
