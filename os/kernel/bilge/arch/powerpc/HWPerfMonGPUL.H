#ifndef __HW_PERF_MON_GPUL_H_
#define __HW_PERF_MON_GPUL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWPerfMonGPUL.H,v 1.5 2005/08/22 21:48:59 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 ****************************************************************************/
#include "bilge/HWPerfMon.H"

#define MAX_GROUPS 32
#define GPUL_COUNTERS 8

#define MAMBO_SUPPORT
#ifdef MAMBO_SUPPORT

enum PlatformType {
   GPUL_HARDWARE = 0,
   GPUL_MAMBO
 };



#define L2_PENALTY 10       // a rough figure to be used for measuring CPI breakdown
#define MEM_PENALTY 100     // a rough figure to be used for measuring CPI breakdown

class PMCCtlRegSetMap {
public:
   uval number;
   uval32 mmcr0;
   uval64 mmcr1;
   uval32 mmcra;
};
#endif  // MAMBO_SUPPORT 


class HPMTimerEvent;

class HWPerfMonGPUL : public HWPerfMon {
    
    class MMCR0 {
    public:
        union {
            uval64 value;
            struct  {
                uval32 ni0;             // Not implemented

                uval32 freeze:1;	// Set by the processor if fcece bit is on when 
                                        //  an event happens (see below)
                uval32 freeze_sv:1;     // Freeze counters in supervisor state
                uval32 freeze_pr:1;	// Freeze counters in problem state
                uval32 monitor_on:1;	// Freeze counters when MSR[PMM] = 1
                uval32 monitor_off:1;	// Freeze counters when MSR[PMM] = 0
                uval32 interrupt:1;	// ENABLE interrupts
                uval32 fcece:1;         // Freeze Counters on Enabled Cond. or Event 
                                        //   0 => counters are incremented.
                                        //   1 => counters are incremented until
                                        //   detection of 
                                        //    an enabled 
                                        //      i)  counter negative condition 
					//     or
                                        //      ii) time base transition event 
                                        //    and 
                                        //     the MMCR0[TRIGGER] = 0
                                        //   As a result
                                        //      MMCR0[FREEZ] <- 1
                                        //   until the condition is reset by software

                uval32 tbselect:2;      // Time base select
                                        //   00 => Time base bit 63 is selected 
                                        //   01 => Time base bit 55 is selected 
                                        //   10 => Time base bit 51 is selected 
                                        //   11 => Time base bit 47 is selected  
					//  (FIXME: More meaningful comments?)

                uval32 tbee:1;          // Time base exception enable
                uval32 threshold:6;     // Threshold value, counting occurs only for 
                                        //   those events whose duration exceeds 
                                        //   the threshold value in cycles

                uval32 pmc1ce:1;        // PMC1 count enable: 
                                        //   0 => Disable PMC1 counter negative condition 
                                        //   1 => Enable PMC1 counter negative condition 

                uval32 pmcNce:1;        // PMCN (2 <= N <= 8) count enable
                                        //   0 => Disable PMCN counter negative condition 
                                        //   1 => Enable PMCN counter negative condition 

                uval32 trigger:1;       // Trigger Enable:
					//   0 => PMCs are incremented
					//   1 => PMCs (2-8) are not incremented 
					//        until PMC1 is negative or an enabled
                                        //        event occurs.

                uval32 pmc1sel:5;       // PMC1 event selector
                uval32 pm_alert:1;      // Performance Monitor Alert occured!
                uval32 ni1:1;           // Not implemented
                uval32 pmc2sel:5;       // PMC2 event selector
                uval32 fch:1;           // Freezes the counters in hypervisor mode
            } bits;
        };

        inline void get() {
            __asm __volatile("mfspr %0, 779" : "=r"(value));
        }

        inline void set() {
            __asm __volatile("mtspr 795, %0" : : "r"(value));
        }
    } mmcr0;

    class MMCR1 {
    public:
        union {
            uval64 value;
            struct {
		uval32 ttm0sel:2;         // Unit Select
                                          //   00 => FPU
                                          //   01 => ISU
                                          //   10 => IFU
                                          //   11 => VMX 

                uval32 ttc0sel:1;         // reserved
                uval32 ttm1sel:2;         // Unit Select
                                          //   00 => IDU
                                          //   01 => undefined
                                          //   10 => ISU
                                          //   11 => STS (??) 
                uval32 ttc1sel:1;         // reserved
                uval32 ttm2sel:2;         // reserved
                uval32 ttc2sel:1;         // reserved

                uval32 ttm3sel9:1;        // LSU1 select
                                          //   0 => byte 2 is LSU1 upper (LSU1, byte 2) 
                                          //   1 => byte 2 is LSU1 lower (LSU1, byte 6) 

                uval32 ttm3sel10:1;       // LSU1 select
                                          //   0 => byte 3 is LSU1 upper (LSU1, byte 3) 
                                          //   1 => byte 3 is LSU1 lower (LSU1, byte 7) 

                uval32 ttc3sel:1;         // reserved
                   
                uval32 td_cp_dbg0sel:2;   // Byte lane 0 unit select
                                          // 00 => Unit from ttm0
                                          // 01 => Unit from ttm1
                                          // 10 => LSU0 byte 0
                                          // 11 => LSU1 byte 0

