/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: HWPerfMonGPUL.C,v 1.7 2005/08/22 21:48:59 bob Exp $
 *****************************************************************************/
#include "kernIncs.H"
#include "HWPerfMonGPUL.H"
#include "bilge/arch/powerpc/BootInfo.H"
#include "trace/traceHWPerfMon.h"
#include "exception/ExceptionLocal.H"

#ifdef MAMBO_SUPPORT

PMCCtlRegSetMap pmcCtlRegSetMaps[] =
{
  /* Group 0 - Time Slice 0 */
  { 0, 0x0000051E, 0x000000000A46F18CULL, 0x00002000},
  /* Group 1 - Group for use with eprof */
  { 1, 0x00000F1E, 0x4003001005F09000ULL, 0x00002000},
  /* Group 2 - Basic performance indicators */
  { 2, 0x0000091E, 0x4003001005F09000ULL, 0x00002000},
  /* Group 3 - Information on the Load Store Unit */
  { 3, 0x00000000, 0x000F00007A400000ULL, 0x00002000},
  /* Group 4 - Floating Point events */
  { 4, 0x00000000, 0x00000000001E0480ULL, 0x00002000},
  /* Group 5 - Floating Point events */
  { 5, 0x00000000, 0x000020E87A400000ULL, 0x00002000},
  /* Group 6 - ISU Rename Pool Events */
  { 6, 0x00001228, 0x400000218E6D84BCULL, 0x00002000},
  /* Group 7 - ISU Rename Pool Events */
  { 7, 0x0000132E, 0x40000000851E994CULL, 0x00002000},
  /* Group 8 - ISU Instruction Flow Events */
  { 8, 0x0000181E, 0x400000B3D7B7C4BCULL, 0x00002000},
  /* Group 9 - ISU Indicators of Work Blockage */
  { 9, 0x00000402, 0x400000050FDE9D88ULL, 0x00002000},
  /* Group 10 - Floating Point events by unit */
  {10, 0x00001028, 0x000000008D6354BCULL, 0x00002000},
  /* Group 11 - Floating Point events by unit */
  {11, 0x0000122C, 0x000000009DE774BCULL, 0x00002000},
  /* Group 12 - Floating Point events by unit */
  {12, 0x00001838, 0x000000C0851E9958ULL, 0x00002000},
  /* Group 13 - Floating Point events by unit */
  {13, 0x0000193A, 0x000000C89DDE97E0ULL, 0x00002000},
  /* Group 14 - LSU Flush Events */
  {14, 0x0000122C, 0x000C00007BE774BCULL, 0x00002000},
  /* Group 15 - LSU Load Events */
  {15, 0x00001028, 0x000F0000851E9958ULL, 0x00002000},
  /* Group 16 - LSU Store Events */
  {16, 0x0000112A, 0x000F00008D5E99DCULL, 0x00002000},
  /* Group 17 - LSU Store Events */
  {17, 0x00001838, 0x0003C0D08D76F4BCULL, 0x00002000},
  /* Group 18 - Information on the Load Store Unit */
  {18, 0x0000122C, 0x000830047BD2FE3CULL, 0x00002000},
  /* Group 19 - Misc Events for testing */
  {19, 0x00000404, 0x0000000023C69194ULL, 0x00002000},
  /* Group 20 - PE Benchmarker group for FP analysis */
  {20, 0x00000000, 0x10001002001E0480ULL, 0x00002000},
  /* Group 21 - PE Benchmarker group for L1 and TLB */
  {21, 0x00001420, 0x000B000004DE9000ULL, 0x00002000},
  /* Group 22 - Hpmcount group for L1 and TLB behavior */
  {22, 0x00001404, 0x000B000004DE9000ULL, 0x00002000},
  /* Group 23 - Hpmcount group for computation */
  {23, 0x00000000, 0x000020289DDE0480ULL, 0x00002000},
  /* Group 24 - L1 misses and branch misspredict analysis */
  {24, 0x0000091E, 0x8003C01D0636FCE8ULL, 0x00002000},
  /* Group 25 - Instruction mix: loads, stores and branches */
  {25, 0x0000091E, 0x8003C021061FB000ULL, 0x00002000},
  /* Group 26 - Information on marked instructions */
  {26, 0x00000006, 0x00008080790852A4ULL, 0x00002001},
  /* Group 27 - Marked Instructions Processing Flow */
  {27, 0x0000020A, 0x0000000079484210ULL, 0x00002001},
  /* Group 28 - Marked Stores Processing Flow */
  {28, 0x0000031E, 0x00203004190A3F24ULL, 0x00002001},
  /* Group 29 - Load Store Unit Marked Events */
  {29, 0x00001B34, 0x000280C08D5E9850ULL, 0x00002001},
  /* Group 30 - Load Store Unit Marked Events */
  {30, 0x00001838, 0x000280C0959E99DCULL, 0x00002001}
};
#endif // MAMBO_SUPPORT

#define MIN_ROUND_COUNT 0
#define MAX_ROUND_COUNT 1000000000

#define CHECK(func) \
{ if (!func()) {\
      err_printf("Conflict in setting a counter \n");\
      return -1;\
      }\
}

/* static */ SysStatus
HWPerfMonGPUL::VPInit()
{
    // The reason these new local variables are declared is
    // that this method is a static method, and therefore, 
    // normal object attributes (e.g. mmcr0, or pmc1) cannot be
    // accessed here. One way to solve this, is to make everything
    // static since we have only one instance of each hardware resource
    // (e.g. control registers, hardware counters, etc.)  

    MMCR0 s_mmcr0; 
    MMCR1 s_mmcr1; 
    MMCRA s_mmcra; 
    PMC1 s_pmc1;

    s_mmcr0.value = 0;
    s_mmcr1.value = 0;
    s_mmcra.value = 0;

    s_mmcr0.bits.freeze_sv   = 1; // disable supervisor mode
    s_mmcr0.bits.freeze_pr   = 1; // disable problem mode
    s_mmcr0.bits.monitor_on  = 1; // disable when monitoring is on
    s_mmcr0.bits.monitor_off = 1; // disable when monitoring is off
    s_mmcr0.bits.fcece       = 1; // freeze counters when perf uval occurs
    s_mmcr0.bits.tbee        = 0; // turn off Timebase interrupts
    s_mmcr0.bits.pmc1ce      = 0; // turn off PMC1 counter overflow intrs
    s_mmcr0.bits.pmcNce      = 0; // turn off counter overflow intrs for counts >1
    s_mmcr0.bits.interrupt   = 0; // turn off Perf Monitoring interrupts
    s_mmcr0.bits.fch         = 1; // freez counters in hypervisor mode

    s_pmc1.set(0);
    s_mmcr0.set();
    s_mmcr1.set();
    s_mmcra.set();

  
    return 0;
}

void
HWPerfMonGPUL::resetGroups()
{
    for (uval i = 0; i < maxGroups; i++) {
        groupList[i].reset();
    }
    groupNum = 0;
    groupCurrent = 0;
}

void 
HWPerfMonGPUL::reset()
{
    roundCount = 0;
    countingMode = CM_BOTH;
    periodType = PT_TIME;
    multiplexingRound = 1000000;
    logPeriod = multiplexingRound;
    logRatio = 1;
    samplingDelay = 0;
    watching = false;

    resetGroups();
    releaseHW();
    counterVector.reset();
    cpiBundle.reset();
}

void 
HWPerfMonGPUL::releaseHW()
{
    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
        pmcs[i]->release();
    }

    for (uval i = 0; i < 4; i++) {
        td_cp_dbg_mux[i] = TD_CP_DBGX_UNUSED;
    }
  
    ttm0_mux = TTM0_UNUSED;
    ttm1_mux = TTM1_UNUSED;
    ttm3_9_mux = TTM3_9_UNUSED;
    ttm3_10_mux = TTM3_10_UNUSED;
}

HWPerfMonGPUL::HWPerfMonGPUL(CObjRoot *root
#ifdef MAMBO_SUPPORT
                             , PlatformType ptype
#endif 
) : HWPerfMon(root)
{
    busy = 0;
    pmcs[1] = &pmc1; pmcs[2] = &pmc2; pmcs[3] = &pmc3; pmcs[4] = &pmc4;
    pmcs[5] = &pmc5; pmcs[6] = &pmc6; pmcs[7] = &pmc7; pmcs[8] = &pmc8;

    totalSharesBits = 5;
    maxGroups = MAX_GROUPS;

    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
        events[i] = CounterVector::INVALID_INDEX;
    }

    countersDescription = CountersDescription::createCountersDescription();
    interruptAction = &HWPerfMonGPUL::periodicTraceAction;

    reset();
    timerEvent = new HPMTimerEvent(this);
#ifdef MAMBO_SUPPORT
    platform_type = ptype;
#endif 
}

/* virtual */ SysStatus
HWPerfMonGPUL::setPeriod(uval32 p)
{
    uval32 countdownValue; 
    if ( p <= (uval32)0x7FFFFFFF ) {
        countdownValue  = (uval32)0x7FFFFFFF - p;
    } else {
        //FIXME: Silently reducing requested period to max supported
        countdownValue = 0;
    }

    if (periodType == PT_TIME) {
        timer = cycleCounter;
    } else 
    if (periodType == PT_INSTR) {
        timer = instrCmplCounter;
    }

    pmcs[timer]->set(countdownValue);
    return 0;
}

void
HWPerfMonGPUL::setMode()
{
    // ASSUMPTION:
    // the counting mode attribute is alread set by the user through
    //  calling the setMode method of the HWPerfMon class.
    switch (countingMode) {
    case 0:  // count both 
        // err_printf("COUNTING MODE: Both User and OS \n");
        mmcr0.bits.fch = 0;
        mmcr0.bits.freeze_sv   = 0; // enable supervisor mode
        mmcr0.bits.freeze_pr   = 0; // enable problem mode
        break;
    case 1:  // kernel only 
        // err_printf("COUNTING MODE: Kernel Only \n");
        mmcr0.bits.fch = 0;
        mmcr0.bits.freeze_sv   = 0; // enable supervisor mode
        mmcr0.bits.freeze_pr   = 1; // disable problem mode
        break;
    case 2:  // user only 
        // err_printf("COUNTING MODE: User Only \n");
        mmcr0.bits.fch = 1;
        mmcr0.bits.freeze_sv   = 1; // disable supervisor mode
        mmcr0.bits.freeze_pr   = 0; // enable problem mode
        break;
    default: // count none
        // err_printf("COUNTING MODE: None \n");
        mmcr0.bits.fch = 1;
        mmcr0.bits.freeze_sv   = 1; // disable supervisor mode
        mmcr0.bits.freeze_pr   = 1; // disable problem mode
        break;
    }
}

