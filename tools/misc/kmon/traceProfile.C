/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceProfile.C,v 1.68 2005/08/25 13:06:39 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>            // open
#include <unistd.h>           // close
#include "sys/errno.h"
#include "sys/mman.h"
#include "sys/stat.h"
#include "sys/dir.h"
#include <assert.h>

#ifdef K42
# include <sys/sysIncs.H>
# include <sys/systemAccess.H>
# include <usr/ProgExec.H>
#else
# include "sys/hostSysTypes.H"
#endif

#include "trace/traceUtils.H"
#include "trace/trace.H"
#include "trace/traceIncs.H"
#include "trace/traceUnified.H"
#include "traceCommon.H"


#define KERN_TEXT_OFFSET 0xc000000002000000ULL
#define PROC_TEXT_OFFSET 0x0000000010000000ULL

#define LOCK_STATS_COUNT_FILE	"traceLockStatsCount"
#define LOCK_STATS_TIME_FILE	"traceLockStatsTime"
#define LOCK_STATS_MAXTIME_FILE "traceLockStatsMaxTime"
#define KMON_EXEC_FILE		"execStats.txt"
#define KMON_LOCK_FILE		"lockStats.txt"
#define MAPPED_PIDS_FILE	"traceMappedPIDs"
#define OUT_FILE		"traceOut"

#define MAX_FILES	24
#define MAX_PIDS	20
#define MAX_CALLDEPTH	3
#define MAX_LOW_PIDS	0x10000
#define HASH_BUCKETS	0x1000
#define NUMB_BINS	0x400000
#define MAX_MINOR_PAIRS 32
#define MAX_ACTIVE_COUNT 5
#define MAX_REGION_COUNT 20
#define MAX_TIMESTAMP	0xffffffff
#define INVALID_PID	((uval64)-1)

#define FILENAME_SIZE	256
#define COMMAND_SIZE	256
#define MAX_LINE	8192
#define LINE_LENGTH	256

#define VERBOSE_L1	0x1
#define VERBOSE_L2	0x2
#define VERBOSE_L3	0x4


static FILE *kmonExecFP, *kmonLockFP;

static struct LockInst {
    uval64  addr;
    uval64  pid;
    uval64  callChain[MAX_CALLDEPTH];
    uval64  initPC, spin, time, maxTime;
    uval    count;
    struct LockInst *next;
    struct LockInst *lockChain;
} *lockHash[HASH_BUCKETS];

// keep track of number of hits on low number pids
// used to figure out what pids are worth mapping
static sval lowPIDCount[MAX_LOW_PIDS];

struct SymFreq {
    uval count, symNum;
};

static struct {
    uval64  PID;    // -1 indicates invalid
    uval    count;  // keep track of number of hits on mapped pids
    sval32  *bin;
    uval64  text_offset;
    const char *filename;
    uval64  *symAddrs;
    uval    symAddrCount;
    struct SymFreq *symFreqs;
    uval    symFreqCount;
} mappedPIDs[MAX_PIDS];
static uval mappedPIDCount;

struct PairMinor {
    uval majorID, startMinorID, stopMinorID;
    uval sumTime, startTime, idleTime, kernelTime, count, min, max;
    bool active;
} pairStats[MAX_MINOR_PAIRS];

static struct GenCount {
    uval64  lastSetTime;
    uval64  lastDbgTime; // for counting debug events
    uval64  lastGenSetTime; // for generation count info
    uval64  lastSwitchTime; // for null swap test
    uval    countTimeElapsed, countTimeActive, countTimeGen, countTimeSwitch;
    uval    countTimeDbg;
    uval64  sumTimeElapsed, maxTimeElapsed, minTimeElapsed;
    uval64  sumTimeActive, maxTimeActive, minTimeActive;
    uval64  sumTimeGen, maxTimeGen, minTimeGen;
    uval64  sumTimeSwitch, maxTimeSwitch, minTimeSwitch;
    uval64  sumTimeDbg, maxTimeDbg, minTimeDbg;
    uval    activeCount[MAX_ACTIVE_COUNT]; // 0 to sum those greater than MAX
    uval    currentActive;
    uval    sumCountDbg, countCountDbg;
    uval    dbgCount[MAX_REGION_COUNT]; // 0 to sum those greater than MAX
} genCount[MAX_PIDS];

enum event {EVENT_TIMER, EVENT_HWPERF};
enum time {TIME_SECS, TIME_TICKS};
enum sample {PCSAMPLE, LRSAMPLE};

static struct {
    uval    verboseMask;
    enum event	eventType;
    enum time	timeType;
    enum sample sampleType;
    bool    noSym;
    bool    lockStats;
    bool    kmonFlag;
    bool    genCountFlag;
    bool    countDbgFlag;
    bool    overrideGarbled;
    bool    pairStatsFlag;
    bool    useLockAddr;
    bool    lockChainCount;
    uval    callDepth;
    uval    numPairStats;
    uval    numbToDisplay;
} options;

static struct {
    uval64  physProc;
    uval64  ticksPerSecond;
    uval    numbEvents;
    uval64  lastTime;
    sval64  wrapAdjust;
    uval64  timestamp;
    uval64  PID;
    uval64  COMMID;
    uval64  kernelTime;
    uval64  idleTime;
    uval64  lastContextSwitchTime;
} current_trace;

static struct {
    uval    exEvents;
    uval64  allEvents;
    uval    highPIDs;
    uval    unmappedPIDs;
    double  time;
} totals;

static bool byteSwap = false;

static void print_usage();


static void
addMappedPID(uval64 PID, uval64 text_offset, const char *filename)
{
    int i = mappedPIDCount;

    if (i >= MAX_PIDS) {
    	printf("Error: too many mapped PIDs, max %d\n", MAX_PIDS);
	exit(-1);
    } else {
    	mappedPIDCount++;
    }

    mappedPIDs[i].PID = PID;
    mappedPIDs[i].count = 0;
    mappedPIDs[i].text_offset = text_offset;
    mappedPIDs[i].filename = filename;
    mappedPIDs[i].symAddrs = NULL;
    mappedPIDs[i].symAddrCount = 0;
}

static void
dumpMappedPIDs()
{
    uval i;
    FILE *fp;

    fp = fopen(MAPPED_PIDS_FILE, "w");
    if (fp == NULL) {
	printf("Error: couldn't write to %s\n", MAPPED_PIDS_FILE);
	return;
    }

    for (i = 0; i < mappedPIDCount; i++) {
    	fprintf(fp, "0x%llx 0x%llx %s\n", mappedPIDs[i].PID,
		mappedPIDs[i].text_offset, mappedPIDs[i].filename);
    }

    fclose(fp);
}

static void
init_globals()
{
    int mfd, i;
    sval32 *tmp;

    mfd = open("/dev/zero", O_RDWR, 0);
    if (mfd < 0) {
	printf("error: couldn't open /dev/zero\n");
	exit(-1);
    }

    tmp = (sval32 *)mmap(0, sizeof(sval32)*MAX_PIDS*NUMB_BINS,
			   PROT_READ|PROT_WRITE, MAP_PRIVATE, mfd, 0);
    if (tmp == (sval32 *)-1) {
    	printf("error: couldn't mmap /dev/zero\n");
	exit(-1);
    }

    memset(genCount, 0, sizeof(genCount));

    for (i = 0; i < MAX_PIDS; i++) {
	mappedPIDs[i].PID = INVALID_PID;
	mappedPIDs[i].bin = tmp + i * NUMB_BINS;

	genCount[i].minTimeElapsed = (uval64)(-1);
	genCount[i].minTimeActive = (uval64)(-1);
	genCount[i].minTimeGen = (uval64)(-1);
	genCount[i].minTimeSwitch = (uval64)(-1);
	genCount[i].minTimeDbg = (uval64)(-1);
    }

    memset(lowPIDCount, 0, sizeof(lowPIDCount));
    memset(lockHash, 0, sizeof(lockHash));
    memset(&totals, 0, sizeof(totals));

    memset(&pairStats, 0, sizeof(pairStats));
    for (i = 0; i < MAX_MINOR_PAIRS; i++) {
	pairStats[i].min = MAX_TIMESTAMP;
    }
}

