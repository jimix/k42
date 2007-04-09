/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU GPL. You should have
 * received a copy of the license along with this tool; see the file LICENSE
 * in the same directory for more details.
 *
 * $Id: ktrace2ltt.C,v 1.5 2004/08/20 17:30:51 mostrows Exp $
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include "sys/errno.h"
#include "sys/mman.h"
#include "sys/stat.h"
#include "sys/dir.h"
#include <fcntl.h>
#include <assert.h>


# define __STDC_LIMIT_MACROS /* need this to get UINT32_MAX on Linux, ugh */
# define _ALL_SOURCE

#include <inttypes.h>

#include "sys/hostSysTypes.H"
#include "trace/traceUtils.H"
#include "trace/trace.H"
#include "trace/traceIncs.H"
#include "trace/traceUnified.H"
#include "trace/traceControl.h"
#include "traceCommon.H"

#define LTT_64BIT 1
extern "C" {
#include "ltt-linuxevents.h"
}

#include "linux-net.h"
#include "linux-ppc64-unistd.h"

#define ALIGN_UP(x, s)	    (((x) + (s) - 1) & ~((s) - 1))
#define LTT_ALIGN_UP(x)     ALIGN_UP((x), sizeof(uint64_t))

#define LTT_BUFLEN	    4096 /* size of a simulated LTT buffer, arbitrary */
#define LTT_END_EVENT_LEN   32 /* length of the BUFFER_END event, with params,
				  and the special space lost word */

static FILE *fpOut;
static uint64_t outPos = 0;

static struct {
    uval64 CPUID;
    uval64 commID, PID, userPID;
    uval32 lastTickstamp;
    uval64 wrapAdjust;
    uval64 currentTimestamp;
} runningStats;


static void print_usage(void);
static uint8_t convertSyscallName(const char *);

static inline uval64
commID2PID(uval64 commID)
{
    const uval COMMID_PID_SHIFT = 32;
    const uval PID_BITS = 32;

    return (commID >> COMMID_PID_SHIFT) & ((uval64(1) << PID_BITS) - 1);
}

static void
updateCurrent(uval64 commID)
{
    runningStats.commID = commID;
    runningStats.PID = commID2PID(commID);
    if (runningStats.PID != 0) {
    	runningStats.userPID = runningStats.PID;
    }
}

static int ltt_writeHeader(void);
static int ltt_writeFooter(void);

/* write out an LTT event to the file */
static int
ltt_writeEvent(uint32_t time, uint8_t event, uint8_t subid, void *data,
	       size_t len, const void *varData, size_t varLen)
{
    uint64_t spaceLeft;
    size_t data_bytes, total_bytes;
    trace_event_header header;
    int rc;

    /* check if we first need to start a new "buffer" within the file */
    spaceLeft = LTT_BUFLEN - (outPos % LTT_BUFLEN);

    data_bytes = sizeof(header.raw) + (data ? len : 0) + (varData ? varLen : 0);
    total_bytes = LTT_ALIGN_UP(data_bytes);

    if (event != TRACE_BUFFER_END && spaceLeft
				     < (LTT_END_EVENT_LEN + total_bytes)) {
	assert(event != TRACE_BUFFER_START);
	assert(event != TRACE_START);

	/* write the end of buffer event */
	rc = ltt_writeFooter();
	if (rc != 0) {
	    return rc;
	}

	/* start the new buffer */
	rc = ltt_writeHeader();
	if (rc != 0) {
	    return rc;
	}
    }

    header.x.timestamp = time;
    header.x.event_id = event;
    header.x.event_sub_id = subid;
    assert(total_bytes <= UINT16_MAX);
    header.x.size = (uint16_t)total_bytes;

    rc = fwrite(&header.raw, sizeof(header.raw), 1, fpOut);
    if (rc != 1) {
	goto error;
    }

    if (data != NULL && len > 0) {
	rc = fwrite(data, len, 1, fpOut);
	if (rc != 1) {
	    goto error;
	}
    }

    if (varData != NULL && varLen > 0) {
	rc = fwrite(varData, varLen, 1, fpOut);
	if (rc != 1) {
	    goto error;
	}
    }

    if (total_bytes > data_bytes) {
	rc = fseek(fpOut, total_bytes - data_bytes, SEEK_CUR);
	if (rc != 0) {
	    perror("Error: fseek");
	    return -1;
	}
    }

    outPos += total_bytes;
    assert(outPos == (uval64)ftello(fpOut));

#if 0
    printf("%2hhu.%02hhu @ %10u: <%2u> <%2u> = %2u = %2u\n",
	   event, subid, time, len, varLen, data_bytes, total_bytes);
#endif

    return 0;

error:
    perror("Error: fwrite");
    return -1;
}