/* virtual */ SysStatus
HWPerfMonGPUL::enablePeriodicOverflow(uval32 period)
{
    // FIXME: fix race conditions here eg. disable interrupts first
    setPeriod(period);
    setMode();          // set the counting mode to user or kernel only, or both
  
    mmcr0.bits.freeze      = 0; // the same as enable
    mmcr0.bits.monitor_on  = 0; // don't freeze when monitoring is on
    mmcr0.bits.monitor_off = 0; // don't freeze when monitoring is off
    mmcr0.bits.fcece       = 1; // See .H file
    mmcr0.bits.tbee        = 0; // Turn off Timebase interrupts
    mmcr0.bits.pmc1ce      = 1; // Turn on PMC1 counter overflow intrs
    mmcr0.bits.pmcNce      = 1; // Turn on counter overflow intrs for counts >1
    mmcr0.bits.interrupt   = 1; // Turn on Perf Monitoring interrupts

    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::enablePeriodicTimerEvent(uval32 period)
{
    timerEvent->enable(period);  // to have the trace action called periodically
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::disablePeriodic()
{
    // FIXME: fix race conditions here eg. disable interrupts first

    mmcr0.get();

    mmcr0.bits.pmc1ce  = 0;   // Turn off PMC1 counter overflow intrs
    mmcr0.bits.pmcNce  = 0;   // Turn off PMCN counter overflow intrs

    sync();
    mmcr0.set();
    sync();	
   
#ifdef MAMBO_SUPPORT
    if (platform_type == GPUL_MAMBO) { 
        timerEvent->disable();
    }
#endif 

    return 0;
}

SysStatus
HWPerfMonGPUL::countCycles()
{
    // only useful where there is no overflow exceptions
    // this is a direct event
    // all counters can count this
    const uval counters[] = {1,2,3,4,5,6,7,8};
    const uval num_counters = 8;

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, PMC::Cycles, LOW_BYTE); 
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_CYC);
           return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countInstrCompleted() 
{
    // This is  a direct event
    // all counters can count this
    const uval counters[] = {1,2,3,4,5,6,7,8};
    const uval num_counters = 8;

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, PMC::InstrCompleted_2, LOW_BYTE); 
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_INST_CMPL);
           return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countMInstrCompleted() 
{
    // This is  a direct event
    // all counters can count this
    const uval counters[] = {1,4,6,7,8};
    const uval num_counters = 5;

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, PMC1::InstrsCompleted, LOW_BYTE); 
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_MINST_CMPL);
           return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus 
HWPerfMonGPUL::countGCTFull()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, GCTFull, HIGH_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_GCT_FULL);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}
SysStatus 
HWPerfMonGPUL::countFPRMapperFull()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, FPRMapperFull, HIGH_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_FPR_MAPPER_FULL);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus 
HWPerfMonGPUL::countFPUIssueQueueFull()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    // also  
    // it's an adder 3+7 event, i.e.: 
    //  FXU/LSU issue queue full=  FPU0 issue queue full (3) +  FPU1 issue queue full (7)
    // so, only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    pmc5.assignEvent(&mmcr0 , &mmcr1, PMC5::Add3_7, HIGH_BYTE); 
    events[5] = addToVector((uval32)5, TRACE_GP970_FPU_ISSUE_QUEUE_FULL);
    return 5; 

}

SysStatus
HWPerfMonGPUL::countFXULSUIssueQueueFull()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    // also  
    // it's an adder 0+4 event, i.e.: 
    //  FXU/LSU issue queue full=  FXU0/LSU0 issue queue full (0) +  FXU1/LSU1 issue queue full (4)
    // so, only pmc8 can count this
    if (pmc8.busy) {
        return 0;
    }

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = TTM0;
        td_cp_dbg_mux[1] = TTM0;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    pmc8.assignEvent(&mmcr0 , &mmcr1, PMC8::Add0_4, HIGH_BYTE); 
    events[8] = addToVector((uval32)8, TRACE_GP970_FXU_LSU_ISSUE_QUEUE_FULL);
    return 8; 
}

SysStatus 
HWPerfMonGPUL::countCRQFull()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = TTM0;
        td_cp_dbg_mux[1] = TTM0;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, CRIssueQueueFull, HIGH_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_CRQ_FULL);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus 
HWPerfMonGPUL::countLRQFull()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = TTM0;
        td_cp_dbg_mux[1] = TTM0;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else 
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, LRQFull, HIGH_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_LRQ_FULL);
            return counters[i];
        }
    }  
    return 0;   //  no counters available

}

SysStatus 
HWPerfMonGPUL::countSRQFull()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = TTM0;
        td_cp_dbg_mux[1] = TTM0;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != TTM0)  {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0)  {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, SRQFull, HIGH_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_SRQ_FULL);
            return counters[i];
        }
    }  
    return 0;   //  no counters available

}

SysStatus 
HWPerfMonGPUL::countFlushOriginatedByLSU()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = TTM0;
        td_cp_dbg_mux[1] = TTM0;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
	                                   FlushOriginatedByLSU, 
					   HIGH_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_FLUSH_ORG_LSU);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus 
HWPerfMonGPUL::countFlushOriginatedByMispredict()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = TTM0;
        td_cp_dbg_mux[1] = TTM0;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
       return 0;
    } 

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
                                           &mmcr1, 
                                           FlushOriginatedByMispredict, 
                                           HIGH_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_FLUSH_ORG_BR_MPRED);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}


SysStatus
HWPerfMonGPUL::countInstrDispatched()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, InstrsDispatched, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_INST_DISP);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countDispatchValid()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }
 
    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, DispatchValid, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_GRP_DISP_VALID);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countDispatchReject()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, DispatchReject, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_GRP_DISP_REJECT);
            return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countGPRMapperFull()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = TTM0;
        td_cp_dbg_mux[3] = TTM0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, GPRMapperFull, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_GPR_MAPPER_FULL);
            return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countFXU0ProdRes()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = TTM1;
        td_cp_dbg_mux[3] = TTM1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM1) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm1_mux == TTM1_UNUSED) {
        mmcr1.bits.ttm1sel = ISU_TTM1;
        ttm1_mux = ISU_TTM1;
    }
    else
    if (ttm1_mux != ISU_TTM1) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, FXU0ProducedResult, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_FXU0_PROD_RES);
            return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countFXU1ProdRes()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = TTM1;
        td_cp_dbg_mux[3] = TTM1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM1) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm1_mux == TTM1_UNUSED) {
        mmcr1.bits.ttm1sel = ISU_TTM1;
        ttm1_mux = ISU_TTM1;
    }
    else
    if (ttm1_mux != ISU_TTM1) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, FXU1ProducedResult, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_FXU1_PROD_RES);
            return counters[i];
       }
    }  
    return 0;   //  no counters available
}


SysStatus
HWPerfMonGPUL::countBranchMispredictions()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // FIXME: it can be programmed both on TTM0 and TTM1,
    // I'm only using TTM0 here 

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, BranchMispredict, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_GRP_EXP_BR_MPRED);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countBranchMispredictDueToCR()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = TTM0;
        td_cp_dbg_mux[3] = TTM0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM0) {
        return 0; 
    }

    // this is an IFU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    }
    else
    if (ttm0_mux != IFU) {
       return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
                                           BranchMispredictDuToCR, LOW_BYTE); 

	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_BR_MPRED_CR);
            return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countBranchMispredictDueToTarget()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = TTM0;
        td_cp_dbg_mux[3] = TTM0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM0) {
        return 0; 
    }

    // this is an IFU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    }
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   BranchMispredictDuToTarget, 
					   LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_BR_MPRED_TA);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countCyclesICacheWriteActive()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = TTM0;
        td_cp_dbg_mux[3] = TTM0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM0) {
        return 0; 
    }

    // this is an IFU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    }
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
                                           CyclesL1ICacheWriteActive,           
					   LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_CYCLES_ICACHE_WRITE_ACTIVE);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countBranchExecutionIssueValid()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8  can count this
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = TTM0;
        td_cp_dbg_mux[3] = TTM0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM0) {
        return 0; 
    }

    // this is an IFU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    }
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
 	                                  &mmcr1, 
 					  BranchExecutionIssueValid, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_BR_ISSUED);
            return counters[i];
       }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countL1Prefetch()
{
    // it's on byte late 3 (LOW_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.


    // it's an LSU0 event. 
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg3sel = LSU0;
        td_cp_dbg_mux[3] = LSU0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU0) {
        return 0; 
    }

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L1CachePrefetch, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_L1_PRFTCH);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countL2Prefetch()
{
    // it's on byte late 3 (LOW_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.


    // it's an LSU0 event. 
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg3sel = LSU0;
        td_cp_dbg_mux[3] = LSU0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU0) {
        return 0; 
    }

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L2CachePrefetch, LOW_BYTE); 
	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_L2_PRFTCH);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus
HWPerfMonGPUL::countFloatingPointLoad()
{
    // it's on byte late 3 (LOW_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.
    // also 
    // it's an adder 0+4 event, i.e.: 
    //  floating point load =  FloatingPointLoadSide0 (0) +  FloatingPointLoadSide1 (4)

    // So only pmc8 can do this

    if (pmc8.busy) {
        return 0;
    }


    // it's an LSU0 event. 
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg3sel = LSU0;
        td_cp_dbg_mux[3] = LSU0;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU0) {
        return 0; 
    }

    events[8] = addToVector((uval32)8, TRACE_GP970_FPU_LOAD);
    pmc8.assignEvent(&mmcr0 , &mmcr1, PMC8::Add0_4, LOW_BYTE); 
    return 8; 
}

SysStatus
HWPerfMonGPUL::countFloatingPointStore()
{
    // it's on byte late 2 (LOW_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this.
    // also 
    // it's an adder 2+6 event, i.e.: 
    //  FPU Store =  FPU0 Store (2) +  FPU1 Store (6)

    // So only pmc6 can do this

    if (pmc6.busy) {
        return 0;
    }


    // it's an FPU event. 
    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    events[6] = addToVector((uval32)6, TRACE_GP970_FPU_STORE);
    pmc6.assignEvent(&mmcr0 , &mmcr1, PMC6::Add2_6, LOW_BYTE); 
    return 6; 
}

SysStatus
HWPerfMonGPUL::countFPUAddMultSubCompareFsel()
{
    // it's on byte late 0 (HIGH_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this.
    // also 
    // it's an adder 3+7 event, i.e.: 
    //  FPU all =  FPU0 all (3) +  FPU1 all (7)

    // So only pmc5 can do this

    if (pmc5.busy) {
        return 0;
    }


    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
       return 0;
    }

    events[5] = addToVector((uval32)5, TRACE_GP970_FPU_ADD_MUL_SUB_CMP_FSEL);
    pmc5.assignEvent(&mmcr0 , &mmcr1, PMC5::Add3_7, HIGH_BYTE); 
    return 5;
}

SysStatus
HWPerfMonGPUL::countFPURoundConvert()
{
    // it's on byte late 1 (HIGH_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.
    // also 
    // it's an adder 1+5 event, i.e.: 
    //  FPU Round Convert =  FPU0 Round Convert  (1) +  FPU1  Round Convert (5)

    // So only pmc7 can do this

    if (pmc7.busy) {
        return 0;
    }


    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    events[7] = addToVector((uval32)7, TRACE_GP970_FPU_ROUND_CONV);
    pmc7.assignEvent(&mmcr0 , &mmcr1, PMC7::Add1_5, HIGH_BYTE); 
    return 7;
}

SysStatus 
HWPerfMonGPUL::countFPUDenormOperand()
{
    // it's on byte late 2 (LOW_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this.
    // also 
    // it's an adder 0+4 event, i.e.: 
    //  FPU Denorm Op =  FPU0 Denorm Op (0) +  FPU1 Denorm Op (4)

    // So only pmc1 can do this

    if (pmc1.busy) {
        return 0;
    }


    // it's an FPU event. 
    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
       return 0;
    }

    events[1] = addToVector((uval32)1, TRACE_GP970_FPU_DENORM);
    pmc1.assignEvent(&mmcr0 , &mmcr1, PMC1::Add0_4, LOW_BYTE); 
    return 1; 

}

