#ifndef __HW_PERF_MON_H_
#define __HW_PERF_MON_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWPerfMon.H,v 1.24 2005/08/22 21:48:58 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: encapsulates machine-dependent performance monitoring
 * **************************************************************************/
#include <cobj/CObjRootMultiRep.H>
#include "trace/traceHWPerfMon.h"
#include "trace/traceHWEventList.h"


class HWPerfMon : public CObjRep {
    class HWPerfMonRoot : public CObjRoot {
        HWPerfMonRoot() : CObjRoot((RepRef)GOBJK(TheHWPerfMonRef),Install) {}

        /******* WARNING *****
        * This object is accessed at exception level so ensure that it is
        * allocated in pinned memory
        *********************/
        DEFINE_PINNEDGLOBALPADDED_NEW(HWPerfMonRoot);
    public:
        virtual CObjRep *getRepOnThisVP();
        virtual SysStatus handleMiss(COSTransObject * &co, CORef ref,
                                     uval methodNum);
        virtual SysStatus cleanup(COSMissHandler::CleanupCmd);
        virtual VPSet getTransSet();
        virtual VPSet getVPCleanupSet();

        friend class HWPerfMon;
    };
    friend class HWPerfMon::HWPerfMonRoot;

public:
    enum CountingMode {
        CM_BOTH = 0,
	CM_KERNEL = 1,
	CM_USER = 2
    };

    enum PeriodType {
        PT_TIME = 0,
        PT_INSTR
    };

protected:
    enum {MAX_COUNTERS = 64};
    
    // number of bits to represent subintervals a sampling round is divided into
    uval8 totalSharesBits;  
    
    // maximum number of groups being counted simultaneously 
    uval8 maxGroups;   

    uval32 multiplexingRound;	 // in terms of the # of periodType events  
    uval32 logPeriod;            // in terms of the # of periodType events  

    CountingMode  countingMode;
    PeriodType periodType;
    bool watching;

    class CountersDescription {
    public:
	uval counter[MAX_COUNTERS];
	uval groupID;
	uval counterNum;
	uval logPeriod;
	uval markedEvent;
	uval sampleFreq;

	DEFINE_GLOBAL_NEW(CountersDescription);

	static CountersDescription *createCountersDescription() {
	    CountersDescription *countersDescription;
	    uval i;

	    countersDescription = new CountersDescription();
	    countersDescription->counterNum = 0;
	    for (i=0; i<MAX_COUNTERS; i++ ) { 
		countersDescription->counter[i] = 0;
	    }
	    countersDescription->groupID = (uval)(-1);
	    countersDescription->logPeriod = 0;

       	    return countersDescription;
	}

	void setLogCounter(uval counterIndex, uval description) {
	    counter[counterIndex] = description;
	    TraceHWHWPerfMonCounterDescr(counterIndex,
			    counter[counterIndex]);
	}

	void setLogDataSample(uval _markedEvent, uval freq) {
	    markedEvent = _markedEvent;
	    sampleFreq = freq;
	}

	void setLogGroupID(uval lgroupID) {
	    groupID = lgroupID;
	    TraceHWHWPerfMonGroupID(groupID);
	}

        void setLogPeriod(uval period)
	{
	    logPeriod = period;
	}

        void setCounterNum(uval _counterNum) {
	    counterNum = _counterNum;
	}

	void logCounters() {
	    uval i;
	    TraceHWHWPerfMonLogPeriod(logPeriod);
	    for (i=0; i < counterNum; i++) { 
		TraceHWHWPerfMonCounterDescr(i+1, counter[i]);
	    }
	    TraceHWHWPerfMonDataSamplingDescr(markedEvent, sampleFreq);
	}
    };

    class CounterVector {
    public:
        enum { 
	    CYCLE = 0,
	    INSTR_COMPLETED = 1,
	    INVALID_INDEX = 0XFF 
	};
        uval8 map[MAX_COUNTERS];   
        uval8 reverseMap[TRACE_GP970_DUMMY_LAST - TRACE_GP970_UNPROGRAMMED];
	uval64 values[MAX_COUNTERS];
	uval8 eventCount;

        CounterVector() {
            for (uval i = 0; i < MAX_COUNTERS; i++) {
                map[i] = TRACE_GP970_UNPROGRAMMED;
		values[i] = 0;
            }	

	    for (uval i = 0; i < (TRACE_GP970_DUMMY_LAST - TRACE_GP970_UNPROGRAMMED); i++) {
	        reverseMap[i] = INVALID_INDEX;
	    }

  	}

	CounterVector(CounterVector &another) {
            eventCount = another.eventCount; 
            for (uval i = 0; i < eventCount; i++) {
                map[i] = another.map[i];
		values[i] = 0;
	    }

            for (uval i = 0; i < (TRACE_GP970_DUMMY_LAST - TRACE_GP970_UNPROGRAMMED); i++) {
	        reverseMap[i] = another.reverseMap[i];
	    }
        }
    
        // it just clears out the values 
	void clear() {
	    memset((void *)values, 0, eventCount * sizeof(uval64));
	}

