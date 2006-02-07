#ifndef __EXCEPTION_LOCAL_H_
<<<< include machine independent file - not this machine dependent file >>>>
#endif /* #ifndef __EXCEPTION_LOCAL_H_ */
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ExceptionLocal.H,v 1.90 2005/08/22 14:54:07 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#define EXC_MAX_NUM (0x10+2)  // maximum number of exceptions we deal with

#include "sys/syscalls.H"
#include "sys/VolatileState.H"
#include "mem/arch/powerpc/InvertedPageTable.H"
#include "misc/hardware.H"
#include "init/arch/powerpc/MemoryMgrPrimitiveRealMode.H"

/*
 * Structure for holding the registers that are passed through intact
 * on IPC, needed for remote IPC delivery.
 */
struct IPCRegsArch {
    uval64 r3_r10[8];
    uval64 r15_r31[17];
    uval64 f0_f31[32];
    uval64 cr;
};

struct RfiState {uval64 r0, r1, r2;};

struct ExceptionMachine {
    /*
     * NOTE:  each processor needs a bolted area for restoring registers
     * so the critical code sequences that do rfid with paging on do not
     * take any page faults.  We provide this storage in several bolted
     * pages, and each exception local addresses the space reserved
     * for that processor.  Each area is on a cache line.
     */

    RfiState* boltedRfiState;
    RfiState* boltedRfiStatePage;
    uval boltedRfiStatePageSize;
    uval64 msrUserChange;	// MSR bits that a program is allowed to change
    struct {uval64 r2, r3, r4, r5, r6, r7;} lolitaState;

    struct {uval64 hv_r0, hv_r1, hv_r2, hv_r3, hv_r4, hv_r5, hv_r6, hv_r7,
		    hv_r8, hv_r9, hv_r10, hv_r11, hv_r12,
		    hv_srr0, hv_srr1 , hv_ctr, hv_lr, hv_cr;} lolitaState2;

    struct {uval64 pre0, pre1, hv_stack, post0, post2;} lolitaStack;
    struct {uval64 hv_ret0, hv_ret1, hv_ret2, hv_ret3;} lolitaRet;

    uval kernelRegionsEnd;	// lower bound on well known kernel-address
    uval vMapsRStart;		// start of V->R range
    uval vMapsREnd;		// end of V->R range
    uval vMapsRDelta;		// V-to-R delta for V->R range
    uval kernelPSRStart;	// start of kernel processor-specific range
    uval kernelPSREnd;		// end of kernel processor-specific range
    uval kernelPSRDelta;	// V-to-R delta for kernel p-specific range
    SLB_VSID kernelPSRVSID;	// vsid to use on this processor
    uval commonPSRStart;	// start of common processor-specific range
    uval commonPSREnd;		// end of common processor-specific range
    uval commonPSRDelta;	// V-to-R delta for common p-specific range
    SLB_VSID commonPSRVSID;     // vsid for this processor
    uval commonPSRWritable;	// start of the user-writable subrange of
				//     common processor-specific range
    uval traceRgnStart;		// start of tracing range
    uval traceRgnEnd;		// end of tracing range
    uval traceRgnDelta;		// V-to-R delta for tracing range
    uval ioRgnStart;		// start of I/O range
    uval ioRgnEnd;		// end of I/O range
    uval ioRgnMap;		// address of map for I/O range

    codeAddress trapHandler;	// phys jump address for trap exc handling
    codeAddress handlers[EXC_MAX_NUM];	// phys jump addresses for exc handling