static inline uval64
commID2PID(uval64 commID)
{
    const uval COMMID_PID_SHIFT = 32;
    const uval PID_BITS = 32;

    return (commID >> COMMID_PID_SHIFT) & ((uval64(1) << PID_BITS) - 1);
}

static sval
PIDToIndex(uval64 pid)
{
    uval i;

    for (i = 0; i < mappedPIDCount; i++) {
	if (mappedPIDs[i].PID == pid) {
	    return i;
	}
    }

    return -1;
}

static inline uval
hashFunc(uval64 x)
{
    return (x >> 4) & (HASH_BUCKETS - 1);
}

static float
ticks2secsf(float ticks)
{
    return ticks/((float)current_trace.ticksPerSecond);
}

static inline float
ticks2secs(uval64 ticks)
{
    return ticks2secsf((float)ticks);
}

static int
printTime(uval64 val, char *retStr, size_t len)
{
    if (options.timeType == TIME_SECS) {
	return snprintf(retStr, len, "%.9f", ticks2secs(val));
    } else {
	return snprintf(retStr, len, "%lld", val);
    }
}

static void
binPC(uval64 currentPID, uval64 pcVal)
{
    sval pIndex;

    totals.exEvents++;

    if (currentPID < MAX_LOW_PIDS) {
	lowPIDCount[currentPID] += 1;
    } else {
	totals.highPIDs++;
    }
    pIndex = PIDToIndex(currentPID);
    if (pIndex == -1) {
	totals.unmappedPIDs++;
	return;
    }

    mappedPIDs[pIndex].count++;

    if ((pcVal - mappedPIDs[pIndex].text_offset) >= NUMB_BINS) {
	printf("warning pc val %llx out of range ignoring\n",
	       pcVal - mappedPIDs[pIndex].text_offset);
	return;
    }
    mappedPIDs[pIndex].bin[pcVal - mappedPIDs[pIndex].text_offset]++;
}

static void
processHWPerfMonEvent(const uval64 *event, uval sampleType)
{
    const uval PCSAMPLE_POS = 1;
    const uval LRSAMPLE_POS = 2;
    uval minorID;

    minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));
    if (minorID == TRACE_HWPERFMON_PERIODIC_ALL) {
	if (sampleType == PCSAMPLE) {
	    binPC(current_trace.PID, TRACE_SWAP64(event[PCSAMPLE_POS]));
	} else {
	    binPC(current_trace.PID, TRACE_SWAP64(event[LRSAMPLE_POS]));
	}
    }
}

static inline void
newLockChain(uval64 addr, uval64 spin, uval64 sTime, uval64 ch0,
	     uval64 ch1, uval64 ch2, LockInst **li)
{
    LockInst *tmp;

    tmp = (LockInst *)malloc(sizeof(LockInst));
    tmp->addr = addr;
    tmp->spin = spin;
    tmp->time = sTime;
    tmp->maxTime = sTime;
    tmp->pid = current_trace.PID;
    tmp->callChain[0] = ch0;
    tmp->callChain[1] = ch1;
    tmp->callChain[2] = ch2;
    tmp->initPC = 0;
    tmp->count = 1;
    tmp->next = NULL;
    tmp->lockChain = NULL;
    *li = tmp;
}

static inline void
newLockHash(uval bucket, uval64 addr, uval64 spin, uval64 sTime,
	    uval64 ch0, uval64 ch1, uval64 ch2)
{
    LockInst *tmp;

    tmp = (LockInst *)malloc(sizeof(LockInst));
    tmp->addr = addr;
    tmp->spin = spin;
    tmp->time = sTime;
    tmp->maxTime = sTime;
    tmp->pid = current_trace.PID;
    tmp->callChain[0] = ch0;
    tmp->callChain[1] = ch1;
    tmp->callChain[2] = ch2;
    tmp->initPC = 0;
    tmp->count = 1;
    tmp->next = lockHash[bucket];
    newLockChain(addr, spin, sTime, ch0, ch1, ch2, &(tmp->lockChain));
    lockHash[bucket] = tmp;
}

static inline void
updateLockChain(LockInst *bucketPtr, uval64 addr, uval64 spin, uval64 sTime,
		uval64 ch0, uval64 ch1, uval64 ch2)
{
    LockInst *tmp;

    tmp = bucketPtr->lockChain;

    while (1) {
	if ((tmp->addr == addr) && (tmp->callChain[0] == ch0) &&
	    (tmp->callChain[1] == ch1) && (tmp->callChain[2] == ch2)) {
	    tmp->count += 1;
	    tmp->spin += spin;
	    tmp->time += sTime;
	    if (sTime > tmp->maxTime) {
		tmp->maxTime = sTime;
	    }
	    return;
	}
	if (tmp->next == NULL) {
	    newLockChain(addr, spin, sTime, ch0, ch1, ch2, &(tmp->next));
	    return;
	} else {
	    tmp = tmp->next;
	}
    }
}

static void
processDbgEvent(const uval64 *event)
{
    uval minorID;
    uval64 pid, timeDiff;
    sval pIndex;

    pid = commID2PID(current_trace.COMMID);
    if (pid != 0) {
	printf("should only be from kernel for now\n");
	exit(-1);
    }

    pIndex = PIDToIndex(pid);

    minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));

    // FIXME need to add pairing logic
    if (minorID == TRACE_DBG_EVENT4_START) {
	genCount[pIndex].lastDbgTime = current_trace.timestamp;
    } else
    if (minorID == TRACE_DBG_EVENT4_END) {
	timeDiff = current_trace.timestamp - genCount[pIndex].lastDbgTime;
	genCount[pIndex].sumTimeDbg += timeDiff;
	genCount[pIndex].countTimeDbg += 1;
	if (timeDiff < genCount[pIndex].minTimeDbg) {
	    genCount[pIndex].minTimeDbg = timeDiff;
	}
	if (timeDiff > genCount[pIndex].maxTimeDbg) {
	    genCount[pIndex].maxTimeDbg = timeDiff;
	}
	genCount[pIndex].lastDbgTime = 0;
    } else
    if (minorID == TRACE_DBG_EVENT4_IMPULSE) {
	genCount[pIndex].sumCountDbg+=TRACE_SWAP64(event[3]);
	genCount[pIndex].countCountDbg++;
	genCount[pIndex].dbgCount[TRACE_SWAP64(event[3])] += 1;
    }
}

