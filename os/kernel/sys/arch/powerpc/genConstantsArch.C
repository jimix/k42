/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: genConstantsArch.C,v 1.52 2005/08/22 14:54:14 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     generation of the machine-dependent assembler constants
 *     NOTE:  This file is a C fragment that is included directly in the
 *            machine-independent genConstants.C.
 * **************************************************************************/

#include "init/arch/powerpc/MPinit.H"

    DEFINED_CONSTANT(ONSIM_MAMBO, SIM_MAMBO);
    DEFINED_CONSTANT(ONSIM_SIMOSPPC, SIM_SIMOSPPC);
    DEFINED_CONSTANT(SVC_DIRECT_VEC_ADDR,
			DispatcherDefault::SVCDirectVectorAddr);

    FIELD_OFFSET1(EL, ExceptionLocal, kernelRegionsEnd);
    FIELD_OFFSET1(EL, ExceptionLocal, vMapsRStart);
    FIELD_OFFSET1(EL, ExceptionLocal, vMapsREnd);
    FIELD_OFFSET1(EL, ExceptionLocal, vMapsRDelta);
    FIELD_OFFSET1(EL, ExceptionLocal, kernelPSRStart);
    FIELD_OFFSET1(EL, ExceptionLocal, kernelPSREnd);
    FIELD_OFFSET1(EL, ExceptionLocal, kernelPSRDelta);
    FIELD_OFFSET1(EL, ExceptionLocal, kernelPSRVSID);
    FIELD_OFFSET1(EL, ExceptionLocal, commonPSRStart);
    FIELD_OFFSET1(EL, ExceptionLocal, commonPSREnd);
    FIELD_OFFSET1(EL, ExceptionLocal, commonPSRDelta);
    FIELD_OFFSET1(EL, ExceptionLocal, commonPSRVSID);
    FIELD_OFFSET1(EL, ExceptionLocal, commonPSRWritable);
    FIELD_OFFSET1(EL, ExceptionLocal, traceRgnStart);
    FIELD_OFFSET1(EL, ExceptionLocal, traceRgnEnd);
    FIELD_OFFSET1(EL, ExceptionLocal, traceRgnDelta);
    FIELD_OFFSET1(EL, ExceptionLocal, ioRgnStart);
    FIELD_OFFSET1(EL, ExceptionLocal, ioRgnEnd);
    FIELD_OFFSET1(EL, ExceptionLocal, ioRgnMap);
    FIELD_OFFSET1(EL, ExceptionLocal, trapHandler);
    FIELD_OFFSET1(EL, ExceptionLocal, handlers);
    FIELD_OFFSET1(EL, ExceptionLocal, toc);
    FIELD_OFFSET1(EL, ExceptionLocal, msr);
    FIELD_OFFSET1(EL, ExceptionLocal, msrUserChange);
    FIELD_OFFSET1(EL, ExceptionLocal, elocalVirt);
    FIELD_OFFSET1(EL, ExceptionLocal, excPgfltExceptionUser);
    FIELD_OFFSET1(EL, ExceptionLocal, excPgfltExceptionKernel);
    FIELD_OFFSET1(EL, ExceptionLocal, excIOInterrupt);
    FIELD_OFFSET1(EL, ExceptionLocal, excDecInterrupt);
    FIELD_OFFSET1(EL, ExceptionLocal, excPerfInterrupt);
    FIELD_OFFSET1(EL, ExceptionLocal, excTrapExceptionUser);
    FIELD_OFFSET1(EL, ExceptionLocal, excTrapExceptionKernel);
    FIELD_OFFSET1(EL, ExceptionLocal, svc);
    FIELD_OFFSET1(EL, ExceptionLocal, num_null);
    FIELD_OFFSET1(EL, ExceptionLocal, num_exi);
    FIELD_OFFSET1(EL, ExceptionLocal, num_dec);
    FIELD_OFFSET1(EL, ExceptionLocal, num_trap);
    FIELD_OFFSET1(EL, ExceptionLocal, num_nonnative_svc);
    FIELD_OFFSET1(EL, ExceptionLocal, boltedRfiState);


    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, pte);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, lolitaLock);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, segLoad);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, num_dsi);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, num_dsgi);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, num_isi);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, num_isgi);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, num_segmiss);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, num_map_fault);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, num_map_evict);
    FIELD_OFFSET3(EL_lgPg, ExceptionLocal, pageTable, largePage, numSizes);

    FIELD_OFFSET1(RfiState, RfiState, r0);
    FIELD_OFFSET1(RfiState, RfiState, r1);
    FIELD_OFFSET1(RfiState, RfiState, r2);

    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState, r2);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState, r3);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState, r4);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState, r5);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState, r6);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState, r7);

    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r0);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r1);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r2);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r3);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r4);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r5);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r6);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r7);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r8);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r9);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r10);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r11);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_r12);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_srr0);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_srr1);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_ctr);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_lr);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaState2, hv_cr);

    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaStack, hv_stack);
    FIELD_OFFSET2(EL_lolita, ExceptionLocal, lolitaRet, hv_ret0);

    FIELD_OFFSET1(EL, ExceptionLocal, trapSkipDecr);
    FIELD_OFFSET1(EL, ExceptionLocal, trapVolatileState);

    FIELD_OFFSET2(PA, ProcessAnnex, machine, msr);
    FIELD_OFFSET2(PA, ProcessAnnex, machine, asr);
    FIELD_OFFSET2(PA, ProcessAnnex, machine, syscallStackPtr);
    FIELD_OFFSET2(PA, ProcessAnnex, machine, syscallReturnAddr);
    FIELD_OFFSET2(PA, ProcessAnnex, machine, excStatePhysAddr);

    FIELD_OFFSET2(DD, DispatcherDefault, arch, msr);
    FIELD_OFFSET2(DD, DispatcherDefault, arch, fpscr);
    FIELD_OFFSET2(DD, DispatcherDefault, arch, alignHdlrStkPtr);

    FIELD_OFFSET1(EPD, EntryPointDesc, toc);

    FIELD_OFFSET1(VS, VolatileState, iar);
    FIELD_OFFSET1(VS, VolatileState, msr);
    FIELD_OFFSET1(VS, VolatileState, r0);
    FIELD_OFFSET1(VS, VolatileState, r1);
    FIELD_OFFSET1(VS, VolatileState, r2);
    FIELD_OFFSET1(VS, VolatileState, r3);
    FIELD_OFFSET1(VS, VolatileState, r4);
    FIELD_OFFSET1(VS, VolatileState, r5);
    FIELD_OFFSET1(VS, VolatileState, r6);
    FIELD_OFFSET1(VS, VolatileState, r7);
    FIELD_OFFSET1(VS, VolatileState, r8);
    FIELD_OFFSET1(VS, VolatileState, r9);
    FIELD_OFFSET1(VS, VolatileState, r10);
    FIELD_OFFSET1(VS, VolatileState, r11);
    FIELD_OFFSET1(VS, VolatileState, r12);
    FIELD_OFFSET1(VS, VolatileState, r13);
    FIELD_OFFSET1(VS, VolatileState, f0);
    FIELD_OFFSET1(VS, VolatileState, f1);
    FIELD_OFFSET1(VS, VolatileState, f2);
    FIELD_OFFSET1(VS, VolatileState, f3);
    FIELD_OFFSET1(VS, VolatileState, f4);
    FIELD_OFFSET1(VS, VolatileState, f5);
    FIELD_OFFSET1(VS, VolatileState, f6);
    FIELD_OFFSET1(VS, VolatileState, f7);
    FIELD_OFFSET1(VS, VolatileState, f8);
    FIELD_OFFSET1(VS, VolatileState, f9);
    FIELD_OFFSET1(VS, VolatileState, f10);
    FIELD_OFFSET1(VS, VolatileState, f11);
    FIELD_OFFSET1(VS, VolatileState, f12);
    FIELD_OFFSET1(VS, VolatileState, f13);
    FIELD_OFFSET1(VS, VolatileState, fpscr);
    FIELD_OFFSET1(VS, VolatileState, cr);
    FIELD_OFFSET1(VS, VolatileState, ctr);
    FIELD_OFFSET1(VS, VolatileState, lr);
    FIELD_OFFSET1(VS, VolatileState, xer);

    FIELD_OFFSET1(ER, ExpRegs, r0);
    FIELD_OFFSET1(ER, ExpRegs, r3);
    FIELD_OFFSET1(ER, ExpRegs, r14);
    FIELD_OFFSET1(ER, ExpRegs, f0);
    FIELD_OFFSET1(ER, ExpRegs, cr);

    FIELD_OFFSET1(EPL, EntryPointLauncher, iar);
    FIELD_OFFSET1(EPL, EntryPointLauncher, msr);
    FIELD_OFFSET1(EPL, EntryPointLauncher, toc);

    FIELD_OFFSET1(KA, KernelInitArgs, relocal);
    FIELD_OFFSET1(KA, KernelInitArgs, iar);
    FIELD_OFFSET1(KA, KernelInitArgs, toc);
    FIELD_OFFSET1(KA, KernelInitArgs, msr);
    FIELD_OFFSET1(KA, KernelInitArgs, sdr1);
    FIELD_OFFSET1(KA, KernelInitArgs, stackAddr);

