/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: trace.C,v 1.91 2004/09/30 11:21:55 cyeoh Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: implementation of tracing facility
 * **************************************************************************/

#include "kernIncs.H"
#include <trace/tracePack.H>
#include "trace/traceBase.H"
#include "mem/RegionDefault.H"
#include "mem/PageAllocatorKernPinned.H"
#include "traceTimer.H"
#include "trace/traceCtr.H"
#include "init/kernel.H"
#include <stub/StubKBootParms.H>
 
void
traceTest()
{
  
    uval8 a,b,c,d,e,f,g,h;
    uval16 i,j,k,l;
    uval32 m,n;
    uval64 o,p,q;
    char test[32];
    char test1[32];
    uval64 testb[4];
    
    // code for timing trace events
    if (1) {
	o=0x1234;
	p=0x5678;
	q=0x9012;
	TraceOSTestTest0();
	for (i=0;i<10000;i++) {
	    m=i+p+m;
	}
	TraceOSTestTest0();

	for (i=0;i<10000;i++) {
	    asm volatile("#foo");
	}
	TraceOSTestTest0();

	TraceOSTestTest1(1);
	for (i=0;i<10000;i++) {
	    TraceOSTestTest0();
	}
	TraceOSTestTest1(1);
	for (i=0;i<10000;i++) {
	  TraceOSTestTest1(o);
	}
	TraceOSTestTest1(1);
	for (i=0;i<10000;i++) {
	  TraceOSTestTest2(o,p);
	}
	TraceOSTestTest1(1);
	for (i=0;i<10000;i++) {
	  TraceOSTestTest3(o,p,q);
	}
	TraceOSTestTest0();
	return;
    }

    testb[0] = 0;testb[1] = 1;testb[2] = 2;testb[3] = 3;
    cprintf("running trace test\n");

    strcpy(test, "test");
    strcpy(test1, "hello world");

    TraceOSTestStr(test1);
    TraceOSTestBytes(testb[0], testb[1], testb[2], testb[3]);

//    TraceOSTestPack3232Str(
//		 TRACE_PACK_32_32(0x1234ULL, 0x5678ULL), test1);

    TraceOSTestGen(test1);		 
    TraceOSTestStrData(0x1234ULL, test, test1);

    for (i=0;i<1;i++) {
      TraceOSTestTest1(1);
      TraceOSTestTest1(2);
      TraceOSTestTest2(0x1234567890abcdefULL,
		       0x1122334455667788ULL);
    }

    a=1;b=2;c=3;d=4;e=5;f=6;g=7;h=8;i=9;j=10;k=11;l=12;m=13;n=14;
    TraceOSTestPack3232(m,n);
    TraceOSTestPack(TRACE_PACK_32_16_16(m,i,j));
    TraceOSTestPack(TRACE_PACK_32_16_8_8(m,i,a,b));
    TraceOSTestPack(TRACE_PACK_32_8_8_8_8(m,a,b,c,d));
    TraceOSTestPack(TRACE_PACK_16_16_16_16(i,j,k,l));
    TraceOSTestPack(TRACE_PACK_16_16_16_8_8(i,j,k,a,b));
    TraceOSTestPack(TRACE_PACK_16_16_8_8_8_8(i,j,a,b,c,d));
    TraceOSTestPack(TRACE_PACK_16_8_8_8_8_8_8(i,a,b,c,d,e,f));
    TraceOSTestPack(TRACE_PACK_8_8_8_8_8_8_8_8(a,b,c,d,e,f,g,h));

    cprintf("trace test completed successfully\n");
}

void
sanityCheckTraceClasses()
{
    uval i,j;

    for (i=0; i<TRACE_LAST_MAJOR_ID_CHECK; i++) {
	for (j=0; j<traceUnified[i].enumMax; j++) {
	    passert((j==traceUnified[i].traceEventParse[j].eventMinorID),
		    err_printf("missing minorID %ld in trace%s declaration\n",
			       j, traceUnified[i].name));
	}
    }
}

void
TraceReset(VPNum vp)
{
    uval64 mask;
    uval i;
    TraceInfo *const trcInfo = &(exceptionLocal.kernelInfoPtr->traceInfo);
    TraceControl *const trcCtrl = trcInfo->traceControl;

    mask = trcInfo->mask;
    trcInfo->mask = 0;

    exceptionLocal.copyTraceInfo();

    if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	err_printf("reset trace structures on vp %ld\n", vp);
    }

    // heuristics - allow outstanding events to be entered
    //Scheduler::DelayMicrosecs(1000000); // 1 second

    trcCtrl->index = 0;
    trcCtrl->buffersProduced = 0;
    trcCtrl->buffersConsumed = 0;
    trcCtrl->writeOkay = 1;

    // Set the counters to indicate we're using the first buffer and that
    // the rest are full.
    trcCtrl->bufferCount[0] = 0;
    for (i = 1; i < TRACE_MAX_BUFFER_NUMBER; i++) {
	trcCtrl->bufferCount[i] = TRACE_BUFFER_SIZE;
    }

    trcInfo->mask = mask;
    exceptionLocal.copyTraceInfo();
}