static int
ltt_writeHeader(void)
{
    trace_buffer_start buffer_start;
    static uint32_t bufID = 0;

    buffer_start.Time.tv_sec = runningStats.currentTimestamp / 1000000;
    buffer_start.Time.tv_usec = runningStats.currentTimestamp % 1000000;
    buffer_start.TSC = 0; /* not used */
    buffer_start.ID = bufID++;

    return ltt_writeEvent((uint32_t)runningStats.currentTimestamp,
			  TRACE_BUFFER_START, 0, &buffer_start,
			  sizeof(buffer_start), NULL, 0);
}

static int
ltt_writeStart(uint64_t cpu)
{
    trace_start start;

    start.MagicNumber = TRACER_MAGIC_NUMBER;
    start.ArchType = TRACE_ARCH_TYPE_PPC64;
    start.ArchVariant = TRACE_ARCH_VARIANT_PPC_PSERIES;
    start.SystemType = TRACE_SYS_TYPE_VANILLA_LINUX;
    start.MajorVersion = TRACER_SUP_VERSION_MAJOR;
    start.MinorVersion = TRACER_SUP_VERSION_MINOR;
    start.BufferSize = LTT_BUFLEN;
    start.UseTSC = FALSE;
    assert(cpu < 256); /* LTT only has an 8-bit CPU ID */
    start.CPUID = (uint8_t)cpu;
    start.FlightRecorder = FALSE;
    start.DetailsMask = start.EventMask = ~((trace_event_mask)0);

    return ltt_writeEvent(0, TRACE_START, 0, &start, sizeof(start), NULL, 0);
}

static int
ltt_writeFooter(void)
{
    trace_buffer_end buffer_end;
    uint64_t spaceLeft;
    uint32_t spaceLost;
    int rc;

    buffer_end.Time.tv_sec = runningStats.currentTimestamp / 1000000;
    buffer_end.Time.tv_usec = runningStats.currentTimestamp % 1000000;
    buffer_end.TSC = 0; /* not used */

    spaceLeft = LTT_BUFLEN - (outPos % LTT_BUFLEN);

    rc = ltt_writeEvent((uint32_t)runningStats.currentTimestamp,
			TRACE_BUFFER_END, 0, &buffer_end, sizeof(buffer_end),
			NULL, 0);
    if (rc != 0) {
	return rc;
    }

    /* the "space lost" in the buffer includes the buffer end event */
    spaceLost = (uint32_t)spaceLeft;

    /* skip over the remaining space, except for the last 32-bit word... */
    rc = fseeko(fpOut, spaceLeft - LTT_END_EVENT_LEN + sizeof(spaceLost),
		SEEK_CUR);
    if (rc != 0) {
	perror("Error: fseeko");
	return -1;
    }

    /* ...where we write the amount of space that was lost. ugh! */
    rc = fwrite(&spaceLost, sizeof(spaceLost), 1, fpOut);
    if (rc != 1) {
	perror("Error: fwrite");
	return -1;
    }

    outPos += spaceLeft - LTT_END_EVENT_LEN + 2 * sizeof(spaceLost);
    assert(outPos % LTT_BUFLEN == 0);
    assert(outPos == (uint64_t)ftello(fpOut));

    return 0;
}

/* calculate the LTT time delta (in microseconds), given the K42
 * timestamp (in CPU ticks) and number of ticks per second */
static int
calcTimeDelta(uval32 rawTickstamp, uval64 ticksPerSec, uint32_t *ret)
{
    uval64 newTimestamp;
    int rc;

    if (rawTickstamp < runningStats.lastTickstamp) {
	// a wrap occurred
	if (runningStats.lastTickstamp == 0) {
	    // actually this was the first one
	    runningStats.wrapAdjust = (sval64)(-1 * (sval64)rawTickstamp);
	} else {
	    runningStats.wrapAdjust += ((uval64)1) << TRACE_TIMESTAMP_BITS;
	}
    }
    runningStats.lastTickstamp = rawTickstamp;
    newTimestamp
	= (rawTickstamp + runningStats.wrapAdjust) * 1000000 / ticksPerSec;

    while (newTimestamp - runningStats.currentTimestamp > UINT32_MAX) {
	/* need to write a heartbeat event */
	runningStats.currentTimestamp += UINT32_MAX;
	rc = ltt_writeEvent((uint32_t)runningStats.currentTimestamp,
			    TRACE_HEARTBEAT, 0, NULL, 0, NULL, 0);
	if (rc != 0) {
	    return rc;
	}
    }

    *ret = (uint32_t)newTimestamp;
    runningStats.currentTimestamp = newTimestamp;

    return 0;
}

