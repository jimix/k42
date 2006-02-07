/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelInit.C,v 1.585 2005/08/22 21:49:00 bob Exp $
 *****************************************************************************/

/*****************************************************************************
 * Module Description:
 * KernelInit is the initial machine independent program after boot.
 * On entry, the machine dependent startup has:
 *           Basic machine set up
 *           Zero'd bss, etc. so programs will run.
 *           Running on a stack which we need to reclaim.
 * **************************************************************************/

#include "kernIncs.H"
#include <sys/KernelInfo.H>
#include <bilge/COSMgrObjectKernObject.H>
#include <cobj/TypeFactory.H>
#include <cobj/TypeMgrServer.H>
#include <cobj/Example.H>
#include "defines/mem_debug.H"
#include "defines/paging.H"
#include <cobj/XHandleTrans.H>
#include "bilge/LocalConsole.H"
#include "bilge/StreamServerConsole.H"
#include "bilge/SysEnviron.H"
#include "bilge/Wire.H"
#include "bilge/ThinIP.H"
#include "bilge/ThinWireMgr.H"
#include "proc/kernRunProcess.H"
#include "proc/Process.H"
#include "proc/ProcessDefaultKern.H"
#include "proc/ProcessSetKern.H"
#include "mem/FCMReal.H"
#include "mem/HATKernel.H"
#include "mem/FRComputation.H"
#include "mem/FRLTransTable.H"
#include "mem/FRVA.H"
#include "mem/FRPA.H"
#include "mem/FRPANonPageable.H"
#include "mem/FRPANonPageableRamOnly.H"
#include "mem/FRCRW.H"
#include "mem/FRKernelPinned.H"
#include "bilge/NetDev.H"
#include "mem/KernelPagingTransportPA.H"
#include "mem/KernelPagingTransportVA.H"
#include "mem/PageAllocatorKern.H"
#include "mem/PageAllocatorKernPinned.H"
#include "mem/PageAllocatorKernUnpinned.H"
#include "mem/RegionDefault.H"
#include "mem/RegionReplicated.H"
#include <stub/StubResMgr.H>
#include "mem/RegionPerProcessor.H"
#include "mem/RegionFSComm.H"
#include "mem/HardwareSpecificRegions.H"
#include "mem/RegionRedZone.H"
#include "mem/PMRoot.H"
#include "mem/SyncService.H"
#include "misc/linkage.H"
#include "bilge/FSRamSwap.H"
#include "bilge/TestSwitch.H"
#include "bilge/SystemMisc.H"
#include "bilge/PrivilegedService.H"
#include "bilge/TestScheduler.H"
#include "bilge/ToyBlockDev.H"
#include "bilge/IPSock.H"
#if defined(TARGET_powerpc)
#include "linux/LinuxBlockDev.H"
#include "linux/LinuxCharDev.H"
#endif /* #if defined(TARGET_powerpc) */
#include "init/kernel.H"
#include "init/memoryMapKern.H"
#include "init/MemoryMgrPrimitiveKern.H"
#include "trace/traceBase.H"
#include "bilge/HWPerfMon.H"
#include "io/SocketServer.H"
#if defined(TARGET_powerpc)
#include "bilge/arch/powerpc/simos.H"
#endif /* #if defined(TARGET_powerpc) */
#include <sys/ppccore.H>
#include <sys/ProcessServer.H>
#include <sys/ProcessClient.H>
#include <scheduler/Scheduler.H>
#include <scheduler/DispatcherMgr.H>
#include <scheduler/SchedulerService.H>
#include <sync/MPMsgMgrEnabled.H>
#include <sync/MPMsgMgrDisabled.H>
#include "exception/MPMsgMgrException.H"
#include "exception/HWInterrupt.H"
#include "exception/DispatcherDefaultKern.H"
#include "exception/KernelInfoMgr.H"
#include <sync/BlockedThreadQueues.H>
#include <misc/hardware.H>
#include <sys/time.h>
#include <stub/StubLogin.H>
#include <stub/StubKBootParms.H>
#include <io/FileLinux.H>
#include <io/PacketServer.H>
#include <stub/StubMountPointMgr.H>
#include <meta/MetaStreamServer.H>
#include "bilge/SystemControl.H"
#include "defines/sim_bugs.H"
#include "bilge/BuildDate.H"
#include "bilge/KParms.H"
#include "bilge/KBootParms.H"

#if defined(TARGET_powerpc)
#include "bilge/arch/powerpc/openfirm.h"
#endif /* #if defined(TARGET_powerpc) */

//MAA temp
#include "mem/FCMComputation.H"

#include "mem/PMLeafExp.H"
#include "mem/PMLeafChunk.H"

#include "bilge/MIP.H"

#include "InitServer.H"
#include <sys/InitStep.H>
#include <sys/Initialization.H>

void InitKickOff();
INIT_OBJECT_PTR(KickOffInit, "initialization kickoff",
		INIT_KICKOFF, InitKickOff);

extern "C" uval testCR(uval x);

// exists just to act as a trigger for simos checkpointing, still experimental
extern "C" void TriggerSimosCheckpoint()
{
#if defined(TARGET_mips64)
    // need filler to make simos happy under high optimizations on mips
    *(volatile char *)TriggerSimosCheckpoint;
    *(volatile char *)TriggerSimosCheckpoint;
    *(volatile char *)TriggerSimosCheckpoint;
#endif /* #if defined(TARGET_mips64) */
}

#if defined(TARGET_mips64)
/*static*/ SysStatus
DoCheckpointCleanup(uval arg)
{
    // workaround simos bug; simos doesn't reinstate timer, so we force
    // a timer interrupt in the near future to kick things off
    uval count, compare, interval;
    interval = 0x200;
    __asm __volatile(ASM_NR
		     "mfc0 %0, $%3; daddu %1, %0, %2; mtc0 %1, $%4"
		     ASM_R
		     : "=&r"(count), "=&r"(compare)
		     : "r"(interval), "i"(C0_COUNT), "i"(C0_COMPARE));
    return 0;
}
#endif /* #if defined(TARGET_mips64) */

void DoCheckpoint()
{
    // do pre-checkpoint stuff here
    disableHardwareInterrupts();
    TriggerSimosCheckpoint();
    // do post-checkpoint stuff here (note, no way yet to determine if
    // returning from a checkpoint save or a checkpoint restore yet)
#if defined(TARGET_mips64)
    enableHardwareInterrupts();		// need to be fully enabled for this
    SysStatus rc, retRC;
    VPNum myvp = Scheduler::GetVP();
    VPNum vpCnt = _SGETUVAL(DREFGOBJK(TheProcessRef)->vpCount());
    // workaround simos bug; simos doesn't reinstate timer, so we force
    // a timer interrupt in the near future to kick things off on all
    // processors
    for (VPNum i = 0; i < vpCnt; i++) {
	if (i == myvp) continue;
	rc = MPMsgMgr::SendSyncUval(Scheduler::GetEnabledMsgMgr(),
				    SysTypes::DSPID(0, i),
				    DoCheckpointCleanup, 0, retRC);
	tassert(_SUCCESS(rc), err_printf("oops\n"));
    }
    DoCheckpointCleanup(0);		// now do it for myself
    disableHardwareInterrupts();	// set back as expected
#endif /* #if defined(TARGET_mips64) */
    enableHardwareInterrupts();
    err_printf("Triggered [Restore]Checkpoint\n");
}

SysStatus
PrintStatusAllMsgHandler(uval dumpThreads)
{
    err_printf("PrintStatus for pp %ld\n", Scheduler::GetVP());
    ExceptionLocal::PrintStatus(dumpThreads);
    return 0;
}

// do ExceptionLocal::PrintStatus on all processors
static void
PrintStatusRange(VPNum startVP, VPNum endVP, uval dumpThreads)
{
    VPNum numVPs, vp, myvp;

    myvp = Scheduler::GetVP();
    numVPs = _SGETUVAL(DREFGOBJK(TheProcessRef)->vpCount());
    if (endVP > numVPs) endVP = numVPs;

    for (vp = startVP; vp < endVP; vp++) {
	if (vp == myvp) {
	    err_printf("PrintStatus for pp %ld\n", uval(myvp));
	    ExceptionLocal::PrintStatus(dumpThreads);
	} else {
	    SysStatus rc, retRC;
	    rc = MPMsgMgr::SendSyncUval(Scheduler::GetEnabledMsgMgr(),
					SysTypes::DSPID(0, vp),
					PrintStatusAllMsgHandler,
					dumpThreads, retRC);
	    tassertWrn(_SUCCESS(rc), "SendSyncUval failed\n");
	    tassertWrn(_SUCCESS(retRC),"PrintStatusMsgMgr failed\n");
	}
    }
}

// do ExceptionLocal::PrintStatus on all processors
// static FIUXME: make static again
void
PrintStatusAll()
{
    PrintStatusRange(0, Scheduler::VPLimit, 1);
    err_printf("\nPending Page Faults\n");
    FCM::PrintStatus(FCM::PendingFaults);
    kernRunInternalProcess("baseServers", "-ps", NULL, /*wait*/0);
}

sval
KernelAtoi(char* p)
{
    sval i=0;
    sval s=1;
    char c;

    while ((*p == ' ') || (*p == '\t')) p++;
    if (*p == '-') {
	s = -1;
	p++;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
	// interpret hex values
	p += 2;
	while ((c=*p++)) {
	    uval x;
	    if ((c >= 'a') && (c <= 'f')) {
		x = c - 'a' +10;
	    } else if ((c >= 'A') && (c <= 'F')) {
		x = c - 'A' +10;
	    } else if ((c >= '0') && (c <= '9')) {
		x = c - '0';
	    } else {
		break;
	    }
	    i = (i << 4) + x;
	}
    } else {
	while ((c=*p++)) {
	    if (c<'0'||c>'9') break;
	    i=i*10+(c-'0');
	}
    }
    return s*i;
}

extern sval
printfBuf(const char *fmt0, va_list argp, char *buf, sval buflen);

// prints out memory resources
extern void resourcePrint(uval all=1);
extern void resourcePrintFragmentation();