static void
processClustObjEvent(const uval64 *event)
{
    uval minorID;
    uval64 pid, timeDiff;
    sval pIndex;

    pid = commID2PID(current_trace.COMMID);
    pIndex = PIDToIndex(pid);

    if (pIndex == -1) {
	// printf("warning un-evaluated pid 0x%llx\n", pid);
	return;
    }

    minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));

    if (minorID == TRACE_CLUSTOBJ_MARK_SET) {
	if (genCount[pIndex].lastSetTime != 0) {
//	    printf("warning: double set for pid 0x%llx\n", pid);
	}
	genCount[pIndex].lastSetTime = current_trace.timestamp;
	genCount[pIndex].currentActive = 0;
    } else
    if (minorID == TRACE_CLUSTOBJ_MARK_ELAPSED) {
	timeDiff = current_trace.timestamp - genCount[pIndex].lastSetTime;
	genCount[pIndex].sumTimeElapsed += timeDiff;
	genCount[pIndex].countTimeElapsed += 1;
	if (timeDiff < genCount[pIndex].minTimeElapsed) {
	    genCount[pIndex].minTimeElapsed = timeDiff;
	}
	if (timeDiff > genCount[pIndex].maxTimeElapsed) {
	    genCount[pIndex].maxTimeElapsed = timeDiff;
	}

	genCount[pIndex].lastSetTime = 0;
    } else
    if (minorID == TRACE_CLUSTOBJ_MARK_ACTIVE) {
	timeDiff = current_trace.timestamp - genCount[pIndex].lastSetTime;

	genCount[pIndex].sumTimeActive += timeDiff;
	genCount[pIndex].countTimeActive += 1;
	if (timeDiff < genCount[pIndex].minTimeActive) {
	    genCount[pIndex].minTimeActive = timeDiff;
	}
	if (timeDiff > genCount[pIndex].maxTimeActive) {
	    genCount[pIndex].maxTimeActive = timeDiff;
	}
	genCount[pIndex].currentActive += 1;
	if (genCount[pIndex].currentActive >= MAX_ACTIVE_COUNT) {
	    genCount[pIndex].activeCount[0] += 1;
	} else {
	    genCount[pIndex].activeCount[genCount[pIndex].currentActive] += 1;
	}
    } else
    if (minorID == TRACE_CLUSTOBJ_GLOBAL_CNT) {
	if (genCount[pIndex].lastGenSetTime != 0) {
	    // i.e., we've alrady seen one
	    timeDiff = current_trace.timestamp
	    	       - genCount[pIndex].lastGenSetTime;

	    genCount[pIndex].sumTimeGen += timeDiff;
	    genCount[pIndex].countTimeGen += 1;
	    if (timeDiff < genCount[pIndex].minTimeGen) {
		genCount[pIndex].minTimeGen = timeDiff;
	    }
	    if (timeDiff > genCount[pIndex].maxTimeGen) {
		genCount[pIndex].maxTimeGen = timeDiff;
	    }
	}
	genCount[pIndex].lastGenSetTime = current_trace.timestamp;
    } else
    if (minorID == TRACE_CLUSTOBJ_SWAP_START) {
	if (genCount[pIndex].lastSetTime != 0) {
//	    printf("warning: double hot swap start for pid 0x%llx\n", pid);
	}
	genCount[pIndex].lastSwitchTime = current_trace.timestamp;
    } else
    if (minorID == TRACE_CLUSTOBJ_SWAP_DONE) {
	timeDiff = current_trace.timestamp - genCount[pIndex].lastSwitchTime;

	if (options.verboseMask & VERBOSE_L2) {
	    printf("hot swap diff %.9f time %.9f\n",
		   ticks2secs(timeDiff), ticks2secs(current_trace.timestamp));
	}
	genCount[pIndex].sumTimeSwitch += timeDiff;
	genCount[pIndex].countTimeSwitch += 1;

	if (timeDiff < genCount[pIndex].minTimeSwitch) {
	    genCount[pIndex].minTimeSwitch = timeDiff;
	}
	if (timeDiff > genCount[pIndex].maxTimeSwitch) {
	    genCount[pIndex].maxTimeSwitch = timeDiff;
	}
	genCount[pIndex].lastSwitchTime = 0;
    }
}

static void
processLockEvent(const uval64 *event)
{
    uval minorID;
    uval bucket;
    uval64 addr, ch0, ch1, ch2, spin, sTime;
    LockInst *tmp;

    minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));
    if ((minorID == TRACE_LOCK_CONTEND_SPIN) ||
	(minorID == TRACE_LOCK_CONTEND_BLOCK))
    {
	addr = TRACE_SWAP64(event[1]);
	spin = TRACE_SWAP64(event[2]);
	sTime = TRACE_SWAP64(event[3]);
	// heuristic to eliminate garbled first events when we started tracing
	// in k42 between lock start and lock end
	if (ticks2secs(sTime) > 1.0) {
	    printf("Warning: discarding lock event time %f secs, addr 0x%llx\n",
		   ticks2secs(sTime), addr);
	    return;
	}
	ch0 = TRACE_SWAP64(event[4]);
	ch1 = TRACE_SWAP64(event[5]);
	ch2 = TRACE_SWAP64(event[6]);
	bucket = hashFunc(ch0);
	if (lockHash[bucket] == NULL) {
	    newLockHash(bucket, addr, spin, sTime, ch0, ch1, ch2);
	} else {
	    // walk hash chain looking for element
	    tmp = lockHash[bucket];
	    while (tmp != NULL) {
		if (options.useLockAddr) {
		    if (options.callDepth == 1) {
			if ((tmp->addr == addr) && (tmp->callChain[0] == ch0)) {
			    tmp->count += 1;
			    tmp->spin += spin;
			    tmp->time += sTime;
			    if (sTime > tmp->maxTime) {
				tmp->maxTime = sTime;
			    }
			    updateLockChain(tmp, addr, spin, sTime,
					    ch0, ch1, ch2);
			    return;
			}
		    } else if (options.callDepth == 2) {
			if ((tmp->addr == addr) && (tmp->callChain[0] == ch0) &&
			    (tmp->callChain[1] == ch1)) {
			    tmp->count += 1;
			    tmp->spin += spin;
			    tmp->time += sTime;
			    if (sTime > tmp->maxTime) {
				tmp->maxTime = sTime;
			    }
			    updateLockChain(tmp, addr, spin, sTime,
					    ch0, ch1, ch2);
			    return;
			}
		    } else {
			if (options.callDepth != 3) {
			    printf("error: call depth out or range [1..3]\n");
			    exit(-1);
			}
			if ((tmp->addr == addr) && (tmp->callChain[0] == ch0) &&
			    (tmp->callChain[1] == ch1) &&
			    (tmp->callChain[2] == ch2)) {
			    tmp->count += 1;
			    tmp->spin += spin;
			    tmp->time += sTime;
			    if (sTime > tmp->maxTime) {
				tmp->maxTime = sTime;
			    }
			    updateLockChain(tmp, addr, spin, sTime,
					    ch0, ch1, ch2);
			    return;
			}
		    }
		} else {
		    if (options.callDepth == 1) {
			if (tmp->callChain[0] == ch0) {
			    tmp->count += 1;
			    tmp->spin += spin;
			    tmp->time += sTime;
			    if (sTime > tmp->maxTime) {
				tmp->maxTime = sTime;
			    }
			    updateLockChain(tmp, addr, spin, sTime,
					    ch0, ch1, ch2);
			    return;
			}
		    } else if (options.callDepth == 2) {
			if ((tmp->callChain[0] == ch0) &&
			    (tmp->callChain[1] == ch1)) {
			    tmp->count += 1;
			    tmp->spin += spin;
			    tmp->time += sTime;
			    if (sTime > tmp->maxTime) {
				tmp->maxTime = sTime;
			    }
			    updateLockChain(tmp, addr, spin, sTime,
					    ch0, ch1, ch2);
			    return;
			}
		    } else {
			if (options.callDepth != 3) {
			    printf("error: call depth out or range [1..3]\n");
			    exit(-1);
			}
			if ((tmp->callChain[0] == ch0) &&
			    (tmp->callChain[1] == ch1) &&
			    (tmp->callChain[2] == ch2)) {
			    tmp->count += 1;
			    tmp->spin += spin;
			    tmp->time += sTime;
			    if (sTime > tmp->maxTime) {
				tmp->maxTime = sTime;
			    }
			    updateLockChain(tmp, addr, spin, sTime,
					    ch0, ch1, ch2);
			    return;
			}
		    }
		}
		tmp = tmp->next;
	    }
	    newLockHash(bucket, addr, spin, sTime, ch0, ch1, ch2);
	}
    }
}

