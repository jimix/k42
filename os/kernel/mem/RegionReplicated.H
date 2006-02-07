#ifndef __REGION_REPLICATED_H_
#define __REGION_REPLICATED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionReplicated.H,v 1.25 2003/12/31 20:03:51 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for object invocation of Region
 *
 * Why RequestCount?: Regions map frames by asking the FCM to prepare
 * the frame and then calling the HAT to complete the mapping.  We do
 * it this way to avoid races between map requests from the region and
 * unmap request from the FCM.  The FCM's page lock synchronizes the
 * two.
 *
 * When a region is to be destroyed, the region does a bulk unmap of
 * all its pages.  But we must delay this until all map request to
 * the FCM are completed.  The order is:
 *   finish all map requests, preventing any new ones.
 *   unmap the region.
 *   disconnect from the FCM.  (must be done AFTER the unmap).
 *
 * Rather than holding locks, we use the RequestCount to synchronize
 * this.  It waits for outstanding request while preventing new ones.
 * It also prevents destroy from executing more than once.
 *
 * The implementation of RequestCount is with atomic arithmetic,
 * rather than locks, making it the cheapest possible explicit
 * synchronization. (Until we think of a cheaper way :-)
 * **************************************************************************/

#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>
#include "mem/Region.H"
#include "mem/Access.H"
#include "sync/RequestCount.H"
#include <cobj/CObjRootMultiRep.H>


#ifndef EXPORT_REGIONREPLICATED
#define EXPORT_REGIONREPLICATED
#include <mem/RegionType.H>
#endif // EXPORT_REGIONREPLICATED

class TestSwitch;

// FIXME: should probably introduce an intermediate Region class
class RegionReplicated : public Region __xbase(Obj) {

enum RegionState { CREATING=0, NORMAL=1, DESTROYING=2 };

protected:
#ifndef _IN_STUBGEN
    class RegionReplicatedRoot : public CObjRootMultiRep {
    public:
	uval         regionVaddr;	// virtual address of region
	uval         regionSize;	// size of region
	uval         alignment;		// requested alignment of region
	HATRef       hat;		// HAT we use; bit of a hack for now
	ProcessRef   proc;		// Process we are attached to
	FCMRef       fcm;
	uval         writeAllowed;	// The fr xobject allows write
	uval         fileOffset;
	uval         regionState;
	sval         destroyCount;
	AccessMode::mode access;	// type of access, e.g. sup-wr/user-rd

	/*
	 * Needed for detaching region from hat across all processors that have
	 * accessed this region.  Eventually, we should be able to get this from
	 * the clustered object infrastrcture, where the root object will know
	 * on which processors the region has been accessed.
	 *
	 * FIXME: Jonathan Appavoo
	 *
	 * Also used to tell FCM if this is a first access from region on a proc.
	 */
	VPSet ppset;			// all procs that accessed region
	virtual CObjRep * createRep(VPNum vp);
	RegionReplicatedRoot();

	RegionReplicatedRoot(RepRef ref,
			     CObjRoot::InstallDirective idir =
			     CObjRoot::Install);


	DEFINE_GLOBALPADDED_NEW(RegionReplicatedRoot);

	SysStatus startDestroyAll();

	// handles all the details of initializing a region
	SysStatus initRegion(ProcessRef pRef, uval &vaddr, uval vaddr2,
			     uval size, uval alignreq,
			     FRRef frRef, uval writable,
			     uval fOff, AccessMode::mode accessreq,
			     uval useVaddr,
			     RegionType::Type regionType=RegionType::K42Region);
	virtual SysStatus isSharedOffset(uval offset) {
	    uval vaddr = regionVaddr + offset - fileOffset;
	    return ((vaddr&~SEGMENT_MASK)>=regionVaddr) &&
		((vaddr&~SEGMENT_MASK)+SEGMENT_SIZE) <=
		(regionVaddr+regionSize);
	}

	virtual SysStatus isSharedVaddr(uval vaddr) {
	    return ((vaddr&~SEGMENT_MASK)>=regionVaddr) &&
		((vaddr&~SEGMENT_MASK)+SEGMENT_SIZE) <=
		(regionVaddr+regionSize);
	}
    };

    friend class RegionReplicated::RegionReplicatedRoot;
    friend class TestSwitch;

    RegionReplicatedRoot *root() { return (RegionReplicatedRoot *)myRoot; }
#else /* #ifndef _IN_STUBGEN */
    struct RegionReplicatedRoot;  // for declarations later in this file
#endif /* #ifndef _IN_STUBGEN */


    RequestCountWithStop requests; // track outstanding operations

    SysStatus destroyLocal();

protected:
    RegionReplicated();		// see RegionReplicated.C


    /*
     * handle a page table miss (not a tlb miss)
     */
    /*inline*/ SysStatus
    handleFaultInternal(AccessMode::pageFaultInfo pfinfo, uval vaddr,
			PageFaultNotification *pn, VPNum vp, uval fileOffset);

    static SysStatus CreateSwitchReplacement(RegionReplicatedRoot *&root,
					     RegionRef regRef);

public:
#ifndef _IN_STUBGEN // needs to be here because of COGLOBAL
    virtual SysStatus forkCloneRegion(
	ProcessRef pref, RegionType::Type regionType);