/*
 * this nonsense runs a thread in a delay loop.
 * if you break in with the debugger and set startziggy non-zero,
 * ziggy will start.  This is useful if a run suddenly hangs
 * for no apparent reason.
 */

sval
zig_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    char buf[256];
    uval len = printfBuf(fmt, ap, buf, sizeof(buf));

#if defined(TARGET_powerpc)
    InterruptState is;
    uval wasEnabled = hardwareInterruptsEnabled();
    if (wasEnabled) {
	disableHardwareInterrupts(is);
    }
    thinwireWrite(ZIGGYDEB_CHANNEL, buf, len);
    if (wasEnabled) {
	enableHardwareInterrupts(is);
    }
#elif defined(TARGET_mips64)
    extern void enabledThinwireWrite(uval channel, const char * buf,
				     uval length);
    enabledThinwireWrite(ZIGGYDEB_CHANNEL, buf, len);
#elif defined(TARGET_amd64)
// check me XXX pdb, like powerpc
    InterruptState is;
    uval wasEnabled = hardwareInterruptsEnabled();
    if (wasEnabled) {
	disableHardwareInterrupts(is);
    }
    thinwireWrite(ZIGGYDEB_CHANNEL, buf, len);
    if (wasEnabled) {
	enableHardwareInterrupts(is);
    }
#elif defined(TARGET_generic64)
    extern void enabledThinwireWrite(uval channel, const char * buf,
				     uval length);
    enabledThinwireWrite(ZIGGYDEB_CHANNEL, buf, len);
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */
    va_end(ap);

    return 0;
}

static void
ziggy(uval)
{
    uval readrc = 0;
    char buf[80 + 1];	// extra space for null terminator
    uval len;
    zig_printf("ziggy: Characters typed here will be redirected "
					"to the k42 console.\n\r");
    zig_printf("       Output will appear at the console, not here.\n\r");
    while (1) {
	zig_printf("ziggy [?]:>");
	len = 0;
	do {
	    Scheduler::DeactivateSelf();
	    Scheduler::DelayMicrosecs(100000);
	    Scheduler::ActivateSelf();
#if defined(TARGET_powerpc)
	    InterruptState is;
	    disableHardwareInterrupts(is);
	    if ((ThinWireChan::thinwireSelect() & (1<<ZIGGYDEB_CHANNEL)) != 0) {
		readrc = thinwireRead(ZIGGYDEB_CHANNEL, buf+len, 80-len);
	    } else {
		readrc = 0;
	    }
	    enableHardwareInterrupts(is);
#elif defined(TARGET_mips64)
	    extern uval enabledThinwireRead(uval channel, char * buf,
					    uval length);
	    extern uval enabledThinwireSelect();
	    if ((enabledThinwireSelect() & (1<<ZIGGYDEB_CHANNEL)) != 0) {
		readrc = enabledThinwireRead(ZIGGYDEB_CHANNEL, buf+len,80-len);
	    } else {
		readrc = 0;
	    }
#elif defined(TARGET_amd64)
// check me XXX pdb, like powerpc
	    InterruptState is;
	    disableHardwareInterrupts(is);
	    if ((ThinWireChan::thinwireSelect() & (1<<ZIGGYDEB_CHANNEL)) != 0) {
		readrc = thinwireRead(ZIGGYDEB_CHANNEL, buf+len, 80-len);
	    } else {
		readrc = 0;
	    }
	    enableHardwareInterrupts(is);
#elif defined(TARGET_generic64)
	    if ((ThinWireChan::thinwireSelect() & (1<<ZIGGYDEB_CHANNEL)) != 0) {
		readrc = thinwireRead(ZIGGYDEB_CHANNEL, buf+len, 80-len);
	    } else {
		readrc = 0;
	    }
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */

	    buf[len+readrc] = '\0';
	    zig_printf("%s", buf+len);
	    len += readrc;

	} while (((len == 0) || (buf[len-1] != '\r')) && (len < 80));
	zig_printf("\n");
	(void) SystemMisc::_SystemControlInsert(buf, len);
    }
}

static uval startziggy=0;

void
ziggy_start(uval)
{
    while (1) {
	if (startziggy) {
	    cprintf("type the following in a pty:"
		    "\n\tconsole localhost:XXXXX\n");
	    cprintf("where XXXXX is the port that thinwire"
		    " says it is waiting on\n");
	    ziggy(0);
	}
	Scheduler::DeactivateSelf();
	Scheduler::DelayMicrosecs(1000000);
	Scheduler::ActivateSelf();
    }
}

void
doHWMonitor()
{
    uval leave = 0;
    sval c;
    class AutoHWPerfMonAcquire {
        SysStatus rc;
    public:
        AutoHWPerfMonAcquire() {
            rc=DREFGOBJK(TheHWPerfMonRef)->acquire();
        }
        ~AutoHWPerfMonAcquire() {
            if (_SUCCESS(rc)) {
                rc=DREFGOBJK(TheHWPerfMonRef)->release();
                passert(_SUCCESS(rc), err_printf("oops release failed"));
            }
        }
        uval gotIt() { return _SUCCESS(rc); }
        DEFINE_NOOP_NEW(AutoHWPerfMonAcquire);
    } HWPerfReservation;

    if (!HWPerfReservation.gotIt()) {
        err_printf("Performance Monitoring Hardware is Busy\n");
        return;
    }

    while (!leave) {
        leave = 1;
        cprintf("  Enter HWPerf Command [?]> ");
        char tbuf[80];
        uval len = sizeof(tbuf) - 1;	// leave room for null terminator
        len = SystemControl::Read(tbuf,len);
        tbuf[len] = '\0';
        c = tbuf[0];
        switch (c) {
        case '0':
            err_printf("Enter The delay (# period events passed)\n\n");
            len = sizeof(tbuf) - 1;
            len = SystemControl::Read(tbuf,len);
            tbuf[len] = '\0';
            {
                uval64 delay = (uval)-1;
                if (tbuf[0] != '\n') {
		    delay=KernelAtoi(tbuf);
		}
                err_printf("setting the delay to %lld\n", delay);
	        SystemMisc::_HWPerfStartSamplingAllProcs(0, delay);
	        // DREFGOBJK(TheHWPerfMonRef)->startSampling(delay);
            }
            break;
        case '1':
	    SystemMisc::_HWPerfStopSamplingAllProcs(0);
	    // DREFGOBJK(TheHWPerfMonRef)->stopSampling();
            break;
        case '2':
            err_printf("Enter The delay (# period events passed)\n\n");
            len = sizeof(tbuf) - 1;
            len = SystemControl::Read(tbuf,len);
            tbuf[len] = '\0';
            {
                uval64 delay = (uval)-1;
                if (tbuf[0] != '\n') {
		    delay=KernelAtoi(tbuf);
		}
                err_printf("setting the delay to %lld\n", delay);
	        SystemMisc::_HWPerfStartCPIBreakdownAllProcs(0, delay);
	        // DREFGOBJK(TheHWPerfMonRef)->startCPIBreakdown(delay);
            }
            break;
        case '3':
	    SystemMisc::_HWPerfStopCPIBreakdownAllProcs(0);
	    // DREFGOBJK(TheHWPerfMonRef)->stopCPIBreakdown();
            break;

        case 't':
            err_printf("Enter The period type :\n0=Time (cycles) \n1=Instructions Completed\n\n");
            len = sizeof(tbuf) - 1;
            len = SystemControl::Read(tbuf,len);
            tbuf[len] = '\0';
            {
                uval32 periodType = (uval)-1;
                if (tbuf[0] != '\n') {
		    periodType=KernelAtoi(tbuf);
		}
                err_printf("setting the period type to %d\n", periodType);
	        SystemMisc::_HWPerfSetPeriodTypeAllProcs(0, periodType);
                // DREFGOBJK(TheHWPerfMonRef)->setPeriodType(periodType);
            }
            break;

        case 'e':
            err_printf("Enter The Counting Mode :\n1=Kernel Only\n2=User Only\n0=Both\n");
            len = sizeof(tbuf) - 1;
            len = SystemControl::Read(tbuf,len);
            tbuf[len] = '\0';
            {
                uval32 mode = (uval)-1;
                if (tbuf[0] != '\n') {
		    mode=KernelAtoi(tbuf);
		}
                err_printf("setting the counting mode to %d\n", mode);
	        SystemMisc::_HWPerfSetCountingModeAllProcs(0, mode);
                // DREFGOBJK(TheHWPerfMonRef)->setCountingMode(mode);
            }
            break;

        case 'R':
            err_printf("Enter the Multiplexing Round in cycles :\n ");
            len = sizeof(tbuf) - 1;
            len = SystemControl::Read(tbuf,len);
            tbuf[len] = '\0';
            {
                uval32 round = (uval)-1;
                if (tbuf[0] != '\n') {
		    round=KernelAtoi(tbuf);
		}
                err_printf("setting the sampling round to %d cycles\n", round);
	        SystemMisc::_HWPerfSetMultiplexingRoundAllProcs(0, round);
                // DREFGOBJK(TheHWPerfMonRef)->setMultiplexingRound(round);
            }
            break;

        case 'L': 
	    {
            err_printf("Enter the Loging Period in cycles :\n ");
            len = sizeof(tbuf) - 1;
            len = SystemControl::Read(tbuf,len);
            tbuf[len] = '\0';
            {
                uval32 period = (uval)-1;
                if (tbuf[0] != '\n') {
		    period=KernelAtoi(tbuf);
		}
                err_printf("setting the logging period to %d cycles\n", period);
	        SystemMisc::_HWPerfSetLogPeriodAllProcs(0, period);
                // DREFGOBJK(TheHWPerfMonRef)->setLogPeriodRound(round);
            }
            break;
            }

        case 'G':
	    {
		err_printf("Enter The (Group Number,Share Pair) :\n ");
		len = sizeof(tbuf) - 1;
		len = SystemControl::Read(tbuf,len);
		uval32 groupNo = (uval)-1;
		uval32 samplingFreq=0;
		char tmpBuf[8];

		int i = 0;
		while (tbuf[i] != ',') {
		    tmpBuf[i] = tbuf[i];
		    i++;
		}
		tmpBuf[i] = '\n';
		if (tmpBuf[0] != '\n') {
		    groupNo=KernelAtoi(tmpBuf);
		}

		int j = 0;
		i++;  // to skip the ","
                while ((tbuf[i] != '\n') && (tbuf[i] != ',')) {
		    tmpBuf[j++] = tbuf[i++];
		}
		tmpBuf[j] = '\n';
		uval32 share = (uval)-1;
		if (tmpBuf[0] != '\n') {
		    share=KernelAtoi(tmpBuf);
		}

		if (tbuf[i] != '\n') {
                    j = 0;
		    i++;
                    while (tbuf[i] != '\n') {
		        tmpBuf[j++] = tbuf[i++];
		    }
		    if (j) {
		       tmpBuf[j] = '\n';
		       samplingFreq = KernelAtoi(tmpBuf);
		    }
		}
	        SystemMisc::_HWPerfAddGroupAllProcs(0, groupNo, share, samplingFreq);
            }
            break;

	case 'g':
	    {
		err_printf("Enter The Group Number :\n ");
		len = sizeof(tbuf) - 1;
		len = SystemControl::Read(tbuf,len);
		uval32 groupNo = (uval)-1;
		if (tbuf[0] != '\n') {
		    groupNo=KernelAtoi(tbuf);
		}
	        SystemMisc::_HWPerfRemoveGroupAllProcs(0, groupNo);
                // DREFGOBJK(TheHWPerfMonRef)->removeGroup(groupNo);
                break;
            }

        case 'Q':
            return;
        case '?':
        default:
          cprintf(" e     - Set counting mode (1=kernel only, 2=user-only, 0=both)\n");
          cprintf(" R     - Set multiplexing round                         \n");
          cprintf(" L     - Set logging period                             \n");
          cprintf(" z     - Zero counters                                  \n");
          cprintf(" G/g   - add/remove a predefined group of counters      \n");
          cprintf(" 0/1   - start/stop Sampling                            \n");
          cprintf(" 2/3   - start/stop CPI breakdown                       \n");
          cprintf(" Q     - return to test loop                            \n");
             leave = 0;
             break;
        }
    }
}