                uval32 td_cp_dbg1sel:2;   // Byte lane 1 unit select
                                          // 00 => Unit from ttm0
                                          // 01 => Unit from ttm1
                                          // 10 => LSU0 byte 0
                                          // 11 => LSU1 byte 0

                uval32 td_cp_dbg2sel:2;   // Byte lane 2 unit select
                                          // 00 => Unit from ttm0
                                          // 01 => Unit from ttm1
                                          // 10 => LSU0 byte 0
                                          // 11 => LSU1 byte 0

                uval32 td_cp_dbg3sel:2;   // Byte lane 3 unit select
                                          // 00 => Unit from ttm0
                                          // 01 => Unit from ttm1
                                          // 10 => LSU0 byte 0
                                          // 11 => LSU1 byte 0
		uval32 ni0:4;             // reserved

                uval32 pmc1_adder_sel:1;  // PMC1 event adder lane select
                                          // 0 => byte lane 0: Add 0 + 4
                                          // 1 => byte lane 2: Add 0 + 4

                uval32 pmc2_adder_sel:1;  // PMC2 event adder lane select
                                          // 0 => byte lane 0: Add 1 + 5
                                          // 1 => byte lane 2: Add 1 + 5

                uval32 pmc6_adder_sel:1;  // PMC6 event adder lane select
                                          // 0 => byte lane 0: Add 2 + 6
                                          // 1 => byte lane 2: Add 2 + 6

                uval32 pmc5_adder_sel:1;  // PMC5 event adder lane select
                                          // 0 => byte lane 0: Add 3 + 7
                                          // 1 => byte lane 2: Add 3 + 7

                uval32 pmc8_adder_sel:1;  // PMC8 event adder lane select
                                          // 0 => byte lane 1: Add 0 + 4
                                          // 1 => byte lane 3: Add 0 + 4

                uval32 pmc7_adder_sel:1;  // PMC7 event adder lane select
                                          // 0 => byte lane 1: Add 1 + 5
                                          // 1 => byte lane 3: Add 1 + 5

                uval32 pmc3_adder_sel:1;  // PMC3 event adder lane select
                                          // 0 => byte lane 1: Add 2 + 6
                                          // 1 => byte lane 3: Add 2 + 6

                uval32 pmc4_adder_sel:1;  // PMC4 event adder lane select
                                          // 0 => byte lane 1: Add 3 + 7
                                          // 1 => byte lane 3: Add 3 + 7

                uval32 pmc3sel:5;         // PMC3 event selector   
                uval32 pmc4sel:5;         // PMC4 event selector   
                uval32 pmc5sel:5;         // PMC5 event selector   
                uval32 pmc6sel:5;         // PMC6 event selector   
                uval32 pmc7sel:5;         // PMC7 event selector   
                uval32 pmc8sel:5;         // PMC8 event selector   

                uval32 specsel:2;         // Speculative count event selector
                                          // 00 => reserved 
                                          // 01 => event A1x   
                                          // 10 => event A2x   
                                          // 11 => event A3x   
            } bits;
        };

        inline void get() {
            __asm __volatile("mfspr %0, 782" : "=r"(value));
        }

        inline void set() {
            __asm __volatile("mtspr 798, %0" : : "r"(value));
        }
    } mmcr1;

    class MMCRA {
    public:
        union {
            uval64 value;
            struct  {
                uval32 ni0;             // Not implemented
                uval32 sco:1;           // SIAR and SDAR from same instruction
                uval32 freeze_1_4:1;	// Freeze counters 1-4
					// 0 => PMC1-4 are incremented. 
					// 1 => PMC1-4 are not incremented. 	

                uval32 freeze_5_8:1;	// Freeze counters 5-8
					// 0 => PMC5-8 are incremented. 
					// 1 => PMC5-8 are not incremented. 	

                uval32 sihv:1;	        // state of MSR[HV] when SIAR set
                uval32 sipr:1;	        // state of MSR[PR] when SIAR set
                  
                uval32 ni1:3;           // reserved
                uval32 thresh_start:3;	// Threshold start event
                uval32 thresh_end:3;	// Threshold end event
                uval32 ni2:3;           // reserved
                uval32 imr_sel:1;	// IMR select: stage 1 eligibility in instruction
                                        //  matching:
                                        //  0 => stage 1 elibible instructions are 
                                        //    determined through predecode bits from 
                                        //    IFU combined with the imr_match and 
                                        //    imr_mask field.
                                        //  1 => imr bit from the IFU IMC match array is
                                        //    used to determine stage 1 eligibility    
 
                uval32 imr_mark:2;	// Chooses the mark mode for which instructions 
                                        //  are stage 2 eligible:
                                        //  00 => all stage 1 eligible IOPs
                                        //  01 => only Stage 1 eligible IOPs that resulted
                                        //     from microcode expansion
                                        //  10 => only one IOP per eligible PPC instruction
                                        //  11 => first IOP that goes to the LSU for 
                                        //     every eligible PPC instruction
 
                uval32 imr_mask:4;	// A mask AND'd with the predecode bits before
                                        // using the imr_match field 

                uval32 imr_match:4;	// The result of imr_mask AND'd with predecode bits
                                        //   must match to be stage 2 eligible. 