static int
convertMem(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    uval minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));
    trace_memory ev;
    int rc = 0;

    switch (minorID) {
    case TRACE_MEM_FR_START_FILL_PAGE:
	ev.event_data = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_MEMORY, TRACE_MEMORY_SWAP_IN,
			    &ev, sizeof(ev), NULL, 0);
	break;

    case TRACE_MEM_FR_START_WRITE:
	ev.event_data = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_MEMORY, TRACE_MEMORY_SWAP_OUT,
			    &ev, sizeof(ev), NULL, 0);
	break;
    }

    return rc;
}

static int
convertIO(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    return 0;
}

static int
convertProc(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    uval minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));
    trace_process evProc;
    int rc = 0;

    switch (minorID) {
    case TRACE_PROC_LINUX_FORK:
	evProc.event_data1 = TRACE_SWAP64(event[2]); /* use the K42 PID */
	rc = ltt_writeEvent(evTime, TRACE_PROCESS, TRACE_PROCESS_FORK,
			    &evProc, sizeof(evProc), NULL, 0);
	break;

    case TRACE_PROC_LINUX_KILL:
	/* FIXME: no parameters, assumes the current process exited */
	rc = ltt_writeEvent(evTime, TRACE_PROCESS, TRACE_PROCESS_EXIT,
			    &evProc, sizeof(evProc), NULL, 0);
	break;

    default:
	break;
    }

    return rc;
}

static int
convertExcept(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    uval64 minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));
    trace_irq_entry evIRQ;
    trace_syscall_entry evSys;
    trace_trap_entry evTrap;
    trace_schedchange evSched;
    uval64 oldPID;
    int rc = 0;

    if (minorID == TRACE_EXCEPTION_IO_INTERRUPT) {
	evIRQ.irq_id = TRACE_SWAP64(event[1]);
	evIRQ.kernel = (runningStats.PID == 0);
	rc = ltt_writeEvent(evTime, TRACE_IRQ_ENTRY, 0, &evIRQ, sizeof(evIRQ),
			    NULL, 0);
    } else if (minorID == TRACE_EXCEPTION_TIMER_INTERRUPT) {
	evTrap.trap_id = 0x900; /* decrementer */
	evTrap.address = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_TRAP_ENTRY, 0, &evTrap,
			    sizeof(evTrap), NULL, 0);
	if (rc != 0) {
		/* K42 doesn't have a trap exit event, so hence we exit straight
		 * away - but FIXME: we might be about to context switch */
		rc = ltt_writeEvent(evTime, TRACE_TRAP_EXIT, 0, NULL, 0, NULL,
				    0);
	}
    }

    if (minorID == TRACE_EXCEPTION_PROCESS_YIELD
	|| minorID == TRACE_EXCEPTION_PPC_CALL
	|| minorID == TRACE_EXCEPTION_AWAIT_DISPATCH
	|| minorID == TRACE_EXCEPTION_PPC_ASYNC_REMOTE
	|| minorID == TRACE_EXCEPTION_AWAIT_PPC_RETRY
	|| minorID == TRACE_EXCEPTION_IPC_REMOTE) {
	evSys.syscall_id = __NR_ipc; /* FIXME! what to do with K42 syscalls? */
	evSys.address = 0; /* unknown */
	rc = ltt_writeEvent(evTime, TRACE_SYSCALL_ENTRY, 0, &evSys,
			    sizeof(evSys), NULL, 0);
    } else if (minorID == TRACE_EXCEPTION_PGFLT) {
	evTrap.trap_id = 0x300; /* data access */
	evTrap.address = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_TRAP_ENTRY, 0, &evTrap,
			    sizeof(evTrap), NULL, 0);
    }
    if (rc != 0) {
	return rc;
    }

    if (minorID == TRACE_EXCEPTION_PROCESS_YIELD
	|| minorID == TRACE_EXCEPTION_PPC_CALL
	|| minorID == TRACE_EXCEPTION_PPC_RETURN
	|| minorID == TRACE_EXCEPTION_IPC_REFUSED
	|| minorID == TRACE_EXCEPTION_PGFLT_DONE
	|| minorID == TRACE_EXCEPTION_AWAIT_DISPATCH_DONE
	|| minorID == TRACE_EXCEPTION_PPC_ASYNC_REMOTE_DONE
	|| minorID == TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE
	|| minorID == TRACE_EXCEPTION_IPC_REMOTE_DONE) {
	oldPID = runningStats.userPID;
	updateCurrent(TRACE_SWAP64(event[1]));
	if (oldPID != runningStats.userPID && oldPID != 0) {
	    evSched.out = oldPID;
	    evSched.out_state = 0; /* FIXME: unknown */
	    evSched.in = runningStats.userPID;
	    rc = ltt_writeEvent(evTime, TRACE_SCHEDCHANGE, 0, &evSched,
				sizeof(evSched), NULL, 0);
	}
    } else if (minorID == TRACE_EXCEPTION_PGFLT
	       || minorID == TRACE_EXCEPTION_AWAIT_DISPATCH
	       || minorID == TRACE_EXCEPTION_PPC_ASYNC_REMOTE
	       || minorID == TRACE_EXCEPTION_AWAIT_PPC_RETRY
	       || minorID == TRACE_EXCEPTION_IPC_REMOTE) {
	updateCurrent(runningStats.CPUID);
    }
    if (rc != 0) {
	return rc;
    }

    if (minorID == TRACE_EXCEPTION_PROCESS_YIELD
	|| minorID == TRACE_EXCEPTION_PPC_RETURN
	|| minorID == TRACE_EXCEPTION_IPC_REFUSED
	|| minorID == TRACE_EXCEPTION_AWAIT_DISPATCH_DONE
	|| minorID == TRACE_EXCEPTION_PPC_ASYNC_REMOTE_DONE
	|| minorID == TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE
	|| minorID == TRACE_EXCEPTION_IPC_REMOTE_DONE) {
	rc = ltt_writeEvent(evTime, TRACE_SYSCALL_EXIT, 0, NULL, 0, NULL, 0);
    } else if (minorID == TRACE_EXCEPTION_PGFLT_DONE) {
	rc = ltt_writeEvent(evTime, TRACE_TRAP_EXIT, 0, NULL, 0, NULL, 0);
    }

    return rc;
}

