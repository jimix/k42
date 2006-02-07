#ifndef __REGION_DEFAULT_H_
#define __REGION_DEFAULT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: RegionDefault.H,v 1.98 2004/10/08 21:40:09 jk Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
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
#ifndef EXPORT_REGIONDEFAULT
#define EXPORT_REGIONDEFAULT
#include <mem/RegionType.H>
#endif /* #ifndef EXPORT_REGIONDEFAULT */
#ifndef _IN_STUBGEN
#include "mem/FCM.H"
class TestSwitch;
#endif /* #ifndef _IN_STUBGEN */

// FIXME: should probably introduce an intermediate Region class
class RegionDefault : public Region
    __xbase(Obj)
{
#ifndef _IN_STUBGEN
    friend class TestSwitch;
#endif /* #ifndef _IN_STUBGEN */
protected:
    uval         regionVaddr;		// virtual address of region
    uval         regionSize;		// size of region
    uval         attachSize;		// size of address space covered
    uval         pageSize;		// minimum page size this region
    uval         alignment;		// requested alignment of region
    HATRef       hat;			// HAT we use; bit of a hack for now
    ProcessRef   proc;			// Process we are attached to
    FCMRef       fcm;
    uval         writeAllowed;		// The fr xobject allows write
    uval         fileOffset;
    AccessMode::mode access;		// type of access, e.g. sup-wr/user-rd
    RequestCountWithStop
	         requests;              // track outstanding operations
                                        // and synchronize shutdown and forks

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

protected:
    RegionDefault() { }

    /*
     * handle a page table miss (not a tlb miss)
     */
    /*inline*/ SysStatus
    handleFaultInternal(AccessMode::pageFaultInfo pfinfo, uval vaddr,
			PageFaultNotification *pn, VPNum vp, uval fileOffset);

#ifndef _IN_STUBGEN
    // handles all the details of initializing a region
    // if vaddr2 is not zero, vaddr/vaddr2 provide bounds for where
    // new region can appear region
    SysStatus initRegion(ProcessRef pRef, uval &vaddr, uval vaddr2,
			 uval size, uval alignreq,
			 FRRef frRef, uval writable, uval fOff,
			 AccessMode::mode accessreq, uval useVaddr,
			 RegionType::Type regionType = RegionType::K42Region,
                         uval skipProcessAttach=0);
#endif /* #ifndef _IN_STUBGEN */

public:
    // these first two are public so RegionReplicated, for example,
    // can use them.  They are static and don't do anything dangerous
    static SysStatus PrefFromTarget(
	XHandle target, ProcessID callerPID, ProcessRef& pref);


    static SysStatus GetFR(
	XHandle target, ProcessID callerPID, ProcessRef& pref,
	ObjectHandle frOH, FRRef& frRef, uval &mrights);

    static SysStatus GetFR(
	XHandle target, ProcessID callerPID, ProcessRef& pref,
	ObjectHandle frOH, FRRef& frRef) {
	uval mrights;
	return GetFR(target, callerPID, pref, frOH, frRef, mrights);
    }

    static SysStatus CheckCaller(
	XHandle target, ProcessID callerPID, ProcessRef& pref,
	ObjectHandle frOH, AccessMode::mode accessreq,
	FRRef& frRef, uval& writable);
    
#ifndef _IN_STUBGEN
    /* map a part of a file to address space                */
    /*    - with a fixed length                             */
    static SysStatus
    CreateFixedLen(RegionRef& ref, ProcessRef p,
		   uval& regionVaddr, uval regionSize, uval alignmentreq,
		   FRRef frRef, uval writable, uval fileOffset,
		   AccessMode::mode accessreq,
		   RegionType::Type regionType=RegionType::K42Region,
		   RegionRef use_this_ref=0);

#ifdef PROTOTYPE_SUBLIST
    static SysStatus
    CreateFixedLenSubRegion(RegionRef& ref, ProcessRef p,
                            uval& regionVaddr, uval regionSize, 
                            uval alignmentreq, FRRef frRef, uval writable,
                            uval fileOffset, AccessMode::mode accessreq,
                            RegionType::Type regionType=RegionType::K42Region,
                            RegionRef use_this_ref=0);
#endif

    /*    - at a fixed address, with a fixed length         */
    static SysStatus
    CreateFixedAddrLen(RegionRef& ref, ProcessRef p,
		       uval regionVaddr, uval regionSize,
		       FRRef frRef, uval writable, uval fileOffset,
		       AccessMode::mode accessreq,
		       RegionType::Type regionType=RegionType::K42Region,
		       RegionRef use_this_ref=0);

    /*    - with a fixed length, within a given range       */
    /*      bounds given by regionVaddr1/regionVaddr2, result in regionVaddr1*/
    static SysStatus
    CreateFixedLenWithinRange(RegionRef& ref, ProcessRef p,
			      uval& regionVaddr1, uval regionVaddr2,
			      uval regionSize, uval alignmentreq,
			      FRRef frRef, uval writable, uval fileOffset,
			      AccessMode::mode accessreq,
			      RegionType::Type regionType=RegionType::K42Region,
			      RegionRef use_this_ref=0);

    static SysStatus
    CreateUnAttachedMIPRegion(RegionRef &mipRef, ProcessRef procRef, 
                              uval mipVaddr, uval mipSize, 
                              uval mipOffset, AccessMode::mode mipAccessMode,
                              FRRef mipFrRef);

#endif /* #ifndef _IN_STUBGEN */
    
    virtual SysStatus getFCM(FCMRef &ref) { ref=fcm; return 0; }

    /* FIXME: DANGEROUS BIG KLUDGE */
    virtual SysStatus setFCM(FCMRef ref) { fcm=ref; return 0; }

    virtual SysStatus  getVaddr(uval &v)
    { v=regionVaddr; return 0; }

    virtual void setVaddr(uval v)	        { regionVaddr = v; }

    virtual SysStatus getOffset(uval &o) { o=fileOffset; return 0; } 

    virtual void setOffset(uval v)	        { fileOffset = v; }

    virtual SysStatus getSize (uval &s)	{ s=regionSize;  return 0; }

    virtual SysStatus isAccessMode(AccessMode::mode m) {
        return (SysStatusUval)(m==access); 
    }

    // return 0 if vaddr NOT in range of covered by Region
    // return 1 if vaddr is in range  covered by Region
    virtual SysStatusUval coverVaddr(uval vaddr) {
        SysStatusUval rc=1;
        requests.enter();
	if ((vaddr < regionVaddr) ||
	    (vaddr >= (regionVaddr + regionSize))) {
	    rc=0;
        }
        requests.leave();
        return rc;
    }

#ifndef _IN_STUBGEN			// SEGMENT_MASK etc. not available
    virtual SysStatus isSharedOffset(uval offset) {
	SysStatus rc;
	uval vaddr = regionVaddr + offset - fileOffset;
	rc = (SEGMENT_ROUND_DOWN(vaddr)>=regionVaddr) &&
	    ((SEGMENT_ROUND_DOWN(vaddr)+SEGMENT_SIZE) <=
	    (regionVaddr+regionSize));
	tassertMsg((rc == 0) || !(DREF(hat)->privateSegment(vaddr)),
		   "Shared address %lx mapped private\n", vaddr);
	return rc;
    }

    virtual SysStatus isSharedVaddr(uval vaddr) {
	SysStatus rc;
	rc = (SEGMENT_ROUND_DOWN(vaddr)>=regionVaddr) &&
	    ((SEGMENT_ROUND_DOWN(vaddr)+SEGMENT_SIZE) <=
	    (regionVaddr+regionSize));

	tassertMsg((rc == 0) || 
                   !(DREF(hat)->privateSegment(vaddr)) ||
                   (fcm!=0 && !(DREF(fcm)->sharedSegments())),
		   "Shared address %lx mapped private\n", vaddr);
	return rc;
    }
#endif 

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

    virtual SysStatus forkCloneRegion(ProcessRef pref, 
                                      RegionType::Type regionType);

    virtual SysStatus cloneUnattachedFixedAddrLen(RegionRef& regRef,
                                                  uval addr, uval sz, uval off,
                                                  RegionType::Type regionType);

    virtual SysStatusUval stall()   { return requests.stop();       }
    virtual SysStatus     unStall() { requests.restart(); return 0; }

    virtual SysStatus destroy();

    DEFINE_GLOBALPADDED_NEW(RegionDefault);

    // initializes meta information for creating regions of this type
    static void ClassInit(VPNum vp);

__xprotected:
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
	__in RegionType::Type regionType, 
	__CALLER_PID callerPID);

    static SysStatus _CreateFixedLenExtKludge(
	__out uval& regionVaddr, __in uval regionSize, __in uval alignmentreq,
	__in uval fileOffset, __in uval accessreq, __in XHandle target,
	__in uval partitioned, __in RegionType::Type regionType, 
	__CALLER_PID callerPID);

    static SysStatus _CreateFixedAddrLenExt(
	__in uval regionVaddr, __in uval regionSize, __in ObjectHandle frOH,
	__in uval fileOffset, __in uval accessreq, __in XHandle target,
	__in RegionType::Type regionType, 
	__CALLER_PID callerPID);

    static SysStatus _CreateFixedLenWithinRangeExt(
	__inout uval& regionVaddr, __in uval regionVaddr2,
	__in uval regionSize, __in uval alignmentreq,
	__in ObjectHandle frOH, __in uval fileOffset, __in uval accessreq,
	__in XHandle target, __in RegionType::Type regionType,
	__CALLER_PID callerPID);

    /*
     * Create a region that maps to the memory area where the kernel image
     * is saved for fast reboot.
     */
    static SysStatus _CreateRebootImage(__out uval& regionVaddr,
					__out uval& regionSize,
					__CALLER_PID callerPID);

    virtual SysStatusProcessID getPID();

    virtual SysStatus truncate(uval start, uval size);
};