static uval doDefault=0;

//FIXME: Did not want to replicate this in SystemMisc however this is not
//       The right place for it.  Please cleanup.  For that matter it might
//       be good to unify the this menu with traceControl and SystemMisc
//       Interfaces.
void
dumpTraceBuffers()
{
    uval wait = 0; // do not block for traced
    char arg1[8];

    strcpy(arg1, "--dump");

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;

    if (trcInfo->tracedRunning) {
	err_printf("tracing daemon already running\n");
	return;
    }
    kernRunInternalProcess("/kbin/tracedServer", arg1, NULL, wait);
}

/*
 * Allows selection of tracing
 */
void
traceMenu(uval len, char *inbuf)
{
    sval c,d;
    uval wait;
    char vps[8], vpsN[8];
    extern void tracePrintBuffers(uval);

    TraceInfo *trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);

    while (1) {
	c = inbuf[0];
	switch (c) {
	case '\n':
	case '\r':
	    break;

	case 'B':
	    tracePrintBuffers(0);
	    return;
	case 'C':
	    SystemMisc::_TraceSetMaskAllProcs(0, 0);
	    return;
	case 'D':
	    wait = 0; // do not block for traced
	    strcpy(vps, "--vp");
	    strcpy(vpsN, "1");
	    if (!(trcInfo->tracedRunning)) {
		//FIXME we could use a lock or some sychronization here
		trcInfo->tracedRunning = 1;
		kernRunInternalProcess("tracedServer", vps, vpsN, wait);
	    } else {
		SystemMisc::_TraceStopTraceDAllProcs(0);
	    }
	    return;
	case 'E':
	    dumpTraceBuffers();
	    return;
	case 'F':
	    SystemMisc::_TraceSetMaskAllProcs(TRACE_ALL_MASK, 0);
	    return;
	case 'G':
	    SystemMisc::_TracePrintMaskAllProcs(0);
	    return;
	case 'H':
	    uval mask;
	    mask = (len > 2) ? KernelAtoi(inbuf+1) : TRACE_ALL_MASK;
	    SystemMisc::_TraceSetMaskAllProcs(mask, 0);
	    return;
	case 'I':
	    SystemMisc::_TraceGetInfoAllProcs(0);
	    return;
	case 'O':
	    cprintf("NYI\n");
//	    SystemMisc::_TraceSetOverflowBehaviorAllProcs(
//		1-trcInfo->overflowBehavior, 0);
	    return;
	case 'P':
	    tracePrintBuffers(0);
	    return;
	case 'Q':
	    return;
	case 'R':
	    SystemMisc::_TraceResetAllProcs(0);
	    return;
	case 'S':
	    if (len>=2) {
		d = inbuf[1];
		switch (d) {
		case '0':
		    SystemMisc::_TraceSetMaskAllProcs(0, 0);
		    break;
		case 'F':
		    SystemMisc::_TraceSetMaskAllProcs(TRACE_ALL_MASK, 0);
		    break;
		case '1':
		    SystemMisc::_TraceSetMaskAllProcs(TRACE_CONTROL_MASK, 0);
		    break;
		case '2':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_HWPERFMON_MASK|TRACE_EXCEPTION_MASK),0);
		    break;
		case '3':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_HWPERFMON_MASK|TRACE_EXCEPTION_MASK|
			TRACE_LOCK_MASK),0);
		    break;
		case '4':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_EXCEPTION_MASK|TRACE_CLUSTOBJ_MASK),0);
		    break;
		case '5':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_CLUSTOBJ_MASK),0);
		    break;
		case '6':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_RESMGR_MASK),0);
		    break;
		case '7':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_HWPERFMON_MASK|TRACE_EXCEPTION_MASK|
			TRACE_RESMGR_MASK),0);
		    break;
		case '8':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_HWPERFMON_MASK|TRACE_EXCEPTION_MASK|
			TRACE_LOCK_MASK|TRACE_SCHEDULER_MASK),0);
		    break;
		case '9':
		    SystemMisc::_TraceSetMaskAllProcs((TRACE_CONTROL_MASK|
			TRACE_FS_MASK), 0);
		    break;
		default:
		    break;
		}
	    }
	    return;
	case '?':
	default:
            cprintf(" B - print trace buffers               ");

	    cprintf(" C - clear trace mask - set to 0     \n");
	        if (trcInfo->tracedRunning) {
	    cprintf(" D - stop trace logging daemon         ");
	        } else {
	    cprintf(" D - start trace logging daemon        ");
	        }
            cprintf(" E - dump trace buffers              \n");
	    cprintf(" F - set trace mask - all majors on    ");
	    cprintf(" G - get trace mask                  \n");
	    cprintf(" H [<mask>] - set trace mask           ");
	    cprintf(" I - get trace info                  \n");
            cprintf(" P - print trace buffers from kernel   ");
	    cprintf(" 0 - toggle overflow behavior        \n");
	    cprintf(" Q - return to test loop               ");
            cprintf(" R - reset trace structures          \n");
            cprintf(" S#- set mask to 'or' of values      \n");
            cprintf("     S0: 0\n");
            cprintf("     SF: TRACE_ALL_MASK\n");
            cprintf("     S1: TRACE_CONTROL\n");
            cprintf("     S2: CONTROL, HWPERFMON, EXCEPTION (for profiling)\n");
            cprintf("     S3: CONTROL, HWPERFMON, EXCEPTION, LOCK "
		    "(for profiling locks)\n");
            cprintf("     S4: CONTROL, EXCEPTION, CLUSTOBJ (for gen count)\n");
            cprintf("     S5: CONTROL, CLUSTOBJ (for gen count)\n");
            cprintf("     S6: CONTROL, RESMGR (for resource manager)\n");
            cprintf("     S7: CONTROL, RESMGR, HWPERFMON, EXCEPTION\n");
            cprintf("     S8: CONTROL, HWPERFMON, EXCEPTION, LOCK, SCHEDULER\n");
            cprintf("     S9: CONTROL, FS\n");
	    break;
	}
	cprintf(" trace [?]> ");
	char tbuf[80];
	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf,len);
	tbuf[len] = '\0';
	inbuf = tbuf;
    }
}

/*
 * Allows selection of which test or none to perform
 * we need to get a timeout in here so that
 * you don't always have to answer the question.
 */
