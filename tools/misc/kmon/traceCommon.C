/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceCommon.C,v 1.12 2005/06/28 19:48:33 rosnbrg Exp $
 *****************************************************************************/

/* common code shared by the trace tools */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>

#ifdef K42
# include <sys/sysIncs.H>
# include <sys/KernelInfo.H>
# include <io/FileLinux.H>
# include <scheduler/Scheduler.H>
# include <sys/SystemMiscWrapper.H>
# include <sync/MPMsgMgr.H>
# include <usr/ProgExec.H>
# include <sys/systemAccess.H>
# define dbg_printf err_printf
#else
# include <sys/hostSysTypes.H>
# define dbg_printf printf
#endif

#include <trace/trace.H>
#include <trace/traceUtils.H>
#include <trace/traceControl.h>
#include "traceCommon.H"

int overrideCount, overrideWords, overrideGarbled;

/* open and mmap the trace file "filename"
 * "byteSwapRef" must point to the byteSwap flag in the caller
 * "n_words" is set to the number of 64-bit words in the file, if non-NULL
 * "headerInfo" is set to be a pointer to the header at the start of the file
 * returns the file descriptor, or -1 on error
 */
static int
openTraceFile(const char *filename, bool *byteSwapRef, uval *n_words,
    	      traceFileHeaderV3 **headerInfo, bool verbose)
{
    struct stat statBuffer;
    bool byteSwap = *byteSwapRef;
    int rc, fd;

    fd = open(filename, O_RDONLY);
    if (fd == -1) {
	printf("Error: open of %s failed: %s\n", filename, strerror(errno));
	return -1;
    }

    rc = fstat(fd, &statBuffer);
    if (rc == -1) {
	printf("Error: stat of %s failed: %s\n", filename, strerror(errno));
	close(fd);
	return -1;
    }

    if (n_words != NULL) {
	*n_words = statBuffer.st_size / sizeof(uval64);
    }

    *headerInfo = (traceFileHeaderV3 *)mmap(0, statBuffer.st_size, PROT_READ,
					    MAP_PRIVATE, fd, 0);
    if (*headerInfo == (traceFileHeaderV3 *)-1) {
	printf("Error: mmap of %s failed: %s\n", filename, strerror(errno));
	close(fd);
	return -1;
    }

    if (TRACE_SWAP32((*headerInfo)->version) != 3 && !byteSwap) {
	byteSwap = true;
	if (TRACE_SWAP32((*headerInfo)->version) != 3) {
	    printf("Error: %s has wrong file header version\n", filename);
	    close(fd);
	    return -1;
	}
    }

    if (verbose) {
	printf("%s: K42 trace file v%u, align size %lld%s\n",
	       filename, TRACE_SWAP32((*headerInfo)->version),
	       TRACE_SWAP64((*headerInfo)->alignmentSize),
	       byteSwap ? ", byte swaps enabled" : "");
    }
    if (verbose) {
	printf("%s: ticks per second %lld\n", filename,
	       TRACE_SWAP64((*headerInfo)->ticksPerSecond));
    }
    *byteSwapRef = byteSwap;

    return fd;
}

/* call "func" on every trace event in the given buffer
 * "headerInfo" points to the header info for this buffer/file
 * "n_words" contains the number of 64-bit words in the buffer
 * "first" and "last" flags indicate if it's first/last buffer for the processor
 */