static void
processExceptionEvent(const uval64 *event)
{
    uval minorID, i;

    minorID = TRACE_DATA_GET(TRACE_SWAP64(event[0]));

    if ((minorID == TRACE_EXCEPTION_PROCESS_YIELD) ||
	(minorID == TRACE_EXCEPTION_PPC_CALL) ||
	(minorID == TRACE_EXCEPTION_PPC_RETURN) ||
	(minorID == TRACE_EXCEPTION_IPC_REFUSED) ||
	(minorID == TRACE_EXCEPTION_PGFLT_DONE) ||
	(minorID == TRACE_EXCEPTION_AWAIT_DISPATCH_DONE) ||
	(minorID == TRACE_EXCEPTION_PPC_ASYNC_REMOTE_DONE) ||
	(minorID == TRACE_EXCEPTION_AWAIT_PPC_RETRY_DONE) ||
	(minorID == TRACE_EXCEPTION_IPC_REMOTE_DONE) ||
	(minorID == TRACE_EXCEPTION_PGFLT) ||
	(minorID == TRACE_EXCEPTION_AWAIT_DISPATCH) ||
	(minorID == TRACE_EXCEPTION_PPC_ASYNC_REMOTE) ||
	(minorID == TRACE_EXCEPTION_AWAIT_PPC_RETRY) ||
	(minorID == TRACE_EXCEPTION_IPC_REMOTE))
    {
	if (current_trace.PID == 0) {
	    if (current_trace.COMMID == current_trace.physProc) {
		current_trace.kernelTime += current_trace.timestamp
					- current_trace.lastContextSwitchTime;
                for (i=0; i<options.numPairStats; i++) {
	            if (pairStats[i].active) {
                        pairStats[i].kernelTime += current_trace.timestamp
					  - current_trace.lastContextSwitchTime;
                    }
                }
	    } else {
		current_trace.idleTime += current_trace.timestamp
					- current_trace.lastContextSwitchTime;

                for (i=0; i<options.numPairStats; i++) {
	            if (pairStats[i].active) {
                        pairStats[i].idleTime += current_trace.timestamp
					  - current_trace.lastContextSwitchTime;
                    }
                }
	    }
	}
	current_trace.lastContextSwitchTime = current_trace.timestamp;
        if ((minorID == TRACE_EXCEPTION_PGFLT) ||
	    (minorID == TRACE_EXCEPTION_AWAIT_DISPATCH) ||
	    (minorID == TRACE_EXCEPTION_PPC_ASYNC_REMOTE) ||
	    (minorID == TRACE_EXCEPTION_AWAIT_PPC_RETRY) ||
	    (minorID == TRACE_EXCEPTION_IPC_REMOTE)) {
	    /* implicit switch to kernel */
            current_trace.COMMID = current_trace.physProc;
	    current_trace.PID = 0;
        } else {
            /* explicit switch to specific CommID */
	    current_trace.COMMID = TRACE_SWAP64(event[1]);
	    current_trace.PID = commID2PID(TRACE_SWAP64(event[1]));
        }
    } else if (minorID == TRACE_EXCEPTION_TIMER_INTERRUPT) {
	if (options.eventType == EVENT_TIMER) {
	    binPC(current_trace.PID, TRACE_SWAP64(event[1]));
	}
    }
}

static int
compareSymFreqs(const void *sf1, const void *sf2)
{
    if (((struct SymFreq *)sf1)->count > ((struct SymFreq *)sf2)->count) {
    	return -1;
    } else {
    	return 1;
    }
}

static void
pcsToMethod(uval pIndex)
{
    uval i, index, count, pos = 0, sum = 0;
    uval64 text_off;
    uval64 *symAddrs;
    char filename[FILENAME_SIZE];
    struct SymFreq *symFreqs;
    FILE *fp;

    if (pIndex > mappedPIDCount || mappedPIDs[pIndex].symAddrCount == 0) {
    	return;
    }

    symAddrs = mappedPIDs[pIndex].symAddrs;
    text_off = mappedPIDs[pIndex].text_offset;

    symFreqs = (struct SymFreq *)malloc(sizeof(struct SymFreq)
					* mappedPIDs[pIndex].symAddrCount);
    assert(symFreqs != NULL);

    for (i = 1; i < mappedPIDs[pIndex].symAddrCount; i++) {
	if (symAddrs[i] - text_off >= NUMB_BINS) {
	    break;
	}

	count = 0;
	for (index = symAddrs[i - 1] - text_off;
	     index < symAddrs[i] - text_off; index++) {
	    if (mappedPIDs[pIndex].bin[index] > 0) {
	    	count += mappedPIDs[pIndex].bin[index];
	    }
	}
	if (count > 0) {
	    symFreqs[pos].count = count;
	    symFreqs[pos].symNum = i - 1;
	    pos++;
	    sum += count;
	}
    }

    /* free remaining mem by reallocing down */
    realloc(symFreqs, sizeof(struct SymFreq) * pos);

    qsort(symFreqs, pos, sizeof(struct SymFreq), compareSymFreqs);

    mappedPIDs[pIndex].symFreqs = symFreqs;
    mappedPIDs[pIndex].symFreqCount = pos;

    snprintf(filename, FILENAME_SIZE, "%s0x%llx", OUT_FILE,
    	     mappedPIDs[pIndex].PID);
    fp = fopen(filename, "w");
    if (fp == NULL) {
	printf("Error: failed to open %s for writing\n", filename);
	exit(-1);
    }

    for (i = 0; i < pos; i++) {
	fprintf(fp, "%7ld SYM:%llx:%llx", symFreqs[i].count,
		mappedPIDs[pIndex].PID, symAddrs[symFreqs[i].symNum]);
	for (index = symAddrs[symFreqs[i].symNum] - text_off;
	     index < symAddrs[symFreqs[i].symNum + 1] - text_off; index++) {
	    if (mappedPIDs[pIndex].bin[index] > 0) {
		fprintf(fp, " 0x%llx,%d", index + text_off,
			mappedPIDs[pIndex].bin[index]);
	    }
	}
	fprintf(fp, "\n");
    }

    fclose(fp);

    printf(" for pid 0x%llx sum %lu expected %lu\n", mappedPIDs[pIndex].PID,
	   sum, mappedPIDs[pIndex].count);
}

static void
getSymbols(int index, const char *pathname)
{
    char filename[FILENAME_SIZE], command[COMMAND_SIZE], addrBuf[17];
    uval64 pid, addr;
    uval64 *symAddrs;
    uval pos = 0, arrLen = 1024; // initial length
    char type;
    char *endptr;
    FILE *fd;
    int i;

    pid = mappedPIDs[index].PID;
    if (pid == INVALID_PID) {
	return;
    }

    // check to see if we already know about this file
    for (i = 0; i < index; i++) {
	if (strcmp(mappedPIDs[index].filename, mappedPIDs[i].filename) == 0) {
	    mappedPIDs[index].symAddrs = mappedPIDs[i].symAddrs;
	    mappedPIDs[index].symAddrCount = mappedPIDs[i].symAddrCount;

	    if (options.verboseMask & VERBOSE_L1) {
		printf("using the same symbols for PID 0x%llx as PID 0x%llx\n",
		       pid, mappedPIDs[i].PID);
	    }

	    return;
	}
    }

    if (strcmp(pathname, ".") == 0 || mappedPIDs[index].filename[0] == '/') {
	strncpy(filename, mappedPIDs[index].filename, FILENAME_SIZE);
    } else {
	snprintf(filename, FILENAME_SIZE, "%s/%s", pathname,
		 mappedPIDs[index].filename);
    }

    if (access(filename, R_OK) != 0) {
	printf("Error: failed to open %s for PID 0x%llx\n", filename, pid);
	exit(-1);
    }

    if (options.verboseMask & VERBOSE_L1) {
	printf("reading symbols for PID 0x%llx from %s\n", pid, filename);
    }

    snprintf(command, COMMAND_SIZE, "powerpc64-linux-nm -n %s", filename);
    fd = popen(command, "r");
    if (fd == NULL) {
    	printf("Error: couldn't popen %s\n", command);
	exit(-1);
    }

    symAddrs = (uval64 *)malloc(arrLen * sizeof(uval64));
    assert(symAddrs != NULL);

    while (fscanf(fd, "%16c %c %*s%*[\n]", addrBuf, &type) == 2) {
    	if (!(type == 't' || type == 'T' || type == 'W')) {
	    continue; // we're not interested in the other symbols
	}

	// parse the address
	addrBuf[16] = '\0';
	addr = strtoull(addrBuf, &endptr, 16);
	if (endptr != &addrBuf[16]) {
	    printf("Error: couldn't parse '%s' in nm output\n", addrBuf);
	    continue;
	}

	symAddrs[pos++] = addr;
	if (pos == arrLen) {
	    // we need to grow the array, double it
	    arrLen *= 2;
	    symAddrs = (uval64 *)realloc(symAddrs, arrLen * sizeof(uval64));
	    assert(symAddrs != NULL);
	}
    }

    mappedPIDs[index].symAddrs = symAddrs;
    mappedPIDs[index].symAddrCount = pos;

    pclose(fd);

    if (options.verboseMask & VERBOSE_L1) {
	printf("...done, got %lu symbols\n", pos);
    }
}