#if !defined(USE_EXPEDIENT_USER_PGFLT) || \
    !defined(USE_EXPEDIENT_PPC) || \
    !defined(USE_EXPEDIENT_SCHEDULER) || \
    !defined(USE_EXPEDIENT_RESERVED_THREAD) || \
    !defined(USE_EXPEDIENT_USER_RESUME) || \
    !defined(USE_EXPEDIENT_INTERRUPT) || \
    !defined(USE_EXPEDIENT_SVC)

    FIELD_OFFSET2(PA, ProcessAnnex, machine, dispatcherPhysAddr);
#endif

    FIELD_OFFSET1(EL, ExceptionLocal, lolitaSVCHandler);
    FIELD_OFFSET2(EL, ExceptionLocal, pageTable, logNumPTEs);
    FIELD_OFFSET1(EL, ExceptionLocal, refdPTECount);
    FIELD_OFFSET1(EL, ExceptionLocal, refdPTEArray);
    STRUCT_SIZE(EL_RPA, exceptionLocal.refdPTEArray);

    FIELD_OFFSET1(ST, SegmentTable_Hardware, segDescTable);
    FIELD_OFFSET1(ST, SegmentTable_SLB, slbNext);
    FIELD_OFFSET1(ST, SegmentTable_SLB, cacheNext);
    FIELD_OFFSET1(ST, SegmentTable_SLB, cacheMax);
    FIELD_OFFSET1(ST, SegmentTable_SLB, numBolted);
    FIELD_OFFSET1(ST, SegmentTable_SLB, SLBCache);
    DEFINED_CONSTANT(ST_NUMCACHED, SegmentTable_SLB::NumCached);
    DEFINED_CONSTANT(EL_SOFTWARE_LOCKED, InvertedPageTable::SOFTWARE_LOCKED);
    DEFINED_CONSTANT(ST_NUMBER_OF_SLBS, SLB_VSID::NumberOfSLBs);
