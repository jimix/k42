/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traced.C,v 1.54 2005/08/30 00:49:00 neamtiu Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: dameon code that awakes occasionally and writes
 *                     out tracing buffers
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
#include <io/FileLinux.H>
#include <scheduler/Scheduler.H>
#include <sys/SystemMiscWrapper.H>
#include <sync/MPMsgMgr.H>
#include <usr/ProgExec.H>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <trace/traceTest.h>
#include <trace/traceControl.h>
#include <trace/trace.H>
#include <sys/systemAccess.H>

#include "DiskTrace.H"

#define MAX_CPUS	32
#define DEFAULT_PORT	4242

#define DEFAULT_PATH    "/knfs"
#define DEFAULT_BASE    "trace-out"

const char *outPathName = DEFAULT_PATH, *baseFileName = DEFAULT_BASE;
bool traceToNetwork = false;
bool traceToDisk = false;
uval verbose;
FileLinuxRef traceFileRef[MAX_CPUS];
DiskTrace *diskTrace = NULL;

struct {
    int sockfd;
    struct sockaddr_in dest; /* REMOVEME: see comment in setUpNetwork() */
    uint64_t seqnum;
} traceNetwork[MAX_CPUS];

/* include this here so we don't need to shadow the declarations of the above
 * globals... needs cleaning up */
#include "trace2stream.H"

// atomic count used to synchronize ending of trace daemon
sval runningChildren;

void
writeFileHeader(VPNum physProc)
{
    traceFileHeaderV3 *headerInfo;
    uval index = 0;
    SysStatusUval rc;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    uval64 *const trcArray = trcInfo->traceArray;

    // the page we have to communicate with the cut through is
    //   right after the trace buffer
    index = trcInfo->numberOfBuffers * TRACE_BUFFER_SIZE;
    headerInfo = (traceFileHeaderV3 *)(&trcArray[index]);

    headerInfo->endianEncoding = TRACE_BIG_ENDIAN;
    headerInfo->extra[0] = 0;
    headerInfo->extra[1] = 0;
    headerInfo->extra[2] = 0;
    headerInfo->version = 3;
    headerInfo->headerLength = sizeof(traceFileHeaderV3);
    headerInfo->flags = 0;
    headerInfo->alignmentSize = TRACE_BUFFER_SIZE * sizeof(uval64);
    headerInfo->ticksPerSecond = Scheduler::TicksPerSecond();
    headerInfo->physProc = physProc;
    headerInfo->initTimestamp = Scheduler::SysTimeNow();

    rc = trace2streamWrite(physProc, (uval)headerInfo,
			   headerInfo->headerLength);
    if (_SGETUVAL(rc) != headerInfo->headerLength) {
	cprintf("trace2stream write (3) failed\n");
    }
}