void
doCntl()
{
    sval c;
    uval i, msecs;
    extern void runObjGC();
    extern void tracePrintBuffers(uval);
    while (1) {
	cprintf(" cmd [?]> ");
	char tbuf[80];
	uval len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf,len);
	tbuf[len] = '\0';
	c = tbuf[0];
	switch (c) {
	case '\n':
	case '\r':
	    break;
#if defined (TARGET_powerpc)
	case 'B':
//	    rtas_system_reboot();
	    return;
#endif /* #if defined (TARGET_powerpc) */
	case 'C':
	    while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	    tbuf[len] = '\0';
	    i = 1; while ((i < len) && (tbuf[i] == ' ')) i++;
	    if (i < len) {
		uval ctrlFlags;
		if (tbuf[i] == 'C') {
		    ctrlFlags = 0;
		} else if (tbuf[i] == 'S') {	// for SDET
		    ctrlFlags =
			uval(1) << KernelInfo::RUN_SILENT |
			uval(1) << KernelInfo::DISABLE_IO_CPU_MIGRATION |
			uval(1) << KernelInfo::SLOW_THINWIRE_POLLING |
			uval(1) << KernelInfo::UID_PROCESSOR_ASSIGNMENT |
			uval(1) << KernelInfo::DYN_PROCESSOR_ASSIGNMENT |
			uval(1) << KernelInfo::PAGING_OFF |
			uval(1) << KernelInfo::NON_SHARING_FILE_OPT |
			uval(1) << KernelInfo::USE_MULTI_REP_FCMS |
			uval(1) << KernelInfo::NO_ALLOC_SANITY_CHECK |
			uval(1) << KernelInfo::SMALL_FILE_OPT;
		} else {
		    ctrlFlags = KernelAtoi(&tbuf[i]);
		}
		KernelInfoMgr::SetControl(ctrlFlags);
	    }
	    err_printf("KernelInfo::ControlFlags:  0x%lx\n",
					KernelInfo::GetControlFlags());
#define PRINT_BIT(name) \
	    err_printf("    bit %2d [0x%016lx] (%s) is %s\n", \
		KernelInfo::name, \
		uval(1) << KernelInfo::name, \
		#name, \
		KernelInfo::ControlFlagIsSet(KernelInfo::name) ? "ON" : "OFF")
	    PRINT_BIT(RUN_SILENT);
	    PRINT_BIT(DISABLE_IO_CPU_MIGRATION);
	    PRINT_BIT(SLOW_THINWIRE_POLLING);
	    PRINT_BIT(UID_PROCESSOR_ASSIGNMENT);
	    PRINT_BIT(DYN_PROCESSOR_ASSIGNMENT);
	    PRINT_BIT(PAGING_OFF);
	    PRINT_BIT(NON_SHARING_FILE_OPT);
	    PRINT_BIT(USE_MULTI_REP_FCMS);
	    PRINT_BIT(NO_ALLOC_SANITY_CHECK);
	    PRINT_BIT(SMALL_FILE_OPT);
	    PRINT_BIT(DBG_FLAG);
	    PRINT_BIT(SLOW_EXEC);
	    PRINT_BIT(NFS_INTERCEPTION);
	    PRINT_BIT(SHARED_PAGE_TABLE);
	    PRINT_BIT(DONT_DISTRIBUTE_PMROOT);
	    PRINT_BIT(DONT_DISTRIBUTE_PROCESS);
	    PRINT_BIT(DONT_DISTRIBUTE_PRMTV_FCM);
	    PRINT_BIT(NO_NONBLOCKING_HASH);
	    PRINT_BIT(NO_SHARED_SEGMENTS);
	    PRINT_BIT(NO_NUMANODE_PER_VP);
	    PRINT_BIT(TEST_FLAG);
	    PRINT_BIT(NFS_REVALIDATION_OFF);
	    PRINT_BIT(BREAKPOINT_KERNEL_INIT);
#undef PRINT_BIT
	    return;
	case 'D':
	    while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	    tbuf[len] = '\0';
	    i = 1; while ((i < len) && (tbuf[i] == ' ')) i++;
	    char *image;
	    image = (i < len) ? &tbuf[i] : NULL;
	    kernRunInternalProcess("baseServers", "-d", image, /*wait*/ 0);
	    return;
	case 'E':
	    resourcePrintFragmentation();
	    return;
#if 0
	case 'F': {
	    uval local = 0;
	    uval thinwire = 0;
	    for (i = 1; i < len; i++) {
		if (((len - i) >= 5) &&
		    (strncmp(tbuf+i, "local", 5) == 0)) {
		    local = 1;
		}
		if (((len - i) >= 8) &&
		    (strncmp(tbuf+i, "thinwire", 8) == 0)) {
		    thinwire = 1;
		}
	    }
	    if (!local && !thinwire) {
		cprintf("No console specified; using local console.\n");
		local = 1;
	    }
	    exceptionLocal.console.setConsole(local, thinwire);
	    return;
	}
#endif
	case 'G':
	    breakpoint();
	    return;
	case 'I':
	    doDefault = doDefault?0:1;
	    cprintf("doDefault set to %ld\n", doDefault);
	    return;
	case 'K':{
	    ProcessID pid;
	    BaseProcessRef pref;
	    while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	    tbuf[len] = '\0';
	    i = 1; while ((i < len) && (tbuf[i] == ' ')) i++;
	    pid = KernelAtoi(tbuf + i);
	    DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, pref);
	    DREF((ProcessRef) pref)->resetLeakInfo();
	    return;
	}
	case 'L':{
	    ProcessID pid;
	    BaseProcessRef pref;
	    while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	    tbuf[len] = '\0';
	    i = 1; while ((i < len) && (tbuf[i] == ' ')) i++;
	    pid = KernelAtoi(tbuf + i);
	    DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, pref);
	    DREF((ProcessRef) pref)->dumpLeakInfo();
	    return;
	}
	case 'M':
	    msecs = (len > 2) ? KernelAtoi(tbuf+1) : 2000;
	    ((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(msecs);
	    return;
	case 'N':
	    // MAA test stuff
	{
#if 0
	    uval year, month, day, hour, minute, second, nonoseconds, rc;
	    rc = rtas_get_time_of_day(year, month, day,
				      hour, minute, second, nonoseconds);
	    err_printf("%ld %ld %ld %ld %ld %ld %ld %ld \n", rc,
		       year, month, day, hour, minute, second, nonoseconds);
	    struct timeval tv;
	    ThinIP::GetThinTimeOfDay(tv);
	    err_printf("%ld %ld", tv.tv_sec, tv.tv_usec);
	    break;
#endif /* #if 0 */
	}

	case 'O':{
	    ProcessID pid;
	    BaseProcessRef pref;
	    while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	    tbuf[len] = '\0';
	    i = 1; while ((i < len) && (tbuf[i] == ' ')) i++;
	    pid = KernelAtoi(tbuf + i);
	    DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, pref);
	    DREF((ProcessRef) pref)->dumpCObjTable();
	    return;
	}
	case 'Q':
	    return;
	case 'R':
	    resourcePrint();
#ifdef DEBUG_MEMORY
	    DREFGOBJK(ThePinnedPageAllocatorRef)->leakProofPrint();
	    DREFGOBJK(ThePinnedPageAllocatorRef)->leakProofReset();
#endif /* #ifdef DEBUG_MEMORY */
#ifdef DEBUG_LEAK
	    allocLeakProof->print();
	    allocLeakProof->reset();
#endif /* #ifdef DEBUG_LEAK */
	    DREFGOBJ(TheCOSMgrRef)->print();
	    return;
#if defined (TARGET_powerpc)
	case 'S':
  	    if (KernelInfo::OnSim() == SIM_SIMOSPPC) {
		// whatever we wish to simos to return when it exits
		uval code = 17;
		SimOSSupport(SimExitCode, code);
		cprintf("shutdown sim\n");
	    } else if (KernelInfo::OnSim() == SIM_MAMBO) {
	        // FIXME: needs to invoke proper mambo interface
	        cprintf("Ignoring this command on mambo\n");
	    } else {
//		rtas_shutdown(-1,-1);
	    }
	    return;
#endif /* #if defined (TARGET_powerpc) */
	case 'T':
	    if (tbuf[1] == '|') {
		traceMenu(strlen(&(tbuf[2])), &(tbuf[2]));
	    } else {
		strcpy(tbuf, "?");
		traceMenu(1, tbuf);
	    }
	    return;
	case 'U':
	    Scheduler::DeactivateSelf();
	    runObjGC();
	    Scheduler::ActivateSelf();
	    return;
	case 'V':
#if defined (TARGET_powerpc)
	    exceptionLocal.pageTable.printStats();
	    while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	    tbuf[len] = '\0';
	    i = 1; while ((i < len) && (tbuf[i] == ' ')) i++;
	    if ((i < len) && (tbuf[i] == 'C')) {
		exceptionLocal.pageTable.clear();
	    }
#endif
	    void MarcPrintStats();
	    MarcPrintStats();
	    return;
	case 'W':
	    cprintf("key          min max\n");
	    cprintf("A            10ms  10sec\n");
	    cprintf("B            100ms 10sec\n");
	    cprintf("C            10ms  1sec\n");
	    cprintf("D(efault)    130ms 130ms\n");
	    cprintf("key> ");
	    len = sizeof(tbuf) - 1;	// leave room for null terminator
	    len = SystemControl::Read(tbuf,len);
	    c = tbuf[0];
	    if (c == 'A') {
		cprintf("putting thinwire poll to min: 10ms, max 10sec\n");
		ThinWireMgr::SetPollDelay(10000, 10000000);
	    } else if (c == 'B') {
		cprintf("putting thinwire poll to min: 100ms, max 10sec\n");
		ThinWireMgr::SetPollDelay(100000, 10000000);
	    } else if (c == 'C') {
		cprintf("putting thinwire poll to min: 10ms, max 1sec\n");
		ThinWireMgr::SetPollDelay(10000, 1000000);
	    } else {
		cprintf("putting thinwire poll to min: 13ms, max 13ms\n");
		ThinWireMgr::SetPollDelay(130000, 130000);
	    }

	    return;
	case 'X':
	    uval killThinwire, physProcs, ctrlFlags;
	    killThinwire = 0;
	    physProcs = KernelInfo::CurPhysProcs();
	    ctrlFlags = KernelInfo::GetControlFlags();
	    while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	    tbuf[len] = '\0';
	    i = 1;
	    if ((i < len) && (tbuf[i] == 'X')) {
		killThinwire = 1;
		physProcs = 1; // reset physProcs on XX
		ctrlFlags = 0; // reset control flags on XX
		i++;
	    }
	    if (i < len) {
		physProcs = KernelAtoi(tbuf + i);
	    }
	    KernelExit(killThinwire, physProcs, ctrlFlags);
	    // NOTREACHED
	    return;

	case 'Z':
	    startziggy=1;
	    return;

	case '?':
	default:
            cprintf(" F [local] [thinwire] - enable console ");
            cprintf(" R - print resources - memory etc    \n");
	    cprintf(" I - toggle doDefault                  ");
	    cprintf(" T - goto trace menu                 \n");
            cprintf(" M <msecs> - set COSMgr cleanup        ");
            cprintf(" E - print memory fragmentation stats \n");
	    cprintf(" K <pid> - reset leak info             ");
	    cprintf(" L <pid> - print leaks (if compiled) \n");
            cprintf(" U - run obj gc                        ");
	    cprintf(" G - connect to gdb                  \n");
	    cprintf(" C - display current control flags     ");
	    cprintf(" CC - clear all control flags        \n");
	    cprintf(" CS - set control flags for SDET       ");
	    cprintf(" C <flags> - set control flags       \n");
	    cprintf(" V - print pagetable stats             ");
	    cprintf(" VC - print stats and clear pagetable\n");
	    cprintf(" X [NUMB PROCS] - (fast reboot)        ");
	    cprintf(" XX - exit kernel and kill thinwire  \n");
	    cprintf(" D [<boot_image>] - download image     ");
	    cprintf(" Z - start ziggy debug               \n");
	    cprintf(" O <pid> - dump process c-obj table    ");
	    cprintf(" W - change thinwire poll time       \n");
	    cprintf(" Q - return to test loop               ");
#if defined (TARGET_powerpc)
	    cprintf(" B - RTAS reboot (slow)              \n");
	    cprintf(" S - shutdown                          ");
#endif /* #if defined (TARGET_powerpc) */
	    cprintf("\n");
	    break;
	}
    }
}