static void
printResultsAll(FILE *lfp)
{
    uval index;
    uval sum=0;
    uval skipped=0;
    sval interesting;

    fprintf(lfp, "overall result counts for how many hits each pid received\n");
    fprintf(lfp, "first all active pids then mapped ones\n");
    fprintf(lfp, "\n  count   pid\n");
    interesting = totals.exEvents/100;
    for (index=0; index<MAX_LOW_PIDS; index++) {
	if (lowPIDCount[index] >= interesting) {
	    fprintf(lfp, "%7ld   0x%lx\n", lowPIDCount[index], index);
	    sum+=lowPIDCount[index];
	} else {
	    skipped+=lowPIDCount[index];
	}
    }

    if (totals.highPIDs) {
	fprintf(lfp, "%7ld   >0x%x\n", totals.highPIDs, MAX_LOW_PIDS);
    }

    if (skipped) {
	fprintf(lfp, "%7ld   in pids with fewer than %ld hits each\n",
	       skipped, interesting);
    }

    fprintf(lfp, "\n%7ld   total hits\n", totals.exEvents);

    if ((sum+skipped+totals.highPIDs) != totals.exEvents) {
	fprintf(lfp, "\n!!!!! %ld missing events\n",
	       totals.exEvents-(sum+skipped+totals.highPIDs));
    }

    sum = 0;

    fprintf(lfp, "\n  count   pid  mapped file name\n");
    for (index=0; index<mappedPIDCount; index++) {
	fprintf(lfp, "%7ld   0x%llx     %s\n", mappedPIDs[index].count,
	        mappedPIDs[index].PID, mappedPIDs[index].filename);
	sum += mappedPIDs[index].count;
    }

    fprintf(lfp, "%7ld   in unmapped pids\n",  totals.unmappedPIDs);

    if ((sum + totals.unmappedPIDs) != totals.exEvents) {
	fprintf(lfp, "\n!!!!! %ld missing events\n",
	        totals.exEvents - (sum + totals.unmappedPIDs));
    }
}

static void
printResultsPID(uval index, FILE *lfp)
{
    struct SymFreq symFreq;
    uval i;

    fprintf(lfp, "\n\nhistogram for pid 0x%llx mapped filename %s\n",
	    mappedPIDs[index].PID, mappedPIDs[index].filename);
    fprintf(lfp, "  top %ld values - for full list see %s0x%llx\n\n",
	    options.numbToDisplay, OUT_FILE, mappedPIDs[index].PID);
    fprintf(lfp, "  count   method\n");

    for (i = 0; i < mappedPIDs[index].symFreqCount; i++) {
    	symFreq = mappedPIDs[index].symFreqs[i];
	fprintf(lfp, "%7ld   SYM:%llx:%llx\n", symFreq.count,
		mappedPIDs[index].PID,
		mappedPIDs[index].symAddrs[symFreq.symNum]);

	if (i >= options.numbToDisplay) {
	    break;
	}
    }
}

static void
printLockChain(LockInst *li, FILE *fp)
{
    LockInst *tmp;
    uval count = 0;
    char time1[64], time2[64];

    fprintf(fp,"\n");

    for (tmp = li->lockChain; tmp != NULL; tmp = tmp->next) {
	if (options.lockChainCount) {
	    count++;
	} else {
	    printTime(tmp->time, time1, sizeof(time1));
	    printTime(tmp->maxTime, time2, sizeof(time2));
	    fprintf(fp, "                                  ");
	    fprintf(fp, "%s,%ld,%lld,%s,0x%llx\n", time1, tmp->count,
		    tmp->spin, time2, tmp->addr);
	    fprintf(fp, "                                       ");
	    fprintf(fp,"SYM:%llx:%llx\n", li->pid, li->callChain[0]);
	    fprintf(fp, "                                       ");
	    fprintf(fp,"SYM:%llx:%llx\n", li->pid, li->callChain[1]);
	    fprintf(fp, "                                       ");
	    fprintf(fp,"SYM:%llx:%llx\n", li->pid, li->callChain[2]);
	}
    }
    if (options.lockChainCount) {
	fprintf(fp, "                                       ");
	fprintf(fp, "%ld instances\n\n", count);
    }
}

enum lockType {BYTIME, BYCOUNT, BYMAXTIME};

struct LockStat {
    uval64 stat;
    struct LockInst *li;
};

static int
compareLockStats(const void *ls1, const void *ls2)
{
    if (((struct LockStat *)ls1)->stat > ((struct LockStat *)ls2)->stat) {
    	return -1;
    } else {
    	return 1;
    }
}

static void
printLockStats(enum lockType type, FILE *lfp)
{
    uval index, printCount = 0, pos = 0, arrLen = 128; // initial length
    struct LockStat *lockStats;
    LockInst *li;
    char time1[64], time2[64];
    char *sfilename, *by;
    FILE *fp, *fpl;

    if (type == BYTIME) {
	sfilename = LOCK_STATS_TIME_FILE;
	by = "time";
    } else if (type == BYMAXTIME) {
	sfilename = LOCK_STATS_MAXTIME_FILE;
	by = "max time";
    } else if (type == BYCOUNT) {
	sfilename = LOCK_STATS_COUNT_FILE;
	by = "count";
    } else {
	printf("Error: printLockStats: unknown type\n");
	exit(-1);
    }

    fpl = fopen(sfilename, "w");
    if (fpl == NULL) {
	printf("Error: failed to open %s for write\n", sfilename);
	exit(-1);
    }

    for (fp = lfp; true; fp = fpl) {
	fprintf(fp, "\n\nTop contended locks by %s\n", by);
	if (fp == lfp) {
	    fprintf(fp, "  top %ld contended locks - for full list see %s\n\n",
		    options.numbToDisplay, sfilename);
	}
	fprintf(fp, "       time   count     spin     max time  pid");
	if (options.useLockAddr) {
	    fprintf(fp, "   lock addr");
	}
	fprintf(fp, "\n");
	fprintf(fp, "                                       call chain\n\n");
	if (fp == fpl) break;
    }

    lockStats = (struct LockStat *)malloc(arrLen * sizeof(struct LockStat));
    assert(lockStats != NULL);

    for (index = 0; index < HASH_BUCKETS; index++) {
	for (li = lockHash[index]; li != NULL; li = li->next) {
	    lockStats[pos].li = li;
	    if (type == BYTIME) {
		lockStats[pos].stat = li->time;
	    } else if (type == BYMAXTIME) {
		lockStats[pos].stat = li->maxTime;
	    } else if (type == BYCOUNT) {
		lockStats[pos].stat = li->count;
	    }

	    if (++pos == arrLen) {
		// we need to grow the array, double it
		arrLen *= 2;
		lockStats = (struct LockStat *)realloc(lockStats, arrLen *
						       sizeof(struct LockStat));
		assert(lockStats != NULL);
	    }
	}
    }

    qsort(lockStats, pos, sizeof(struct LockStat), compareLockStats);

    for (index = 0; index < pos; index++) {
    	li = lockStats[index].li;

	printTime(li->time, time1, sizeof(time1));
	printTime(li->maxTime, time2, sizeof(time2));

    	for (fp = lfp; true; fp = fpl) {
	    if (fp == lfp && printCount++ > options.numbToDisplay) {
		continue;
	    }

	    fprintf(fp, "%11s %7ld %8lld  %11s  0x%llx", time1, li->count,
		    li->spin, time2, li->pid);
	    if (options.useLockAddr) {
		fprintf(fp, " 0x%llx", li->addr);
	    }
	    fprintf(fp, "\n");

	    fprintf(fp,"                                       ");
	    fprintf(fp,"SYM:%llx:%llx\n", li->pid, li->callChain[0]);
	    if (options.callDepth >= 2) {
		fprintf(fp,"                                       ");
		fprintf(fp,"SYM:%llx:%llx\n", li->pid, li->callChain[1]);
	    }
	    if (options.callDepth >= 3) {
		fprintf(fp,"                                       ");
		fprintf(fp,"SYM:%llx:%llx\n", li->pid, li->callChain[1]);
	    }
	    if (li->lockChain->next != NULL) {
		printLockChain(li, fp);
	    }
	    if (fp == fpl) break;
	}
    }

    free(lockStats);
    fclose(fpl);
}

