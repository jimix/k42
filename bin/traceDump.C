/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceDump.C,v 1.6 2005/06/28 19:42:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: dameon code that awakes occasionally and writes
 *                     out tracing buffers
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <io/FileLinux.H>
#include <trace/trace.H>
#include <scheduler/Scheduler.H>
#include <sync/MPMsgMgr.H>
#include <usr/ProgExec.H>
#include <misc/baseStdio.H>
#include <sync/Barrier.H>
#define cprintf printf
#include <stdio.h>
#include <trace/traceUtils.H>
#include <sys/systemAccess.H>


void
tracePrintEvents(uval startIndex, uval stopIndex)
{
    uval i;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    uval64 *const trcArray = trcInfo->traceArray;

    for (i = startIndex; i != stopIndex;
	 i = (i + TRACE_LENGTH_GET(trcArray[i])) & trcInfo->indexMask) {
	tracePrintEvent(&trcArray[i], Scheduler::TicksPerSecond());
    }
}

void
tracePrintLastEvents(uval numb)
{
    uval currentIndex, currentBufferStart, startIndex;
    uval bufferStart, bufferEnd;
    uval i, found;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    TraceControl *const trcCtrl = trcInfo->traceControl;
    uval64 *const trcArray = trcInfo->traceArray;

    currentIndex = (trcCtrl->index & trcInfo->indexMask);
    currentBufferStart = TRACE_BUFFER_OFFSET_CLEAR(currentIndex);

    found = 0;
    startIndex = currentIndex;
    bufferStart = currentBufferStart;
    bufferEnd = currentIndex;

    for (;;) {
	if (trcCtrl->bufferCount[TRACE_BUFFER_NUMBER_GET(bufferStart)] !=
						   (bufferEnd - bufferStart)) {
	    // buffer isn't yet completed
	    break;
	}

	// buffer is complete, so we can include it in the output
	startIndex = bufferStart;

	// count the events in this buffer
	for (i = bufferStart; i < bufferEnd;
		    i += TRACE_LENGTH_GET(trcArray[i])) {
	    found++;
	}

	if (found >= numb) {
	    // we have enough events now
	    break;
	}

	// move to previous buffer
	if (bufferStart == 0) {
	    bufferStart = (trcInfo->numberOfBuffers - 1) * TRACE_BUFFER_SIZE;
	} else {
	    bufferStart -= TRACE_BUFFER_SIZE;
	}
	bufferEnd = bufferStart + TRACE_BUFFER_SIZE;

	if (bufferStart == currentBufferStart) {
	    // we're back to the beginning
	    break;
	}
    }

    // skip events that weren't requested
    while (found > numb) {
	startIndex = (startIndex +
	      TRACE_LENGTH_GET(trcArray[startIndex])) & trcInfo->indexMask;
	found--;
    }

    // print the rest
    tracePrintEvents(startIndex, currentIndex);
}


void
childMain(VPNum vp)
{
    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;

    cprintf("--- VP %ld ---\n", vp);
    uval numb = trcInfo->numberOfBuffers * TRACE_BUFFER_SIZE;
    tracePrintLastEvents(numb);
}

struct LaunchChildMsg : public MPMsgMgr::MsgSync {
    VPNum vp;

    virtual void handle() {
	childMain(vp);
	reply();
    }
};

int
main()
{
    NativeProcess();

    VPNum numbVPs, vp;
    SysStatus rc;

    numbVPs = DREFGOBJ(TheProcessRef)->ppCount();

    // create vps
    for (vp = 1; vp < numbVPs; vp++) {
	rc = ProgExec::CreateVP(vp);
	passert(_SUCCESS(rc), err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
    }

    // I'm vp 0
    childMain(0);

    // start vps
    for (vp = 1; vp < numbVPs; vp++) {
	MPMsgMgr::MsgSpace msgSpace;
	LaunchChildMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr(), msgSpace) LaunchChildMsg;
	msg->vp = vp;
	rc = msg->send(SysTypes::DSPID(0, vp));
	tassert(_SUCCESS(rc), err_printf("traced: send failed\n"));
    }

    err_printf("done...\n");

    return 0;
}