extern "C" void linuxPSAndKill(uval, uval);
void
linuxPSAndKill(uval cmd, uval num)
{
#if 0
  int i;
  SysStatus rc;
  ProcessLinux::LinuxInfo linuxInfo;

  if (cmd == 0) {
    for (i=300;i<1000;i++) {
      rc = DREFGOBJ(TheProcessLinuxRef)->getInfoLinuxPid(i, linuxInfo);
      if (_SUCCESS(rc)) {
        printk("pid:%d\n", linuxInfo.pid);
      }
    }
  } else {
     printk("kill:%d\n", num);
     rc = DREFGOBJ(TheProcessLinuxRef)->kill(num, 9);
     if (_SUCCESS(rc)) {
       printk("killed pid:%d\n", num);
     } else {
       printk("error: rc:%llx kill pid:%d\n", rc, num);
     }
  }
#endif
}



extern uval configNetDev(char* devname, char* addr, char* mask, char* router);
void StartBlockDev(uval ignore);

void
run_test(char *tbuf, uval len, uval doDefault)
{
    sval c;
    extern void testAlloc(uval, uval);
    extern void testPageAllocator(uval);
    extern void stubgenTest(void);
    extern void testPageFaults(void);
    extern void testPageFaultAllocs(void);
    extern void stubgenTestPgFault(void);
    extern void goop(void);
    extern void testAsync();
    extern void traceTest();
    extern void testBadge();
//    extern void start_lkAcenic(int vp);
    void runRegression();

    c = tbuf[0];
    switch (tbuf[0]) {
    case '\n':
    case '\r':
	break;
    case '0':
	doCntl();
	break;
    case '1':
	testAlloc(AllocPool::PINNED, doDefault); break;
    case '2':
	testPageAllocator(doDefault); break;
    case '3':
	extern uval mhTest();
	mhTest();
	break;
    case '4':
	stubgenTest(); break;
    case '5':
	testPageFaults(); break;
    case '6':
	testPageFaultAllocs(); testAlloc(AllocPool::PAGED, doDefault); break;
    case '7':
	stubgenTestPgFault(); break;
    case '8': {
	err_printf("Test 8 has been disabled\n");
#if 0
	uval wait;
	wait = 0; // don't block for sampleServer
	kernRunInternalProcess("sampleServer", NULL, NULL, wait);
	wait = 1; // block for userProcServer to complete
	if (doDefault == 0) {
	    kernRunInternalProcess("userProcServer", NULL, NULL, wait);
	} else if (doDefault == 1) {
	    kernRunInternalProcess("userProcServer", "regress", NULL, wait);
	} else {
	    tassert(1, err_printf("unknown doDefault value\n"));
	}
#endif
	break;
    }
    case '9':
    {
#if defined(TARGET_powerpc)
	extern void simosGetTimeOfDay(uval32 &secs, uval32 &usecs);
	uval32 secs, usecs;
	if (KernelInfo::OnHV() || !KernelInfo::OnSim())
	  {
	    cprintf("simos GetTimeOfDay is not supported on HV (%ld) or HW (%d)\n",
		    KernelInfo::OnHV(),  !KernelInfo::OnSim());
	  }
	else
	  {
	    simosGetTimeOfDay(secs, usecs);
	    cprintf("secs %d usecs %d\n", secs, usecs);
	  }
#elif defined(TARGET_mips64)
	cprintf("simosGetTime only works on powerpc\n");
#elif defined(TARGET_amd64)
// check me XXX pdb, like mips64, but Simics may ?????? XXX
	cprintf("simosGetTime only works on powerpc\n");
#elif defined(TARGET_generic64)
	cprintf("simosGetTime only works on powerpc\n");
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */
	break;
    }
    case 'A':
    {
	SysTime now;
#if defined(TARGET_powerpc)
	now = getClock();
	cprintf("getClock 0x%08llx%08llx\n",now>>32,now);
	setDecrementer(0x100);
#elif defined(TARGET_mips64)
	disableHardwareInterrupts();
	now = exceptionLocal.kernelTimer.kernelClock.getClock();
	enableHardwareInterrupts();
	uval32 sr = GetCP0Reg(C0_SR);
	cprintf("getClock 0x%llx, sr %x, num_ints 0x%lx, %llx/%x/%x\n",
		now, sr,
		exceptionLocal.num_ints,
		exceptionLocal.kernelTimer.kernelClock.getNow(),
		exceptionLocal.kernelTimer.kernelClock.getc0CountSnapshot(),
		GetCountRegister());
	disableHardwareInterrupts();
	exceptionLocal.kernelTimer.kernelClock.setInterval(0x100);
	enableHardwareInterrupts();
#elif defined(TARGET_amd64)
// check me XXX pdb, like powerpc
	now = getClock();
	cprintf("getClock 0x%08llx%08llx\n",now>>32,now);
	setDecrementer(0x100);
#elif defined(TARGET_generic64)
	now = getClock();
	cprintf("getClock 0x%08llx%08llx\n",now>>32,now);
	setDecrementer(0x100);
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */
    }
    break;
    case 'B':
	testAsync(); break;
    case 'C':
	extern void ConcTestAlloc(uval pool, uval numTests);
	if (doDefault) {
	    ConcTestAlloc(AllocPool::PINNED, 1000);
	    ConcTestAlloc(AllocPool::PAGED, 1000);
	} else {
	    ConcTestAlloc(AllocPool::PINNED, 10000);
	    ConcTestAlloc(AllocPool::PAGED, 10000);
	}
	break;
    case 'D': {
	double space[2];
	double *volatile dbl_ptr = (double *)(uval(space) + 1);
	*dbl_ptr = 2.0;
	*dbl_ptr += 3.0;
	if (*dbl_ptr == 5.0) {
	    err_printf("Misaligned accesses okay.\n");
	} else {
	    err_printf("Misaligned accesses failed.\n");
	}
	break;
    }
    case 'E':{
	SysStatus rc;
	rc = StubLogin::_StartConsoleLogin();
	if (!_SUCCESS(rc)) {
	    err_printf("test: E: Login is not ready. Try again later\n");
	}
	break;
    }
    case 'G':
	traceTest(); break;
    case 'I':
    {
	ProcessID pid;
	BaseProcessRef pref;
	SysStatus rc;
	cprintf("pid to interrupt (base 10): ");
	len = sizeof(tbuf) - 1;	// leave room for null terminator
	len = SystemControl::Read(tbuf,len);
	tbuf[len] = '\0';

	pid = KernelAtoi(tbuf);
	if (pid == 0) {
	    breakpoint();
	    break;
	}
	rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, pref);
	if (_FAILURE(rc)) {
	    cprintf("can't convert pid to process ref\n");
	    break;
	}
	DREF((ProcessRef)pref)->callBreakpoint();
    }
    break;

    case 'J':
    {
	SysTime now;
#if defined(TARGET_powerpc)
	now = getClock();
	cprintf("getClock: 0x%08llx%08llx  Frequency: %lld MHz\n", now>>32, now,
		(exceptionLocal.kernelTimer.kernelClock.getTicksPerSecond()
		 / 1000000));
	cprintf("CPU frequency: %lld MHz\n",
		(_BootInfo->clock_frequency) / 1000000);
#elif defined(TARGET_mips64)
	now = Scheduler::SysTimeNow();
	Scheduler::DelayMicrosecs(5000);
	SysTime now2 = Scheduler::SysTimeNow();
	cprintf("getClock %lld %lld\n", now, now2);
#elif defined(TARGET_amd64)
// check me XXX pdb, like powerpc but w/o CPU frequency, can do better but in time
	now = getClock();
	cprintf("getClock: 0x%08llx%08llx  Frequency: %lld MHz\n", now>>32, now,
		(exceptionLocal.kernelTimer.kernelClock.getTicksPerSecond()
		 / 1000000));
#elif defined(TARGET_generic64)
	now = getClock();
	cprintf("getClock: 0x%08llx%08llx  Frequency: %lld MHz\n", now>>32, now,
		(exceptionLocal.kernelTimer.kernelClock.getTicksPerSecond()/1000000));
#else /* #if defined(TARGET_powerpc) */
#error Need TARGET_specific code
#endif /* #if defined(TARGET_powerpc) */
    }
	break;
    case 'K':
	testBadge(); break;
    case 'M':
        doHWMonitor();
	break;
    case 'O':
    {
        SysStatus num;
        const uval sizeInBytes = PAGE_SIZE * 4;
        const sval numDescs    = sizeInBytes/sizeof(CODesc);
        VPNum myvp = Scheduler::GetVP();
        err_printf("Clustered Objects on vp=%ld:\n", myvp);
        CODesc *coDescs  = (CODesc *)AllocLocalStrict::alloc(sizeInBytes);
        num = DREFGOBJ(TheCOSMgrRef)->getCOList(coDescs, numDescs);
        if (_SUCCESS(num)) {
            tassertMsg(num <= numDescs, "num = %ld > %ld = numDescs\n",
                       num, numDescs);
            for (sval i=0; i<num; i++) {
                err_printf("  ref=%p root=%p typeToken=%p\n",
                           (void *)coDescs[i].getRef(),
                           (void *)coDescs[i].getRoot(),
                           (void *)coDescs[i].getTypeToken());
            }
            err_printf("vp=%ld: Found %ld objects on \n", myvp, num);
            if (num == numDescs) {
                err_printf("*** might have missed some -- "
                           "not enough descriptors to be sure\n");
            }

        } else {
            err_printf("vp=%ld oops getObjList failed\n", myvp);
        }
        AllocLocalStrict::free(coDescs, sizeInBytes);
    }
        break;
    case 'P':
	while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	tbuf[len] = '\0';
	if (tbuf[1] == '\0') {
	    PrintStatusAll();
	} else if (tbuf[1] == 'A') {
	    if (tbuf[2] == '\0') {
		PrintStatusRange(0, Scheduler::VPLimit, 0);
	    } else {
		VPNum pp;
		pp = KernelAtoi(&tbuf[2]);
		PrintStatusRange(pp, pp+1, 0);
	    }
	} else if (tbuf[1] == 'F') {
	    err_printf("\nPending Page Faults\n");
	    FCM::PrintStatus(FCM::PendingFaults);
	    kernRunInternalProcess("baseServers", "-ps", NULL, /*wait*/ 0);
	} else if (tbuf[1] == 'M') {
	    err_printf("\nMemory Use by Process\n");
	    FCM::PrintStatus(FCM::MemoryUse);
	    resourcePrint(0);		// print memory free...
	} else {
	    CommID commID;
	    ProcessID pid;
	    DispatcherID dspid;
	    BaseProcessRef pref;
	    SysStatus rc;

	    commID = KernelAtoi(&tbuf[1]);
	    pid = SysTypes::PID_FROM_COMMID(commID);
	    dspid = SysTypes::DSPID_FROM_COMMID(commID);

	    if (pid == _KERNEL_PID) {
		if (dspid == Scheduler::GetDspID()) {
		    Scheduler::PrintStatus();
		    rc = 0;
		} else {
		    RDNum rd; VPNum vp;
		    SysTypes::UNPACK_DSPID(dspid, rd, vp);
		    if ((rd == 0) &&
			(vp < _SGETUVAL(DREFGOBJ(TheProcessRef)->vpCount())))
		    {
			rc = DREFGOBJ(TheProcessRef)->
				sendInterrupt(dspid, SoftIntr::PRINT_STATUS);
		    } else {
			rc = _SERROR(2188, 0, EINVAL);
		    }
		}
	    } else {
		rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, pref);
		if (_SUCCESS(rc)) {
		    rc = DREF((ProcessRef) pref)->
				sendInterrupt(dspid, SoftIntr::PRINT_STATUS);
		}
	    }
	    if (_FAILURE(rc)) {
		err_printf("Couldn't send status request to commID 0x%lx\n",
			   commID);
	    }
	}
