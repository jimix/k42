#ifndef __TRACE_CTR_H_
#define __TRACE_CTR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: traceCtr.H,v 1.13 2004/10/08 21:40:09 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: defines varios flags which configure experimental
 * features for experimentation.
 * **************************************************************************/

#include <trace/trace.H>

struct TraceAutoCount {
    typedef void (* PRINTFUNC)(const char *, ...);
    enum CtrIdx {
	FCMDEFAULT_MAPPAGE,
	FCMDEFAULT_GETPAGEI,
	FCMCOMPUTATION_MAPPAGE,
	FCMCOMPUTATION_GETPAGEI,
	FCMCOMPUTATION_GETPAGEI_PARENT,
	FCMCOMPUTATION_GETPAGEI_CHILD,
	FCMCOMPUTATION_FORKCOPY,
	FCMCOMPUTATION_FORKCOLLAPSE,
	FCMCOMPUTATION_GETPAGEI_FROMCHILD,
	FCMCOMPUTATION_GETFORKPAGE,
	FCMDEFAULTMR_MAPPAGE,
	FCMDEFAULTMR_GETPAGEI,
	FCMCRW_MAPPAGE,
	EXCEPTION_PGFLT,
	FCMCOMMON_LOCK_CONTENTION,
	NFS_RPC_ACQUIRE,
	NFS_RPC_BLOCKED,
	NFS_RPC_OP,
	LINUX_SC,
	FCMDEFAULT_STARTFILLPAGE,
	FCMDEFAULT_STARTFILLPAGEZERO,
	FCMDEFAULT_FINDORALLOCATEPAGEANDLOCK,
	FCMDEFAULT_ALLOCATEPAGE,
	// --------
	MAX
    };

    CtrIdx ci;
    SysTime tstart;
    TraceAutoCount(const CtrIdx i) : ci(i), tstart(Scheduler::SysTimeNow()) {
	/* empty body */
    }
    DEFINE_NOOP_NEW(TraceAutoCount);
    ~TraceAutoCount() {
	const SysTime tstop = Scheduler::SysTimeNow();

	volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
	uval64 *const trcCntrs = trcInfo->traceCounters;

	(void)FetchAndAdd64(&(trcCntrs[toTimeIdx(ci)]), (tstop-tstart));
	(void)FetchAndAdd64(&(trcCntrs[toNumIdx(ci)]), 1);
    }

    inline static uval toTimeIdx(CtrIdx i) { return i<<1; }
    inline static uval toNumIdx(CtrIdx i) { return (i<<1)+1; }

    static void initCounters() {
	volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
	uval64 *const trcCntrs = trcInfo->traceCounters;

	for (int i = 0; i < 2*MAX; i++) {
	    trcCntrs[i] = 0;
	}
    }
    static void dumpCounterPair(CtrIdx i, PRINTFUNC myprintf = cprintf) {
	const char *countName[] = {
	    "FCMDefault::mapPage()                     ",
	    "FCMDefault::getPageInternal()             ",
	    "FCMComputation::mapPage()                 ",
	    "FCMComputation::getPageInternal()         ",
	    "FCMComputation::getPageInternal_PARENT    ",
	    "FCMComputation::getPageInternal_CHILD     ",
	    "FCMComputation::forkCopy()                ",
	    "FCMComputation::forkCollapse()            ",
	    "FCMComputation::getPageInternal_FROMCHILD ",
	    "FCMComputation::getForkPage()             ",
	    "FCMDefaultMultiRep::mapPage()             ",
	    "FCMDefaultMultiRep::getPageInternal()     ",
	    "FCMCRW::mapPage()                         ",
	    "ExceptionLocal_PgfltHandler()             ",
	    "FCMCommon LOCK CONTENTION                 ",
	    "NFS_RPC_AQUIRE                            ",
	    "NFS_RPC_BLOCKED                           ",
	    "NFS_RPC_OP				       ",
	    "Linux syscall			       ",
            "FCMDefault::startFillPage()               ",
            "FCMDefault::startFillPage():ZeroFill      ",
	    "FCMDefault::findOrAllocatePageAndLock     ",
	    "FCMDefault::findOrAllocatePageAndLock:allc",

	    "---ERR-THIS-IS-MAX---"
	};

	volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
	uval64 *const trcCntrs = trcInfo->traceCounters;

	myprintf("%s num=%lld, time=%lld\n",
		 countName[i], trcCntrs[toNumIdx(i)], trcCntrs[toTimeIdx(i)]);
    }

    static void fetchCounterPair(CtrIdx i, uval64 *time, uval64 *num) {
	volatile TraceInfo *const trcInfo = &kernelInfoLocal.traceInfo;
	uval64 *const trcCntrs = trcInfo->traceCounters;

	*time = trcCntrs[toTimeIdx(i)];
	*num = trcCntrs[toNumIdx(i)];
    }

};

#define TRACECTR_LOCK_ACQUIRE(lck,x)	\
    if (!(lck).tryAcquire()) {	\
	TraceAutoCount tac(x);	\
	(lck).acquire();	\
    }

template <class Lock_Class, TraceAutoCount::CtrIdx CTRIDX>
class TraceCtrLock : public Lock_Class {
public:
    // TODO: in the constructor add trace event with the this ptr
    void acquire(void) {
	if (!this->tryAcquire()) {
	    TraceAutoCount tac(CTRIDX);
	    // TODO: could add trace event with (this, CTRIDX)
	    Lock_Class::acquire();
	}
    }
};

#endif /* #ifndef __TRACE_CTR_H_ */