static int
convertSched(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    return 0;
}

static int
convertFS(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    return 0;
}

static int
convertLinux(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    uval minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));
    trace_irq_entry evIRQ;
    trace_soft_irq evSoftIRQ;
    trace_socket evSock;
    trace_file_system evFS;
    static unsigned activeIRQs = 0;
    int rc = 0;

    switch (minorID) {
    case TRACE_LINUX_INT:
	activeIRQs++;
	evIRQ.irq_id = 0; /* FIXME: unknown */
	evIRQ.kernel = (runningStats.PID == 0);
	rc = ltt_writeEvent(evTime, TRACE_IRQ_ENTRY, 0, &evIRQ, sizeof(evIRQ),
			    NULL, 0);
	break;

    case TRACE_LINUX_END:
    	if (activeIRQs > 0) {
	    activeIRQs--;
	    rc = ltt_writeEvent(evTime, TRACE_IRQ_EXIT, 0, NULL, 0, NULL, 0);
	}
	break;

    case TRACE_LINUX_BH:
	evSoftIRQ.event_data = 0; /* unused field */
	rc = ltt_writeEvent(evTime, TRACE_SOFT_IRQ, TRACE_SOFT_IRQ_BOTTOM_HALF,
			    &evSoftIRQ, sizeof(evSoftIRQ), NULL, 0);
	break;

    case TRACE_LINUX_ACCEPT:
	evSock.event_data1 = SYS_ACCEPT;
	evSock.event_data2 = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_SOCKET, TRACE_SOCKET_CALL, &evSock,
			    sizeof(evSock), NULL, 0);
	break;

    case TRACE_LINUX_SOCKET:
	evSock.event_data1 = SYS_SOCKET;
	evSock.event_data2 = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_SOCKET, TRACE_SOCKET_CALL, &evSock,
			    sizeof(evSock), NULL, 0);
	evSock.event_data1 = TRACE_SWAP64(event[1]);
	evSock.event_data2 = 0; /* FIXME: socket type */
	rc = ltt_writeEvent(evTime, TRACE_SOCKET, TRACE_SOCKET_CREATE, &evSock,
			    sizeof(evSock), NULL, 0);
	break;

    case TRACE_LINUX_RECV:
	evSock.event_data1 = SYS_RECV;
	evSock.event_data2 = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_SOCKET, TRACE_SOCKET_CALL, &evSock,
			    sizeof(evSock), NULL, 0);
	evSock.event_data1 = TRACE_SWAP64(event[1]);
	evSock.event_data2 = TRACE_SWAP64(event[2]);
	rc = ltt_writeEvent(evTime, TRACE_SOCKET, TRACE_SOCKET_RECEIVE, &evSock,
			    sizeof(evSock), NULL, 0);
	break;

    case TRACE_LINUX_SEND:
	evSock.event_data1 = SYS_SEND;
	evSock.event_data2 = TRACE_SWAP64(event[1]);
	rc = ltt_writeEvent(evTime, TRACE_SOCKET, TRACE_SOCKET_CALL, &evSock,
			    sizeof(evSock), NULL, 0);
	evSock.event_data1 = TRACE_SWAP64(event[1]);
	evSock.event_data2 = TRACE_SWAP64(event[2]);
	rc = ltt_writeEvent(evTime, TRACE_SOCKET, TRACE_SOCKET_SEND, &evSock,
			    sizeof(evSock), NULL, 0);
	break;

    case TRACE_LINUX_CLOSE:
	evFS.event_data1 = TRACE_SWAP64(event[1]);
	evFS.event_data2 = 0; /* unused */
	rc = ltt_writeEvent(evTime, TRACE_FILE_SYSTEM, TRACE_FILE_SYSTEM_CLOSE,
			    &evFS, sizeof(evFS), NULL, 0);
	break;

    default:
	break;
    }

    return rc;
}