SysStatus 
HWPerfMonGPUL::countFPUStall3()
{
    // it's on byte late 2 (LOW_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this.
    // also 
    // it's an adder 1+5 event, i.e.: 
    //  FPU stall =  FPU0 stall  (1) +  FPU1 stall Op (5)

    // So only pmc1 can do this

    if (pmc2.busy) {
        return 0;
    }

    // it's an FPU event. 
    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    events[2] = addToVector((uval32)2, TRACE_GP970_FPU_STALL3);
    pmc2.assignEvent(&mmcr0 , &mmcr1, PMC2::Add1_5, LOW_BYTE); 
    return 2;  

}

SysStatus 
HWPerfMonGPUL::countFPUDivide()
{
    // it's on byte late 0 (HIGH_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this.
    // also 
    // it's an adder 0+4 event, i.e.: 
    //  FPU Div =  FPU0 Div (0) +  FPU1 Div (4)

    // So only pmc1 can do this

    if (pmc1.busy) {
        return 0;
    }

    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
       return 0;
    }

    events[1] = addToVector((uval32)1, TRACE_GP970_FPU_DIV);
    pmc1.assignEvent(&mmcr0 , &mmcr1, PMC1::Add0_4, HIGH_BYTE); 
    return 1;   
}

SysStatus 
HWPerfMonGPUL::countFPUMultAdd()
{
    // it's on byte late 0 (HIGH_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this.
    // also 
    // it's an adder 1+5 event, i.e.: 
    //  FPU all =  FPU0 MultAdd (1) +  FPU1 MultAdd (5)

    // So only pmc2 can do this

    if (pmc2.busy) {
        return 0;
    }

    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
       return 0;
    }

    events[2] = addToVector((uval32)2, TRACE_GP970_FPU_MUL_ADD);
    pmc2.assignEvent(&mmcr0 , &mmcr1, PMC2::Add1_5, HIGH_BYTE); 
    return 2;  
}

SysStatus 
HWPerfMonGPUL::countFPUSquareRoot()
{
    // it's on byte late 0 (HIGH_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this.
    // also 
    // it's an adder 2+6 event, i.e.: 
    //  FPU all =  FPU0 SquareRoot (2) +  FPU1 SquareRoot (6)

    // So only pmc6 can do this

    if (pmc6.busy) {
        return 0;
    }

    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
       return 0;
    }

    events[6] = addToVector((uval32)6, TRACE_GP970_FPU_SQRT);
    pmc6.assignEvent(&mmcr0 , &mmcr1, PMC6::Add2_6, HIGH_BYTE); 
    return 6;  
}


SysStatus 
HWPerfMonGPUL::countFPUMoveEstimate()
{
    // it's on byte late 1 (HIGH_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.
    // also 
    // it's an adder 0+4 event, i.e.: 
    //  FPU Move Estimate =  FPU0 Move Estimate  (0) +  FPU1  Move Estimate (4)

    // So only pmc8 can do this

    if (pmc8.busy) {
        return 0;
    }


    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    events[8] = addToVector((uval32)8, TRACE_GP970_FPU_MOV_EST);
    pmc8.assignEvent(&mmcr0 , &mmcr1, PMC8::Add0_4, HIGH_BYTE); 
    return 8;
}

SysStatus 
HWPerfMonGPUL::countFPU0FinishedAndProdRes()
{
    // it's on byte late 1 (HIGH_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   FPU0FinishedProdRes, 
					   HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_FPU0_FIN_PROD_RES);
            return counters[i];
        }
    }

    return 0;
}

SysStatus 
HWPerfMonGPUL::countFPU1FinishedAndProdRes()
{
    // it's on byte late 1 (HIGH_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   FPU1FinishedProdRes, 
					   HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_FPU1_FIN_PROD_RES);
            return counters[i];
        }
    }

    return 0;
}

SysStatus 
HWPerfMonGPUL::countFPUEstimate()
{   
    // it's on byte late 1 (HIGH_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.
    // also 
    // it's an adder 2+6 event, i.e.: 
    //  FPU Estimate =  FPU0 Estimate  (2) +  FPU1  Estimate (6)

    // So only pmc3 can do this

    if (pmc3.busy) {
        return 0;
    }


    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    events[3] = addToVector((uval32)3, TRACE_GP970_FPU_EST);
    pmc3.assignEvent(&mmcr0 , &mmcr1, PMC3::Add2_6, HIGH_BYTE); 
    return 3;

}

SysStatus 
HWPerfMonGPUL::countFPUFinish()
{    
    // it's on byte late 1 (HIGH_BYTE), hence
    // pmc3, pmc4, pmc7, and pmc8 can do this.
    // also 
    // it's an adder 3+7 event, i.e.: 
    //  FPU Finish =  FPU0 Finish  (3) +  FPU1  Finish (7)

    // So only pmc4 can do this

    if (pmc4.busy) {
        return 0;
    }


    // it's an FPU event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM0;
        td_cp_dbg_mux[0] = TTM0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM0) {
        return 0; 
    }

    // it's an FPU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = FPU;
        ttm0_mux = FPU;
    } 
    else
    if (ttm0_mux != FPU) {
        return 0;
    }

    events[4] = addToVector((uval32)4, TRACE_GP970_FPU_FINISH);
    pmc4.assignEvent(&mmcr0 , &mmcr1, PMC4::Add3_7, HIGH_BYTE); 
    return 4;

}


SysStatus 
HWPerfMonGPUL::countFlushUnalignedLoad() 
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 0+4 event, i.e.: 
    // FlushUnalignedLoad =  FlushUnalignedLoadSide0 (0) +  FlushUnalignedLoadSide1 (4)

    // So only pmc1 can count this
    if (pmc1.busy) {
        return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = LSU1;
        td_cp_dbg_mux[0] = LSU1;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU1) {
        return 0; 
    }

    events[1] = addToVector((uval32)1, TRACE_GP970_LSU_FLUSH_ULD);
    pmc1.assignEvent(&mmcr0 , &mmcr1, PMC1::Add0_4, HIGH_BYTE); 
    return 1;	
}

SysStatus 
HWPerfMonGPUL::countFlushUnalignedStore()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 1+5 event, i.e.: 
    // FlushUnalignedStore =  FlushUnalignedStoreSide0 (1) +  FlushUnalignedStoreSide1 (5)

    // So only pmc2 can count this
    if (pmc2.busy) {
        return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = LSU1;
        td_cp_dbg_mux[0] = LSU1;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU1) {
        return 0; 
    }

    events[2] = addToVector((uval32)2, TRACE_GP970_LSU_FLUSH_UST);
    pmc2.assignEvent(&mmcr0 , &mmcr1, PMC2::Add1_5, HIGH_BYTE); 
    return 2;	

}

SysStatus 
HWPerfMonGPUL::countFlushFromLRQ()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 0+4 event, i.e.: 
    // FlushLRQ =  FlushLRQSide0 (2) +  FlushLRQSide1 (6)

    // So only pmc6 can count this
    if (pmc6.busy) {
        return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = LSU1;
        td_cp_dbg_mux[0] = LSU1;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU1) {
        return 0; 
    }

    events[6] = addToVector((uval32)6, TRACE_GP970_LSU_FLUSH_LRQ);
    pmc6.assignEvent(&mmcr0 , &mmcr1, PMC6::Add2_6, HIGH_BYTE); 
    return 6;	

}

SysStatus 
HWPerfMonGPUL::countFlushFromSRQ()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 3+7 event, i.e.: 
    // FlushSRQ =  FlushSRQSide0 (3) +  FlushSRQSide1 (7)

    // So only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = LSU1;
        td_cp_dbg_mux[0] = LSU1;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU1)  {
        return 0; 
    }

    events[5] = addToVector((uval32)5, TRACE_GP970_LSU_FLUSH_SRQ);
    pmc5.assignEvent(&mmcr0 , &mmcr1, PMC5::Add3_7, HIGH_BYTE); 
    return 5;	

}

SysStatus
HWPerfMonGPUL::countL1DEntriesInvalidatedFromL2()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   L1DEntriesInvalidatedFromL2, 
					   HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_L1_INV_L2);
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countL1DLoadMiss()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    // also 
    // it's an adder 2+6 event, i.e.: 
    //  l1dcache load miss =  L1DCacheLoadMissSide0 (2) +  L1DCacheLoadMissSide1 (6)

    // So only pmc3 can count this
    if (pmc3.busy) {
        return 0;
    }

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }
 
    events[3] = addToVector((uval32)3, TRACE_GP970_LD_MISS_L1);
    pmc3.assignEvent(&mmcr0 , &mmcr1, PMC3::Add2_6, HIGH_BYTE); 
    return 3;	
}

SysStatus
HWPerfMonGPUL::countMarkedL1DLoadMiss()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 0+4 event, i.e.: 
    //  Marked l1dcache load miss =  Marked L1DCacheLoadMissSide0 (0) +  Marked L1DCacheLoadMissSide1 (4)

    // So only pmc1 can count this
    if (pmc1.busy) {
        return 0;
    }

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg2sel = LSU0;
        td_cp_dbg_mux[2] = LSU0;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[2] != LSU0) {
        return 0; 
    }
 
    events[1] = addToVector((uval32)1, TRACE_GP970_MARKED_LD_MISS_L1);
    pmc1.assignEvent(&mmcr0 , &mmcr1, PMC1::Add0_4, LOW_BYTE); 
    sampleEvent = TRACE_GP970_MARKED_LD_MISS_L1;
    return 1;	
}

SysStatus
HWPerfMonGPUL::countL1DStoreMiss()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;


    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_ST_MISS_L1);
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L1DCacheStoreMiss, HIGH_BYTE); 
            return counters[i];
        }
    }  
    return 0;
} 

SysStatus
HWPerfMonGPUL::countL1DLoadSide0()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_LD_REF_L1_0);
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L1DCacheLoadSide0, HIGH_BYTE); 
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countL1DLoadSide1()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_LD_REF_L1_1);
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L1DCacheLoadSide1, HIGH_BYTE); 
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countL1DStoreSide0()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_ST_REF_L1_0);
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L1DCacheStoreSide0, HIGH_BYTE); 
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countL1DStoreSide1()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 

    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_ST_REF_L1_1);
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L1DCacheStoreSide1, HIGH_BYTE); 
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countL1DLoad()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    // also 
    // it's an adder 0+4 event, i.e.: 
    //  l1dcache load =  L1DCacheLoadSide0 (0) +  L1DCacheLoadSide1 (4)

    // So only pmc8 can count this
    if (pmc8.busy) {
        return 0;
    }

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    events[8] = addToVector((uval32)8, TRACE_GP970_LD_REF_L1);
    pmc8.assignEvent(&mmcr0 , &mmcr1, PMC8::Add0_4, HIGH_BYTE); 
    return 8;
}

SysStatus
HWPerfMonGPUL::countL1DStore()
{
    // it's on byte lane 1 (HIGH_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    // also 
    // it's an adder 1+5 event, i.e.: 
    //  l1dcache store =  L1DCacheStoreSide0 (1) +  L1DCacheStoreSide1 (5)

    // So only pmc7 can count this
    if (pmc7.busy) {
        return 0;
    }

    // byte lane 1 is controlled by td_cp_dbg1, check if it's programmed
    if (td_cp_dbg_mux[1] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg1sel = LSU1;
        td_cp_dbg_mux[1] = LSU1;
    }
    else 
    // check if td_cp_dbg1 is already programmed with something else
    if (td_cp_dbg_mux[1] != LSU1) {
        return 0; 
    }

    events[7] = addToVector((uval32)7, TRACE_GP970_ST_REF_L1);
    pmc7.assignEvent(&mmcr0 , &mmcr1, PMC7::Add1_5, HIGH_BYTE); 
    return 7;	
}