void
childMain(VPNum vp, uval dump, uval debugLevel)
{
    SysStatusUval rc;
    SysStatus rc1;
    uval myBuffer;
    uval currentIndex, currentBuffer;
    uval index, size, buffersReady;
    uval loopCount, logCount, lostCount;
    uval endCount=0;
    VPNum physProc;

    volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
    TraceControl *const trcCtrl = trcInfo->traceControl;
    uval64 *const trcArray = trcInfo->traceArray;

    physProc = KernelInfo::PhysProc();

    if (debugLevel != 0) {
	cprintf("printf debug info for Processor %ld\n", physProc);
	cprintf(" buffers %lld bufferSize %lld\n", trcInfo->numberOfBuffers,
		TRACE_BUFFER_SIZE);
	for (myBuffer=0; myBuffer < trcInfo->numberOfBuffers; myBuffer++) {
	    cprintf("proc %ld buffer %ld bufferCount %ld\n", 
		    physProc, myBuffer, trcCtrl->bufferCount[myBuffer]);
	}
	FetchAndAddSigned(&runningChildren, -1);
	return;
    }

    TraceOSControlTracedStart();
    rc = DREFGOBJ(TheSystemMiscRef)->traceEnableTraceD();
    if (verbose >= 1) {
	cprintf("trace daemon logging facility started on vp %ld physProc %ld\n",
		vp, physProc);
    }

    rc1 = trace2streamOpen(physProc);
    if (_FAILURE(rc1)) {
	cprintf("open of trace file failed on physProc %ld\n", physProc);
	return;
    }

    writeFileHeader(physProc);

    if (dump == 1) {
	rc = DREFGOBJ(TheSystemMiscRef)->traceStopTraceD();
    }

    //cprintf("trace mask %llx\n", trcInfo->mask);
    //cprintf("ticks per second %lld\n", Scheduler::TicksPerSecond());

    myBuffer = trcCtrl->buffersConsumed % trcInfo->numberOfBuffers;

    buffersReady = trcCtrl->buffersProduced - trcCtrl->buffersConsumed;

    if (buffersReady >= trcInfo->numberOfBuffers) {
	cprintf("WARNING: Trace buffers were full before the trace logging\n");
	cprintf("         daemon started; events have been lost. ");
	cprintf("physProc %ld\n", physProc);
	if (dump == 1) {
	    // we want to dump out what's in the trace array and exit
	    (void) SwapVolatile(&(trcCtrl->buffersConsumed),
				trcCtrl->buffersProduced -
				(trcInfo->numberOfBuffers - 1));
	} else {
	    // normal case we're just starting up the daemon
	    (void) SwapVolatile(&(trcCtrl->buffersConsumed),
				trcCtrl->buffersProduced - 1);
	}
	myBuffer = trcCtrl->buffersConsumed % trcInfo->numberOfBuffers;
    }

    loopCount = logCount = lostCount = 0;
    for (;;) {
	//cprintf("traced time %lld bufready %ld\n",
	//Scheduler::SysTimeNow()/1000000,
	//trcCtrl->buffersProduced - trcCtrl->buffersConsumed);
	loopCount++;
	while (!(trcCtrl->writeOkay)) {
	    //cprintf("blocking on write not okay\n");
	    Scheduler::DelayMicrosecs(1000000); // 1 second
	}

	// See if we've been lapped.
	buffersReady = trcCtrl->buffersProduced - trcCtrl->buffersConsumed;
#if 0	// for debugging
	passertMsg(trcCtrl->buffersProduced >= trcCtrl->buffersConsumed,
		   "look prod %ld, cons %ld, loopCount %ld\n",
		   trcCtrl->buffersProduced,
		   trcCtrl->buffersConsumed, loopCount);
#endif

	if (buffersReady >= trcInfo->numberOfBuffers) {
	    lostCount += buffersReady-1;
	    cprintf("WARNING: traced fallen behind events have been lost.\n");
	    cprintf("         time %lld loop %ld logged %ld lost %ld ",
		    Scheduler::SysTimeNow()/1000000, loopCount,
		    logCount, lostCount);
	    cprintf("buffersReady %ld ", buffersReady);
	    cprintf("physProc %ld\n", physProc);

	    TraceOSTestTest2(1, 2);

	    (void) SwapVolatile(&(trcCtrl->buffersConsumed),
				trcCtrl->buffersProduced - 1);
	    myBuffer = trcCtrl->buffersConsumed % trcInfo->numberOfBuffers;
	}

	currentIndex = trcCtrl->index & trcInfo->indexMask;
	currentBuffer = TRACE_BUFFER_NUMBER_GET(currentIndex);

	//cprintf("buf prod %ld buf consum %ld mybuff %ld size %ld\n",
	//trcCtrl->buffersProduced, trcCtrl->buffersConsumed,myBuffer,
	//trcCtrl->bufferCount[myBuffer]);
	TraceOSTestTest1(1);
	// Write out all buffers that have been produced and are complete.
    write_buffer:
	while ((myBuffer != currentBuffer) &&
	       (trcCtrl->bufferCount[myBuffer] == TRACE_BUFFER_SIZE)) {
	    index = myBuffer * TRACE_BUFFER_SIZE;
	    size = TRACE_BUFFER_SIZE * sizeof(uval64);
	    rc = trace2streamWrite(physProc, (uval) &trcArray[index], size);
	    logCount++;

	    if (_SGETUVAL(rc) != size) {
		cprintf("trace2stream write (1) failed physProc %ld\n",physProc);
	    }
	    myBuffer = (myBuffer + 1) % trcInfo->numberOfBuffers;
	    AtomicAddVolatile(&(trcCtrl->buffersConsumed), 1);
	}
	if (myBuffer != currentBuffer) {
	    // that must mean that buffer count of myBuffer != BUFFER_SIZE
	    cprintf("WARNING: unwritten event, trace stream invalid\n");
	    cprintf("\nmy buffer %ld count %ld SIZE %lld\n",
		    myBuffer, trcCtrl->bufferCount[myBuffer],
		    TRACE_BUFFER_SIZE);
	    //myBuffer = (myBuffer + 1) % trcInfo->numberOfBuffers;
	    //AtomicAddVolatile(&(trcCtrl->buffersConsumed), 1);
	    //goto write_buffer;
	}
	if (!(trcInfo->tracedRunning)) {
	    // something has asked the traced to stop:
	    //      write out the rest of remaining data and exit

	    // this will help us check to make sure we picked up all the
	    // events we were supposed to
	    TraceOSControlTracedStop();

	    currentIndex = trcCtrl->index & trcInfo->indexMask;
	    currentBuffer = TRACE_BUFFER_NUMBER_GET(currentIndex);

	    if ((myBuffer != currentBuffer) && (endCount == 0)) {
		// we missed some intervening events - guess this could
		//   cause livelock and we never finish but lets hope not
		// we'll ensure no livelock by only "looping once"
		endCount = 1;
		goto write_buffer;
	    }
	    index = myBuffer * TRACE_BUFFER_SIZE;
	    size =  (currentIndex * sizeof(uval64)) -
		(currentBuffer * TRACE_BUFFER_SIZE * sizeof(uval64));

	    rc = trace2streamWrite(physProc, (uval) &trcArray[index], size);

	    if (_SGETUVAL(rc) != size) {
		cprintf("trace2stream write (2) failed physProc %ld\n", physProc);
	    }
	    break;
	}
	TraceOSTestTest1(2);
	//cprintf("buf prod %ld buf consum %ld\n",
	//trcCtrl->buffersProduced, trcCtrl->buffersConsumed);
	//cprintf("traced time %lld loop %ld logged %ld lost %ld bufready %ld\n",
	//Scheduler::SysTimeNow()/1000000, loopCount, logCount,
	//lostCount, trcCtrl->buffersProduced - trcCtrl->buffersConsumed);

	Scheduler::DelayMicrosecs(100000);	// 100 ms.
    }
    if (verbose >= 1) {
	cprintf("trace daemon finished on physProc %ld\n", physProc);
    }
    trace2streamClose(physProc);
    FetchAndAddSigned(&runningChildren, -1);
}