class RegionDefaultKernel: public RegionDefault {
public:

#ifndef _IN_STUBGEN
    /* map a part of a file to address space                */
    /*    - with a fixed length                             */
    static SysStatus
    CreateFixedLen(RegionRef& ref, ProcessRef p,
		   uval& regionVaddr, uval regionSize, uval alignmentreq,
		   FRRef frRef, uval writable, uval fileOffset,
		   AccessMode::mode accessreq,
		   RegionType::Type regionType=RegionType::K42Region,
		   RegionRef use_this_ref=0);

    /*    - at a fixed address, with a fixed length         */
    static SysStatus
    CreateFixedAddrLen(RegionRef& ref, ProcessRef p,
		       uval regionVaddr, uval regionSize,
		       FRRef frRef, uval writable, uval fileOffset,
		       AccessMode::mode accessreq, 
		       RegionType::Type regionType=RegionType::K42Region,
		       RegionRef use_this_ref=0);

    /*    - with a fixed length, within a given range       */
    /*      bounds given by regionVaddr1/regionVaddr2, result in regionVaddr1*/
    static SysStatus
    CreateFixedLenWithinRange(RegionRef& ref, ProcessRef p,
			      uval& regionVaddr1, uval regionVaddr2,
			      uval regionSize, uval alignmentreq,
			      FRRef frRef, uval writable, uval fileOffset,
			      AccessMode::mode accessreq,
			      RegionType::Type regionType=RegionType::K42Region,
			      RegionRef use_this_ref=0);
#endif /* #ifndef _IN_STUBGEN */

    DEFINE_PINNEDGLOBALPADDED_NEW(RegionDefaultKernel);

    virtual SysStatus isSharedOffset(uval offset) {
	(void) offset;return 0; }

    virtual SysStatus isSharedVaddr(uval vaddr) {
	(void) vaddr; return 0; }
};

#endif /* #ifndef __REGION_DEFAULT_H_ */