SysStatus 
HWPerfMonGPUL::countDataFetchedFromL2()
{
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = LSU1;
        td_cp_dbg_mux[3] = LSU1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU1) {
        return 0; 
    }

    // FIXME only pmc1 can do this (direct event with source 0000)
    // It's on byte lane 3 (LOW_BYTE) 
   
    // check ttm3sel10 as well
    if (ttm3_10_mux == TTM3_10_UNUSED) {
        mmcr1.bits.ttm3sel10 = LSU1_3;
        ttm3_10_mux = LSU1_3;
    } else 
    if (ttm3_10_mux != LSU1_3) {
        return 0;
    }

    if (pmc1.busy) {
        return 0;
    }

    events[1] = addToVector((uval32)1, TRACE_GP970_DATA_FROM_L2);
    pmc1.assignEvent(&mmcr0, &mmcr1, PMC1::DataSrcEncode, LOW_BYTE);
    return 1;
}

SysStatus 
HWPerfMonGPUL::countDataFetchedFromAnotherL2Shared()
{
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = LSU1;
        td_cp_dbg_mux[3] = LSU1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU1) {
        return 0; 
    }

    // FIXME only pmc1 can do this (direct event with source 1000)
    // It's on byte lane 3 (LOW_BYTE) 
   
    // check ttm3sel10 as well
    if (ttm3_10_mux == TTM3_10_UNUSED) {
        mmcr1.bits.ttm3sel10 = LSU1_3;
        ttm3_10_mux = LSU1_3;
    } else 
    if (ttm3_10_mux != LSU1_3) {
        return 0;
    }

#if 0
    if (pmc1.busy) {
        return 0;
    }

    events[1] = addToVector((uval32)1, TRACE_GP970_DATA_FROM_OTHER_L2_SH);
    pmc1.assignEvent(&mmcr0, &mmcr1, PMC1::Byte3Decode, LOW_BYTE);
    return 1;
#endif 
    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_DATA_FROM_OTHER_L2_SH);
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L1CacheReloadDataSource2, LOW_BYTE); 
           return counters[i];
       }
    }  
    return 0;
}

SysStatus 
HWPerfMonGPUL::countDataFetchedFromAnotherL2Modified()
{
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = LSU1;
        td_cp_dbg_mux[3] = LSU1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU1) {
        return 0; 
    }

    // FIXME only pmc3 can do this (direct event with source 1010)
    // It's on byte lane 3 (LOW_BYTE) 
   
    // check ttm3sel10 as well
    if (ttm3_10_mux == TTM3_10_UNUSED) {
        mmcr1.bits.ttm3sel10 = LSU1_3;
        ttm3_10_mux = LSU1_3;
    } else 
    if (ttm3_10_mux != LSU1_3) {
        return 0;
    }

    if (pmc3.busy) {
        return 0;
    }

    events[3] = addToVector((uval32)3, TRACE_GP970_DATA_FROM_OTHER_L2_MO);
    pmc3.assignEvent(&mmcr0, &mmcr1, PMC3::Byte3Decode, LOW_BYTE);
    return 3;
}

#if 0
SysStatus 
HWPerfMonGPUL::countMarkedDataFetchedFromAnotherL2Modified()
{
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = LSU1;
        td_cp_dbg_mux[3] = LSU1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU1) {
        return 0; 
    }

    // FIXME only pmc3 can do this (direct event with source 1010)
    // It's on byte lane 3 (LOW_BYTE) 
   
    // check ttm3sel10 as well
    if (ttm3_10_mux == TTM3_10_UNUSED) {
        mmcr1.bits.ttm3sel10 = LSU1_7;
        ttm3_10_mux = LSU1_7;
    } else 
    if (ttm3_10_mux != LSU1_7) {
        return 0;
    }

    if (pmc3.busy) {
        return 0;
    }

    events[3] = addToVector((uval32)3, TRACE_GP970_MARKED_DATA_FROM_OTHER_L2_MO);
    pmc3.assignEvent(&mmcr0, &mmcr1, PMC3::Byte3Decode, LOW_BYTE);
    return 3;
}
#endif 

SysStatus 
HWPerfMonGPUL::countDataFetchedFromMemory() 
{
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg3sel = LSU1;
        td_cp_dbg_mux[3] = LSU1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != LSU1) {
        return 0; 
    }

    // FIXME only pmc3 can do this (direct event with source 0010)
    // It's on byte lane 3 (LOW_BYTE) 
   
    // check ttm3sel10 as well
    if (ttm3_10_mux == TTM3_10_UNUSED) {
        mmcr1.bits.ttm3sel10 = LSU1_3;
        ttm3_10_mux = LSU1_3;
    } else 
    if (ttm3_10_mux != LSU1_3) {
        return 0;
    }

    if (pmc3.busy) {
        return 0;
    }

    events[3] = addToVector((uval32)3, TRACE_GP970_DATA_FROM_MEM);
    pmc3.assignEvent(&mmcr0, &mmcr1, PMC3::DataSrcEncode, LOW_BYTE);
    return 3;
}

SysStatus
HWPerfMonGPUL::countFXUMarkedInstrFinish()
{
    // it's a direct event, only PMC6 can count it.
    if (pmc6.busy) {
        return 0;
    }
    events[6] = addToVector((uval32)6, TRACE_GP970_FXU_MARKED_INSTR_FIN);
    pmc6.assignEvent(&mmcr0, &mmcr1, PMC6::FXUMarkedInstrFinish, LOW_BYTE);

    return 6;
}

SysStatus
HWPerfMonGPUL::countFPUMarkedInstrFinish()
{
    // it's a direct event, only PMC7 can count it.
    if (pmc7.busy) {
        return 0;
    }
    events[7] = addToVector((uval32)7, TRACE_GP970_FPU_MARKED_INSTR_FIN);
    pmc7.assignEvent(&mmcr0, &mmcr1, PMC7::FPUMarkedInstrFinish, LOW_BYTE);
    return 7;
}

SysStatus
HWPerfMonGPUL::countLSUMarkedInstrFinish()
{
    // it's a direct event, only PMC8 can count it.
    if (pmc8.busy) {
        return 0;
    }
    events[8] = addToVector((uval32)8, TRACE_GP970_LSU_MARKED_INSTR_FIN);
    pmc8.assignEvent(&mmcr0, &mmcr1, PMC8::LSUMarkedInstrFinish, LOW_BYTE);
    sampleEvent = TRACE_GP970_LSU_MARKED_INSTR_FIN;
    return 8;
}

SysStatus
HWPerfMonGPUL::countMarkedGroupComplete()
{
    // it's a direct event, only PMC4 can count it.
    if (pmc4.busy) {
        return 0;
    }
    events[4] = addToVector((uval32)4, TRACE_GP970_MARKED_GROUP_COMPLETE);
    pmc4.assignEvent(&mmcr0, &mmcr1, PMC4::MarkedGroupComplete, LOW_BYTE);
    sampleEvent = TRACE_GP970_MARKED_GROUP_COMPLETE;
    return 4;
}

SysStatus 
HWPerfMonGPUL::countInstrsFetchedFromL2()
{
    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an IFU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    } 
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

#define INTERPRETATION_1
#ifdef INTERPRETATION_1  // the one explicitly suggested in the manual
    // FIXME only pmc1 can do this (direct event with source 0000)
    // It's on byte lane 2 (LOW_BYTE) 
   
    if (pmc1.busy) {
        return 0;
    }

    events[1] = addToVector((uval32)1, TRACE_GP970_INST_FROM_L2);
    pmc1.assignEvent(&mmcr0, &mmcr1, PMC1::InstSrcEncode, LOW_BYTE);
    return 1;
#else
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   L1ICacheFetchFromL2, 
					   LOW_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_INST_FROM_L2);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
#endif 
}

SysStatus 
HWPerfMonGPUL::countInstrsFetchedFromMemory()
{
    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }


    // it's an IFU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    } 
    else
    if (ttm0_mux != IFU) {
        return 0;
    }


#define INTERPRETATION_1
#ifdef INTERPRETATION_1 // the one explicitly suggested in the manual
    // FIXME only pmc3 can do this (direct event with source 0010)
    // It's on byte lane 2 (LOW_BYTE) 
   
    if (pmc3.busy) {
        return 0;
    } 

    events[3] = addToVector((uval32)3, TRACE_GP970_INST_FROM_MEM);
    pmc3.assignEvent(&mmcr0, &mmcr1, PMC3::InstSrcEncode, LOW_BYTE);
    return 3;
#else
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   L1ICacheFetchFromMemory, 
					   LOW_BYTE); 
    	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_INST_FROM_MEM);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
#endif 

}

SysStatus 
HWPerfMonGPUL::countInstrPrefetchRequest()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an IFU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    } 
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   InstrPrefetchRequest, 
					   LOW_BYTE); 
    	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_INST_PRFTCH_REQ);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus 
HWPerfMonGPUL::countInstrPrefetchInstalled()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an IFU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    } 
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   InstrPrefetchInstalledInPrefetchBuffer, 
					   LOW_BYTE); 
    	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_INST_PRFTCH_INSTL);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}


SysStatus 
HWPerfMonGPUL::countIERATMiss()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an IFU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    } 
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
					   TranslationWrittenToIERAT, 
					   LOW_BYTE); 
    	    events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_IERAT_MISS);
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}

SysStatus 
HWPerfMonGPUL::countValidInstrAvailable()
{
    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0) {
        return 0; 
    }

    // it's an IFU event
    if (ttm0_mux == TTM0_UNUSED) { 
        mmcr1.bits.ttm0sel = IFU;
        ttm0_mux = IFU;
    } 
    else
    if (ttm0_mux != IFU) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , 
	                                   &mmcr1, 
                                           ValidInstrAvailable,
					   LOW_BYTE); 
            return counters[i];
        }
    }  
    return 0;   //  no counters available
}


SysStatus 
HWPerfMonGPUL::countGCTEmptyByICacheMiss()
{
    // it's a speculative event
    // only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC5::GCTEmptyByICacheMiss;
    events[5] = addToVector((uval32)5, TRACE_GP970_GCT_EMPTY_ICACHE_MISS);
    pmc5.assignEvent(&mmcr0, &mmcr1, PMC5::SpeculativeC, LOW_BYTE);
    return 5;
}