static void
printCountDbgResults()
{

    sval index, i;

    //for (index=0; index<MAX_PIDS; index++) {
    for (index=0; index<1; index++) {

	if (mappedPIDs[index].PID != INVALID_PID) {

	    printf("\nNumber events pairs:\n");
	    printf("  Number: %ld\n", genCount[index].countTimeDbg);
	    printf("  Total time: %.9f\n",
		   ticks2secsf((float)(genCount[index].sumTimeDbg)));
	    printf("  Average time: %.9f\n",
		   ticks2secsf((float)(genCount[index].sumTimeDbg)/
			      (float)(genCount[index].countTimeDbg)));
	    printf("  Min time: %.9f\n",
		   ticks2secs(genCount[index].minTimeDbg));
	    printf("  Max time: %.9f\n",
		   ticks2secs(genCount[index].maxTimeDbg));

	    printf("  Histogram for count\n");
	    for (i=1; i<MAX_REGION_COUNT; i++) {
		printf("    %ld: %ld\n", i, genCount[index].dbgCount[i]);
	    }
	    printf("   >%ld: %ld\n", (uval)MAX_REGION_COUNT,
		   genCount[index].dbgCount[0]);
	}
    }
}

static void
printGenCountResults()
{
    sval index, i;

    //for (index=0; index<MAX_PIDS; index++) {
    for (index=0; index<1; index++) {
	if (mappedPIDs[index].PID != INVALID_PID) {
	    printf("ticks per second %lld \n\n", current_trace.ticksPerSecond);
	    printf("Gen Count results for pid 0x%llx\n", mappedPIDs[index].PID);

	    printf("\nGeneration count advances:\n");
	    printf("  Number: %ld\n", genCount[index].countTimeGen);
	    printf("  Average time: %.9f\n",
		   ticks2secsf((float)(genCount[index].sumTimeGen)/
			      (float)(genCount[index].countTimeGen)));
	    printf("  Min time: %.9f\n",
		   ticks2secs(genCount[index].minTimeGen));
	    printf("  Max time: %.9f\n",
		   ticks2secs(genCount[index].maxTimeGen));

	    printf("\nNumber of elapsed %ld\n",
		   genCount[index].countTimeElapsed);
	    printf("  Ave time: %.9f\n",
		   ticks2secsf((float)(genCount[index].sumTimeElapsed)/
			       (float)(genCount[index].countTimeElapsed)));
	    printf("  Min time: %.9f\n",
		   ticks2secs(genCount[index].minTimeElapsed));
	    printf("  Max time: %.9f\n",
		   ticks2secs(genCount[index].maxTimeElapsed));

	    printf("\nNumber of active %ld\n",
		   genCount[index].countTimeActive);
	    printf("  Ave time: %.9f\n",
		   ticks2secsf((float)(genCount[index].sumTimeActive)/
			       (float)(genCount[index].countTimeActive)));
	    printf("  Min time: %.9f\n",
		   ticks2secs(genCount[index].minTimeActive));
	    printf("  Max time: %.9f\n",
		   ticks2secs(genCount[index].maxTimeActive));

	    printf("  Number of consecutive active:\n");
	    for (i=1; i<MAX_ACTIVE_COUNT; i++) {
		printf("    %ld: %ld\n", i, genCount[index].activeCount[i]);
	    }
	    printf("   >%ld: %ld\n", (uval)MAX_ACTIVE_COUNT,
		   genCount[index].activeCount[0]);
	}
    }
}

static void
printPairResults()
{
    uval i;
    double totalTime = 0.0;

    printf("\nPair Results:\n\n");
    for (i=0; i<options.numPairStats; i++) {
	printf(" pair [%s, %s]\ntotal time %f\n kernel time %f\n",
               traceUnified[pairStats[i].majorID].traceEventParse
                   [pairStats[i].startMinorID].eventString,
               traceUnified[pairStats[i].majorID].traceEventParse
                   [pairStats[i].stopMinorID].eventString,
	       ticks2secs(pairStats[i].sumTime),
	       ticks2secs(pairStats[i].kernelTime));
	printf(" idle time %f count %ld", 
               ticks2secs(pairStats[i].idleTime), pairStats[i].count);

	if (pairStats[i].count > 0) {
	    printf(" min %f max %f\n", ticks2secs(pairStats[i].min),
	       ticks2secs(pairStats[i].max));
	} else {
	    printf("\n");
	}
	totalTime += ticks2secs(pairStats[i].sumTime);
    }
    printf("\n  Sum all Time %f\n", totalTime);
    printf("\n");
}

static void
updatePairStats(uval majorID, uval minorID, uval timestamp)
{
    uval i;
    uval timeVal;

    for (i=0; i<options.numPairStats; i++) {
	if (majorID == pairStats[i].majorID) {
	    if (minorID == pairStats[i].startMinorID) {
		if (pairStats[i].startTime != 0) {
		    printf("error: repeat start event ignoring\n");
		} else {
		    pairStats[i].startTime = timestamp;
                    pairStats[i].active = true;
		}
	    } else if (minorID == pairStats[i].stopMinorID) {
		if (pairStats[i].startTime == 0) {
		    printf("error: bare stop event ignoring\n");
		} else {
		    if (timestamp < pairStats[i].startTime) {
			// timer wrapped
			timeVal = (MAX_TIMESTAMP-pairStats[i].startTime) + 
		            timestamp;
		    } else {
			timeVal = (timestamp-pairStats[i].startTime);
		    }
		    pairStats[i].sumTime += timeVal;
		    if (pairStats[i].min > timeVal) {
			pairStats[i].min = timeVal;
		    }
		    if (pairStats[i].max < timeVal) {
			pairStats[i].max = timeVal;
		    }
		    pairStats[i].count += 1;
		    pairStats[i].startTime = 0;
                    pairStats[i].active = false;
		}
	    }
	}
    }
}

static int
processTraceEvent(const traceFileHeaderV3 *headerInfo, const uval64 *event,
		  uval len, bool byteSwap, bool first, bool last, bool verbose)
{
    uval64 first_word = TRACE_SWAP64(*event);
    uval majorID, minorID, timestamp;

    if (first) {
	current_trace.physProc = TRACE_SWAP64(headerInfo->physProc);
	current_trace.ticksPerSecond = TRACE_SWAP64(headerInfo->ticksPerSecond);
	current_trace.kernelTime = 0;
	current_trace.idleTime = 0;
	current_trace.lastContextSwitchTime = 0;
	current_trace.numbEvents = 0;
	current_trace.lastTime = TRACE_TIMESTAMP_GET(first_word);
	current_trace.wrapAdjust = (-1) * current_trace.lastTime;
	current_trace.COMMID = (uval64)-1;
	current_trace.PID = commID2PID(current_trace.COMMID);
    }

    current_trace.numbEvents++;

    timestamp = TRACE_TIMESTAMP_GET(first_word);
    if (timestamp < current_trace.lastTime) {
	current_trace.wrapAdjust += ((uval64)1)<<(TRACE_TIMESTAMP_BITS);
    }
    current_trace.lastTime = timestamp;
    current_trace.timestamp = timestamp + current_trace.wrapAdjust;

    majorID = TRACE_MAJOR_ID_GET(first_word);
    minorID = TRACE_DATA_GET(first_word);

    if (options.pairStatsFlag) {
	updatePairStats(majorID, minorID, timestamp);
    }

    switch (majorID) {
    case TRACE_EXCEPTION_MAJOR_ID:
	processExceptionEvent(event);
	break;
    case TRACE_HWPERFMON_MAJOR_ID:
	if (options.eventType == EVENT_HWPERF) {
	    processHWPerfMonEvent(event, options.sampleType);
	}
	break;
    case TRACE_LOCK_MAJOR_ID:
	if (options.lockStats) {
	    processLockEvent(event);
	}
	break;
    case TRACE_CLUSTOBJ_MAJOR_ID:
	if (options.genCountFlag) {
	    processClustObjEvent(event);
	}
	break;
    case TRACE_DBG_MAJOR_ID:
	if (options.countDbgFlag) {
	    processDbgEvent(event);
	}
	break;
    default:
	break;
    }

    if (last) {
	totals.allEvents += current_trace.numbEvents;
	totals.time += ticks2secs(current_trace.timestamp);

	if (options.verboseMask & VERBOSE_L1) {
	    printf("  read %ld events spanning a time period of %f seconds\n",
	      	   current_trace.numbEvents,
		   ticks2secs(current_trace.timestamp));
	}
	printf("  kernel time: %f seconds\n",
	       ticks2secs(current_trace.kernelTime));
	printf("  idle time:   %f seconds\n",
	       ticks2secs(current_trace.idleTime));
    }

    return 0;
}

