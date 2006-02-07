#ifndef __BASE_PROCESS_H_
#define __BASE_PROCESS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: BaseProcess.H,v 1.79 2004/12/16 23:09:03 awaterl Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A process object keeps track of all other
 * objects associated with a process.
 * **************************************************************************/

#ifndef EXPORT_BASEPROCESS
#define EXPORT_BASEPROCESS

#include <sys/entryPoints.H>
#include <sys/SoftIntr.H>
#include <defines/experimental.H>
#include <mem/RegionType.H>

// the type of processes this object knows how to create
#define PROCESS_DEFAULT 1

#endif /* #ifndef EXPORT_BASEPROCESS */
class MemTrans;
typedef MemTrans **MemTransRef;

#define __NO_REGISTER_WITH_KERNEL_WRAPPER

class BaseProcess : public Obj {
public:
    enum { START_PERF_MON, STOP_PERF_MON, COLLECT_PERF_MON, ZERO_PERF_MON,
	   PRINT_PERF_MON, BEGIN_PERF_MON, END_PERF_MON,
	   START_MLS_STATS, DUMP_ZERO_MLS_STATS };

    struct ResourceUsage {
        uval minflt;   /* Number of in-core page faults.  */
        uval majflt;   /* Number of out-of-core page faults.  */

        ResourceUsage() {
	    minflt = 0;
	    majflt = 0;
	}
    };

public:
    /*
     * attach x object to process for garbage collection
     * to avoid races with destruction, the creator
     * first locks, then registers/unregisters, then unlocks.
     * the rest of the registration/unregistration also occurs while
     * this lock is held.
     */
    virtual SysStatus lockMatchedXObjectList()=0;
    virtual SysStatus unlockMatchedXObjectList()=0;
    virtual SysStatus addMatchedXObj(XHandle xhandle)=0;
    virtual SysStatus removeMatchedXObj(XHandle xhandle)=0;
    virtual SysStatus closeMatchedXObjectList()=0;
    virtual SysStatus isClosedMatchedXObjectList()=0;
    virtual SysStatus isEmptyMatchedXObjectList()=0;

    // call from other process to wait for process termination
    virtual SysStatus waitForTermination()=0;

    // Number of vp's currently allocated to this process,
    // (for the kernel, this is also the number of physical processors
    // available), and number of physical processors available.
    virtual SysStatusUval vpCount()=0;
    virtual SysStatusUval ppCount()=0;

    virtual SysStatus userHandleFault(uval vaddr, VPNum vp)=0;

    virtual SysStatus sendInterrupt(DispatcherID dspid,
				    SoftIntr::IntrType i)=0;

    virtual SysStatus unmapRange(uval start, uval size)=0;

    virtual SysStatus kill()=0;

    virtual SysStatus regress()=0;

    virtual SysStatus perfMon(uval action, uval ids)=0;

    virtual SysStatus breakpoint()=0;

    // note, if making call on own process, cool to ignore error check
    virtual SysStatusProcessID getPID()=0;

    // FIXME: a temporary test, delete when change test user-user
    virtual SysStatus testUserIPC(ObjectHandle oh)=0;

    // dummy for testing base IPC performance
    virtual SysStatus testIPCPerf()=0;

    // Acquire per process resource usage counters.
    virtual SysStatus getRUsage(BaseProcess::ResourceUsage& usage)=0;

    //The OS personality can use these functions to maintain a link
    //to personality data associated with the process
    virtual SysStatus setOSData(uval data)=0;

    virtual SysStatus getOSData(uval& data)=0;

    virtual SysStatus getNextRegion(uval& vaddr, RegionRef& regionRef)=0;

    virtual SysStatus getLocalSMT(MemTransRef &mtr, XHandle &remoteSMT,
				  uval key) = 0;

    virtual SysStatus removeSMT(MemTransRef mtr) = 0;

    virtual SysStatus addSMT(MemTransRef mtr, XHandle remoteSMT,uval key) = 0;

    // -------------- all the lazy stuff ----------------------
    // request from client to re-open a file
    virtual SysStatus lazyReOpen(sval file, uval &type, 
				 ObjectHandle &oh, char *data, 
				 uval &dataLen)=0;
    // request from client to close file
    virtual SysStatus lazyClose(sval file)=0;
    // request from server to give access lazily, closeChain identifies
    // another fd to also close when closing this one -1 terminates
    virtual SysStatus lazyGiveAccess(sval file, uval type, ObjectHandle oh, 
				     sval closeChain,
				     AccessRights match, AccessRights nomatch,
				     char *data, uval dataLen)=0;
    // request from client to copy all lazy state
    virtual SysStatus lazyCopyState(XHandle target)=0; // copy to target proc
    // -------------- end the lazy stuff ----------------------

    // -------------- region list operations ------------------
    virtual SysStatus preFork(XHandle childXH)=0;
    virtual SysStatus preExec()=0;

    virtual SysStatus findRegion(uval start,
				 RegionType::RegionElementInfo& element)=0;

    // was there before
    virtual SysStatus regionDestroy(uval regionAddr)=0;
    // virtual SysStatus removeRegion(uval start); <- check above covers
    virtual SysStatus regionTruncate(uval start, uval length)=0;
    // virtual SysStatus truncateRegion(uval start, uval length); <- check above

    // For the moment we use very restricted interface for MIP regions
    //  Only creates mip region in callerPID, 
    //  alignment must be on a page boundary, 
    //  fr is created internally
    //  offset is calculated from pre-existing region
    //  access is assumed to be read-write
    //  region type is hard coded will
    // will probably export more functionality to user space later.
    virtual SysStatus createMIP(uval &regionVaddr, uval regionSize,
                                uval chunkSize, uval mipOptions) = 0;
    virtual SysStatus destroyMIP(uval regionVaddr, uval mipOptions) = 0;
    virtual SysStatus flushMIP(uval regionVaddr, uval regionSize,
                               uval mipOptions) = 0;
    virtual SysStatus fetchMIP(uval regionVaddr, uval regionSize,  
                               uval mipOptions)  = 0;
    // -------------- end region list operations --------------

};

#endif /* #ifndef __BASE_PROCESS_H_ */