SysStatus 
HWPerfMonGPUL::countGCTEmptyByMispredict()
{
    // it's a speculative event
    // only pmc7 can count this
    if (pmc7.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC7::GCTEmptyByMispredict;
    events[7] = addToVector((uval32)7, TRACE_GP970_GCT_EMPTY_BR_MPRED);
    pmc7.assignEvent(&mmcr0, &mmcr1, PMC7::SpeculativeD, LOW_BYTE);
    return 7;
}

SysStatus 
HWPerfMonGPUL::countComplStallByLSU()
{
    // it's a speculative event
    // only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC5::CompletionStallByLSUInst;
    events[5] = addToVector((uval32)5, TRACE_GP970_COMPL_STALL_LSU);
    pmc5.assignEvent(&mmcr0, &mmcr1, PMC5::SpeculativeA, LOW_BYTE);
    return 5;
}

SysStatus 
HWPerfMonGPUL::countComplStallByFPU()
{
    // it's a speculative event
    // only pmc7 can count this
    if (pmc7.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC7::CompletionStallByFPUInst;
    events[7] = addToVector((uval32)7, TRACE_GP970_COMPL_STALL_FPU);
    pmc7.assignEvent(&mmcr0, &mmcr1, PMC7::SpeculativeB, LOW_BYTE);
    return 7;
}

SysStatus 
HWPerfMonGPUL::countComplStallByLongFPU()
{
    // it's a speculative event
    // only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC5::CompletionStallByLongFPUInst;
    events[5] = addToVector((uval32)5, TRACE_GP970_COMPL_STALL_LONG_FPU);
    pmc5.assignEvent(&mmcr0, &mmcr1, PMC5::SpeculativeC, LOW_BYTE);
    return 5;
}

SysStatus 
HWPerfMonGPUL::countComplStallByFXU()
{
    // it's a speculative event
    // only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC5::CompletionStallByFXUInst;
    events[5] = addToVector((uval32)5, TRACE_GP970_COMPL_STALL_FXU);
    pmc5.assignEvent(&mmcr0, &mmcr1, PMC5::SpeculativeA, LOW_BYTE);
    return 5;
}

SysStatus 
HWPerfMonGPUL::countComplStallByLongFXU()
{
    // it's a speculative event
    // only pmc7 can count this
    if (pmc7.busy) {
        return 0;
    }
    mmcr1.bits.specsel = PMC7::CompletionStallByLongFXUInst;
    events[7] = addToVector((uval32)7, TRACE_GP970_COMPL_STALL_LONG_FXU);
    pmc7.assignEvent(&mmcr0, &mmcr1, PMC7::SpeculativeB, LOW_BYTE);
    return 7;
}

SysStatus 
HWPerfMonGPUL::countComplStallByERATMiss()
{
    // it's a speculative event
    // only pmc7 can count this
    if (pmc7.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC7::CompletionStallByERATMiss;
    events[7] = addToVector((uval32)7, TRACE_GP970_COMPL_STALL_ERAT_MISS);
    pmc7.assignEvent(&mmcr0, &mmcr1, PMC7::SpeculativeD, LOW_BYTE);
    return 7;
}

SysStatus 
HWPerfMonGPUL::countComplStallByReject()
{
    // it's a speculative event
    // only pmc7 can count this
    if (pmc7.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC7::CompletionStallByReject;
    events[7] = addToVector((uval32)7, TRACE_GP970_COMPL_STALL_REJECT);
    pmc7.assignEvent(&mmcr0, &mmcr1, PMC7::SpeculativeB, LOW_BYTE);
    return 7;
}

SysStatus 
HWPerfMonGPUL::countComplStallByDCacheMiss()
{
    // it's a speculative event
    // only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    mmcr1.bits.specsel = PMC5::CompletionStallByDCacheMiss;
    events[5] = addToVector((uval32)5, TRACE_GP970_COMPL_STALL_DCACHE_MISS);
    pmc5.assignEvent(&mmcr0, &mmcr1, PMC5::SpeculativeA, LOW_BYTE);
    return 5;
}

SysStatus
HWPerfMonGPUL::countCyclesInHypervisor()
{
    // it's a direct event, only PMC3 can count it.
    if (pmc3.busy) {
        return 0;
    }

    events[3] = addToVector((uval32)3, TRACE_GP970_HV_CYC);
    pmc3.assignEvent(&mmcr0, &mmcr1, PMC3::HypervisorCycles, LOW_BYTE);
    return 3;
}

SysStatus
HWPerfMonGPUL::countGroupCompleted()
{
    // it's a direct event, only PMC7 can count it.
    if (pmc7.busy) {
        return 0;
    }

    events[7] = addToVector((uval32)7, TRACE_GP970_GRP_CMPL);
    pmc7.assignEvent(&mmcr0, &mmcr1, PMC7::GroupCompleted, LOW_BYTE);
    return 7;
}

SysStatus
HWPerfMonGPUL::countGroupDispatchReject()
{
    // it's a either direct event, only PMC8 can count it.
    if (!pmc8.busy) {
        pmc8.assignEvent(&mmcr0, &mmcr1, PMC8::GroupDispatchReject, LOW_BYTE);
        return 8;
    }

    // or it can be (FIXME: can be??) configured through the performance 
    // monitor bus 

    // it's on byte lane 2 (LOW_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6  can count this
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = TTM0;
        td_cp_dbg_mux[2] = TTM0;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != TTM0)  {
        return 0; 
    }

    // this is an ISU event
    // check if ttm0 is already programmed
    if (ttm0_mux == TTM0_UNUSED) {
        mmcr1.bits.ttm0sel = ISU_TTM0;
        ttm0_mux = ISU_TTM0;
    }
    else
    if (ttm0_mux != ISU_TTM0) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, DispatchReject, LOW_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_GRP_DISP_REJECT);
            return counters[i];
        }
    }  
    return 0;   //  no counters available

}

SysStatus
HWPerfMonGPUL::countITLBMiss()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = LSU0;
        td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, ITLBMiss, HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_ITLB_MISS);
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countISLBMiss()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = LSU0;
        td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, ISLBMiss, HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_ISLB_MISS);
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countDTLBMiss()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = LSU0;
        td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, DTLBMiss, HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_DTLB_MISS);
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countDSLBMiss()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = LSU0;
        td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0) {
        return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, DSLBMiss, HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_DSLB_MISS);
            return counters[i];
        }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countTLBMiss()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 0+4 event, i.e.: 
    //  TLB Miss =  ITLBMiss (0) +  DTLBMiss (4)

    // So only pmc1 can count this
    if (pmc1.busy) {
        return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = LSU0;
        td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0) {
        return 0; 
    }

    events[1] = addToVector((uval32)1, TRACE_GP970_TLB_MISS);
    pmc1.assignEvent(&mmcr0 , &mmcr1, PMC1::Add0_4, HIGH_BYTE); 
    return 1;
}

SysStatus
HWPerfMonGPUL::countSLBMiss()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 1+5 event, i.e.: 
    //  SLB Miss =  ISLBMiss (1) +  DSLBMiss (5)

    // So only pmc2 can count this
    if (pmc2.busy) {
        return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg0sel = LSU0;
        td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0)  {
        return 0; 
    }

    events[2] = addToVector((uval32)2, TRACE_GP970_SLB_MISS);
    pmc2.assignEvent(&mmcr0 , &mmcr1, PMC2::Add1_5, HIGH_BYTE); 
    return 2;
}


SysStatus
HWPerfMonGPUL::countDERATMiss()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    // also 
    // it's an adder 2+6 event, i.e.: 
    //  DERAT miss =  dERAT miss side 0 (2) +  dERAT miss side 1 (6)

    // So only pmc6 can count this
    if (pmc6.busy) {
       return 0;
    }

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
       mmcr1.bits.td_cp_dbg0sel = LSU0;
       td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0) {
       return 0; 
    }

    events[6] = addToVector((uval32)6, TRACE_GP970_DERAT_MISS);
    pmc6.assignEvent(&mmcr0 , &mmcr1, PMC6::Add2_6, HIGH_BYTE); 
    return 6;
}

SysStatus
HWPerfMonGPUL::countTableWalkDuration()
{
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 

    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED) {
       mmcr1.bits.td_cp_dbg0sel = LSU0;
       td_cp_dbg_mux[0] = LSU0;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != LSU0) {
       return 0; 
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
        if (!pmcs[counters[i]]->busy) {
            pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, TableWalkDuration, HIGH_BYTE); 
            events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_TABLE_WALK_DURATION);
            return counters[i];
        }
    }
    return 0;
}

SysStatus
HWPerfMonGPUL::countSnoopCausedTransM2I()
{    
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // it's an GPS (GUS??) event. 
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg3sel = TTM1;
        td_cp_dbg_mux[3] = TTM1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM1) { 
        return 0; 
    }

    // it's an GPS event
    if (ttm1_mux == TTM1_UNUSED) { 
        mmcr1.bits.ttm1sel = GPS;
        ttm1_mux = GPS;
    } 
    else
    if (ttm1_mux != GPS) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_SNOOP_M_2_I);
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, SnoopCausedTransM2I, LOW_BYTE); 
           return counters[i];
       }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countSnoopCausedTransM2ES()
{    
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // it's an GPS (GUS??) event. 
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg3sel = TTM1;
        td_cp_dbg_mux[3] = TTM1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM1) { 
        return 0; 
    }

    // it's an GPS event
    if (ttm1_mux == TTM1_UNUSED) { 
        mmcr1.bits.ttm1sel = GPS;
        ttm1_mux = GPS;
    } 
    else
    if (ttm1_mux != GPS) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_SNOOP_M_2_ES);
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, SnoopCausedTransM2ES, LOW_BYTE); 
           return counters[i];
       }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countSnoopCausedTransESR2I()
{    
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // it's an GPS (GUS??) event. 
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg3sel = TTM1;
        td_cp_dbg_mux[3] = TTM1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM1) { 
        return 0; 
    }

    // it's an GPS event
    if (ttm1_mux == TTM1_UNUSED) { 
        mmcr1.bits.ttm1sel = GPS;
        ttm1_mux = GPS;
    } 
    else
    if (ttm1_mux != GPS) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_SNOOP_ES_2_RI);
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, SnoopCausedTransESR2I, LOW_BYTE); 
           return counters[i];
       }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countSnoopCausedTransE2S()
{    
    // it's on byte lane 3 (LOW_BYTE), hence 
    // pmc3, pmc4, pmc7, and pmc8 can do this 
    const uval counters[] = {3,4,7,8};
    const uval num_counters = 4;

    // it's an GPS (GUS??) event. 
    // byte lane 3 is controlled by td_cp_dbg3, check if it's programmed
    if (td_cp_dbg_mux[3] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg3sel = TTM1;
        td_cp_dbg_mux[3] = TTM1;
    }
    else 
    // check if td_cp_dbg3 is already programmed with something else
    if (td_cp_dbg_mux[3] != TTM1) { 
        return 0; 
    }

    // it's an GPS event
    if (ttm1_mux == TTM1_UNUSED) { 
        mmcr1.bits.ttm1sel = GPS;
        ttm1_mux = GPS;
    } 
    else
    if (ttm1_mux != GPS) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_SNOOP_E_2_S);
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, SnoopCausedTransE2S, LOW_BYTE); 
           return counters[i];
       }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countL2MissSharedIntervention()
{    
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // it's an GPS (GUS??) event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM1;
        td_cp_dbg_mux[0] = TTM1;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM1) { 
        return 0; 
    }

    // it's an GPS event
    if (ttm1_mux == TTM1_UNUSED) { 
        mmcr1.bits.ttm1sel = GPS;
        ttm1_mux = GPS;
    } 
    else
    if (ttm1_mux != GPS) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_L2_MISS_SHARED);
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L2MissSharedIntervention, HIGH_BYTE); 
           return counters[i];
       }
    }  
    return 0;
}

SysStatus
HWPerfMonGPUL::countL2MissModifiedIntervention()
{    
    // it's on byte lane 0 (HIGH_BYTE), hence 
    // pmc1, pmc2, pmc5, and pmc6 can do this 
    const uval counters[] = {1,2,5,6};
    const uval num_counters = 4;

    // it's an GPS (GUS??) event. 
    // byte lane 0 is controlled by td_cp_dbg0, check if it's programmed
    if (td_cp_dbg_mux[0] == TD_CP_DBGX_UNUSED)  {
        mmcr1.bits.td_cp_dbg0sel = TTM1;
        td_cp_dbg_mux[0] = TTM1;
    }
    else 
    // check if td_cp_dbg0 is already programmed with something else
    if (td_cp_dbg_mux[0] != TTM1) { 
        return 0; 
    }

    // it's an GPS event
    if (ttm1_mux == TTM1_UNUSED) { 
        mmcr1.bits.ttm1sel = GPS;
        ttm1_mux = GPS;
    } 
    else
    if (ttm1_mux != GPS) {
        return 0;
    }

    // search for a free counter
    for (uval i = 0; i < num_counters; i++) {
       if (!pmcs[counters[i]]->busy) {
           events[counters[i]] = addToVector((uval32)counters[i], TRACE_GP970_L2_MISS_MODIFIED);
           pmcs[counters[i]]->assignEvent(&mmcr0 , &mmcr1, L2MissModifiedIntervention, HIGH_BYTE); 
           return counters[i];
       }
    }  
    return 0;
}