int
main(int argc, char **argv)
{
#ifdef K42
    NativeProcess();
#endif

    char srcFilename[MAX_FILES][FILENAME_SIZE];
    bool fileSpecified[MAX_FILES];
    char *baseFilename = "trace-out", *pathname = ".";
    uval currentPairStatsMajor;
    bool pairStatsOnly = false, live = false;
    uval i, numbProcs = 1, proc;
    uval64 pid, text_offset;
    int index, rc;

    init_globals();

    // setup defaults
    options.verboseMask = VERBOSE_L1;
    options.eventType = EVENT_TIMER;
    options.timeType = TIME_SECS;
    options.noSym = false;
    options.lockStats = true;
    options.kmonFlag = false;
    options.genCountFlag = false;
    options.countDbgFlag = false;
    options.overrideGarbled = false;
    options.pairStatsFlag = false;
    options.useLockAddr = false;
    options.lockChainCount = false;
    options.callDepth = MAX_CALLDEPTH;
    options.numPairStats = 0;
    options.numbToDisplay = 10;

    // FIXME eventually we should pull this from the headers of the file
    addMappedPID(0, KERN_TEXT_OFFSET, "boot_image.dbg");
    addMappedPID(1, PROC_TEXT_OFFSET, "servers/baseServers/baseServers.dbg");
    addMappedPID(2, PROC_TEXT_OFFSET, "servers/baseServers/baseServers.dbg");

    for (i = 0; i < MAX_FILES; i++) {
	fileSpecified[i] = false;
    }

    for (index = 1; index < argc; index++) {
	if (strcmp(argv[index], "--callDepth") == 0) {
	    options.callDepth = (uval)strtol(argv[++index], NULL, 0);
	    if (options.callDepth < 1 || options.callDepth > MAX_CALLDEPTH) {
		printf("error: call depth out of range [1..%d]\n",
		       MAX_CALLDEPTH);
		exit(-1);
	    }
	} else
	if (strcmp(argv[index], "--countDbg") == 0) {
	    options.countDbgFlag = true;
	} else
	if (strcmp(argv[index], "--event") == 0) {
	    if (strcmp(argv[++index], "timer") == 0) {
	    	options.eventType = EVENT_TIMER;
	    } else if (strcmp(argv[index], "hwperf") == 0) {
	    	options.eventType = EVENT_HWPERF;
	    } else {
		printf("error unknown event type %s\n", argv[index]);
		exit(-1);
	    }
	} else
	if (strcmp(argv[index], "--file") == 0) {
	    strcpy(srcFilename[0], argv[++index]);
	    fileSpecified[0] = true;
	} else
	if (strcmp(argv[index], "--genCount") == 0) {
	    options.genCountFlag = true;
	} else
	if (strcmp(argv[index], "--help") == 0) {
	    print_usage();
	    exit(0);
	} else
	if (strcmp(argv[index], "--kmon") == 0) {
	    options.kmonFlag = true;
	} else
	if (strcmp(argv[index], "--lockChainCount") == 0) {
	    options.lockChainCount = true;
	} else
	if (strcmp(argv[index], "--lockStats") == 0) {
	    options.lockStats = false;
	} else
	if (strcmp(argv[index], "--maj") == 0) {
	    sscanf(argv[++index], "%ld", &currentPairStatsMajor);
	} else
	if (strcmp(argv[index], "--map") == 0) {
	    pid = strtoull(argv[++index], NULL, 0);
	    // FIXME eventually we should pull this from the headers of the file
	    text_offset = (pid == 0) ? KERN_TEXT_OFFSET : PROC_TEXT_OFFSET;
	    addMappedPID(pid, text_offset, argv[++index]);
	} else
	if (strcmp(argv[index], "--min") == 0) {
	    sscanf(argv[++index], "%ld",
	    	   &(pairStats[options.numPairStats].startMinorID));
	    sscanf(argv[++index], "%ld",
	    	   &(pairStats[options.numPairStats].stopMinorID));
	    pairStats[options.numPairStats].majorID = currentPairStatsMajor;
	    if (++options.numPairStats >= MAX_MINOR_PAIRS) {
		printf("error: too many minor pairs FIXME - realloc\n");
		exit(-1);
	    }
	} else
	if (strcmp(argv[index], "--mp") == 0) {
	    sscanf(argv[++index], "%ld", &numbProcs);
	} else
	if (strcmp(argv[index], "--mpBase") == 0) {
	    baseFilename = argv[++index];
	} else
	if (strcmp(argv[index], "--mpFile") == 0) {
	    sscanf(argv[++index], "%ld", &proc);
	    strcpy(srcFilename[proc], argv[++index]);
	    fileSpecified[proc] = true;
	} else
	if (strcmp(argv[index], "--noSym") == 0) {
	    options.noSym = true;
	} else
	if (strcmp(argv[index], "--numb") == 0) {
	    sscanf(argv[++index], "%ld", &options.numbToDisplay);
	} else
	if (strcmp(argv[index], "--overrideGarbled") == 0) {
	    options.overrideGarbled = 1;
	} else
	if (strcmp(argv[index], "--pairStats") == 0) {
	    options.pairStatsFlag = true;
	} else
	if (strcmp(argv[index], "--pairStatsOnly") == 0) {
	    options.pairStatsFlag = true;
	    pairStatsOnly = true;
	} else
	if (strcmp(argv[index], "--path") == 0) {
	    pathname = argv[++index];
	} else
	if (strcmp(argv[index], "--sample") == 0) {
	    if (strcmp(argv[++index], "lr") == 0) {
		options.sampleType = LRSAMPLE;
	    } else {
		options.sampleType = PCSAMPLE;
	    }
	} else
	if (strcmp(argv[index], "--time") == 0) {
	    if (strcmp(argv[++index], "secs") == 0) {
	    	options.timeType = TIME_SECS;
	    } else if (strcmp(argv[index], "ticks") == 0) {
	    	options.timeType = TIME_TICKS;
	    } else {
		printf("error unknown time type\n");
		exit(-1);
	    }
	} else
	if (strcmp(argv[index], "--useLockAddr") == 0) {
	    options.useLockAddr = (uval)strtol(argv[++index], NULL, 0);
	    if (! (options.useLockAddr == 0 || options.useLockAddr == 1)) {
		printf("error: invalid useLockAddr value\n");
		exit(-1);
	    }
	} else
	if (strcmp(argv[index], "--verbose") == 0) {
	    options.verboseMask = (uval64)strtol(argv[++index], NULL, 0);
	} else
	if (strcmp(argv[index], "--live") == 0) {
#ifdef K42
	    live = true;
#else
	    printf("Error: --live not valid for this version of the tool\n");
	    return -1;
#endif
	} else {
	    printf("error unknown option %s\n", argv[index]);
	    print_usage();
	    exit(-1);
	}
    }

    if (options.kmonFlag) {
	kmonExecFP = fopen(KMON_EXEC_FILE, "w");
	if (kmonExecFP == NULL) {
	    printf("error: failed to open %s for writing\n", KMON_EXEC_FILE);
	    exit(-1);
	}
	kmonLockFP = fopen(KMON_LOCK_FILE, "w");
	if (kmonLockFP == NULL) {
	    printf("error: failed to open %s for writing\n", KMON_LOCK_FILE);
	    exit(-1);
	}
    }

    dumpMappedPIDs();

#if K42
    if (live) {
	numbProcs = DREFGOBJ(TheProcessRef)->ppCount();
    }
#endif

    for (i = 0; i < numbProcs; i++) {
	if (!fileSpecified[i]) {
	    sprintf(srcFilename[i], "%s.%ld.trc", baseFilename, i);
	}
    }

    if (options.eventType == EVENT_TIMER) {
	options.sampleType = PCSAMPLE;
    }

    if (!options.genCountFlag && !options.countDbgFlag) {
	for (i = 0; i < mappedPIDCount; i++) {
	    getSymbols(i, pathname);
	}
    }

    if (live) {
	rc = processTraceBuffers(processTraceEvent,
				 (options.verboseMask & VERBOSE_L1) != 0);
    } else {
	for (proc = 0; proc < numbProcs; proc++) {
	    rc = processTraceFile(srcFilename[proc], processTraceEvent,
				  &byteSwap,
				  (options.verboseMask & VERBOSE_L1) != 0);
	    if (rc != 0 && !options.overrideGarbled) {
		printf("Error encountered. Exiting, see --overrideGarbled\n");
		return rc;
	    }
	}
    }

    if ((! options.genCountFlag) && (! options.countDbgFlag)) {
	printf("matching pc values to methods\n");
	for (i=0; i<mappedPIDCount; i++) {
	    pcsToMethod(i);
	}
	printf(" done\n");
    }

    printf("\n                    ========================================\n");
    printf("\nFinished processing\n");
    printf("  read %lld events spanning a time period of %f seconds\n\n",
	   totals.allEvents, totals.time);
    if (options.kmonFlag) {
	fprintf(kmonExecFP, "read %lld events spanning a time period of %f "
	    	"seconds\n\n", totals.allEvents, totals.time);
    }

    if (options.pairStatsFlag) {
	printPairResults();
	if (pairStatsOnly) {
	    return 0;
	}
    }

    if (options.genCountFlag) {
	printGenCountResults();
	return 0;
    }

    if (options.countDbgFlag) {
	printCountDbgResults();
	return 0;
    }

    printResultsAll(stdout);
    if (options.kmonFlag) {
    	printResultsAll(kmonExecFP);
    }

    for (i = 0; i < mappedPIDCount; i++) {
	printResultsPID(i, stdout);
	if (options.kmonFlag) {
	    printResultsPID(i, kmonExecFP);
	}
    }

    if (options.kmonFlag) {
    	fclose(kmonExecFP);
    }

    if (options.lockStats) {
	printLockStats(BYTIME, stdout);
	printLockStats(BYCOUNT, stdout);
	printLockStats(BYMAXTIME, stdout);
	if (options.kmonFlag) {
	    printLockStats(BYTIME, kmonLockFP);
	    printLockStats(BYCOUNT, kmonLockFP);
	    printLockStats(BYMAXTIME, kmonLockFP);
	    fclose(kmonLockFP);
	}
    } else if (options.kmonFlag) {
	fprintf(kmonLockFP, "lock stats not run\n");
	fclose(kmonLockFP);
    }

    return 0;
}