static int
processTraceBuffer(const traceFileHeaderV3 *headerInfo, const uval64 *buf,
		   uval n_words, traceEvtFn_t func, bool byteSwap,
		   bool first, bool last, bool verbose)
{
    uval length, index;
    int rc;
    int localGarbled;

    localGarbled = 0;
    for (index = 0; index < n_words; index += length) {
	length = TRACE_LENGTH_GET(TRACE_SWAP64(buf[index]));
	if (length + index > n_words) {
	    if (length < 30) {
		printf("warning: might have missed last event missing %lld words\n",
		       uval64((length + index) - n_words));
		return 0;
	    }
	    if (verbose) {
		dbg_printf("Error: end of trace stream garbled\n");
		dbg_printf("       wanted to read %d words\n", (int)length);
	    }
	    return -1;
	} else if (length == 0) {
	    if (verbose) {
		dbg_printf("Error: zero length event at word %ld: 0x%llx\n",
			   index, TRACE_SWAP64(buf[index]));
    	    }
	    length = 1;
	} else {
	    rc = func(headerInfo, &buf[index], length, byteSwap,
		      first && index == 0, last && (index + length) >= n_words,
		      verbose);
	    if (rc != 0) {
		if (overrideGarbled) {
		    length = 1;
		    if (localGarbled == 0) {
			overrideCount++;
		    }
		    overrideWords++;
		    localGarbled = 1;
		    printf("found garbled count %d words %d\n", 
			   overrideCount, overrideWords);
		} else {
		    return rc;
		}
	    } else {
		localGarbled = 0;
	    }
	}
    }

    return 0;
}

/* open the trace file named by "filename", calling "func" for each event */
int
processTraceFile(const char *filename, traceEvtFn_t func, bool *byteSwapRef,
		 bool verbose)
{
    traceFileHeaderV3 *headerInfo;
    uval64 *traceArray;
    uval n_words, index;
    bool byteSwap;
    int fd, rc;

    fd = openTraceFile(filename, byteSwapRef, &n_words, &headerInfo, verbose);
    if (fd == -1) {
	return -1;
    }

    byteSwap = *byteSwapRef;

    traceArray = (uval64 *)headerInfo;
    index = TRACE_SWAP32(headerInfo->headerLength) / sizeof(uval64);

    if (overrideGarbled) {
	overrideCount = 0;
	overrideWords = 0;
    }
    rc = processTraceBuffer(headerInfo, &traceArray[index], n_words - index,
			    func, byteSwap, true, true, verbose);

    close(fd);

    return rc;
}


#ifdef K42
/* child which executes on every processor to process the local trace buffers */
static int
processLocalBuffers(traceEvtFn_t func, bool verbose)
{
    SysStatusUval rc;
    uval myBuffer, currentIndex, currentBuffer;
    VPNum physProc = KernelInfo::PhysProc();
    traceFileHeaderV3 headerInfo;
    bool isFirst = true;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    TraceControl *const trcCtrl = trcInfo->traceControl;
    uval64 *const trcArray = trcInfo->traceArray;

    headerInfo.endianEncoding = TRACE_BIG_ENDIAN;
    headerInfo.extra[0] = 0;
    headerInfo.extra[1] = 0;
    headerInfo.extra[2] = 0;
    headerInfo.version = 3;
    headerInfo.headerLength = sizeof(traceFileHeaderV3);
    headerInfo.flags = 0;
    headerInfo.alignmentSize = TRACE_BUFFER_SIZE * sizeof(uval64);
    headerInfo.ticksPerSecond = Scheduler::TicksPerSecond();
    headerInfo.physProc = physProc;
    headerInfo.initTimestamp = Scheduler::SysTimeNow();

    while (!(trcCtrl->writeOkay)) {
    	if (verbose) {
	    dbg_printf("trace logger on PP %lu: blocking on write not okay\n",
		       physProc);
	}
	Scheduler::DelayMicrosecs(1000000); // 1 second
    }

    if (trcCtrl->buffersProduced - trcCtrl->buffersConsumed
	>= trcInfo->numberOfBuffers) {
	dbg_printf("WARNING: Trace buffers were full before the trace logger\n"
		   "started; events have been lost (physProc %ld)\n", physProc);

	myBuffer = (trcCtrl->buffersProduced - (trcInfo->numberOfBuffers - 1))
		   % trcInfo->numberOfBuffers;
    } else {
    	myBuffer = 0;
    }

    currentIndex = trcCtrl->index & trcInfo->indexMask;
    currentBuffer = TRACE_BUFFER_NUMBER_GET(currentIndex);

    if (verbose) {
	dbg_printf("trace logger on PP %lu: produced %ld, consumed %ld, "
		   "mybuf %ld, size %ld\n", physProc, trcCtrl->buffersProduced,
		   trcCtrl->buffersConsumed, myBuffer,
		   trcCtrl->bufferCount[myBuffer]);
    }

    // process all buffers that have been produced and are complete.
    while ((myBuffer != currentBuffer) &&
	   (trcCtrl->bufferCount[myBuffer] == TRACE_BUFFER_SIZE)) {
	if (verbose) {
	    dbg_printf("trace logger on PP %lu: processing %ld\n",
		       physProc, myBuffer);
	}
	rc = processTraceBuffer(&headerInfo,
				&trcArray[myBuffer * TRACE_BUFFER_SIZE],
				TRACE_BUFFER_SIZE, func, false, isFirst, false,
				verbose);
	if (rc != 0) {
	    return rc;
	}

	isFirst = false;
	myBuffer = (myBuffer + 1) % trcInfo->numberOfBuffers;
    }

    if (myBuffer != currentBuffer) {
	// that must mean that buffer count of myBuffer != BUFFER_SIZE
	dbg_printf("WARNING: unwritten event on PP %lu, trace stream invalid\n"
		   "         my buffer %ld count %ld SIZE %lld\n", physProc,
		   myBuffer, trcCtrl->bufferCount[myBuffer], TRACE_BUFFER_SIZE);
	return -1;
    }

    /* process what's left in the current buffer */
    rc = processTraceBuffer(&headerInfo,
			    &trcArray[myBuffer * TRACE_BUFFER_SIZE],
			    currentIndex - currentBuffer * TRACE_BUFFER_SIZE,
			    func, false, isFirst, true, verbose);
    if (rc != 0) {
	return rc;
    }

    if (verbose) {
	dbg_printf("trace logger on PP %lu: finished successfully\n", physProc);
    }

    return 0;
}