SysStatus 
HWPerfMonGPUL::countLMQReject()
{
    // it's on byte late 2 (LOW_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this
    // also 
    // it's an adder 1+5 event, i.e.: 
    //  LMQ Reject =  LMQ Reject Side 0 (1) +  LMQ Reject Side 1 (5)

    // So only pmc2 can count this
    if (pmc2.busy) {
        return 0;
    }

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = LSU1;
        td_cp_dbg_mux[2] = LSU1;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != LSU1)  {
        return 0; 
    }

    // check ttm3sel9 as well
    if (ttm3_9_mux == TTM3_9_UNUSED) {
        mmcr1.bits.ttm3sel9 = LSU1_6;
        ttm3_9_mux = LSU1_6;
    } else 
    if (ttm3_9_mux != LSU1_6) {
        return 0;
    }

    events[2] = addToVector((uval32)2, TRACE_GP970_REJECT_LMQ_FULL);
    pmc2.assignEvent(&mmcr0 , &mmcr1, PMC2::Add1_5, LOW_BYTE); 
    return 2;
}

SysStatus 
HWPerfMonGPUL::countLSURejectERATMiss()
{
    // it's on byte late 2 (LOW_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this
    // also 
    // it's an adder 3+7 event, i.e.: 
    //  LSURejectERATMiss =  LSU0RejectERATMiss (3) +  LSU1RejectERATMiss  (7)

    // So only pmc5 can count this
    if (pmc5.busy) {
        return 0;
    }

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = LSU1;
        td_cp_dbg_mux[2] = LSU1;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != LSU1)  {
        return 0; 
    }

    // check ttm3sel9 as well
    if (ttm3_9_mux == TTM3_9_UNUSED) {
        mmcr1.bits.ttm3sel9 = LSU1_6;
        ttm3_9_mux = LSU1_6;
    } else 
    if (ttm3_9_mux != LSU1_6) {
        return 0;
    }

    events[5] = addToVector((uval32)5, TRACE_GP970_REJECT_ERAT_MISS);
    pmc5.assignEvent(&mmcr0 , &mmcr1, PMC5::Add3_7, LOW_BYTE); 
    return 5;
}

SysStatus 
HWPerfMonGPUL::countLSURejectCDForTagCollision()
{
    // it's on byte late 2 (LOW_BYTE), hence
    // pmc1, pmc2, pmc5, and pmc6 can do this
    // also 
    // it's an adder 2+6 event, i.e.: 
    // LSURejectCDForTagCollision= LSURejectCDForTagCollision(2) + LSURejectCDForTagCollision(6)

    // So only pmc6 can count this
    if (pmc6.busy) {
        return 0;
    }

    // byte lane 2 is controlled by td_cp_dbg2, check if it's programmed
    if (td_cp_dbg_mux[2] == TD_CP_DBGX_UNUSED) {
        mmcr1.bits.td_cp_dbg2sel = LSU1;
        td_cp_dbg_mux[2] = LSU1;
    }
    else 
    // check if td_cp_dbg2 is already programmed with something else
    if (td_cp_dbg_mux[2] != LSU1)  {
        return 0; 
    }

    // check ttm3sel9 as well
    if (ttm3_9_mux == TTM3_9_UNUSED) {
        mmcr1.bits.ttm3sel9 = LSU1_6;
        ttm3_9_mux = LSU1_6;
    } else 
    if (ttm3_9_mux != LSU1_6) {
        return 0;
    }

    events[6] = addToVector((uval32)6, TRACE_GP970_REJECT_CDF_TAG_COL);
    pmc6.assignEvent(&mmcr0 , &mmcr1, PMC6::Add2_6, LOW_BYTE); 
    return 6;
}

/*virtual*/  SysStatus 
HWPerfMonGPUL::addGroup(uval32 groupNo, uval32 share, uval32 samplingFreqBase)
{
    SysStatus stat;

    if (groupNum == maxGroups) {
        err_printf("the group cannot be added, active groups exceeds the capacity \n");
        return -1;
    }

    sval32 shareBits = -1;

    // calculating the shareBits as a log2(share) assuming share is a power of 2
    while (share > 0) {      
        shareBits++;
	share = share >> 1;
    }

    mmcr0.value = 0;
    mmcr1.value = 0;
    mmcra.value = 0;

    mmcra.bits.imr_sel = 0x0;
    mmcra.bits.imr_mark = 0x2; // 10 binary (ppc instructions)
    mmcra.bits.sample_en  = 1;
    //   mmcra.bits.fcwait  = 1;    // freeze the counters  while in the wait state 
    mmcra.bits.sco  = 1;

    sampleCounter = 0;
    sampleEvent = 0;

    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
        events[i] = CounterVector::INVALID_INDEX;
    }

    uval period = (multiplexingRound << shareBits) >> totalSharesBits;
    releaseHW();
    switch (groupNo) {
    case 0: stat = programCounterGroup0(); break;
    case 1: stat = programCounterGroup1(); break;
    case 3: stat = programCounterGroup3(); break;
    case 4: stat = programCounterGroup4(); break;
    case 5: stat = programCounterGroup5(); break;
    case 24: stat = programCounterGroup24(); break;
    case 31: stat = programCounterGroup31(); break;
    case 32: stat = programCounterGroup32(); break;
    case 33: stat = programCounterGroup33(); break;
    case 34: stat = programCounterGroup34(); break;
    case 35: stat = programCounterGroup35(); break;
    case 36: stat = programCounterGroup36(); break;
    case 37: stat = programCounterGroup37(); break;
    case 38: stat = programCounterGroup38(); break;
    case 39: stat = programCounterGroup39(); break;
    case 40: stat = programCounterGroup40(); break;
    case 41: stat = programCounterGroup41(); break;
    case 42: stat = programCounterGroup42(); break;
    case 43: stat = programCounterGroup43(); break;
    case 44: stat = programCounterGroup44(); break;
    case 45: stat = programCounterGroup45(); break;
    case 46: stat = programCounterGroup46(); break;
    case 47: stat = programCounterGroup47(); break;
    case 50: stat = programCounterGroup50(); break;
    case 51: stat = programCounterGroup51(); break;
    case 52: stat = programCounterGroup52(); break;
    case 60: stat = programCounterGroup60(); break;
    case 61: stat = programCounterGroup61(); break;
    case 62: stat = programCounterGroup62(); break;
    case 63: stat = programCounterGroup63(); break;
    case 64: stat = programCounterGroup64(); break;

    default:
          err_printf("the group is not defined. \n");
	  return -1;
    };

    if (stat == -1) {
          err_printf("No group is not defined. \n");
	  return stat;
    }
 
#ifdef MAMBO_SUPPORT
    if (platform_type == GPUL_MAMBO) {
        enablePeriodicTimerEvent(period);
    } else 
#endif 
    enablePeriodicOverflow(period);

    countersDescription->setLogGroupID(groupNo);
    groupList[groupNum].gid = groupNo;
    groupList[groupNum].shareBits = shareBits;
    groupList[groupNum].mmcr0.value = mmcr0.value;
    groupList[groupNum].mmcr1.value = mmcr1.value;
    groupList[groupNum].mmcra.value = mmcra.value;

    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
        groupList[groupNum].events[i] = events[i];
    }

    groupList[groupNum].timer = timer;
    groupList[groupNum].periodicCountdownValue = pmcs[timer]->value;

    groupList[groupNum].sampleCounter = sampleCounter;
    if (sampleCounter) {
        // calculating the countdown value 
        groupList[groupNum].samplingCountdownValue = 0x7fffffff - samplingFreqBase;

	// setting it to the sampling hardware counter 
        pmcs[sampleCounter]->set(groupList[groupNum].samplingCountdownValue);
        countersDescription->setLogDataSample(sampleEvent, samplingFreqBase);
    }
    
    groupNum++;

    // err_printf("A new group is added: groupno %d  share %d sampling freq %d \n", 
    //		    groupNo, share, samplingFreqBase);

    return 0;
}


/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup0()
{
   instrCmplCounter = countInstrCompleted();
   cycleCounter = countCycles();
   return 0;
};