static int
convertUser(const uval64 *event, uint32_t evTime, bool byteSwap)
{
    uval minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));
    trace_syscall_entry evSys;
    trace_file_system evFS;
    size_t len;
    int rc = 0;

    switch (minorID) {
    case TRACE_USER_START_EXEC:
	len = strlen((const char *)&event[1]);
	evFS.event_data1 = 0;
	evFS.event_data2 = len;
	rc = ltt_writeEvent(evTime, TRACE_FILE_SYSTEM, TRACE_FILE_SYSTEM_EXEC,
			    &evFS, sizeof(evFS), &event[1], len);
	break;

    case TRACE_USER_SYSCALL_ENTER:
	evSys.syscall_id = convertSyscallName((const char *) &event[4]);
	evSys.address = TRACE_SWAP64(event[2]);
	rc = ltt_writeEvent(evTime, TRACE_SYSCALL_ENTRY, 0, &evSys,
			    sizeof(evSys), NULL, 0);
	break;

    case TRACE_USER_SYSCALL_EXIT:
	rc = ltt_writeEvent(evTime, TRACE_SYSCALL_EXIT, 0, NULL, 0, NULL, 0);
	break;

    default:
	break;
    }

    return rc;
}

static int
convertEvent(const uval64 *event, uval64 ticksPerSec, bool byteSwap)
{
    uval64 first_word = TRACE_SWAP64(event[0]);
    uval majorID = TRACE_MAJOR_ID_GET(first_word);
    uint32_t evTime;
    int rc;

    rc = calcTimeDelta(TRACE_TIMESTAMP_GET(first_word), ticksPerSec, &evTime);
    if (rc != 0) {
	return rc;
    }

    switch (majorID) {
    case TRACE_MEM_MAJOR_ID:
    	rc = convertMem(event, evTime, byteSwap);
	break;

    case TRACE_IO_MAJOR_ID:
    	rc = convertIO(event, evTime, byteSwap);
	break;

    case TRACE_PROC_MAJOR_ID:
    	rc = convertProc(event, evTime, byteSwap);
	break;

    case TRACE_EXCEPTION_MAJOR_ID:
    	rc = convertExcept(event, evTime, byteSwap);
	break;

    case TRACE_SCHEDULER_MAJOR_ID:
    	rc = convertSched(event, evTime, byteSwap);
	break;

    case TRACE_FS_MAJOR_ID:
    	rc = convertFS(event, evTime, byteSwap);
	break;

    case TRACE_LINUX_MAJOR_ID:
    	rc = convertLinux(event, evTime, byteSwap);
	break;

    case TRACE_USER_MAJOR_ID:
    	rc = convertUser(event, evTime, byteSwap);
	break;

    default:
	return 0; /* unhandled event */
    }

    return rc;
}

static int
processTraceEvent(const traceFileHeaderV3 *headerInfo, const uval64 *event,
		  uval len, bool byteSwap, bool first, bool last, bool verbose)
{
    uval64 ticksPerSec = TRACE_SWAP64(headerInfo->ticksPerSecond);
    int rc;

    if (first) {
	memset(&runningStats, 0, sizeof(runningStats));
	runningStats.CPUID = TRACE_SWAP64(headerInfo->physProc);

	rc = ltt_writeHeader();
	if (rc != 0) {
	    return rc;
	}

	rc = ltt_writeStart(runningStats.CPUID);
	if (rc != 0) {
	    return rc;
	}
    }

    rc = convertEvent(event, ticksPerSec, byteSwap);
    if (rc != 0) {
	return rc;
    }

    if (last) {
	rc = ltt_writeFooter();
	if (rc != 0) {
	    return rc;
	}
    }

    return 0;
}