struct LaunchChildMsg : public MPMsgMgr::MsgAsync {
    VPNum vp;
    uval dump;
    uval debugLevel;

    virtual void handle() {
	uval const myVP = vp;
	uval const myDump = dump;
	uval const myDebugLevel = debugLevel;
	free();
	childMain(myVP, myDump, myDebugLevel);
    }
};

static int
setUpNetwork(const char *host, uval16 port, uval cpus, const char *filename)
{
    struct hostent *hostent;
    uval cpu;
    int rc, fd;
    SysStatusUval krc;

    /* FIXME: gethostbyname seems broken on K42 for doing actual DNS lookups,
     * but presumably someone will fix that. in the meantime specifying an
     * IP address works */
    hostent = gethostbyname(host);
    if (hostent == NULL) {
	cprintf("Error: gethostbyname(\"%s\"): %d\n", host, h_errno);
	herror("Error: gethostbyname");
	exit(1);
    }

    for (cpu = 0; cpu < cpus; cpu++) {
    	if (cpus == 1) {
	    /* special case for when we only run on one local CPU */
	    cpu = KernelInfo::PhysProc();
	}

	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
	    perror("Error: socket");
	    return -1;
	}

	/* FIXME: we shouldn't need to remember the sockaddr for each socket,
	 * we should be able to just do the connect below and then forget it,
	 * but currently K42 doesn't remember the address used in connect
	 * when dealing with datagram sockets */
	traceNetwork[cpu].dest.sin_family = AF_INET;
	traceNetwork[cpu].dest.sin_port = htons(port);
	port++;
	traceNetwork[cpu].dest.sin_addr = *((struct in_addr *)hostent->h_addr);
	memset(&traceNetwork[cpu].dest.sin_zero, 0,
	       sizeof(traceNetwork[cpu].dest.sin_zero));

	rc = connect(fd, (struct sockaddr *)&traceNetwork[cpu].dest,
		     sizeof(traceNetwork[cpu].dest));
	if (rc != 0) {
	    perror("Error: connect");
	    return -1;
	}

    	traceNetwork[cpu].sockfd = fd;

        /* send the magic start-of-trace marker packet */
	traceNetwork[cpu].seqnum = -1UL;
        krc = trace2netWrite(cpu, (uval)filename, strlen(filename));
        if (_FAILURE(krc)) {
            err_printf("setUpNetwork: sending start-of-trace marker failed\n");
            return -1;
        }
	traceNetwork[cpu].seqnum = 0;
    }

    return 0;
}