/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup1()
{
    // We have a priori knowledge that these groups don't have conflicts
    // therefore, no checks are necessary!

    // I'm cheating to reserve pmc1 for cycle counter
    pmc1.busy = true; 

    countL1DLoadMiss();
    countL1DEntriesInvalidatedFromL2();
    countL1DLoad();
    countL1DStore();
    countInstrDispatched();
    instrCmplCounter = countInstrCompleted();

    pmc1.busy = false; 

#ifdef MAMBO_SUPPORT
    // it's a hack to get Log messages even in the weird Mambo programming model
    if (platform_type == GPUL_MAMBO) {
        mmcr0.value = (uval64)pmcCtlRegSetMaps[1].mmcr0;
        mmcr1.value = pmcCtlRegSetMaps[1].mmcr1;
        mmcra.value = pmcCtlRegSetMaps[1].mmcra;
    } 
#endif 
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup3()
{
    // We have a priori knowledge that these groups don't have conflicts
    // therefore, no checks are necessary!

    countFlushUnalignedLoad();
    countFlushUnalignedStore();
    countL1DLoad();
    countL1DStore();
    countFlushFromLRQ();
    countFlushFromSRQ();
    instrCmplCounter = countInstrCompleted();

#ifdef MAMBO_SUPPORT
    // it's a hack to get Log messages even in the weird Mambo programming model
    if (platform_type == GPUL_MAMBO) {
        mmcr0.value = (uval64)pmcCtlRegSetMaps[3].mmcr0;
        mmcr1.value = pmcCtlRegSetMaps[3].mmcr1;
        mmcra.value = pmcCtlRegSetMaps[3].mmcra;
    } 
#endif 
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup4()
{
    // We have a priori knowledge that these groups don't have conflicts
    // therefore, no checks are necessary!

    countFPUDivide();
    countFPUMultAdd();
    countFPUEstimate();
    countFPUFinish();
    countFPUSquareRoot();
    countFPUMoveEstimate();
    pmc5.busy = true;
    instrCmplCounter = countInstrCompleted();
    pmc5.busy = false;
    
    
#ifdef MAMBO_SUPPORT
    // it's a hack to get Log messages even in the weird Mambo programming model
    if (platform_type == GPUL_MAMBO) {
        mmcr0.value = (uval64)pmcCtlRegSetMaps[4].mmcr0;
        mmcr1.value = pmcCtlRegSetMaps[4].mmcr1;
        mmcra.value = pmcCtlRegSetMaps[4].mmcra;
    } 
#endif 
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup5()
{
    // We have a priori knowledge that these groups don't have conflicts
    // therefore, no checks are necessary!

    pmc3.busy = true;
    countFPUDenormOperand();
    countFPUStall3();
    instrCmplCounter = countInstrCompleted();
    countFPUAddMultSubCompareFsel();
    countFloatingPointStore();
    countFPURoundConvert();
    countFloatingPointLoad();
    pmc3.busy = false;

#ifdef MAMBO_SUPPORT
    // it's a hack to get Log messages even in the weird Mambo programming model
    if (platform_type == GPUL_MAMBO) {
        mmcr0.value = (uval64)pmcCtlRegSetMaps[5].mmcr0;
        mmcr1.value = pmcCtlRegSetMaps[5].mmcr1;
        mmcra.value = pmcCtlRegSetMaps[5].mmcra;
    } 
#endif 
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup24()
{
    // We have a priori knowledge that these groups don't have conflicts
    // therefore, no checks are necessary!

    instrCmplCounter = countInstrCompleted();
    countL1DLoadMiss();
    countBranchExecutionIssueValid();
    countBranchMispredictDueToCR();
    countBranchMispredictDueToTarget();

#ifdef MAMBO_SUPPORT
    // it's a hack to get Log messages even in the weird Mambo programming model
    if (platform_type == GPUL_MAMBO) {
        mmcr0.value = (uval64)pmcCtlRegSetMaps[24].mmcr0;
        mmcr1.value = pmcCtlRegSetMaps[24].mmcr1;
        mmcra.value = pmcCtlRegSetMaps[24].mmcra;
    } 
#endif
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup31()
{
    // to reserve the pmc1 for cycle counter 
    pmc1.busy = true;
    CHECK(countCyclesInHypervisor);
    CHECK(countInstrDispatched);
    instrCmplCounter = countInstrCompleted();
    CHECK(countDispatchValid);
    CHECK(countDispatchReject);
    CHECK(countGroupCompleted);
    pmc1.busy = false;
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup32()
{
    CHECK(countL1DStore);
    CHECK(countTLBMiss);
    CHECK(countL1DLoadMiss);
    CHECK(countLSURejectERATMiss);
    CHECK(countL1DLoad);
    CHECK(countLSURejectCDForTagCollision);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup33()
{
    CHECK(countITLBMiss);
    CHECK(countSLBMiss);
    CHECK(countL1Prefetch);
    CHECK(countL2Prefetch);
    CHECK(countDTLBMiss);
    CHECK(countDERATMiss);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup34()
{
    CHECK(countInstrsFetchedFromL2);
    CHECK(countInstrsFetchedFromMemory);
    CHECK(countInstrPrefetchInstalled);
    CHECK(countInstrPrefetchRequest);
    CHECK(countGCTEmptyByMispredict);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup35()
{
    pmc1.busy = true;
    pmc8.busy = true;
    CHECK(countGroupDispatchReject);
    pmc8.busy = false;
    instrCmplCounter = countInstrCompleted();
    CHECK(countLRQFull);
    CHECK(countFlushFromSRQ);
    CHECK(countFlushFromLRQ);
    CHECK(countSRQFull);
    CHECK(countFXULSUIssueQueueFull);
    pmc1.busy = false;
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup36()
{
    instrCmplCounter = countInstrCompleted();
    CHECK(countSnoopCausedTransM2ES);
    CHECK(countSnoopCausedTransE2S);
    CHECK(countSnoopCausedTransESR2I);
    CHECK(countSnoopCausedTransM2I);
    CHECK(countL2MissSharedIntervention);
    CHECK(countL2MissModifiedIntervention);
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup37()
{
#if 0
    /* snooping counters */
    CHECK(countMarkedL1DLoadMiss();
#endif 
    instrCmplCounter = countInstrCompleted();
    mmcra.bits.imr_mark = 0x3; // so, the following counter would count PPC LD/ST instructions
    mmcra.bits.sample_en  = 1;
    sampleCounter = countMInstrCompleted();
    CHECK(countL1DLoad);
    CHECK(countL1DStore);
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup38()
{
    CHECK(countDataFetchedFromMemory);
    CHECK(countDataFetchedFromL2);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup39()
{
    // CHECK(countDataFetchedFromL2);
    // CHECK(countDataFetchedFromMemory);
    instrCmplCounter = countInstrCompleted();
    CHECK(countComplStallByDCacheMiss);
    CHECK(countComplStallByReject);
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup40()
{
    CHECK(countComplStallByLSU);
    CHECK(countComplStallByERATMiss);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup41()
{
    CHECK(countComplStallByFXU);
    CHECK(countComplStallByLongFXU);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup42()
{
    // CHECK(countFPUDivide);
    // CHECK(countFPUMultAdd);
    // CHECK(countFPUSquareRoot);
    CHECK(countComplStallByFPU);
    CHECK(countComplStallByLongFPU);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup43()
{
    //CHECK(countTLBMiss);
    //CHECK(countSLBMiss);
    CHECK(countComplStallByERATMiss);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup44()
{
    CHECK(countGCTEmptyByMispredict);
    // CHECK(countFlushOriginatedByMispredict);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};


/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup45()
{
    // CHECK(countInstrsFetchedFromL2);
    // CHECK(countInstrsFetchedFromMemory);
    CHECK(countGCTEmptyByICacheMiss);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup46()
{
    // CHECK(countLSURejectERATMiss);
    // CHECK(countLSURejectCDForTagCollision);
    CHECK(countComplStallByReject);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup47()
{
    CHECK(countDataFetchedFromAnotherL2Shared);
//    CHECK(countDataFetchedFromAnotherL2Modified);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup50()
{
    mmcra.bits.imr_mark = 0x3; // so, the following counter would count PPC LD/ST instructions
    mmcra.bits.sample_en  = 1;
    instrCmplCounter = countInstrCompleted();
    CHECK(countMInstrCompleted);

    // the group sample counter
    sampleCounter = countLSUMarkedInstrFinish();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup51()
{
    mmcra.bits.imr_mark = 0x3; // so, the following counter would count PPC LD/ST instructions
    mmcra.bits.sample_en  = 1;

    /* the group sample counter */
    sampleCounter = countMarkedL1DLoadMiss();
    CHECK(countL1DLoadMiss);
    CHECK(countL1DLoad);
    CHECK(countL1DStore);
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup52()
{
    mmcra.bits.imr_mark = 0x3; // so, the following counter would count PPC LD/ST instructions
    mmcra.bits.sample_en  = 1;

    /* the group sample counter */
    sampleCounter = countMarkedGroupComplete();
    instrCmplCounter = countInstrCompleted();
    cycleCounter = countCycles();
    return 0;
};

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup60()
{
    CHECK(countTLBMiss);    // fixed to PMC1
    CHECK(countSLBMiss);    // fixed to PMC2
    CHECK(countDERATMiss);  // fixed to PMC6
    CHECK(countIERATMiss);  // will end up in PMC5
    CHECK(countGroupCompleted);  // fixed to PMC7
    CHECK(countGroupDispatchReject);  // fixed to PMC8

    instrCmplCounter = countInstrCompleted();  // will end up in PMC3
    cycleCounter = countCycles();              // will end up in PMC4
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup61()
{
    CHECK(countInstrsFetchedFromL2);      // fixed to PMC1
    CHECK(countInstrsFetchedFromMemory);  // fixed to PMC3
    CHECK(countBranchExecutionIssueValid); // will end up in PMC4
    CHECK(countBranchMispredictDueToCR);   // will end up in PMC7
    CHECK(countBranchMispredictDueToTarget); // will end up in PMC8

    instrCmplCounter = countInstrCompleted();  // will end up in PMC2 
    cycleCounter = countCycles();              // will end up in PMC5 
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup62()
{
    CHECK(countL1DLoadMiss);    // fixed to PMC3
    CHECK(countL1DLoadSide0);   // will end up in PMC4
    CHECK(countL1DLoadSide1);   // will end up in PMC7
    CHECK(countInstrDispatched) // will end up in PMC1
    CHECK(countDispatchValid);  // will end up in PMC2

    instrCmplCounter = countInstrCompleted();  // will end up in PMC5 
    cycleCounter = countCycles();              // will end up in PMC6 
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup63()
{
    CHECK(countFPU0FinishedAndProdRes); // will end up in PMC3
    CHECK(countFPU1FinishedAndProdRes); // will end up in PMC4
    CHECK(countFXU0ProdRes);   // will end up in PMC7
    CHECK(countFXU1ProdRes);   // will end up in PMC8

    instrCmplCounter = countInstrCompleted();  // will end up in PMC5 
    cycleCounter = countCycles();              // will end up in PMC6 
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::programCounterGroup64()
{
    CHECK(countL1DStoreMiss); // will end up in PMC3
    CHECK(countL1DStoreSide0); // will end up in PMC4
    CHECK(countL1DStoreSide1);   // will end up in PMC7
    CHECK(countCyclesICacheWriteActive);   // will end up in PMC8

    instrCmplCounter = countInstrCompleted();  // will end up in PMC5 
    cycleCounter = countCycles();              // will end up in PMC6 
    return 0;
}

#ifdef MAMBO_SUPPORT
SysStatus
HWPerfMonGPUL::startMamboGroup(uval32 groupNo, uval32 period)
{
    SysStatus stat;
    if (groupNo > 30)  {
        err_printf("the group is not defined. \n");
        return -1;
    }

    mmcr0.value = (uval64)pmcCtlRegSetMaps[groupNo].mmcr0;
    mmcr1.value = pmcCtlRegSetMaps[groupNo].mmcr1;
    mmcra.value = pmcCtlRegSetMaps[groupNo].mmcra;

    stat = enablePeriodicTimerEvent(period);
    mmcr0.set();
    mmcr1.set();
    mmcra.set();
    sync();
    return stat;
}
#endif 

/* virtual */ SysStatus
HWPerfMonGPUL::removeGroup(uval32 groupNo)
{
    for (uval i = 0; i < groupNum; i++) {
        if (groupList[i].gid == groupNo) {
	    groupList[i].shareBits = -1; 
	}
    }
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::startSampling(uval64 delay)
{
    sync();
    
    // for now, I don't do anything with the delay parameter

    logRatio = logPeriod / multiplexingRound;
    interruptAction = &HWPerfMonGPUL::periodicTraceAction;

    countersDescription->setCounterNum(counterVector.eventCount);
    countersDescription->setLogPeriod(logPeriod);

    // err_printf("start sampling is requested \n");

    if (groupNum == 0) {
        err_printf("no group has been programmed \n");
	return -1;
    }

    while (groupList[groupCurrent].shareBits == -1) {
        groupCurrent = (groupCurrent+1) % groupNum;
    }

    timer = groupList[groupCurrent].timer;
    pmcs[timer]->set(groupList[groupCurrent].periodicCountdownValue);
    
    sampleCounter = groupList[groupCurrent].sampleCounter;

    // checking to see if the group is a data sampling group
    if (sampleCounter) {
        interruptAction = &HWPerfMonGPUL::samplingTraceAction;        
        pmcs[sampleCounter]->set(groupList[groupCurrent].samplingCountdownValue);
    }

    groupList[groupCurrent].mmcr0.set();
    groupList[groupCurrent].mmcr1.set();
    groupList[groupCurrent].mmcra.set();

    sync();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::stopSampling()
{

    // err_printf("stop sampling is requested \n");
    mmcr0.get();
    mmcr0.bits.freeze = 1;  
    mmcr0.set();
    disablePeriodic();
    reset();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::startWatch()
{
    // err_printf("start watch is requested \n");
    watching = true;
    pmcs[timer]->get();
    startTime = pmcs[timer]->value;
    watchVector.eventCount = counterVector.eventCount;
    watchVector.clear();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::logAndResetWatch()
{
    // err_printf("log and reset watch is requested \n");
    codeAddress ia;
    ia=exceptionLocal.currentProcessAnnex->excStatePtr()->codeAddr();
    pmcs[timer]->get();
    // timer could be either really "cycle counter" or "instruction counter"
    uval timerIndex = groupList[groupCurrent].events[timer];
    uval32 totalTime = watchVector.values[timerIndex];
    
    uval32  countedTime =  pmcs[timer]->value - startTime; 

    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
        pmcs[i]->get();
        uval eventIndex = groupList[groupCurrent].events[i];
        watchVector.values[eventIndex] += pmcs[i]->value;

        // This is used later on for scaling up the values
        // in the watchVector. The basic idea is to scale up 
        // the value of counter proportional to the total 
        // time period of the watch divided by the time while
        // the counter is actually programmed in PMU.
        watchVector.times[eventIndex] += countedTime;
    }
    watchVector.values[timerIndex]-= startTime; 

    // scaling up all the values in the watch vector
    // timer could be either really "cycle counter" or "instruction counter"
    timerIndex = groupList[groupCurrent].events[timer];

    // starting from "2" skipping CYCLE and INSTR_COMPLETED
    for (uval i = 2; i < watchVector.eventCount; i++) {
 	watchVector.values[i] *= totalTime;
 	watchVector.values[i] /=  watchVector.times[i];
    }

    // log the watch vector counter values
    TraceHWHWPerfMonAperiodicAll ((uval64)ia, watchVector.eventCount, watchVector.values); 

    watchVector.clear();
    pmcs[timer]->get();
    startTime = pmcs[timer]->value;
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::stopWatch()
{
    // err_printf("stop watch is requested \n");
    watching = false;
    watchVector.clear();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::startCPIBreakdown(uval64 delay)
{
    sync();

    // for now, I don't do anything with the delay parameter
    // installing the proper interrupt handler 
    interruptAction = &HWPerfMonGPUL::CPIBreakdownAction;
    
    addGroup(39, 8);
    addGroup(40, 8);
    addGroup(41, 4);
    addGroup(42, 4);
    // addGroup(43, 4);
    addGroup(44, 4);
    addGroup(45, 4);
    // addGroup(46, 4);
    counterVector.clear();

    timer = groupList[groupCurrent].timer;
    pmcs[timer]->set(groupList[groupCurrent].periodicCountdownValue);

    groupList[groupCurrent].mmcr0.set();
    groupList[groupCurrent].mmcr1.set();
    groupList[groupCurrent].mmcra.set();

    sync();
    return 0;
}

/* virtual */ SysStatus
HWPerfMonGPUL::stopCPIBreakdown()
{
    mmcr0.get();
    mmcr0.bits.freeze = 1;  
    mmcr0.set();

    // flush the currently accumulated bundle
    makeCPIBundle();
    disablePeriodic();
    reset();
    return 0;
}

/****************************************************
*
*  Warning the following code runs at ExceptionLevel
*  do not acquire locks or do any other fancy things
*
*****************************************************/
void
HWPerfMonGPUL::defaultAction(void)
{
    passertMsg(0, "Performance Monitor Interrupt occured but no action"
               " specified");
}

void
HWPerfMonGPUL::periodicTraceAction(void)
{
    codeAddress ia;
    codeAddress lr;
    uval64 *values = counterVector.values;
    siar.get();
    sdar.get();
    srr1.get();
    

    ia=exceptionLocal.currentProcessAnnex->excStatePtr()->codeAddr();
    lr = (codeAddress)exceptionLocal.currentProcessAnnex->excStatePtr()->lr;

    uval eventIndex;
    uval groupShareBits = groupList[groupCurrent].shareBits;

    // The number of timer ticks this group has been
    uval32 countedTime = (uval32)0x7FFFFFFF - startTime;

    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
        pmcs[i]->get();
        eventIndex = groupList[groupCurrent].events[i];
	if ((eventIndex == CounterVector::CYCLE) || 
            (eventIndex == CounterVector::INSTR_COMPLETED)) {	
	    values[eventIndex] +=  pmcs[i]->value;
	} else {
            // for all other counters we scale the value to the whole interval
	    values[eventIndex] +=   
	      (uval64)((pmcs[i]->value << totalSharesBits) >> groupShareBits);
	}

	if (watching) {
            // updating watch vector values 
            watchVector.values[eventIndex] += pmcs[i]->value;


	    // This is used later on for scaling up the values
	    // in the watchVector. The basic idea is to scale up 
	    // the value of counter proportional to the total 
	    // time period of the watch divided by the time while
	    // the counter is actually programmed in PMU.
	    watchVector.times[eventIndex] += countedTime;
	}

        pmcs[i]->set(0);
    }

    // timer could be either really "cycle counter" or "instruction counter"
    uval timerIndex = groupList[groupCurrent].events[timer];
    values[timerIndex]-= groupList[groupCurrent].periodicCountdownValue + 3; 

    if (watching) {
        watchVector.values[timerIndex]-= startTime; 
    }

    groupCurrent++;
    if (groupCurrent == groupNum) {  
        // the round is over:
	groupCurrent = 0;
        roundCount++;

	if ((roundCount > MIN_ROUND_COUNT) && (roundCount < MAX_ROUND_COUNT) &&
		!(roundCount % logRatio)) { 
            TraceHWHWPerfMonPeriodicAll ((uval64)ia, counterVector.eventCount, counterVector.values); 
	    counterVector.clear();
	}
    }

    // search for an active group
    while (groupList[groupCurrent].shareBits == -1) {
        groupCurrent = (groupCurrent+1) % groupNum;
    }

    // scheduling the new group
    timer = groupList[groupCurrent].timer;
    pmcs[timer]->set(groupList[groupCurrent].periodicCountdownValue);
    startTime = groupList[groupCurrent].periodicCountdownValue;

    // check to see if the group is a "data sampling" group
    sampleCounter = groupList[groupCurrent].sampleCounter;
    if (sampleCounter) {
        pmcs[sampleCounter]->set(groupList[groupCurrent].samplingCountdownValue);
        interruptAction = &HWPerfMonGPUL::samplingTraceAction;
    }
    groupList[groupCurrent].mmcr1.set();
    groupList[groupCurrent].mmcra.set();
    groupList[groupCurrent].mmcr0.set();
}

void
HWPerfMonGPUL::makeCPIBundle(void)
{
    uval8 *revMap = counterVector.reverseMap;
    uval64 *values = counterVector.values;

    cpiBundle.RoundNum = roundCount;	
    cpiBundle.Cycles                = values[revMap[TRACE_GP970_CYC]];
    cpiBundle.InstrsCompleted       = values[revMap[TRACE_GP970_INST_CMPL]];

    cpiBundle.StallsDCacheMisses    = values[revMap[TRACE_GP970_COMPL_STALL_DCACHE_MISS]];

    cpiBundle.StallsLSULatency      = values[revMap[TRACE_GP970_COMPL_STALL_LSU]];

    cpiBundle.StallsFPULatency      = values[revMap[TRACE_GP970_COMPL_STALL_FPU]] + 
                                      values[revMap[TRACE_GP970_COMPL_STALL_LONG_FPU]];

    cpiBundle.StallsFXULatency      = values[revMap[TRACE_GP970_COMPL_STALL_FXU]] + 
                                      values[revMap[TRACE_GP970_COMPL_STALL_LONG_FXU]];
				     
    cpiBundle.StallsRejects         = /*values[revMap[TRACE_GP970_COMPL_STALL_REJECT]] +   */
                                      values[revMap[TRACE_GP970_COMPL_STALL_ERAT_MISS]] ;

    cpiBundle.CompletionTableEmptyICacheMisses   
                                    = values[revMap[TRACE_GP970_GCT_EMPTY_ICACHE_MISS]];

    cpiBundle.CompletionTableEmptyBrMispredict 
                                    = values[revMap[TRACE_GP970_GCT_EMPTY_BR_MPRED]];

    TraceHWHWPerfMonCPIBreakdown(cpiBundle.Cycles, cpiBundle.InstrsCompleted, 
                                 cpiBundle.StallsFPULatency, cpiBundle.StallsFXULatency, 
                                 cpiBundle.StallsLSULatency, cpiBundle.StallsDCacheMisses, 
				 cpiBundle.CompletionTableEmptyICacheMisses, 
				 cpiBundle.CompletionTableEmptyBrMispredict,  
				 cpiBundle.StallsRejects); 
}

void
HWPerfMonGPUL::CPIBreakdownAction(void)
{
    uval64 *values = counterVector.values;
    // codeAddress lr;
    siar.get();
    sdar.get();
    
    // lr = (codeAddress)exceptionLocal.currentProcessAnnex->excStatePtr()->lr;

    sync();
    uval eventIndex;
    for (uval i = 1; i <= GPUL_COUNTERS; i++) {
        pmcs[i]->get();
        eventIndex = groupList[groupCurrent].events[i];
	if ((eventIndex == CounterVector::CYCLE) ||
            (eventIndex == CounterVector::INSTR_COMPLETED)) {	
	    values[eventIndex] += pmcs[i]->value;
	} else {
            // for all other counters we scale the value to the whole interval
	    values[eventIndex] +=   
	      (uval64)((pmcs[i]->value << totalSharesBits) >> groupList[groupCurrent].shareBits);
	}
        pmcs[i]->set(0);
    }

    eventIndex = groupList[groupCurrent].events[timer];
    values[eventIndex]-= groupList[groupCurrent].periodicCountdownValue; 

    groupCurrent++;

    if (groupCurrent == groupNum) {  
        // the round is over:
	groupCurrent = 0;
        roundCount++;

        if (!(roundCount % 20)) {
	     if ((roundCount > MIN_ROUND_COUNT) && (roundCount < MAX_ROUND_COUNT)) { 
                 makeCPIBundle();
                 // notify the phase tracker
             }
	     counterVector.clear();
	}

    }
    // if (roundCount > MAX_ROUND_COUNT) {
    //    stopCPIBreakdown();
    // }

    // scheduling the new group
    timer = groupList[groupCurrent].timer;
    pmcs[timer]->set(groupList[groupCurrent].periodicCountdownValue);

    groupList[groupCurrent].mmcr1.set();
    groupList[groupCurrent].mmcra.set();
    groupList[groupCurrent].mmcr0.set();
}

void
HWPerfMonGPUL::samplingTraceAction(void)
{
    siar.get();
    sdar.get();
    uval32 samplingJitter;
 
    sync();
    pmcs[timer]->get();
    samplingJitter = (uval32)(pmcs[timer]->value) & (uval32)0XF;  // the result: between 0 .. 15

    if (counterRolled(pmcs[timer]->value)) {
        // we are at the end of the period, call the periodic trace action
        interruptAction = &HWPerfMonGPUL::periodicTraceAction;
	periodicTraceAction();
	return;
    }

    // otherwise, pmcs[sampleCounter] must have rolled so that we got this interrupt
    TraceHWHWPerfMonDataSample((uval64)siar.value, (uval64)sdar.value);

    //I have to add some jittering value to the sampleCountdownValue
    pmcs[sampleCounter]->set(groupList[groupCurrent].samplingCountdownValue - samplingJitter);

    groupList[groupCurrent].mmcr1.set();
    groupList[groupCurrent].mmcr0.set();
}


/* virtual */ SysStatus
HWPerfMonGPUL::HWPerfInterrupt()
{
    (this->*interruptAction)();  // invoke the current interrupt action;
    return 0;
}