int
main(int argc, char **argv)
{
    char *inFilename = NULL;
    char *inBaseFilename = "trace-out", *outBaseFilename = NULL;
    char *filename;
    char namebuf[PATH_MAX];
    uval proc, numbProcs = 1;
    bool overrideGarbled = false, byteSwap = false;
    int rc, index;

    for (index = 1; index < argc; index++) {
	if (strcmp(argv[index], "--help") == 0) {
	    print_usage();
	    return 0;
	} else if (strcmp(argv[index], "--file") == 0) {
	    inFilename = argv[++index];
	} else if (strcmp(argv[index], "--mp") == 0) {
	    sscanf(argv[++index], "%ld", &numbProcs);
	} else if (strcmp(argv[index], "--mpBase") == 0) {
	    inBaseFilename = argv[++index];
	} else if (strcmp(argv[index], "--outBase") == 0) {
	    outBaseFilename = argv[++index];
	} else if (strcmp(argv[index], "--overrideGarbled") == 0) {
	    overrideGarbled = true;
	} else {
	    printf("Error: unknown option %s\n", argv[index]);
	    print_usage();
	    return -1;
	}
    }

    if (outBaseFilename == NULL) {
	outBaseFilename = inBaseFilename;
    }

    if (numbProcs > 1 && inFilename != NULL) {
	printf("Warning: --filename argument ignored\n");
	inFilename = NULL;
    }

    for (proc = 0; proc < numbProcs; proc++) {
	rc = snprintf(namebuf, sizeof(namebuf), "%s.%ld.ltt",
		      outBaseFilename, proc);
	if (rc == -1) {
	    return -1;
	}
	fpOut = fopen(namebuf, "w");
	if (fpOut == NULL) {
	    printf("Error: failed to open %s for writing\n", namebuf);
	    return -1;
	}
	outPos = 0;

	if (inFilename == NULL) {
	    rc = snprintf(namebuf, sizeof(namebuf), "%s.%ld.trc",
			  inBaseFilename, proc);
	    if (rc == -1) {
		return -1;
	    }
	    filename = namebuf;
	} else {
	    filename = inFilename;
	}

	rc = processTraceFile(filename, processTraceEvent, &byteSwap, true);
	if (rc != 0 && !overrideGarbled) {
	    printf("Error encountered. Exiting, see --overrideGarbled\n");
	    return rc;
	}

	fclose(fpOut);
    }

    return 0;
}

static void
print_usage(void)
{
    printf("ktrace2ltt [--help] [--file filename] [--mp N]\n");
    printf("           [--mpBase baseFilename] [--outBase baseFilename]\n");
    printf("           [--overrideGarbled]\n");
    printf("  ktrace2ltt converts K42 trace data to LTT equivalents\n");
    printf("  The --help option prints out this usage information.\n");
    printf("  The --file filename option specifies the binary trace file to\n");
    printf("   be used, the default is trace-out.0.trc\n");
    printf("  The --mp N option is used to ask for N trace files to be\n");
    printf("   processed.  By default it reads trace-out.X.trc, where\n");
    printf("   X is 0 through N - 1. This option overrides --file.\n");
    printf("  The --mpBase filename option is used to specify an\n");
    printf("   alternative base name for input files\n");
    printf("   e.g. --mpBase xxx results in xxx.0.trc, xxx.1.trc, etc.\n");
    printf("  The --outBase option specifies the base name for LTT output,\n");
    printf("   its usage is equivalent to --mpBase.\n");
    printf("  The --overrideGarbled option will cause ktrace2ltt to\n");
    printf("   attempt to continue even if it comes across a completely\n");
    printf("   garbled trace stream.  User beware.  Default is off.\n");
}