	void reset() {
	    for (uval i = 0; i < (TRACE_GP970_DUMMY_LAST - TRACE_GP970_UNPROGRAMMED); i++) {
	        reverseMap[i] = INVALID_INDEX;
            }

	    memset((void *)values, 0, MAX_COUNTERS * sizeof(uval64));

            map[CYCLE] = TRACE_GP970_CYC;
	    reverseMap[TRACE_GP970_CYC] = CYCLE;
            map[INSTR_COMPLETED] = TRACE_GP970_INST_CMPL;
	    reverseMap[TRACE_GP970_INST_CMPL] = INSTR_COMPLETED;
	    // the first two entries are reserved
	    eventCount = INSTR_COMPLETED + 1;
	}

        sval32 allocateSpot(uval eventId) {

            // search whether the event is already programmed
	    if (reverseMap[eventId] != INVALID_INDEX) {
	         return reverseMap[eventId];
	    }

            // else check for overflow first
	    if (eventCount == MAX_COUNTERS) {
	         return INVALID_INDEX;
	    }

	    // else allocate the spot
	    map[eventCount] = eventId;
	    reverseMap[eventId] = eventCount;
	    eventCount++;
	    // returning the index of the allocated spot
	    return (eventCount - 1);  
	}
    } counterVector;


    class WatchVector {
    public:
	uval64 values[MAX_COUNTERS];
	uval32 times[MAX_COUNTERS];
	uval8 eventCount;

        WatchVector() {
	    memset((void *)values, 0, MAX_COUNTERS * sizeof(uval64));
	    memset((void *)times, 0, MAX_COUNTERS * sizeof(uval32));
	    eventCount = 0;
      	}
        // it just clears out the values 
	void clear() {
	    memset((void *)values, 0, eventCount * sizeof(uval64));
	    memset((void *)times, 0, eventCount * sizeof(uval32));
	}
    } watchVector;

    class CPIBundle {
    public:
        uval64 RoundNum;	      // used as a sequencer ? 
        uval64 Cycles;
        uval64 InstrsCompleted;
        uval64 StallsDCacheMisses;
        uval64 StallsLSULatency;
        uval64 StallsFPULatency;
        uval64 StallsFXULatency;
        uval64 StallsRejects;
        uval64 CompletionTableEmptyICacheMisses;
        uval64 CompletionTableEmptyBrMispredict;

	void reset() 
	{
	    RoundNum = 0;
            Cycles = 0;
            InstrsCompleted = 0;
            StallsDCacheMisses = 0;
            StallsLSULatency = 0;
            StallsFPULatency = 0;
            StallsFXULatency = 0;
            StallsRejects = 0;
            CompletionTableEmptyICacheMisses = 0;
            CompletionTableEmptyBrMispredict = 0;
       	}
    } cpiBundle;

public:
    HWPerfMon() { passertMsg(0, "Must use constructor which specifies root");}
    HWPerfMon(CObjRoot *root) : CObjRep(root) {}
    /******* WARNING *****
     * This object is accessed at exception level so ensure that it is
     * allocated in pinned memory
     *********************/
    DEFINE_PINNEDLOCALSTRICT_NEW(HWPerfMon);

    // These two static methods must be implemented in the
    static SysStatus VPInit();             // Lowlevel boot time initialization
    static SysStatus ClassInit(VPNum vp);  // Clustered Object initialization

    // Simple access arbitration for performance hardware.
    // Must first do an aquire to use the Performance Hardware
    // and a release when finished.  At this point there is nothing
    // enforcing this.  It is upto the call to check the return code
    // and honour the value.  Eg. Failure on acquire means you the
    // hardware is busy and you should not invoke any other methods.
    virtual SysStatus acquire()                                 { return -1;}
    virtual SysStatus release()                                 { return 0;}

    // Low Level Interface
    virtual SysStatus zero()                                    { return 0;}
    virtual SysStatus HWPerfInterrupt()                         { return 0;}

    virtual void setCountingMode(CountingMode mode)             { countingMode = mode;}
    virtual void setPeriodType(PeriodType type)                 { periodType  = type;}
    virtual void setMultiplexingRound(uval32 round)             { multiplexingRound = round; }
    virtual void setLogPeriod(uval32 period)                    { logPeriod = period; }

    virtual SysStatus addGroup(uval32 groupNo, 
                                      uval32 share,
				      uval32 samplingFreq)      { return 0;}
    virtual SysStatus removeGroup(uval32 groupNo)               { return 0;}


    virtual SysStatus startSampling(uval64 delay)               { return 0;}
    virtual SysStatus stopSampling()                            { return 0;}

    virtual SysStatus startWatch()				{ return 0;}
    virtual SysStatus logAndResetWatch()				{ return 0;}
    virtual SysStatus stopWatch()				{ return 0;}

    virtual SysStatus startCPIBreakdown(uval64 delay)           { return 0;}
    virtual SysStatus stopCPIBreakdown()                        { return 0;}

    virtual SysStatus print()                                   { return 0;}
    virtual SysStatusUval getCounter(uval counter)		{ return 0;}

    virtual SysStatus logCountersToTrace()                      { return 0;}
};
#endif /* #ifndef __HW_PERF_MON_H_ */