                uval32 fcti:1;          // Freezes counters 
					// 0 => PMCs are incremented. 
					// 1 => PMCs are not incremented. 	

                uval32 ni3:1;           // reserved

                uval32 fcwait:1;        // Freezes counters in Wait State (CNTL[31]=0)
					// 0 => PMCs are incremented. 
					// 1 => PMCs (except those counting cycles are 
                                        // not incremented when CNTL[31] = 0. 	

                uval32 sample_en:1;     // 0 => Sampling is disabled.
                                        // 1 => Sampling is enabled.

            } bits;
        };

        inline void get() {
            __asm __volatile("mfspr %0, 770" : "=r"(value));
        }

        inline void set() {
            __asm __volatile("mtspr 786, %0" : : "r"(value));
        }

    } mmcra;

    struct SRR0 {
        uval64 value;

        inline void get() {
            __asm __volatile("mfspr %0, 26" : "=r"(value));
        }
    } srr0;

    struct SRR1 {
        uval64 value;

        inline void get() {
            __asm __volatile("mfspr %0, 27" : "=r"(value));
        }
    } srr1;

    struct SIAR {
        uval64 value;

        inline void get() {
            __asm __volatile("mfspr %0, 780" : "=r"(value));
        }
    } siar;

    struct SDAR {
        uval64 value;

        inline void get() {
            __asm __volatile("mfspr %0, 781" : "=r"(value));
        }
    } sdar;

    // the generic class (abstract) for all performance counters

    class HWCtrGroup {
    public:
        uval8 gid;            // between 0..255
        sval8 shareBits;      // between 0..totalSharesBits (-1 means the group is stopped)
        MMCR0 mmcr0;
        MMCR1 mmcr1;
        MMCRA mmcra;

	uval8 timer;

	// assumption: we don't have more than 1 event-driven sampling counter within a group
	uval8 sampleCounter;

	uval32 periodicCountdownValue;

	uval32 samplingCountdownValue;

	// we assumed MMCRA value is identical for all the groups

	uval8 events[GPUL_COUNTERS+1];       // specifies the events the group is counting