static uint8_t
convertSyscallName(const char *fullname)
{
    const char *prefix = "__k42_linux_";
    const char *name;
    size_t prefix_len = strlen(prefix);

    if (strncmp(fullname, prefix, prefix_len) != 0) {
	/* special cases */
	if (strcmp(fullname, "dostat") == 0) return __NR_stat;
	if (strcmp(fullname, "dofstat") == 0) return __NR_fstat;
	return 0;
    }
    name = &fullname[prefix_len];

#define CHECKSYS(NAME) if (strcmp(name, #NAME) == 0) return __NR_ ## NAME;
    CHECKSYS(restart_syscall)
    CHECKSYS(exit)
    CHECKSYS(fork)
    CHECKSYS(read)
    CHECKSYS(write)
    CHECKSYS(open)
    CHECKSYS(close)
    CHECKSYS(waitpid)
    CHECKSYS(creat)
    CHECKSYS(link)
    CHECKSYS(unlink)
    CHECKSYS(execve)
    CHECKSYS(chdir)
    CHECKSYS(time)
    CHECKSYS(mknod)
    CHECKSYS(chmod)
    CHECKSYS(lchown)
    CHECKSYS(break)
    CHECKSYS(oldstat)
    CHECKSYS(lseek)
    CHECKSYS(getpid)
    CHECKSYS(mount)
    CHECKSYS(umount)
    CHECKSYS(setuid)
    CHECKSYS(getuid)
    CHECKSYS(stime)
    CHECKSYS(ptrace)
    CHECKSYS(alarm)
    CHECKSYS(oldfstat)
    CHECKSYS(pause)
    CHECKSYS(utime)
    CHECKSYS(stty)
    CHECKSYS(gtty)
    CHECKSYS(access)
    CHECKSYS(nice)
    CHECKSYS(ftime)
    CHECKSYS(sync)
    CHECKSYS(kill)
    CHECKSYS(rename)
    CHECKSYS(mkdir)
    CHECKSYS(rmdir)
    CHECKSYS(dup)
    CHECKSYS(pipe)
    CHECKSYS(times)
    CHECKSYS(prof)
    CHECKSYS(brk)
    CHECKSYS(setgid)
    CHECKSYS(getgid)
    CHECKSYS(signal)
    CHECKSYS(geteuid)
    CHECKSYS(getegid)
    CHECKSYS(acct)
    CHECKSYS(umount2)
    CHECKSYS(lock)
    CHECKSYS(ioctl)
    CHECKSYS(fcntl)
    CHECKSYS(mpx)
    CHECKSYS(setpgid)
    CHECKSYS(ulimit)
    CHECKSYS(oldolduname)
    CHECKSYS(umask)
    CHECKSYS(chroot)
    CHECKSYS(ustat)
    CHECKSYS(dup2)
    CHECKSYS(getppid)
    CHECKSYS(getpgrp)
    CHECKSYS(setsid)
    CHECKSYS(sigaction)
    CHECKSYS(sgetmask)
    CHECKSYS(ssetmask)
    CHECKSYS(setreuid)
    CHECKSYS(setregid)
    CHECKSYS(sigsuspend)
    CHECKSYS(sigpending)
    CHECKSYS(sethostname)
    CHECKSYS(setrlimit)
    CHECKSYS(getrlimit)
    CHECKSYS(getrusage)
    CHECKSYS(gettimeofday)
    CHECKSYS(settimeofday)
    CHECKSYS(getgroups)
    CHECKSYS(setgroups)
    CHECKSYS(select)
    CHECKSYS(symlink)
    CHECKSYS(oldlstat)
    CHECKSYS(readlink)
    CHECKSYS(uselib)
    CHECKSYS(swapon)
    CHECKSYS(reboot)
    CHECKSYS(readdir)
    CHECKSYS(mmap)
    CHECKSYS(munmap)
    CHECKSYS(truncate)
    CHECKSYS(ftruncate)
    CHECKSYS(fchmod)
    CHECKSYS(fchown)
    CHECKSYS(getpriority)
    CHECKSYS(setpriority)
    CHECKSYS(profil)
    CHECKSYS(statfs)
    CHECKSYS(fstatfs)
    CHECKSYS(ioperm)
    CHECKSYS(socketcall)
    CHECKSYS(syslog)
    CHECKSYS(setitimer)
    CHECKSYS(getitimer)
    CHECKSYS(stat)
    CHECKSYS(lstat)
    CHECKSYS(fstat)
    CHECKSYS(olduname)
    CHECKSYS(iopl)
    CHECKSYS(vhangup)
    CHECKSYS(idle)
    CHECKSYS(vm86)
    CHECKSYS(wait4)
    CHECKSYS(swapoff)
    CHECKSYS(sysinfo)
    CHECKSYS(ipc)
    CHECKSYS(fsync)
    CHECKSYS(sigreturn)
    CHECKSYS(clone)
    CHECKSYS(setdomainname)
    CHECKSYS(uname)
    CHECKSYS(modify_ldt)
    CHECKSYS(adjtimex)
    CHECKSYS(mprotect)
    CHECKSYS(sigprocmask)
    CHECKSYS(create_module)
    CHECKSYS(init_module)
    CHECKSYS(delete_module)
    CHECKSYS(get_kernel_syms)
    CHECKSYS(quotactl)
    CHECKSYS(getpgid)
    CHECKSYS(fchdir)
    CHECKSYS(bdflush)
    CHECKSYS(sysfs)
    CHECKSYS(personality)
    CHECKSYS(afs_syscall)
    CHECKSYS(setfsuid)
    CHECKSYS(setfsgid)
    CHECKSYS(_llseek)
    CHECKSYS(getdents)
    CHECKSYS(_newselect)
    CHECKSYS(flock)
    CHECKSYS(msync)
    CHECKSYS(readv)
    CHECKSYS(writev)
    CHECKSYS(getsid)
    CHECKSYS(fdatasync)
    CHECKSYS(_sysctl)
    CHECKSYS(mlock)
    CHECKSYS(munlock)
    CHECKSYS(mlockall)
    CHECKSYS(munlockall)
    CHECKSYS(sched_setparam)
    CHECKSYS(sched_getparam)
    CHECKSYS(sched_setscheduler)
    CHECKSYS(sched_getscheduler)
    CHECKSYS(sched_yield)
    CHECKSYS(sched_get_priority_max)
    CHECKSYS(sched_get_priority_min)
    CHECKSYS(sched_rr_get_interval)
    CHECKSYS(nanosleep)
    CHECKSYS(mremap)
    CHECKSYS(setresuid)
    CHECKSYS(getresuid)
    CHECKSYS(query_module)
    CHECKSYS(poll)
    CHECKSYS(nfsservctl)
    CHECKSYS(setresgid)
    CHECKSYS(getresgid)
    CHECKSYS(prctl)
    CHECKSYS(rt_sigreturn)
    CHECKSYS(rt_sigaction)
    CHECKSYS(rt_sigprocmask)
    CHECKSYS(rt_sigpending)
    CHECKSYS(rt_sigtimedwait)
    CHECKSYS(rt_sigqueueinfo)
    CHECKSYS(rt_sigsuspend)
    CHECKSYS(pread64)
    CHECKSYS(pwrite64)
    CHECKSYS(chown)
    CHECKSYS(getcwd)
    CHECKSYS(capget)
    CHECKSYS(capset)
    CHECKSYS(sigaltstack)
    CHECKSYS(sendfile)
    CHECKSYS(getpmsg)
    CHECKSYS(putpmsg)
    CHECKSYS(vfork)
    CHECKSYS(ugetrlimit)
    CHECKSYS(readahead)
    CHECKSYS(mmap2)
    CHECKSYS(truncate64)
    CHECKSYS(ftruncate64)
    CHECKSYS(stat64)
    CHECKSYS(lstat64)
    CHECKSYS(fstat64)
    CHECKSYS(pciconfig_read)
    CHECKSYS(pciconfig_write)
    CHECKSYS(pciconfig_iobase)
    CHECKSYS(multiplexer)
    CHECKSYS(getdents64)
    CHECKSYS(pivot_root)
    CHECKSYS(fcntl64)
    CHECKSYS(madvise)
    CHECKSYS(mincore)
    CHECKSYS(gettid)
    CHECKSYS(tkill)
    CHECKSYS(setxattr)
    CHECKSYS(lsetxattr)
    CHECKSYS(fsetxattr)
    CHECKSYS(getxattr)
    CHECKSYS(lgetxattr)
    CHECKSYS(fgetxattr)
    CHECKSYS(listxattr)
    CHECKSYS(llistxattr)
    CHECKSYS(flistxattr)
    CHECKSYS(removexattr)
    CHECKSYS(lremovexattr)
    CHECKSYS(fremovexattr)
    CHECKSYS(futex)
    CHECKSYS(sched_setaffinity)
    CHECKSYS(sched_getaffinity)
    CHECKSYS(tuxcall)
    CHECKSYS(sendfile64)
    CHECKSYS(io_setup)
    CHECKSYS(io_destroy)
    CHECKSYS(io_getevents)
    CHECKSYS(io_submit)
    CHECKSYS(io_cancel)
    CHECKSYS(set_tid_address)
    CHECKSYS(fadvise64)
    CHECKSYS(exit_group)
    CHECKSYS(lookup_dcookie)
    CHECKSYS(epoll_create)
    CHECKSYS(epoll_ctl)
    CHECKSYS(epoll_wait)
    CHECKSYS(remap_file_pages)
    CHECKSYS(timer_create)
    CHECKSYS(timer_settime)
    CHECKSYS(timer_gettime)
    CHECKSYS(timer_getoverrun)
    CHECKSYS(timer_delete)
    CHECKSYS(clock_settime)
    CHECKSYS(clock_gettime)
    CHECKSYS(clock_getres)
    CHECKSYS(clock_nanosleep)
    CHECKSYS(swapcontext)
    CHECKSYS(tgkill)
    CHECKSYS(utimes)
    CHECKSYS(statfs64)
    CHECKSYS(fstatfs64)
    CHECKSYS(fadvise64_64)
    CHECKSYS(rtas)
#undef CHECKSYS

    return 0;
}