    /* map a part of a file to address space                */
    /*    - with a fixed length                             */
    static SysStatus CreateFixedLen(
	RegionRef& ref, ProcessRef p,
	uval& regionVaddr, uval regionSize, uval alignmentreq,
	FRRef fr, uval writable, uval fileOffset, AccessMode::mode accessreq,
	RegionType::Type regionType=RegionType::K42Region,
	RegionRef use_this_ref=0);

    /*    - at a fixed address, with a fixed length         */
    static SysStatus CreateFixedAddrLen(
	RegionRef& ref, ProcessRef p,
	uval regionVaddr, uval regionSize,
	FRRef fr, uval writable, uval fileOffset,
	AccessMode::mode accessreq,
	RegionType::Type regionType=RegionType::K42Region,
	RegionRef use_this_ref=0);

    virtual SysStatus getFCM(FCMRef &ref) { ref=COGLOBAL(fcm); return 0;}
    
    virtual SysStatus setFCM(FCMRef ref) { COGLOBAL(fcm)=ref; return 0; }

    virtual SysStatus getVaddr(uval &v) { v=COGLOBAL(regionVaddr); return 0; }

    virtual void setVaddr(uval v)	        { COGLOBAL(regionVaddr) = v; }

    virtual SysStatus getOffset(uval &o) { o=COGLOBAL(fileOffset); return 0; } 

    virtual void setOffset(uval v)	        { COGLOBAL(fileOffset) = v; }


    virtual SysStatus getSize (uval &s)	{ s=COGLOBAL(regionSize);  return 0; }

    virtual SysStatus isAccessMode(AccessMode::mode m) {
        return (SysStatusUval)(m==COGLOBAL(access)); 
    }

    virtual SysStatus isSharedOffset(uval offset) {
	return COGLOBAL(isSharedOffset(offset));
    }

    virtual SysStatus isSharedVaddr(uval vaddr) {
	return COGLOBAL(isSharedVaddr(vaddr));
    }

    virtual SysStatus cloneUnattachedFixedAddrLen(RegionRef& regRef,
                                                  uval addr, uval sz, uval off,
                                                  RegionType::Type regionType)
        {
            passertMsg(0, "NYI\n"); return -1;
        }

    virtual SysStatusUval coverVaddr(uval vaddr)
        { 
            passertMsg(0,"NYI\n"); return -1; 
        }
    virtual SysStatusUval stall() { passertMsg(0,"NYI\n"); return -1; }
    virtual SysStatusUval unStall() { passertMsg(0,"NYI\n"); return -1; }

#endif /* #ifndef _IN_STUBGEN // needs to be ... */

    virtual SysStatusProcessID getPID();
    /*
     * handle a page table miss (not a tlb miss)
     * ==0 in-core, > 0 asynch, < 0 error
     */
    virtual SysStatusUval handleFault(AccessMode::pageFaultInfo pfinfo,
				      uval vaddr, PageFaultNotification *pn,
				      VPNum vp);

    /* unmaps the page at this FCM offset from the process this region
     * is attached to
     */
    virtual SysStatus unmapPage(uval offset);

    struct UnmapRangeMsg;
    static SysStatus UnmapRangeMsgHandler(uval msgUval);
    virtual SysStatus unmapRange(uval start, uval size);

    virtual SysStatus vaddrToFCM(
	VPNum vpNum, uval vaddr, uval writeAccess,
	FCMRef& fcmRef, uval& offset);

    /*
     * create a fork copy of the FCM backing this region
     * and return an oh to the FR that represents the copy.
     */
    virtual SysStatus forkCopy(FRRef& fr);

    virtual SysStatus truncate(uval start, uval length) {
	// stubgen doesn't like the assertion.  Don't know why.
	// passertMsg(0, "NYI");
	// return 0;
	return _SERROR(2361, 0, ENOSYS);
    }
    
    virtual SysStatus destroy();

    DEFINE_GLOBALPADDED_NEW(RegionReplicated);

    // initializes meta information for creating regions of this type
    static void ClassInit(VPNum vp);

__xprivate:
    /*
     * in all the following calls, target is an XHandle for the
     * process that the new region is to be attached to.
     * The XHandle must be a process XHandle and
     * the caller must have attach rights to that XHandle
     * A NULL XHandle request attach to the callers process
     */
    static SysStatus _CreateFixedLenExt(
	__out uval& regionVaddr, __in uval regionSize, __in uval alignmentreq,
	__in ObjectHandle frOH, __in uval fileOffset, __in uval accessreq,
	__in XHandle target, __in RegionType::Type regionType, 
	__CALLER_PID callerPID);

    static SysStatus _CreateFixedLenExtKludgeDyn(
	__out uval& regionVaddr, __in uval regionSize, __in uval alignmentreq,
	__in ObjectHandle frOH, __in uval fileOffset, __in uval accessreq,
	__in XHandle target, __in ObjectHandle tsOH, 
	__in RegionType::Type regionType, __CALLER_PID callerPID);

    static SysStatus _CreateFixedLenExtKludge(
	__out uval& regionVaddr, __in uval regionSize, __in uval alignmentreq,
	__in uval fileOffset, __in uval accessreq, __in XHandle target,
	__in uval partitioned, __in RegionType::Type regionType, 
	__CALLER_PID callerPID);

    static SysStatus _CreateFixedAddrLenExt(
	__in uval regionVaddr, __in uval regionSize, __in ObjectHandle frOH,
	__in uval fileOffset, __in uval accessreq, __in XHandle target,
	__in RegionType::Type regionType, __CALLER_PID callerPID);
};

#endif /* #ifndef __REGION_REPLICATED_H_ */