	void inline reset() { 
	    gid = 0;
	    shareBits = -1;
	    timer = 0;
	    sampleCounter = 0;
	    periodicCountdownValue = 0;
	    samplingCountdownValue = 0;
	    mmcr0.value = 0;
	    mmcr1.value = 0;
	    mmcra.value = 0;

	    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
	        events[i] = CounterVector::INVALID_INDEX; 
	    }
	}
    };

    HWCtrGroup groupList[MAX_GROUPS];
    uval8 groupNum;                  //  the number of groups actively being counted
    uval8 groupCurrent;              //  the index of the current group being counted 
    uval64 roundCount;   // a monotonously increasing counter of multiplexing rounds
    uval  logRatio;  

    class PMC {
    public:
        // common direct events
        enum DirectEvents {
            Adder = 0, 
            undef1,  
            undef2, 
            undef3, 
            undef4, 
            undef5,
            InstSrcEncode,  
            DataSrcEncode,  
            OFF,
            InstrCompleted_2,
            Overflow,
            Reserved1,
            Reserved2,
            Byte2Decode,    
            Byte3Decode,   
            Cycles
        };

    public:
        uval32 value;
        bool busy;

        virtual inline void get() {};
        virtual inline void set(uval32 _value) {};
	virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {};
        inline void release() {
            busy = false;
        }; 

        PMC() {
           value = 0;
           busy = false;
        } 
    };

    class PMC1: public PMC {
    public: 
        enum DirectEvents {
            Add0_4 = 0, 
            InstrsCompleted, 
            MarkedGroupDispatch, 
            MarkedStoreComplete, 
            GlobalCompletionQueueEmpty, 
            RunCycles,
            InstSrcEncode,  // 0000
            DataSrcEncode,  // 0000
            OFF,
            InstrCompleted_2,
            Overflow8,
            ComplStall,
            Reserved,
            Byte2Decode,    // 1000 
            Byte3Decode,    // 1000
            Cycles
        };

        virtual inline void get() {
            __asm __volatile("mfspr %0, 771" : "=r" (value));
        }

        virtual inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 787, %0" : : "r" (value));
        }

        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr0->bits.pmc1sel = Event;
            mmcr1->bits.pmc1_adder_sel = adderSel;  
            busy = true;
        } 

    } pmc1;

    class PMC2 : public PMC {
    public:
        enum DirectEvents {
            Add1_5 = 0, 
            WorkHeld, 
            LSUEmpty, 
            ThresholdTimeoutEvent, 
            GroupDispatch, 
            BRUMarkedInstrFinish,
            InstSrcEncode,  // 0001
            DataSrcEncode,  // 0001
            OFF,
            InstrsCompleted,
            Overflow1,
            GCTEmptybySRQFull,
            Reserved,
            Byte2Decode,    // 1001
            Byte3Decode,    // 1001
            Cycles
        };
        virtual inline void get() {
            __asm __volatile("mfspr %0, 772" : "=r" (value));
        }

        virtual inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 788, %0" : : "r" (value));
        }

        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr0->bits.pmc2sel = Event;
            mmcr1->bits.pmc2_adder_sel = adderSel;  
            busy = true;
        } 
    } pmc2;

    class PMC3: public PMC {
    public:
        enum DirectEvents {
            Add2_6 = 0, 
            StopCompletion,  // ??
            LSUEmpty, 
            MarkedStoreComplete, 
            HypervisorCycles, 
            VMXMarkedInstrFinish,
            InstSrcEncode,  // 0010
            DataSrcEncode,  // 0010
            OFF,
            InstrCompleted_2,
            Overflow2,
            Reserved1,
            Reserved2,
            Byte2Decode,    // 1010
            Byte3Decode,    // 1010
            Cycles
        };

        virtual inline void get() {
            __asm __volatile("mfspr %0, 773" : "=r" (value));
        }

        virtual inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 789, %0" : : "r" (value));
        }

        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr1->bits.pmc3sel = Event;
            mmcr1->bits.pmc3_adder_sel = adderSel;  
            busy = true;
        } 
    } pmc3;

    class PMC4 : public PMC {
    public:
        enum DirectEvents {
            Add3_7 = 0, 
            InstrsCompleted,  
            IdleFXU0andBusyFXU1, 
            SRQEmpty, 
            MarkedGroupComplete, 
            CRUMarkedInstrFinish,
            InstSrcEncode,  // 0011
            DataSrcEncode,  // 0011
            OFF,
            InstrCompleted_2,
            Overflow3,
            Reserved1,
            Reserved2,
            Byte2Decode,    // 1011
            Byte3Decode,    // 1011
            Cycles
        };

        virtual inline void get() {
            __asm __volatile("mfspr %0, 774" : "=r" (value));
        }

        virtual inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 790, %0" : : "r" (value));
        }

        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr1->bits.pmc4sel = Event;
            mmcr1->bits.pmc4_adder_sel = adderSel;  
            busy = true;
        } 
    } pmc4;

    class PMC5 : public PMC{
    public:
        enum DirectEvents {
            Add3_7 = 0, 
            DispatchSuccess,  
            IdleFXU0andIdleFXU1, 
            PPCInstructionCompleted,  // one or more ??
            MarkedGroupInIDU, 
            MarkedGroupCompleteTimeout,
            InstSrcEncode,  // 0100
            DataSrcEncode,  // 0100
            OFF,
            InstrsCompleted_2,
            Overflow4,
            SpeculativeA,      
            SpeculativeC,      
            Byte2Decode,    // 1100
            Byte3Decode,    // 1100
            Cycles
        };

        enum SpeculativeEventsA {
            ReservedA = 0,
            CompletionStallByLSUInst, 
            CompletionStallByFXUInst, 
            CompletionStallByDCacheMiss
        };

        enum SpeculativeEventsC {
            ReservedC1 = 0,
            CompletionStallByLongFPUInst,
            GCTEmptyByICacheMiss, 
            ReservedC2
        };

        virtual inline void get() {
            __asm __volatile("mfspr %0, 775" : "=r" (value));
        }

        virtual inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 791, %0" : : "r" (value));
        }

        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr1->bits.pmc5sel = Event;
            mmcr1->bits.pmc5_adder_sel = adderSel;  
            busy = true;
        } 
    } pmc5;

    class PMC6: public PMC {
    public:
        enum DirectEvents {
            Add2_6 = 0, 
            InstrsCompleted,  
            BusyFXU0andBusyFXU1, 
            MarkedStoreSenttoSTS, 
            FXUMarkedInstrFinish, 
            MarkedGroupIssued,
            InstSrcEncode,  // 0101
            DataSrcEncode,  // 0101
            OFF,
            InstrsCompleted_2,
            Overflow5,
            Reserved1,
            Reserved2,
            Byte2Decode,    // 1101
            Byte3Decode,    // 1101
            Cycles
        };

        inline void get() {
            __asm __volatile("mfspr %0, 776" : "=r" (value));
        }

        inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 792, %0" : : "r" (value));
        }
        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr1->bits.pmc6sel = Event;
            mmcr1->bits.pmc6_adder_sel = adderSel;  
            busy = true;
        } 

    } pmc6;

    class PMC7 : public PMC{
    public:
        enum DirectEvents {
            Add1_5 = 0, 
            InstrsCompleted,  
            BusyFXU0andIdleFXU1, 
            GroupCompleted, 
            FPUMarkedInstrFinish, 
            MarkedInstrFinish,
            InstSrcEncode,  // 0110
            DataSrcEncode,  // 0110
            OFF,
            InstrsCompleted_2,
            Overflow6,
            SpeculativeB, 
            SpeculativeD, 
            Byte2Decode,    // 1110
            Byte3Decode,    // 1110
            Cycles
        };

        enum SpeculativeEventsB {
            ReservedB = 0,
            CompletionStallByFPUInst, 
            CompletionStallByLongFXUInst, 
            CompletionStallByReject
        };

        enum SpeculativeEventsD {
            ReservedD1 = 0,
            CompletionStallByERATMiss,
            GCTEmptyByMispredict,
            ReservedD2
        };

        inline void get() {
            __asm __volatile("mfspr %0, 777" : "=r" (value));
        }

        inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 793, %0" : : "r" (value));
        }
        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr1->bits.pmc7sel = Event;
            mmcr1->bits.pmc7_adder_sel = adderSel;  
            busy = true;
        } 
    } pmc7;

    class PMC8 : public PMC{
    public:
        enum DirectEvents {
            Add0_4 = 0, 
            InstrsCompleted,  
            ExternalInterrupt, 
            GroupDispatchReject, 
            LSUMarkedInstrFinish, 
            TimeBaseEvent,
            InstSrcEncode,  // 0111
            DataSrcEncode,  // 0111
            OFF,
            InstrsCompleted_2,
            Overflow7,
            Reserved1,      
            Reserved2,      
            Byte2Decode,    // 1111
            Byte3Decode,    // 1111
            Cycles
        };


        inline void get() {
            __asm __volatile("mfspr %0, 778" : "=r" (value));
        }

        inline void set(uval32 _value) {
	    value = _value;
            __asm __volatile("mtspr 794, %0" : : "r" (value));
        }
        virtual inline void assignEvent(MMCR0 *mmcr0, MMCR1 *mmcr1, uval32 Event, uval32 adderSel) {
            mmcr1->bits.pmc8sel = Event;
            mmcr1->bits.pmc8_adder_sel = adderSel;  
            busy = true;
        } 

    } pmc8;

    uval   busy;
    CountersDescription *countersDescription;
    HPMTimerEvent *timerEvent;

    PMC  *pmcs[GPUL_COUNTERS+1];      // an array of pointers to the counters

    // temporary variables 
    uval8 cycleCounter;
    uval8 instrCmplCounter;
    uval8 timer;
    uval8 sampleCounter;
    uval8 sampleEvent;

    uval64 samplingDelay;

    uval8 events[GPUL_COUNTERS+1];    // the events the group is counting (temporary variable)
    uval32 startTime;