#if 0
	cprintf("printing Region information:\n");
	DREFGOBJK(TheProcessRef)->printRegions();
	cprintf("printing ExceptionLocalInfo:\n");
	exceptionLocal.print();
#endif /* #if 0 */
	break;
    case 'r':
    case 'R':
	runRegression();
	break;
    case 'T': {
	if (tbuf[1] == '|') {
	    traceMenu(strlen(&(tbuf[2])), &(tbuf[2]));
	} else {
	    strcpy(tbuf, "?");
	    traceMenu(1, tbuf);
	}
	break;
    }
    case 'V':
#if defined(TARGET_powerpc)
	rtas_display_character('\r');
	rtas_display_character('K');
	rtas_display_character('4');
	rtas_display_character('2');
	rtas_display_character(' ');
	err_printf("RTAS displayed K42\n");
#endif /* #if defined(TARGET_powerpc) */
	break;

    case 'W':
    {
#if defined(TARGET_powerpc)
	cprintf("linuxPSAndKill (P=ps, <NUM>=kill, C=CANCEL) : ");

        len = sizeof(tbuf) - 1; // leave room for null terminator
        len = SystemControl::Read(tbuf,len);
        tbuf[len] = '\0';
        int num = KernelAtoi(tbuf);

	if (tbuf[0] == 'C') {
	    break;
	} else if (tbuf[0] == 'P') {
	    linuxPSAndKill(0,0);
	} else {
	    linuxPSAndKill(1,num);
	}
#endif /* #if defined(TARGET_powerpc) */
	break;
    }
    case 'X':
	DoCheckpoint();
	break;

    case 'Y':
#if defined(TARGET_powerpc)
//	start_lkAcenic(0);
#endif /* #if defined(TARGET_powerpc) */
	break;
    case 'z':
	if (KernelInfo::OnSim()) {
	    if (tbuf[1] == 'r') {
		cprintf("---- start ztrace ----\n");
	    }
	    if (tbuf[1] == 'p') {
		cprintf("---- stop ztrace ----\n");
	    }
	}
	break;
    case 'Z':
	if (!KernelInfo::OnSim()) {
	    Scheduler::ScheduleFunction(StartBlockDev,0);
	}
	break;


    case 't':
    {
	extern uval ok_kpsfp;
	extern uval ok_kpsfpc;
	extern uval ok_kpswrite;
	extern uval ok_kpcomp;
	extern uval ok_kptrysucc;
extern uval ok_enqueue;
extern uval ok_wokethread;
extern uval ok_sleepthread;
extern uval ok_wakethread;
extern uval ok_wakeFCM;

	err_printf("fillst %ld, fillstcomp %ld, writest %ld, trywrsucc %ld iocomp %ld\n",
		   ok_kpsfp, ok_kpsfpc, ok_kpswrite, ok_kptrysucc, ok_kpcomp);

	err_printf("enqueued %ld, woke thread %ld, woke FCM %ld, slth %ld, wkt %ld\n", ok_enqueue, ok_wakethread, ok_wakeFCM, ok_sleepthread, ok_wokethread);

	DREFGOBJK(TheFSSwapRef)->printStats();
#if 0
	uval pinavail;
	DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(pinavail);
	err_printf("pinned memory available %lx\n", pinavail);

	err_printf("printing FSSwap stats\n");
	DREFGOBJK(TheFSSwapRef)->printStats();
        // A Place to put throw away tests.
#endif
#if 0
//MAA Test
	while ((tbuf[len-1] == '\r') || (tbuf[len-1] == '\n')) len--;
	tbuf[len] = '\0';
	FCMComputation::CopyOnForkFactor = KernelAtoi(&tbuf[1]);
#endif
        break;
    }
    default:
	cprintf(" 0 - CONTROL                           ");
	cprintf(" 1 - pinned alloc test               \n");
	cprintf(" 2 - page allocator test               ");
	cprintf(" 3 - miss handling test              \n");
	cprintf(" 4 - stubgenerator and PPC             ");
	cprintf(" 5 - page faults                     \n");
	cprintf(" 6 - unpinned alloc Test               ");
	cprintf(" 7 - stubgen / PPC with PageFault    \n");
	cprintf(" 8 - User Process Create (userProcSer) ");
	cprintf(" 9 - Get time (simos only)           \n");
	cprintf(" A - Test Timer Int                    ");
	cprintf(" B - Async Test                      \n");
	cprintf(" C - Concurrent pinned/unpinned alloc  ");
	cprintf(" D - Test kernel alignment handler   \n");
	cprintf(" E - run user shell                    ");
	cprintf(" F - ---- UNUSED ------              \n");
	cprintf(" G - test tracing                      ");
	cprintf(" H - interrupt handler stats (N/A)   \n");
	cprintf(" I - break into process                ");
	cprintf(" J - test timer                      \n");
	cprintf(" K - test badge                        ");
	cprintf(" M - configure hw perf gathering     \n");
	cprintf(" P - print various stuff               ");
	cprintf(" PA - print process annexes (all pps)\n");
	cprintf(" PA <pp> - print PAs for one pp        ");
	cprintf(" PF - print pgflts and Linux status  \n");
	cprintf(" PM - print memory use                 ");
	cprintf(" P <commID> - print dispatcher status\n");
	cprintf(" r - run regression test             \n");
	cprintf(" T - goto tracing menu                 ");
	cprintf(" t - throw away for individual use   \n");
#if defined(TARGET_powerpc)
	cprintf(" V - Display Message using RTAS        ");
	cprintf(" W - linuxPSAndKill		      \n");
	if (KernelInfo::OnSim()) {
	    cprintf(" z <r|p> - start|stop mambo ztracing   ");
	    cprintf("                		          \n");
	}
	if (!KernelInfo::OnSim()) {
	    cprintf(" Y - start gigabit ethernet	       ");
	    cprintf(" Z - start SCSI system     	      \n");
	}
#endif /* #if defined(TARGET_powerpc) */
	cprintf("\n");
	break;
    }
    cprintf("\n");
}

void
runRegression()
{
    cprintf("------ RUNNING: 1 - pinned alloc test -------\n");
    SysEnviron::SuspendThinWireDaemon();
    run_test("1", 1, 1);
    SysEnviron::RestartThinWireDaemon();
    cprintf("------ RUNNING: 2 - page allocator test -------\n");
    run_test("2", 1, 1);

#ifndef FAST_REGRESS_ON_SIM
    cprintf("------ SKIPPING: 3 - miss handling test -----\n");
#else
    cprintf("------ RUNNING: 3 - miss handling test -------\n");
    run_test("3", 1, 1);
#endif

    cprintf("------ RUNNING: 4 - stubgenerator and PPC -------\n");
    run_test("4", 1, 1);
    cprintf("------ RUNNING: 5 - page faults -------\n");
    run_test("5", 1, 1);
    cprintf("------ RUNNING: 6 - unpinned alloc Test -------\n");
    run_test("6", 1, 1);
    cprintf("------ RUNNING: 7 - stubgen / PPC with PageFault -------\n");
    run_test("7", 1, 1);
    cprintf("------ RUNNING: A - Test Timer Int -------\n");
    run_test("A", 1, 1);
    if (Scheduler::GetVP() == 0 ) {
    cprintf("------ RUNNING: B - Async Test -------\n");
    run_test("B", 1, 1);
    } else {
	cprintf("------ Not RUNNING: B - Async Test -------\n"
	    "***********Broken on VP 1\n");
    }

    cprintf("------ RUNNING: C - Concurrent alloc -------\n");
    run_test("C", 1, 1);
    cprintf("------ RUNNING: G - test tracing -------\n");
    run_test("G", 1, 1);
    cprintf("------ RUNNING: J - test timer -------\n");
    run_test("J", 1, 1);
    cprintf("------ RUNNING: K - test badge -------\n");
    run_test("K", 1, 1);
}