/* message used to launch the child process */
struct LaunchChildMsg : public MPMsgMgr::MsgSync {
    traceEvtFn_t func;
    bool verbose;
    int retval;

    virtual void handle() {
	retval = processLocalBuffers(func, verbose);
	reply();
    }
};

/* calls the function "func" for every trace event buffered on every processor
 * returns zero on success, nonzero on failure */
int
processTraceBuffers(traceEvtFn_t func, bool verbose)
{
    VPNum numbVPs, vp;
    MPMsgMgr::MsgSpace space;
    SysStatus rc;

    if (kernelInfoLocal.traceInfo.tracedRunning) {
    	// traced is running: surely this will screw us up?
	printf("Error: traced is running\n");
	return -1;
    }

    numbVPs = DREFGOBJ(TheProcessRef)->ppCount();

    // create VPs
    for (vp = 1; vp < numbVPs; vp++) {
	rc = ProgExec::CreateVP(vp);
	if (!_SUCCESS(rc)) {
	    printf("Error: ProgExec::CreateVP failed (0x%lx)\n", rc);
	    return -1;
	}
    }

    // run locally
    rc = processLocalBuffers(func, verbose);
    if (rc != 0) {
	return rc;
    }

    // now, run sequentially on other VPs
    for (vp = 1; vp < numbVPs; vp++) {
	LaunchChildMsg *const msg =
	    new(Scheduler::GetEnabledMsgMgr(), space) LaunchChildMsg;
	msg->func = func;
	msg->verbose = verbose;

	rc = msg->send(SysTypes::DSPID(0, vp));
	if (!_SUCCESS(rc)) {
	    printf("Error: msg send for VP %lu failed (0x%lx)\n", vp, rc);
	    return -1;
	}

    	if (msg->retval != 0) {
	    return msg->retval;
	}
    }

    return 0;
}

#else

int
processTraceBuffers(traceEvtFn_t func, bool verbose)
{
    printf("Error: processTraceBuffers called on non-K42 version\n");
    return -1;
}

#endif