#ifdef MAMBO_SUPPORT
    PlatformType platform_type;
#endif 
 
    /******* WARNING *****
     * This object is accessed at exception level so ensure that it is
     * allocated in pinned memory
     *********************/
    DEFINE_PINNEDLOCALSTRICT_NEW(HWPerfMonGPUL);
    friend class HWPerfMon;
   
        
    enum TTM0_SEL {
        FPU=0,
        ISU_TTM0,
        IFU, 
        VMX,
        TTM0_UNUSED
    };   
    uval ttm0_mux;         // what value ttm0 mux is programmed with 

    enum TTM1_SEL {
        IDU=0,
        UNDEFINED, 
        ISU_TTM1,
        GPS,
        TTM1_UNUSED
    };   
    uval ttm1_mux;         // what value ttm1 mux is programmed with

    enum TD_CP_DBGX {
        TTM0=0,
        TTM1, 
        LSU0,
        LSU1,
        TD_CP_DBGX_UNUSED
    };   
    uval td_cp_dbg_mux[4]; // shows the current value each mux is programmed with

    enum TTM3_9_SEL {
        LSU1_2 = 0,
        LSU1_6,
        TTM3_9_UNUSED
    };
    uval ttm3_9_mux;

    enum TTM3_10_SEL {
        LSU1_3 = 0,
        LSU1_7,
        TTM3_10_UNUSED
    };
    uval ttm3_10_mux;

    enum LANE_SEL {
        HIGH_BYTE = 0,  // (lane 0, 1)
        LOW_BYTE,       // (lane 2, 3) 
        DIRECT  
    };
        
    // ISU events
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   10 (lane0)  :   000..111   (binary)
    //   events range: 16..23
    enum ISU_BYTE_LANE0 {
        GCTFull=16,
        FPRMapperFull,
        XERMapperFull,
        FPU0IssueQueueFull,
        CRMapperFull,
        BRIssueQueueFull,
        LRCTRMapperFull,
        FPU1IssueQueueFull  
    };
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   10 (lane1)  :   000..111   (binary)
    //   events range: 16..23
    enum ISU_BYTE_LANE1 {
        FXU0LSU0IssueQueueFull=16,
        CRIssueQueueFull,
        LRQFull,
        SRQFull,
        FXU1LSU1IssueQueueFull,
        FlushOriginatedByLSU,
        FlushOriginatedByMispredict,
        FlushLSUandMispredict  
    };
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   11 (lane2)  :   000..111   (binary)
    //   events range: 24..31
    enum ISU_BYTE_LANE2 {
        InstrsDispatched=24,
        InstrsDispatched1,
        InstrsDispatched2,
        DispatchValid,
        DispatchReject,
        ISUNothing,
        BranchRedirect,
        BranchMispredict  
    };
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   11 (lane3)  :   000..111   (binary)
    //   events range: 24..31
    enum ISU_BYTE_LANE3 {
        Nothing31=24,
        DispatchedBlockedByScoreboard,
        FXU0ProducedResult,
        DurationMSREE0,
        Nothing32,
        GPRMapperFull,
        FXU1ProducedResult,
        InterruptPending  
    };


    //   LSU1 events
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   10 (lane1)  :   000..111   (binary)
    //   events range: 16..23
    enum LSU1_BYTE_LANE1 {    
        L1DCacheLoadSide0=16,
        L1DCacheStoreSide0,
        L1DCacheLoadMissSide0,
        L1DCacheStoreMiss,
        L1DCacheLoadSide1,
        L1DCacheStoreSide1,
        L1DCacheLoadMissSide1,
        L1DEntriesInvalidatedFromL2
    };
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   11 (lane3)  :   000..111   (binary)
    //   events range: 24..31
    enum LSU1_BYTE_LANE3 {    
        L1CacheReloadDataSource1=24,
        L1CacheReloadDataSource2,
        L1CacheReloadDataSource3,
        L1CacheReloadDataSource4,
        L1CacheReloadDataValid,
        LMQSlot0Valid,
        LMQSlot0Allocated,
        LMQFull
    };

    //   IFU events
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   11 (lane2)  :   000..111   (binary)
    //   events range: 24..31
    enum IFU_BYTE_LANE2 {    
        L1ICacheFetchFromL2=24,              // ???
        L1ICacheFetchFromAnotherL2Shared,    // ???
        L1ICacheFetchFromAnotherL2Modified,  // ???
        L1ICacheFetchFromMemory,             // ???
        ValidInstrAvailable,
        InstrPrefetchRequest,
        InstrPrefetchInstalledInPrefetchBuffer,
        TranslationWrittenToIERAT
    };
    // PMCSEL(0:1) :  PMCSEL(2:4)
    //   11 (lane3)  :   000..111   (binary)
    //   events range: 24..31
    enum IFU_BYTE_LANE3 {    
        BranchMispredictDuToTarget=24,              
        BranchExecutionIssueValid,   
        BranchMispredictDuToCR, 
        CyclesL1ICacheWriteActive,           
        ValidInstrAvailableIFUHeldByBIQorIDU,
        IFUNothing1,
        IFUNothing2,
        IFUNothing3
    };

    // LSU0 events
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   10 (lane0)  :   000..111   (binary)
    //   events range: 16..23
    enum LSU0_BYTE_LANE0 {
        ITLBMiss=16,
        ISLBMiss,
        DERATMissSide0,
        SnoopTLBie,
        DTLBMiss,
        DSLBMiss,
        DERATMissSide1,
        TableWalkDuration
    };
    //   PMCSEL(0:1) :  PMCSEL(2:4)
    //   11 (lane3)  :   000..111   (binary)
    //   events range: 24..31
    enum LSU0_BYTE_LANE3 {
        FloatingPointLoadSide0 = 24,
        L1CachePrefetch,
        OutOfStreams,
        L2CachePrefetch,
        FloatingPointLoadSide1,
        SRQSyncDuration,
        LSU0ByteLane3Reserved1,
        NewStreamAllocated,
    };  

    // LSU1 events    
    //  PMCSEL(0:1) : PMCSEL(2:4)
    //  10 (lane0)  : 000..111 (binary) 
    // events range: 16..23
    enum LSU1_BYTE_LANE0 {
        FlushUnalignedLoadSide0=16, 
        FlushUnalignedStoreSide0, 
        FlushLRQlhlSide0, 
        FlushLRQlhsSide0, 
        FlushUnalignedLoadSide1, 
        FlushUnalignedStoreSide1, 
        FlushLRQlhlSide1, 
        FlushLRQlhsSide1 
    };

    // GPS events
    //   10 (lane0)  :   000..111   (binary)
    //   events range: 16..23
    enum GPS_BYTE_LANE0 {
        L2AccessColPreftchDST=16, 
        L2AccessColPreftchNDST, 
        L2Store, 
        L2MissOnStore, 
        L2MissSharedIntervention, 
        L2MissModifiedIntervention, 
        I1StoreOp, 
        I1StoreCompletedOnBus, 
    };
    //   11 (lane3)  :   000..111   (binary)
    //   events range: 24..31
    enum GPS_BYTE_LANE3 {
        SnoopExternal=24, 
        SnoopStateMachineDispatched, 
        SnoopRetriedDueToConflict, 
        SnoopRetriedDueToStateMachineBusy, 
        SnoopCausedTransM2ES, 
        SnoopCausedTransE2S, 
        SnoopCausedTransESR2I, 
        SnoopCausedTransM2I, 
    };

    // FPU events:
    // 10 (lane0)   :  000 .. 111 (binary)
    // events range: 16..23
    enum FPU_BYTE_LANE0 {
        FPU0Divide=16,
	FPU0MultAdd,
	FPU0SqureRoot,
	FPU0AddMultSubCmpFsel,
        FPU1Divide,
	FPU1MultAdd,
	FPU1SqureRoot,
	FPU1AddMultSubCmpFsel
    };
    //
    // 10 (lane1)   :  000 .. 111 (binary)
    // events range: 16..23
    enum FPU_BYTE_LANE1 {
        FPU0MoveEstimate=16,
	FPU0RoundConvert,
	FPU0Estimate,
	FPU0FinishedProdRes,
        FPU1MoveEstimate,
	FPU1RoundConvert,
	FPU1Estimate,
	FPU1FinishedProdRes
    };

    enum SOURCE_ENCODING {
        SourceL2Cache = 0,
        SourceMemory = 2,
        SourceSharedInterventionL2Cache = 8,
        SourceModifiedInterventionL2Cache = 10
    };

    SysStatus fetchControlRegs();
    SysStatus commitControlRegs();

    // clears out the groups that have already been programmed
    void resetGroups();

    // resets the HWPerfMon modules completely
    void reset();

    // releases the control muxes so that new groups can be programmed
    void releaseHW();

    inline uval counterRolled(uval32 v) { return  v & 0x80000000; }

    uval addToVector(uval PMCNo, uval eventId) 
    {
        uval eventIndex;

        // PMCNo is no longer used, but kept for compatibility 
	eventIndex = counterVector.allocateSpot(eventId);

	if (eventIndex == CounterVector::INVALID_INDEX) {
	    err_printf(" HWPerfMon: the counter vector is full \n");
	    return CounterVector::INVALID_INDEX;
	}
	// else
	countersDescription->setLogCounter(eventIndex, eventId);
	return eventIndex;
    }

    typedef void (HWPerfMonGPUL::* InterruptAction)(void);
    InterruptAction interruptAction;

    // interrupt handlers
    void defaultAction(void);
    void periodicTraceAction(void);
    void CPIBreakdownAction(void);
    void samplingTraceAction(void);

    inline void sync() {
         __asm __volatile("sync");
    }

    static SysStatus VPInit();

    HWPerfMonGPUL() {passertMsg(0, "Must use constructor which specifies root");}
    HWPerfMonGPUL(CObjRoot *
#ifdef MAMBO_SUPPORT
                  , PlatformType ptype
#endif
		  );

    SysStatus setPeriod(uval32 p);
    SysStatus enablePeriodicOverflow(uval32 period);
    SysStatus enablePeriodicTimerEvent(uval32 period);
    SysStatus disablePeriodic();
    SysStatus enableOverFlow();
    SysStatus disableOverFlow();

    // lower-level methods, each programming a counter to count a certain event

    // all of the following methods return:
    // The index of the logical performance counter register that is assigned to the event 
    SysStatus countCycles();
    SysStatus countCyclesInHypervisor();
    SysStatus countGroupCompleted();
    SysStatus countInstrCompleted();   // powerpc  instructions
    SysStatus countMInstrCompleted();  // micro-operations

    SysStatus countL1Prefetch();
    SysStatus countL2Prefetch();

    SysStatus countGCTFull();
    SysStatus countFPUIssueQueueFull();
    SysStatus countFPRMapperFull();
    SysStatus countGPRMapperFull();

    SysStatus countFXULSUIssueQueueFull();
    SysStatus countCRQFull();
    SysStatus countLRQFull();
    SysStatus countSRQFull();
    SysStatus countFlushOriginatedByLSU();
    SysStatus countFlushOriginatedByMispredict();

    SysStatus countInstrDispatched();
    SysStatus countDispatchValid();

    // FIXME: not sure how different are the two following methods
    SysStatus countDispatchReject();
    SysStatus countGroupDispatchReject();

    // Branch Related A:
    SysStatus countBranchMispredictDueToTarget();
    SysStatus countBranchExecutionIssueValid();
    SysStatus countBranchMispredictDueToCR();

    // Branch Related B: (I would use either A or B, and not both of them together)
    SysStatus countBranchMispredictions();
    SysStatus countConditionalBranch();

    // Flushes
    SysStatus countFlushUnalignedLoad(); 
    SysStatus countFlushUnalignedStore();
    SysStatus countFlushFromLRQ();
    SysStatus countFlushFromSRQ();

    // DCache
    SysStatus countL1DLoadMiss(); 
    SysStatus countL1DStoreMiss();
    SysStatus countL1DLoadSide0();
    SysStatus countL1DLoadSide1();
    SysStatus countL1DLoad();  // It's the sum of the two above events
    SysStatus countL1DStoreSide0();
    SysStatus countL1DStoreSide1();
    SysStatus countL1DStore();  // It's the sum of the two above events
    SysStatus countL1DEntriesInvalidatedFromL2();
    SysStatus countDataFetchedFromL2();
    SysStatus countDataFetchedFromAnotherL2Shared();
    SysStatus countDataFetchedFromAnotherL2Modified();
    SysStatus countDataFetchedFromMemory();

    SysStatus countMarkedL1DLoadMiss(); 

    // ICache
    SysStatus countInstrsFetchedFromL2(); 
    SysStatus countInstrsFetchedFromMemory(); 
    SysStatus countInstrPrefetchRequest();
    SysStatus countInstrPrefetchInstalled();
    SysStatus countIERATMiss();
    SysStatus countValidInstrAvailable();
    SysStatus countCyclesICacheWriteActive();

    // Stall Breakdown 
    SysStatus countGCTEmptyByICacheMiss();   // speculative
    SysStatus countGCTEmptyByMispredict();   // speculative
    SysStatus countComplStallByLSU();        // speculative
    SysStatus countComplStallByFPU();        // speculative
    SysStatus countComplStallByLongFPU();    // speculative
    SysStatus countComplStallByFXU();        // speculative
    SysStatus countComplStallByLongFXU();    // speculative
    SysStatus countComplStallByERATMiss();   // speculative
    SysStatus countComplStallByReject();     // speculative
    SysStatus countComplStallByDCacheMiss(); // speculative

    // TLB/SLB/ERAT
    SysStatus countITLBMiss();
    SysStatus countDTLBMiss();
    SysStatus countTLBMiss();
    SysStatus countISLBMiss();
    SysStatus countDSLBMiss();
    SysStatus countSLBMiss();
    SysStatus countDERATMiss();
    SysStatus countTableWalkDuration();

    // FPU 
    SysStatus countFPUDenormOperand();
    SysStatus countFPUStall3();
    SysStatus countFPUAddMultSubCompareFsel();
    SysStatus countFPURoundConvert();
    SysStatus countFloatingPointLoad();
    SysStatus countFloatingPointStore();
    SysStatus countFPUDivide();
    SysStatus countFPUMultAdd();
    SysStatus countFPUSquareRoot();
    SysStatus countFPUMoveEstimate();
    SysStatus countFPUEstimate();
    SysStatus countFPUFinish();
    SysStatus countFPU0FinishedAndProdRes();
    SysStatus countFPU1FinishedAndProdRes();

    // FXU
    SysStatus countFXU0ProdRes();
    SysStatus countFXU1ProdRes();

    
    // Multiprocessor related 
    SysStatus countSnoopCausedTransM2ES();
    SysStatus countSnoopCausedTransE2S();
    SysStatus countSnoopCausedTransESR2I();
    SysStatus countSnoopCausedTransM2I();

    SysStatus countL2MissSharedIntervention();
    SysStatus countL2MissModifiedIntervention();

    SysStatus countLMQReject();
    SysStatus countLSURejectERATMiss();
    SysStatus countLSURejectCDForTagCollision();

    // used for data sampling 
    SysStatus countFXUMarkedInstrFinish();
    SysStatus countFPUMarkedInstrFinish();
    SysStatus countLSUMarkedInstrFinish();
    SysStatus countMarkedGroupComplete();

    SysStatus programCounterGroup0();  // the delay group
    SysStatus programCounterGroup1();
    SysStatus programCounterGroup3();
    SysStatus programCounterGroup4();
    SysStatus programCounterGroup5();
    SysStatus programCounterGroup24();
    SysStatus programCounterGroup31();
    SysStatus programCounterGroup32();
    SysStatus programCounterGroup33();
    SysStatus programCounterGroup34();
    SysStatus programCounterGroup35();
    SysStatus programCounterGroup36();
    SysStatus programCounterGroup37();  // data sampling
    SysStatus programCounterGroup38();  // snoop-related invalidations
    SysStatus programCounterGroup39();  // speculative counts
    SysStatus programCounterGroup40();  // speculative counts
    SysStatus programCounterGroup41();  // speculative counts
    SysStatus programCounterGroup42();  // speculative counts
    SysStatus programCounterGroup43();  // FPU issue queue and rename buffers
    SysStatus programCounterGroup44();  // Flushes and rejects
    SysStatus programCounterGroup45();  // speculative counts
    SysStatus programCounterGroup46();  // speculative counts
    SysStatus programCounterGroup47();  // Other L2 stats
    SysStatus programCounterGroup50();  // finished ld/st data sampling 
    SysStatus programCounterGroup51();  // L1DCache Load Miss sampling
    SysStatus programCounterGroup52();  // completed ld/st data sampling 


    // Power Modeling Counters 
    SysStatus programCounterGroup60();  
    SysStatus programCounterGroup61();  
    SysStatus programCounterGroup62();  
    SysStatus programCounterGroup63();  
    SysStatus programCounterGroup64();  

    void makeCPIBundle();

    void setMode();

