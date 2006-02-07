#ifndef __KERNEL_INFO_H_
#define __KERNEL_INFO_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000,2001,2002,2003,2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: KernelInfo.H,v 1.41 2005/07/21 14:00:55 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     Defines a shared structure by which the kernel publishes information.
 * **************************************************************************/

#include "trace/traceCore.H"

struct KernelInfo;

extern KernelInfo volatile kernelInfoLocal;
// kernelInfoLocal must be declared volatile because fields will change when
// a thread migrates.



// Linux code refers to a "LinuxVPInfo" structure
// (arch/include/asm-ppc64/smp.h) that it uses to identify which
// cpu it is currently on.
// That data-structure is in fact the first 6 fields of KernelInfo.
// Because of header inclusion problems we must define these independently,
// using different names to avoid clashes.
struct LinuxVPInfoK42 {
    VPNum physProc;			// logical physical processor number
    VPNum hwProc;			// hardware physical processor number
    VPNum numaNode;			// logical numa node number
    VPNum procsInNumaNode;		// num procs in this numa node
    VPNum curPhysProcs;			// current number of processors
    VPNum maxPhysProcs;			// maximum number of processors
};

struct KernelInfo : public LinuxVPInfoK42 {
    uval onSim;
    uval onHV;
    uval16 sCacheLineSize;		// external/2nd-ary cache line size
    uval16 pCacheLineSize;		// internal/primary cache line size
    TraceInfo traceInfo;		// tracing structures
    /*
     * Following are values global to the entire system.
     * Whenever any changes, must publish change to all processors
     * See KernelInfoMgr.
     */
    struct SystemGlobal {
	uval ticksPerSecond;
	uval epoch_sec;			// offset from clock zero
	uval epoch_usec;		// to the epoch 1/1/1970
	uval mountVersionNumber;	// version number for mount information
	uval controlFlags;
    } systemGlobal;

    /*
     * Bit numbers for the bits in systemGlobal.controlFlags.
     */
    enum {
	RUN_SILENT,
	DISABLE_IO_CPU_MIGRATION,
	SLOW_THINWIRE_POLLING,
	UID_PROCESSOR_ASSIGNMENT,
	DYN_PROCESSOR_ASSIGNMENT,
	PAGING_OFF,
	NON_SHARING_FILE_OPT,  // uses specialized impl for single client files
        USE_MULTI_REP_FCMS,    // use multirep fcms in FRVA, FRPA and
                               // FRPANonPageable
	NO_ALLOC_SANITY_CHECK, // Avoid allocator sanit checks
	SMALL_FILE_OPT,        // use small file optimization
	DBG_FLAG,              // use to dynamically turn on or off code
	SLOW_EXEC,	       // Disable "fast exec" optimization
// Following flags are for special measurements - not normally on
// even for performance test runs
	NFS_INTERCEPTION = 31,         // causes passert on NFS opration
	SHARED_PAGE_TABLE,
	DONT_DISTRIBUTE_PMROOT,
	DONT_DISTRIBUTE_PROCESS,
	DONT_DISTRIBUTE_PRMTV_FCM,
	NO_NONBLOCKING_HASH,
	NO_SHARED_SEGMENTS,
	NO_NUMANODE_PER_VP,
	TEST_FLAG,			// for use in experiments that
					// will not be published
	NFS_REVALIDATION_OFF = 62,      
	BREAKPOINT_KERNEL_INIT = 63
     };

    void init(uval onHVArg,
	      uval onSimArg,
	      VPNum numaNodeArg,
	      VPNum procsInNumaNodeArg,
	      VPNum physProcArg,
	      VPNum curPhysProcsArg,
	      VPNum maxPhysProcsArg,
	      uval16 sCacheLineSizeArg,
	      uval16 pCacheLineSizeArg,
	      uval controlFlagsArg);

    static uval   OnHV() { return kernelInfoLocal.onHV; }
    static uval   OnSim()          { return kernelInfoLocal.onSim; }
    static VPNum  NumaNode()       { return kernelInfoLocal.numaNode; }
    static VPNum  ProcsInNumaNode() { return kernelInfoLocal.procsInNumaNode; }
    static VPNum  PhysProc()       { return kernelInfoLocal.physProc; }
    static VPNum  CurPhysProcs() { return kernelInfoLocal.curPhysProcs; }
    static VPNum  MaxPhysProcs() { return kernelInfoLocal.maxPhysProcs; }
    static uval16 SCacheLineSize() { return kernelInfoLocal.sCacheLineSize; }
    static uval16 PCacheLineSize() { return kernelInfoLocal.pCacheLineSize; }

    static uval ControlFlagIsSet(uval ctrlBit) {
	// pseudo-boolean
	return kernelInfoLocal.systemGlobal.controlFlags &
						(uval(1) << ctrlBit);
    }

    static uval GetControlFlags() {
	return kernelInfoLocal.systemGlobal.controlFlags;
    }
};

#endif /* #ifndef __KERNEL_INFO_H_ */