void
TraceStart(VPNum vp)
{
    SysStatus rc;
    uval i, numBufs, arraySize, size, vpbase;
    TraceInfo *trcInfo;
    char varvalue[256];
    uval64 tmpMask;

    static uval LogNumBufs = uval(-1);
    static uval TraceSpace = 0;

    char *const varname = "K42_TRACE_LOG_NUMB_BUFS";

    if (vp == 0) {
	passertMsg(LogNumBufs == uval(-1), "TraceStart(0) called twice.\n");

	passertMsg((TRACE_LAST_MAJOR_ID_CHECK <= (TRACE_MAJOR_ID_MASK+1)),
	           "too many tracing classes - see enum in trace.H\n");

	passertMsg((TRACE_BUFFER_OFFSET_BITS >= TRACE_LENGTH_BITS),
		   "trace buffer size too small - see trace.H\n");

	sanityCheckTraceClasses();

	rc = StubKBootParms::_GetParameterValue(varname, varvalue, 256);
	if (_FAILURE(rc) || (varvalue[0] == '\0')) {
	    err_printf("%s undefined.  Using 2.\n", varname);
	    LogNumBufs = 2;
	} else {
	    LogNumBufs = baseAtoi(varvalue);
	    if (LogNumBufs < 2) {
		err_printf("%s (%ld) too small.  Using 2.\n",
			   varname, LogNumBufs);
		LogNumBufs = 2;
	    } else if (LogNumBufs > TRACE_BUFFER_NUMBER_BITS) {
		err_printf("%s (%ld) too large.  Using %d.\n",
			   varname, LogNumBufs, TRACE_BUFFER_NUMBER_BITS);
		LogNumBufs = TRACE_BUFFER_NUMBER_BITS;
	    }
	}
    }

    /*
     * For each processor we need one page for the TraceControl structure,
     * one page for counters, and space for the array.  The array size is
     * increased by one page because traced uses a page at the end for
     * writing out the trace header.
     */
    numBufs = uval(1) << LogNumBufs;
    arraySize = (numBufs * TRACE_BUFFER_SIZE * sizeof(uval64)) + PAGE_SIZE;
    size = KernelInfo::CurPhysProcs() * ((1 + 1)*PAGE_SIZE + arraySize);

    if (vp == 0) {
	err_printf("Using %ld trace buffers (%ld kilobytes) per processor.\n",
		   numBufs, arraySize / 1024);

	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->allocPages(TraceSpace,size);
	passertMsg(_SUCCESS(rc), "error allocating trace pages\n");
    }

    MapTraceRegion(TRACE_REGION_BASE,
		   PageAllocatorKernPinned::virtToReal(TraceSpace),
		   size);

    trcInfo = &exceptionLocal.kernelInfoPtr->traceInfo;

    vpbase = TRACE_REGION_BASE + (vp * ((1 + 1)*PAGE_SIZE + arraySize));
    trcInfo->traceControl = (TraceControl *) vpbase;
    trcInfo->traceCounters = (uval64 *) (vpbase + PAGE_SIZE);
    trcInfo->traceArray = (uval64 *) (vpbase + 2*PAGE_SIZE);
    trcInfo->traceArrayPhys = (uval64 *)
	    PageAllocatorKernPinned::virtToReal(uval(trcInfo->traceArray));

    trcInfo->mask = 0;
    trcInfo->indexMask = (uval(1) << (LogNumBufs+TRACE_BUFFER_OFFSET_BITS)) - 1;
    trcInfo->numberOfBuffers = numBufs;
    trcInfo->tracedRunning = 0;
    trcInfo->overflowBehavior = TRACE_OVERFLOW_WRAP;

    trcInfo->traceControl->index = 0;
    trcInfo->traceControl->buffersProduced = 0;
    trcInfo->traceControl->buffersConsumed = 0;
    trcInfo->traceControl->writeOkay = 1;

    // Set the counters to indicate we're using the first buffer and that
    // the rest are full.
    trcInfo->traceControl->bufferCount[0] = 0;
    for (i = 1; i < TRACE_MAX_BUFFER_NUMBER; i++) {
	trcInfo->traceControl->bufferCount[i] = TRACE_BUFFER_SIZE;
    }

    err_printf("Trace region addresses (vp %ld):\n", vp);
    err_printf("    traceControl  %p\n", trcInfo->traceControl);
    err_printf("    traceCounters %p\n", trcInfo->traceCounters);
    err_printf("    traceArray    %p\n", trcInfo->traceArray);

    trcInfo->mask = TRACE_ALL_MASK;
    //trcInfo->mask &= (~(TRACE_MEM_MASK));
    //trcInfo->mask &= (~(TRACE_EXCEPTION_MASK));
    // control should only be masked off in rare situations
    trcInfo->mask |= TRACE_CONTROL_MASK;

    // No one depends on tracing from boot time anymore.
    trcInfo->mask = 0;

    exceptionLocal.copyTraceInfo();

    TraceAutoCount::initCounters();

    tmpMask = trcInfo->mask; 
    trcInfo->mask = TRACE_TEST_MASK;
    // traceTest();
    trcInfo->mask = tmpMask;
}

void
TraceStartHeartBeat(VPNum vp)
{
    (void) new TraceTimerEvent();
}