static void
print_usage(char *argv[])
{
    printf("Usage: %s [OPTIONS]\n"
	   " --copyTraceDisk <filename> copy trace data from raw disk to "
	   "                  file\n"
           " --debugLevel level    print out various levels of debug info\n"
           " --dump                write out available buffers and exit\n"
           " --help                print out this message\n"
           " --file <filename>     base file name (default: %s)\n"
           " --net <host>[:<port>] sends trace data to remote host\n"
           " --nodaemon            disables demoon mode, will not return \
                                   until trace daemon finished \n"
           " --path <pathname>     directory for trace files (default: %s)\n"
	   " --rawdisk             sends trace data to raw disk\n"
	   " --verbose <level>     supports 0 and 1 default 1\n"
           " --vp <#>              start on specific VPs, 0=local, 1=all\n",
           argv[0], DEFAULT_PATH, DEFAULT_BASE);
}

int
main(int argc, char *argv[])
{
    NativeProcess();

    VPNum numbVPs, vp;
    SysStatus rc;
    uval procs, dump, network, debugLevel;
    uval runAsDaemon;
    sval iArgc;
    sval localRunningChildren;
    char *net_dest = NULL, *portStr;
    uval16 port = DEFAULT_PORT;

    procs = dump = network = debugLevel = 0;
    runningChildren = 0;
    runAsDaemon = 1;
    verbose = 1;

    for (iArgc = 1; iArgc < argc; iArgc++) {
        if (strcmp(argv[iArgc], "--help") == 0) {
            print_usage(argv);
            exit(0);
	} else if (strcmp(argv[iArgc], "--copyTraceDisk") == 0) {
	    if (argc != 3 || iArgc != 1) {
		if (verbose >= 1) {
		    printf("For --copyTraceDisk, the expected usage is:\n\t"
			   "%s --copyTraceDisk file\n", argv[0]);
		}
	    }
	    DiskTrace::CopyToFile(argv[iArgc+1]);
	    delete diskTrace;
	    exit(1);
	} else if (strcmp(argv[iArgc], "--debugLevel") == 0) {
	    debugLevel = atoi(argv[++iArgc]);
	    if (verbose >= 1) {
		printf("going to execute debug level option %ld\n",debugLevel);
	    }
	    // FIXME for now since we're still calling from the kernel
	    //       we should pick up procs from --vp
	    procs = 1;
	} else if (strcmp(argv[iArgc], "--dump") == 0) {
	    if (verbose >= 1) {
		printf("going to execute dump option\n");
	    }
	    dump = 1;
	    // FIXME for now since we're still calling from the kernel
	    //       we should pick up procs from --vp
	    procs = 1;
	} else if (strcmp(argv[iArgc], "--file") == 0) {
	    baseFileName = argv[++iArgc];
	} else if (strcmp(argv[iArgc], "--net") == 0) {
	    /* parse network arguments, host[:port] */
	    net_dest = argv[++iArgc];
	    portStr = strrchr(net_dest, ':');
	    if (portStr != NULL) {
		*portStr = '\0';
		portStr++;
		port = atoi(portStr);
	    }
	    if (verbose >= 1) {
		printf("going to stream over network to %s:%hd\n", net_dest, port);
	    }
	    network = 1;
	    // FIXME for now since we're still calling from the kernel
	    //       we should pick up procs from --vp
	    procs = 1;
	} else if (strcmp(argv[iArgc], "--nodaemon") == 0) {
	    runAsDaemon = 0;;
	} else if (strcmp(argv[iArgc], "--path") == 0) {
	    outPathName = argv[++iArgc];
	} else if (strcmp(argv[iArgc], "--rawdisk") == 0) {
	    traceToDisk = true;
	} else if (strcmp(argv[iArgc], "--verbose") == 0) {
	    verbose = atoi(argv[++iArgc]);
	} else if (strcmp(argv[iArgc], "--vp") == 0) {
	    procs = atoi(argv[++iArgc]);
	} else {
	    printf("Error: unknown argument %s\n", argv[iArgc]);
            print_usage(argv);
	    exit(1);
	}
    };

    if (runAsDaemon) {
	if (verbose >= 1) {
	    printf("daemonising, check system console for further output\n");
	}
	close(0);
	close(1);
	close(2);
	setpgid(0,0);
	pid_t pid = fork();
	if (pid) {
	    exit(0);
	}
    }

    if (procs > 0) {
	numbVPs = DREFGOBJ(TheProcessRef)->ppCount();
	passert((numbVPs < MAX_CPUS), err_printf("too many vps\n"));
    } else {
	numbVPs = 1;
    }

    if (network == 1) {
	if (setUpNetwork(net_dest, port, numbVPs, baseFileName) != 0) {
	    exit(1);
	}
	traceToNetwork = true;
    }

    if (traceToDisk) {
	diskTrace = new DiskTrace(dump);
	rc = diskTrace->init(numbVPs);
	if (_FAILURE(rc)) {
	    exit(1);
	}
    }

    // Tracking how much take we take
    uval timeBegin, timeEnd;
    timeBegin = Scheduler::SysTimeNow();

    // FIXME eventually this should be a vp set
    if (procs > 0) {
	if (verbose >= 1) {
	    cprintf("starting traced on all processors\n");
	}

	// create vps
	for (vp = 1; vp < numbVPs; vp++) {
	    rc = ProgExec::CreateVP(vp);
	    passertMsg(_SUCCESS(rc), "ProgExec::CreateVP failed (0x%lx)\n",
		       rc);
	}
	// start vps
	for (vp = 1; vp < numbVPs; vp++) {
	    FetchAndAddSigned(&runningChildren, 1);
	    LaunchChildMsg *const msg =
		new(Scheduler::GetEnabledMsgMgr()) LaunchChildMsg;
	    msg->vp = vp;
	    msg->dump = dump;
	    msg->debugLevel = debugLevel;
	    rc = msg->send(SysTypes::DSPID(0, vp));
	    tassert(_SUCCESS(rc), err_printf("traced: send failed\n"));
	}
    }

    // I'm vp 0

    FetchAndAddSigned(&runningChildren, 1);
    childMain(0, dump, debugLevel);
    localRunningChildren = FetchAndAddSigned(&runningChildren, 0);

    while (localRunningChildren > 0) {
	localRunningChildren = FetchAndAddSigned(&runningChildren, 0);
	Scheduler::DelayMicrosecs(1000000); // 1 second
    }

    timeEnd = Scheduler::SysTimeNow();

    uval nsecs = (timeEnd - timeBegin) / Scheduler::TicksPerSecond();

    if (traceToDisk) {
	diskTrace->finishOutstanding();
	diskTrace->sync();
	// sync generates request for meta-data output
	diskTrace->finishOutstanding();
	delete diskTrace;
    }
    if (verbose >= 1) {
	cprintf("trace daemon finished on all processors ... exiting."
		" It took %ld seconds\n", nsecs);
    }
}
