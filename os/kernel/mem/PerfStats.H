#ifndef __PERF_STATS_H_
#define __PERF_STATS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PerfStats.H,v 1.6 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Interfaces for gathering measurements of PF perf.
 * **************************************************************************/
#include <exception/ExceptionLocal.H>
#include <defines/pgflt_stats.H>
enum {
    ZeroFill	=0,	//
    CopyFrame	=1,	//
    UnMapped	=2,	//
    fcmComp	=3,	//
    fcmDefault  =4,	//
    WriteFault	=5,	//
    fcmFixed	=6,	//
    fcmLTrans	=7,	//
    KeepFrame	=8,     //Get and keep from parent
    fcmPrimitive=9,	//
    DetectMod	=10,	// ProcessReplicated
    DistribFCM  =11,	//
    DistribRoot =12,	//
    CreatSeg    =13,	//
    MapShrSeg   =14,	//
    NeedWrite	=15,    //
    Ctr1Shift	=48,    //Reserved
    Ctr2Shift	=32,
    Ctr3Shift	=16,
    CacheSynced	=0xffff,// was 3
    MakeRO	=0xffff,// was 4
    DoFill	=0xffff,// was 6
    FromFreeList=0xffff,// was 7
    UnmapOther	=0xffff,// was 9
};

// At any time, only one type of timer may be assigned to any
// particular counter id (numbers 1 2 3).
#ifdef MEMOP_TIMERS
// copy-tagged pf's and 0-fill pf's are mutually exclusive, so they can
// use the same counter
#define MemCpyTimer		0
#define ZFillTimer		0
#define DMTimer			2
#define HATMapPage		1
#define HATTimer		3
#define GetPageTimer		0
#define MapPageTimer		0
#define RegionHandlerTimer	0
#define ProcessTimer		0
#define ExceptionCode		0

#define RegionLookup		0
#else
#define MemCpyTimer		0
#define ZFillTimer		0
#define GetPageTimer		0
#define MapPageTimer		0
#define RegionHandlerTimer	0
#define ProcessTimer		0
#define ExceptionCode		0
#define RegionLookup		0
#define DMTimer			0
#define HATMapPage		0
#define HATTimer		0
#endif

// This enables the collection of some detailed page-fault stats.
// Up to three 16-bit counter can be maintained along with a 16-bit
// bitmap that identifies the different code paths a page-fault went down.
// The results are included in the PGFLT_DONE event data
#ifdef COLLECT_FAULT_STATS



#include "bilge/HWPerfMon.H"

static inline void
setPFBit(uval bit)
{
    if (bit!=0xffff) {
	uval bits = Scheduler::GetThreadSpecificUvalSelf();
	bits |= (1ULL<<bit);
	Scheduler::SetThreadSpecificUvalSelf(bits);
    }
}



struct pfCtr{
    uval start;
    pfCtr():start(Scheduler::SysTimeNow()) {};
    void record(uval ctrID) {
	uval stop = Scheduler::SysTimeNow();
	uval bits = Scheduler::GetThreadSpecificUvalSelf();
	bits |= (((stop-start)&0xffff)<<ctrID);
	Scheduler::SetThreadSpecificUvalSelf(bits);
    };
};

struct StatTimer{
    const uval ctrID;
    const uval start;
    StatTimer(uval ctr):ctrID(ctr),
			start((ctrID!=0 ? Scheduler::SysTimeNow() : 0)) {};
    void record() {
	if (ctrID!=0) {
	    uval stop = Scheduler::SysTimeNow();
	    uval bits = Scheduler::GetThreadSpecificUvalSelf();
	    bits &= ~((0xffffULL)<<(ctrID*16));
	    bits |= ((((stop-start)>>4)&0xffffULL)<<(ctrID*16));
	    Scheduler::SetThreadSpecificUvalSelf(bits);
	}
    };
    void addRecord() {
	if (ctrID!=0) {
	    uval stop = Scheduler::SysTimeNow();
	    uval bits = Scheduler::GetThreadSpecificUvalSelf();
	    uval val = bits>>(ctrID*16) & 0xffffULL;
	    bits &= ~((0xffffULL)<<(ctrID*16));
	    val+= (stop-start)>>4;
	    bits |= ((val&0xffffULL)<<(ctrID*16));
	    Scheduler::SetThreadSpecificUvalSelf(bits);
	}
    };
};

struct ScopeTime: public StatTimer{
    ScopeTime(uval ctr):StatTimer(ctr) {};
    ~ScopeTime() {
	record();
    };
};


struct InstCtr{
    DEFINE_NOOP_NEW(InstCtr);
    uval start;
    uval ctrID;
    InstCtr(uval ctr) {
	if (exceptionLocal.getHWPerfMonRep()) {
	    start=DREFGOBJK(TheHWPerfMonRef)->getCounter(2);
	    ctrID = ctr;
	} else {
	    start = 0;
	}
    }
    ~InstCtr() {
	if (start) {
	    uval stop = DREFGOBJK(TheHWPerfMonRef)->getCounter(2);
	    uval bits = Scheduler::GetThreadSpecificUvalSelf();
	    bits |= ((((stop - start)>>4)&0xffff)<<(ctrID*16));
	    Scheduler::SetThreadSpecificUvalSelf(bits);
	}
    };
};
struct CycleCtr{
    DEFINE_NOOP_NEW(InstCtr);
    uval start;
    uval ctrID;
    CycleCtr(uval ctr) {
	if (exceptionLocal.getHWPerfMonRep()) {
	    start=DREFGOBJK(TheHWPerfMonRef)->getCounter(1);
	    ctrID = ctr;
	} else {
	    start = 0;
	}
    }
    ~CycleCtr() {
	if (start) {
	    uval stop = DREFGOBJK(TheHWPerfMonRef)->getCounter(1);
	    uval bits = Scheduler::GetThreadSpecificUvalSelf();
	    bits |= ((((stop - start)>>4)&0xffff)<<(ctrID*16));
	    Scheduler::SetThreadSpecificUvalSelf(bits);
	}
    };
};
#else


//Totally ignore this interface
#define setPFBit(x)
struct pfCtr{
    pfCtr() {};
    void record(uval ctrID) {};
} __attribute__ ((unused));

struct ScopeTime{
    ScopeTime(uval ctr) {};
} __attribute__ ((unused));

struct StatTimer{
    StatTimer(uval ctr) {};
    inline void record() {};
    inline void addRecord() {};
} __attribute__ ((unused));

struct DisabledScopeTime{
    DisabledScopeTime(uval ctr) {};
} __attribute__ ((unused));

struct InstCtr{
    InstCtr(uval ctr) {};
}  __attribute__ ((unused));
struct CycleCtr{
    CycleCtr(uval ctr) {};
} __attribute__ ((unused));
#endif /* #ifndef COLLECT_FAULT_STATS */


#endif /* #ifndef __PERF_STATS_H_ */