public:
    // just to be compatible with a higher level interface
    virtual SysStatus acquire()  { return 0; };
    virtual SysStatus release()  { return 0; };

    // FIXME: this is a hack really to do the proof  of concept
    void handleTimerEvent(void) { periodicTraceAction(); };

    virtual SysStatus HWPerfInterrupt();

    virtual SysStatus addGroup(uval32 groupNo, uval32 share, uval32 samplingFreqBase=0);
    virtual SysStatus removeGroup(uval32 groupNo);

    virtual SysStatus startSampling(uval64 delay);
    virtual SysStatus stopSampling();

    virtual SysStatus startWatch();
    virtual SysStatus logAndResetWatch();
    virtual SysStatus stopWatch();

    virtual SysStatus startCPIBreakdown(uval64 delay);
    virtual SysStatus stopCPIBreakdown();

#ifdef MAMBO_SUPPORT
    virtual SysStatus startMamboGroup(uval32 groupNo, uval32 period);
#endif 

    virtual SysStatus logCountersToTrace() {
	countersDescription->logCounters();
	return 0;
    }

};

// trace timer event as an interim solution for not having overflow exceptions
// on Mambo
// FIXME: 
// must be called from the vp it is running on - no cross vp support
class HPMTimerEvent : protected TimerEvent {
    VPNum myvp;
    SysTime interval;
    HWPerfMonGPUL *hpm;
    bool enabled;

public:
    DEFINE_PINNEDLOCALSTRICT_NEW(HPMTimerEvent);

    HPMTimerEvent(HWPerfMonGPUL *_hpm) {
	myvp = Scheduler::GetVP();
	// interval = 1000000000; // 0x3b9aca00
        hpm = _hpm;
        enabled = false;
    }

    void enable(uval32 period) {  
        // FIXME: I should do some calculations to get it in terms of the micro-secs
        interval = period;
	disabledScheduleEvent(interval, TimerEvent::relative);
        enabled = true;
    }

    virtual void handleEvent() {
        if (enabled) {
           hpm->handleTimerEvent();
	   disabledScheduleEvent(interval, TimerEvent::relative);
        }
	return;
    }

    void disable() {
        enabled = false;
    }

    
};

#endif /* #ifndef __HW_PERF_MON_GPUL_H */