void choose_test(uval ignore)
{
    while (1) {
	cprintf(" test [?]> ");
	char tbuf[80];
	uval len = sizeof(tbuf) - 1;	// leave room for null terminator
	tassertSilent( hardwareInterruptsEnabled(), BREAKPOINT );
#if defined(TARGET_amd64)
	/* on amd64, for now, just run regression test */
	tbuf[0] ='r';
	len = 1;
#else
	len = SystemControl::Read(tbuf,len);
#endif
	tassertSilent( hardwareInterruptsEnabled(), BREAKPOINT );
	tbuf[len] = '\0';
	run_test(tbuf, len, doDefault);
    }
}

// This is to kill threads from the debugger:
// set $pc=killThread
void killThread()
{
    while (1) {
	Scheduler::Block();
    }
}


static void
KernelInitPhase2(uval kernelInitArgsUval)
{
    /*
     * Make a copy of the whole KernelInitArgs object on our stack.
     * The original object is on the boot-time stack, which is about to be
     * deallocated.
     */

    KernelInitArgs kernelInitArgs = *(KernelInitArgs*)kernelInitArgsUval;

    DispatcherID const dspid = Scheduler::GetDspID();

    RDNum rd; VPNum vp;
    SysTypes::UNPACK_DSPID(dspid, rd, vp);

    uval currentMemorySize;

    tassert(rd == 0, err_printf("Not on resource domain 0!\n"));

    if (KernelInfo::ControlFlagIsSet(KernelInfo::BREAKPOINT_KERNEL_INIT)) {
	err_printf("KernelInitPhase2: ControlFlag BREAKPOINT_KERNEL_INIT.\n");
	breakpoint();
    }

    // class that handles blocking on locks
    BlockedThreadQueues::ClassInit(vp, &kernelInitArgs.memory);

    // The Enabled class may need to be moved further down, but it's not
    // clear how much further down (if moved, change memory arg to NULL)
    MPMsgMgrEnabled::ClassInit(dspid, &kernelInitArgs.memory);
    MPMsgMgrDisabled::ClassInit(dspid, &kernelInitArgs.memory);
    MPMsgMgrException::ClassInit(dspid, &kernelInitArgs.memory);

    // save current memory size, since the primitive memory allocator will
    // soon be empty (see below).  This value will be saved in SystemMisc
    // later, when SystemMisc initializes.
    currentMemorySize = kernelInitArgs.memory.memorySize();
    PageAllocatorKernPinned::ClassInit(vp, &kernelInitArgs.memory);

    DREFGOBJK(ThePinnedPageAllocatorRef)->
	deallocAllPages(&kernelInitArgs.memory);
    // kernelInitArgs.memory is empty now.

    /* ------  from this point on can dynamically allocate pinned ---------- */

    // Create root pm for the system
    // Kludge - 0 turns off dist mechanisms in PMRoot
    PMRoot::ClassInit(
	vp,
	KernelInfo::ControlFlagIsSet(KernelInfo::DONT_DISTRIBUTE_PMROOT) ?
	0 : 1);

    // Once we can allocate pinned memory, make the virt->real FCM
    FCMReal::ClassInit(vp);

    // before allocate an annex, must initialize HAT so that
    // annex can get SegmentTable
    SysStatus rc = HATKernel::ClassInit(vp);
    tassert(!rc,err_printf("create kernel HAT failed\n"));

    err_printf("doing initKernelProcess\n");
    ProcessDefaultKern::InitKern(vp, KernelInfo::CurPhysProcs(),
				 GOBJK(TheKernelHATRef));

    // In the kernel, we can initialize all the entry points as soon as
    // the kernel process object exists.
    Scheduler::EnableEntryPoint(RUN_ENTRY);
    Scheduler::EnableEntryPoint(INTERRUPT_ENTRY);
    Scheduler::EnableEntryPoint(TRAP_ENTRY);
    Scheduler::EnableEntryPoint(PGFLT_ENTRY);
    Scheduler::EnableEntryPoint(IPC_CALL_ENTRY);
    Scheduler::EnableEntryPoint(IPC_RTN_ENTRY);
    Scheduler::EnableEntryPoint(IPC_FAULT_ENTRY);
    Scheduler::EnableEntryPoint(SVC_ENTRY);

    // FIXME move earlier and combine with initExceptionHandlers
    fixupExceptionHandlers(vp);

    /* The initialization of thinwire has been moved further down.
       It is conditional on the environment on which k42 is running
       and on the setting of certain system parameters.
    */
    // initialize class that manages all non exception thin wire interactions
    //ThinWireMgr::ClassInit(vp);

    // back paged part of object translation tables.
    DREFGOBJ(TheCOSMgrRef)->vpMaplTransTablePaged(vp);

    // initialize virtual components of pinned allocator; we pass "memory"
    // for informational value about memory in system, not for direct use
    PageAllocatorKernPinned::ClassInitVirtual(vp, &kernelInitArgs.memory);

    PageAllocatorKernUnpinned::ClassInit(vp);

    /* ------  from this point on can dynamically allocate paged ---------- */

    DispatcherMgr::ClassInit(SysTypes::DSPID(0, vp));
    XHandleTrans::ClassInit(vp);

    extern void ConfigureLinuxEnv(VPNum vp, PageAllocatorRef pa);
    extern void ConfigureLinuxHWBase(VPNum vp);
    extern void ConfigureLinuxGeneric(VPNum vp);
    ConfigureLinuxEnv(vp, (PageAllocatorRef)GOBJK(ThePinnedPageAllocatorRef));
    ConfigureLinuxHWBase(vp);

    HWInterrupt::ClassInit(vp);

    TypeMgrServer::ClassInit(vp);

    FSSwap::ClassInit(vp);

    if (vp==0) MetaStreamServer::init();

    StreamServerConsole::Init1(vp);	// initializes console for internal

    /* Get kernel parameter data across */
    if (0==vp) {
    	err_printf("Getting Kparms (boot)\n");
	KParms::ClassInit(_BootInfo->boot_data, BOOT_DATA_MAX);
	KBootParms::ClassInit(*KParms::TheKParms);

	char bkpt[64] = { 0, };
	rc = KBootParms::_GetParameterValue("K42_EARLY_BREAKPOINT",
					    bkpt, 64);
	if (_SUCCESS(rc) && (strcmp(bkpt,"1")==0)) {
	    err_printf("Obeying K42_EARLY_BREAKPOINT ...\n");
	    breakpoint();
	}
    }



    SysEnviron::ClassInit(vp);
    SysEnviron::InitThinwire(vp);

    ((PMRoot *)DREFGOBJK(ThePMRootRef))->ClassInit2(vp);
    SystemControl::ClassInit(vp);

    // needs to be done before any user process starts but after thinip init
    TraceStart(vp);

    //FIXME - clock init is a kludge using thinwire, so we
    //        have to call it here.
    //        Once it uses the hardware clock, see if we can move
    //        it back into ExceptionLocal::init
    exceptionLocal.kernelTimer.initTOD(vp);

    // must be after initTOD, and before FRCRW::ClassInit()
    KernelInfoMgr::ClassInit(vp);

    RegionDefault::ClassInit(vp);

    RegionReplicated::ClassInit(vp);

    RegionPerProcessor::ClassInit(vp);

    RegionFSComm::ClassInit(vp);

    RegionRedZone::ClassInit(vp);

    FRComputation::ClassInit(vp);
    FRLTransTable::ClassInit(vp);
    FRVA::ClassInit(vp);
    FRPA::ClassInit(vp);
    FRPANonPageable::ClassInit(vp);
    FRPANonPageableRamOnly::ClassInit(vp);
    FRCRW::ClassInit(vp);
    FRKernelPinned::ClassInit(vp);
    NetDev::ClassInit(vp);
    KernelPagingTransportPA::ClassInit(vp);
    KernelPagingTransportVA::ClassInit(vp);

#ifdef ENABLE_SYNCSERVICE
    SyncService::ClassInit(vp);
#endif /* ENABLE_SYNCSERVICE */

    ProcessSetKern::ClassInit(vp);	// maintains info about processes

    StreamServerConsole::Init2(vp);	// initializes console for external

    ProcessServer::ClassInit(vp);
    ProcessClient::ClassInit(vp);

    // initialize any machine specific regions
    HardwareSpecificRegions::ClassInit(vp);

    if (vp == 0) SchedulerService::ClassInit(_KERNEL_PID);

    TestSwitch::ClassInit(vp);
    TestScheduler::ClassInit(vp);

    // Initialize Clustered Object interface to Performance Monitoring Hardware
    // FIXME:  Figure out the right place for this.  Note the HWPerformance
    //         interrupt handler depends on this so if it gets enabled prior to
    //         this things will go badly
    HWPerfMon::ClassInit(vp);

    MIP::ClassInit(vp);

    PMLeaf::Factory::ClassInit(vp);
    PMLeafExp::Factory::ClassInit(vp);
    if (vp == 0) PMLeafChunk::ClassInit();
    PMLeafChunk::Factory::ClassInit(vp);

    SystemMisc::ClassInit(vp, currentMemorySize, _BootInfo->clock_frequency);
    PrivilegedService::ClassInit(vp);

    MemTrans::ClassInit(vp);

    tassertSilent(hardwareInterruptsEnabled(), BREAKPOINT;);
    if (kernelInitArgs.barrierP) {
	err_printf("Init barrier %p %lx\n", kernelInitArgs.barrierP,
		   *kernelInitArgs.barrierP);
	//sync start of secondary processors
	// first indicate we are OK
	(*kernelInitArgs.barrierP)--;
	// then wait for everyone else
	while (*(kernelInitArgs.barrierP)) {
	    Scheduler::DelayMicrosecs(100000);
	}
    }

    extern void MPinit(VPNum vp);
#ifndef CONFIG_SIMICS
    MPinit(vp);
#else /* #ifndef CONFIG_SIMICS */
    tassertWrn(0,"MPinit not yet implemented under SIMICS\n");
#endif /* #ifndef CONFIG_SIMICS */

#define START_PROMPT_EARLY
#ifdef START_PROMPT_EARLY
    if (vp == 0) {
	Scheduler::ScheduleFunction(choose_test, 0);
    }
#endif /* #ifdef START_PROMPT_EARLY */

#ifdef CLEANUP_DAEMON
    err_printf("\nStarting GC Daemon in kernel\n");
    ((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->startPeriodicGC(0);
    ((COSMgrObject*)DREFGOBJ(TheCOSMgrRef))->setCleanupDelay(100);
#endif /* #ifdef CLEANUP_DAEMON */

#if 0
    if (vp==0) {
	// hack, just for testing paging
	// leave just enough pages to work with
	SysStatus rc;
	uval avail;
	uval ptr;
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(avail);
	uval leaveMem = (4+2*DREFGOBJK(TheProcessRef)->vpCount())*1024*1024;
	uval numPages = (avail > leaveMem) ? (avail - leaveMem)/PAGE_SIZE : 0;
	err_printf("\n\nStealing %ld pages to test swapping\n",
		   numPages);
	for (uval i = 0; i < numPages; i++) {
	    rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
		allocPages(ptr, PAGE_SIZE, PageAllocator::PAGEALLOC_NOBLOCK);
	    passert(_SUCCESS(rc), err_printf("Ooops\n"));
	}
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->getMemoryFree(avail);
	err_printf("Left %ld pages == %ld MB\n\n\n", avail/PAGE_SIZE,
		   avail / 1024 / 1024);
    }
#endif /* #if 0 */

    if (vp==0) {
	Scheduler::ScheduleFunction(ziggy_start, 0);
    }

    TraceStartHeartBeat(vp);

    if (vp == 0) {
	// this initializes the example factories
	TypeID exampleID, exampleAID, exampleBID;
	DREFGOBJ(TheTypeMgrRef)->registerType(TYPEID_NONE,
					      "Example", 1, exampleID);
	DREFGOBJ(TheTypeMgrRef)->registerFactory(exampleID, 1);

	DREFGOBJ(TheTypeMgrRef)->registerType(exampleID,
					      "ExampleA", 1, exampleAID);
	DREFGOBJ(TheTypeMgrRef)->registerFactory(exampleAID, 2);

	DREFGOBJ(TheTypeMgrRef)->registerType(exampleID,
					      "ExampleB", 1, exampleBID);
	DREFGOBJ(TheTypeMgrRef)->registerFactory(exampleBID, 3);
    }

    if (vp == 0) {
	DREFGOBJK(ThePinnedPageAllocatorRef)->startPaging();
    }

    err_printf("processor %ld started\n", vp);

    InitServer::ClassInit(vp);

    BuildDate::ClassInit(vp);

    if (vp == 0) {
	new KickOffInit();
    }

#ifdef FAST_REGRESS_ON_SIM
    err_printf("------------------ RUNNING REGRESSION TEST ------------\n");
    runRegression();
    SimOSSupport(SimExitCode, 0);
    err_printf("-----------DONE:   RUNNING REGRESSION TEST ------------\n");
#else
#ifndef START_PROMPT_EARLY
    if (vp == 0) {
	choose_test(0);
    }
#endif /* #ifndef START_PROMPT_EARLY */
#endif
    //=============================================================
    //=========================  THE END  =========================
    //=============================================================
}

void KernelInit(KernelInitArgs& kernelInitArgs)
{
    MemoryMgrPrimitiveKern *memory = &kernelInitArgs.memory;
    VPNum vp = kernelInitArgs.vp;

    // this is intrinsically local
    RESET_PPC_OVERFLOW();

    // FIXME: Spin longer for all BLocks.
    extern uval FairBLockSpinCount, BitBLockSpinCount;
    FairBLockSpinCount = 1000000;
    BitBLockSpinCount = 1000000;

    memoryMapCheckKern(vp);

    uval dispatcherMem;
    memory->alloc(dispatcherMem, sizeof(DispatcherDefaultKern), PAGE_SIZE);
    extRegsLocal.dispatcher = (Dispatcher *) dispatcherMem;
    extRegsLocal.dispatcher->init(SysTypes::DSPID(0, vp));
    extRegsLocal.dispatcher->storeProgInfo(0, "Kernel");
    /*
     * Dispatcher::init() leaves the VP enabled, which is correct for a newly-
     * created user-process VP.  The VP will be disabled before it is entered
     * at its RUN entry point.  Here in the kernel we're already running, and
     * we have to be disabled to avoid assertions in Scheduler::Init().
     */
    extRegsLocal.disabled = 1;

    Scheduler::Init();

    ActiveThrdCnt::ClassInit(vp);
    BaseObj::ClassInit(vp);

    COSMgrObjectKernObject::ClassInit(vp, memory);

    /*
     * install machines specific code for handling exceptions such
     * as page fault, ppc and other system calls, etc., I/O etc.
     */
    initExceptionHandlers(vp);

    /* this must wait after initExceptionHandlers() because the dummy
     * Timer interrupt handler must be installed before enabling
     * timer interrupts.
     */
    disableHardwareInterrupts();
    exceptionLocal.init(vp, memory);
    enableHardwareInterrupts();

#if 0 /* turn this on for an early breakpoint and led display */
    breakpoint();
    rtas_display_character('\r');
    rtas_display_character('K');
    rtas_display_character('4');
    rtas_display_character('2');
    rtas_display_character(' ');
    err_printf("RTAS displayed K42\n");
#endif /* #if 0 */

    /*
     * Initialize the scheduler and continue on a thread in KernelInitPhase2().
     */
    enum {
	THREAD_COUNT = 500,
	THREAD_SIZE = ExceptionLocal::KernThreadSize,
	THREAD_STACK_RESERVATION = ExceptionLocal::KernPgfltStkSpace
    };
    Scheduler::ClassInit(SysTypes::DSPID(0, vp), NULL, memory,
			 THREAD_COUNT, THREAD_SIZE, THREAD_STACK_RESERVATION,
			 KernelInitPhase2, uval(&kernelInitArgs));
    /* NOTREACHED */
}

void
StartBlockDev(uval ignore)
{
    extern void ConfigureLinuxHWBlock(VPNum vp);
    extern void ConfigureLinuxDelayedFinal(VPNum vp);
    ConfigureLinuxHWBlock(0);
    ConfigureLinuxDelayedFinal(0);
}

SysStatus
DoLinuxInit(uval ignore)
{
    VPNum vp = Scheduler::GetVP();

    enum {
	NONE = 0,
	WIRE = 1,
	LINUX = 2
    };

    uval transport = WIRE;

    extern void ConfigureLinuxNet(VPNum vp);
    extern void ConfigureLinuxHWDev(VPNum vp);
    extern void ConfigureLinuxHWBlock(VPNum vp);
    extern void ConfigureLinuxGeneric(VPNum vp);

    // Early initialization and registration of init calls

    char buf[128];
    StubKBootParms::_GetParameterValue("K42_IOSOCKET_TRANSPORT", buf, 128);
    if (strcmp(buf, "wire") == 0 || strcmp(buf, "WIRE") == 0) {
	err_printf("IOSocket::transport = IOSocket::WIRE\n");
	transport = WIRE;
    } else if (strcmp(buf, "linux") == 0 || strcmp(buf, "LINUX") == 0) {
	err_printf("IOSocket::transport = IOSocket::LINUX\n");
	transport = LINUX;
    } else {
	err_printf("Invalid K42_IOSOCKET_TRANSPORT value: %s\n", buf);
	err_printf("Valid values are: wire, linux\n");
	transport = NONE;
    }

    ConfigureLinuxGeneric(vp);
    ConfigureLinuxHWDev(vp);

#if defined(KFS_ENABLED) || defined(EXT2_ENABLED)
    buf[0] = 0;
    StubKBootParms::_GetParameterValue("K42_FS_DISK", buf, 128);
    // Non-empty --> start disks.
    if (buf[0]) {
	ConfigureLinuxHWBlock(vp);
    }
#endif
    LinuxCharDev::ClassInit(vp);
    LinuxBlockDev::ClassInit(vp);

    if (!KernelInfo::OnSim() || transport==LINUX) {
	ConfigureLinuxNet(vp);
    }
    return 0;
}

void
doLinuxInitCalls()
{
    extern void LinuxStartVP(VPNum vp, SysStatus (*initfn)(uval arg));
    VPNum myvp = Scheduler::GetVP();
    if (myvp!=0) {
	// We must run this from cpu 0
	SysStatus rc;
	MPMsgMgr::SendSyncUval(Scheduler::GetEnabledMsgMgr(),
			       SysTypes::DSPID(0, 0),
			       DoLinuxInit, 0, rc);
	return;
    }

    DoLinuxInit(0);
    VPNum vpCnt = _SGETUVAL(DREFGOBJK(TheProcessRef)->vpCount());
    for (VPNum i = 1; i < vpCnt; i++) {
	LinuxStartVP(i, DoLinuxInit);
    }
    extern void ConfigureLinuxFinal(VPNum vp);
    ConfigureLinuxFinal(0);
}


INIT_OBJECT_PTR(IPSockInit, "TCP/IP socket services",
		INIT_IPSOCK, IPSock::ClassInit);

INIT_OBJECT_PTR(ToyBlockDevInit, "simulated disks",
		INIT_TOYBLOCKDEV, ToyBlockDev::ClassInit);

INIT_OBJECT_PTR(BaseServersInit, "default servers",
		INIT_BASESERVERS_START, StartDefaultServers);

INIT_OBJECT_PTR(LinuxInitCalls, "Linux initcalls",
		INIT_LINUXINIT, doLinuxInitCalls);



void
InitKickOff()
{
    new IPSockInit();
    new ToyBlockDevInit();
    new BaseServersInit();
    new LinuxInitCalls();
}