    uval64 toc;			// kernel toc value for exception level
    uval64 msr;			// kernel msr value for exception level
    uval64 elocalVirt;		// virtual address of exceptionLocal
    // virtual addresses of exception-level entry points
    codeAddress excPgfltExceptionUser;
    codeAddress excPgfltExceptionKernel;
    codeAddress excIOInterrupt;
    codeAddress excDecInterrupt;
    codeAddress excPerfInterrupt;
    codeAddress excTrapExceptionUser;
    codeAddress excTrapExceptionKernel;
    codeAddress svc[SYSCALL_LIMIT];
    InvertedPageTable pageTable;	// per-processor page table
    // statistics
    uval num_null;      		// counter for ignored interrupts
    uval num_exi;      			// counter for external interrupts
    uval num_dec;      			// counter for decrementer interrupts
    uval num_trap;			// counter for trap interrupts
    uval num_nonnative_svc;		// counter for non-native syscalls

    uval trapSkipDecr;			// skip timer interrupt after trap
    VolatileState trapVolatileState;	// save area for kernel-mode traps
    uval physCPU;			// physical cpu number
    uval logicalCPU;			// logical cpu number


    // Hardware supports a very large number of interrupt sources
    // We support 64 simultaneous active interrupts.
    // Use a bitmap to identify which array entries are active.
    uval64 pendingIntrs;		// pending interrupts
					// bits identify active entries in...
    uval16 interrupts[64];		// interrupt numbers
    uval intrHandlerThr;		// interrupt handler thread

    // Memory manager for machines on which not all memory is real-mode
    // accessible.
    MemoryMgrPrimitiveRealMode *realModeMemMgr;

    void machineInit(VPNum vp, MemoryMgrPrimitiveKern *memory) {
	pendingIntrs = 0;
	intrHandlerThr = 0;
	memset(&interrupts, 0xffff, sizeof(interrupts));
	realModeMemMgr = NULL;
    }
    inline void enterDebuggerArch();
    inline void exitDebuggerArch();

    /*
     * We can debug some low-level stuff using kludgy system calls handled
     * directly in lolita.  The lolitaSVCHandler field is needed to support
     * the general mechanism.  The remaining structures are throw-aways.
     */
    codeAddress lolitaSVCHandler;
    uval refdPTECount;	// number of referenced PTE's found
    struct {		// array of referenced PTE's
	uval64 pteAddr;
	uval64 vsidWord;
	uval64 rpnWord;
    } refdPTEArray[32];
};

extern code ExceptionLocal_CriticalPage;
extern code ExceptionLocal_CriticalPageEnd;

extern code ExceptionLocal_TrapExceptionUser;
extern code ExceptionLocal_TrapExceptionKernel;
extern code ExceptionLocal_PgfltExceptionUser;
extern code ExceptionLocal_PgfltExceptionKernel;
extern code ExceptionLocal_PgfltExceptionKernelInDebugger;

extern code ExceptionLocal_IOInterrupt;
extern code ExceptionLocal_DecInterrupt;
extern code ExceptionLocal_PerfInterrupt;

extern code ExceptionLocal_NonnativeSyscall;
extern code ExceptionLocal_SetEntryPointSyscall;
extern code ExceptionLocal_ProcessYieldSyscall;
extern code ExceptionLocal_ProcessHandoffSyscall;
extern code ExceptionLocal_IPCCallSyscall;
extern code ExceptionLocal_IPCReturnSyscall;
extern code ExceptionLocal_PPCPrimitiveSyscall;
extern code ExceptionLocal_IPCAsyncSyscall;
extern code ExceptionLocal_TimerRequestSyscall;

extern code ExceptionLocal_InvalidSyscall;
extern code ExceptionLocal_UserRFISyscall;
extern code ExceptionLocal_TrapRFISyscall;

inline void
ExceptionMachine::enterDebuggerArch()
{
    /*
     * When we're in the debugger, we can't try to resolve page faults
     * because we're not enabled and on a thread.  So we change the page
     * fault handler to one that converts faults directly to traps.
     */
    excPgfltExceptionKernel = &ExceptionLocal_PgfltExceptionKernelInDebugger;
}

inline void
ExceptionMachine::exitDebuggerArch()
{
    excPgfltExceptionKernel = &ExceptionLocal_PgfltExceptionKernel;
}

extern "C" void ExceptionLocal_LoadSLB(uval s);