static void
print_usage()
{
    printf("traceProfile [--help]\n");
    printf("             [--file filename] [--live] [--path pathname]\n");
    printf("             [--numb numb] .. [--event timer|hwperf]\n");
    printf("             [--map N filename] [--sample pc|lr]\n");
    printf("             [--lockStats] [--mp N] [--mpBase filename]\n");
    printf("             [--pairStats | --pairStatsOnly]\n"); 
    printf("                          [[[--maj N]] [--min start stop]]*1 \n");
    printf("             [--mpFile N filename]\n");
    printf("             [--genCount] [--callDepth D]\n");
    printf("             [--useLockAddr Value] [--lockChainCount]\n");
    printf("             [--verbose mask] [--time secs|ticks]\n");
    printf("             [--overrideGarbled] [--kmon]\n");
    printf("  traceProfile takes a binary trace file as input\n");
    printf("   by default this is trace-out.0.trc\n");
    printf("  the binning can be done by either using the timer event\n");
    printf("   or the hwperf event, default is to use the timer event\n");
    printf("  by default it assumes it in the build/os directory, i.e.,\n");
    printf("   powerpc/xxxDeb/os.  Otherwise arguments --path pathname\n");
    printf("   should be specified as to where the os directory is.\n");
    printf("  by default it sets it sets a series of defaults for\n");
    printf("   the mapping of pid to files.  These can be over-ridden\n");
    printf("   by specifying --map N filename.  By default it sets up the\n");
    printf("   below mappings setting --map N invalid will cause the tool\n");
    printf("   not to use that file and not to bin for that pid.  Normally\n");
    printf("   the --map filename is appended to the --path filename unless\n");
    printf("   the --map filename is fully qualified (starts with /)\n");
    printf("    pid 0 : the kernel boot_image.dbg\n");
    printf("    pid 1 : baseServers servers/baseServers/baseServers.dbg\n");
    printf("    pid 2 : init servers/baseServers/baseServers.dbg\n");
    printf("  It produces (to standard output) a histogram of\n");
    printf("   the top --numb \"numb\" pc values, by default numb is 10.\n");
    printf("  It also generates more detailed information in files\n");
    printf("   called traceOutN.sort where N is the pid\n");
    printf("  The --callDepth D - D is the number of levels in call chain\n");
    printf("   that differentiates locks.  The default is 3, setting D to 1\n");
    printf("   means that all locks with the same immediate caller will be\n");
    printf("   binned together\n");
    printf("  The --countDbg - counts the number of debug Start/End pairs\n");
    printf("   and prints out stats about their times including average,\n");
    printf("   min, and max.\n");
    printf("  The --file option specifies the binary trace file or files to\n");
    printf("   be used.  If the --mp option is not specified the entire\n");
    printf("   filename should be given, e.g., trace-out.5.trc\n");
    printf("  The --live option reads events directly from the running\n");
    printf("   system's trace event buffers (only valid on K42)\n");
    printf("  The --genCount option turns on analysis of generation count\n");
    printf("   times.  It disables all other options.  Default is off.\n");
    printf("  The --help option prints out this usage information.\n");
    printf("  The --kmon option generates two files used by the graphical.\n");
    printf("   tool kmon.  These files are execStats.txt and lockStats.txt.\n");
    printf("  The --lockChainCount option prints only the number of\n");
    printf("   instances of a lock chain rather than the call chain for\n");
    printf("   each instance.  The default is off.\n");
    printf("  The --lockStats option turns off collecting of lock stats\n");
    printf("   default value is on (need LOCK_CONTENTION in FairBLock.C)\n");
    printf("  The --mp N indicates to process files multiple processors\n");
    printf("   where N is the number ex: --mp 4 will process files 0-3.\n");
    printf("  The --mpBase filename option is used to specify an\n");
    printf("   alternative base that generates all mp files\n");
    printf("   e.g. trace-lock specific files trace-lock.0.trc through\n");
    printf("   trace-lock.N.trc, used with --mp\n");
    printf("  The --mpFile filename specifies filenames for each of the\n");
    printf("   processors, by default they are trace-out.0.trc, etc.\n");
    printf("   Note: this option may only be specified after --mp.\n");
    printf("   Note: this is should only be used if the filenames are\n");
    printf("   much different than conventioned otherwise see --mpBase\n");
    printf("  The --pairStats | --pairStatsOnly option turns on collecting\n");
    printf("   of pair stats --pairStatsOnly prints only pair stats\n");
    printf("   this will sum up the time between all pairs of these event\n");
    printf("   that occur in the trace\n");
    printf("   the --maj argument is the major id for the pair\n");
    printf("   the --min start stop is the minor id pair to start and stop\n");
    printf("   up to %d pairs of minor ids\n", MAX_MINOR_PAIRS);
    printf("   example --pairStatOnly --maj 19 --min 0 1 --min 2 3\n");
    printf("   will print our pair stats for the resource manager pairs\n");
    printf("   0 - 1  and 2 - 3\n");
    printf("  The --overrideGarbled option will cause traceProfile to\n");
    printf("   attempt to continue even if it comes across a completely\n");
    printf("   garbled trace stream.  User beware.  Default is off\n");
    printf("  The --sample option determines whether the profile generated\n");
    printf("   uses sampled pc or sampled lr (link register) data \n");
    printf("   the lr is useful for determining the caller of the routine\n");
    printf("   (valid with hwperf only, default is 'pc')\n");
    printf("  The --time option determines how time values are printed\n");
    printf("   either in seconds or in ticks.  default is seconds\n");
    printf("  The --useLockAddr Value - 0 or 1 indicates whether the lock\n");
    printf("   address should be used to differentiate locks.  The default\n");
    printf("   is 0 meaning that locks with different addresses are binned\n");
    printf("   together, setting Value to 1 bins them spearately.\n");
    printf("   Related info on call chain (see --callDepth)\n");
    printf("  The --verbose mask sets the level of printing that occurs\n");
    printf("   during the analysis phase, default is 0x1.\n");
    printf("\n");
    printf("The tracePostProc tool will convert addresses in this tool's\n");
    printf("output into symbol names. Run tracePostProc --help for details\n");
}
