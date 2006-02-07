#ifndef __FCM_PARTITIONED_H_
#define __FCM_PARTITIONED_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2004.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPartitioned.H,v 1.1 2004/11/01 19:07:55 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: 
 * **************************************************************************/
#include <misc/ListSimpleKeyLocked.H>
#include <sync/MPMsgMgr.H>
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>
#include <cobj/CObjRootMultiRep.H>
#include "mem/PageDesc.H"
#include "mem/FCM.H"
#include "mem/SegmentHAT.H"
#include <misc/DHashTable.H>
#include "mem/PageDescData.H"

#ifdef FCM_PART_AVOID_LOCALSTRICT
#define FCM_PART_ALLOC AllocGlobalPadded
#else
#define FCM_PART_ALLOC AllocLocalStrict
#endif

class FCMPartPageDesc : public LocalPageDescData {
public:
    inline void clearDirty()     { pd.dirty = PageDesc::CLEAR; }
    VPNum firstReplica() { return Scheduler::VPLimit; }

    VPNum nextReplica(VPNum vp) {
	passertMsg(0, "look");
	return 0;
    }
};

class FCMPartitioned;

class FCMPartitioned : public FCM {
public:
    enum PageListDisposition { CENTRALIZED, DISTRIBUTED, GC };
private:
    class FCMPartitionedRoot : public CObjRootMultiRep {
        friend class FCMPartitioned;
	typedef BLock LockType;
	LockType      lock;	 //Lock only used to serialize attachRegion
                                 //To protect assignment of reg and pm
        FCMPartitioned::PageListDisposition destroyPageListDir;
        PMRef     pm;
        PMRef     oldpm;
        FRRef     fr;
        RegionRef reg;
        VPSet     regPPSet;

        uval  numanode;			// where to allocate frames from
        uval  mappedExecutable:1;	// its been mapped executable
        uval  noSharedSegments:1;	// no shared segments may be used
        uval  beingDestroyed:1;

        // for now, we use the common lock to protect list manipulations
        //list keyed by segment offsets of list keyed by access type of segments
        // accessmode for segments is converted to uval, since this is required
        // for generic list stuff to work properly
        typedef ListSimpleKey<uval,SegmentHATRef,AllocGlobal>
        SegmentHATByAccessModeList;
        ListSimpleKey<uval,SegmentHATByAccessModeList*,AllocGlobal>
        segmentHATList;

        struct {
            uval padding0[16];
            FCMPartitioned *rep;
            uval padding1[16];
        } repArray[Scheduler::VPLimit];

        uval partitionSize;
        uval sizeRep;
        uval skipCount;
        VPNum myvp;

        void init(uval ps, FCMPartitioned::PageListDisposition pld, 
                  FRRef fr, uval initSize=0, uval skipCount=0);

        PMRef uninitPM() { return PMRef(TheBPRef); }
        uval  uninitPM(PMRef pm) { return pm == PMRef(TheBPRef); }

	SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
			       AccessMode::mode accessMode);

        SysStatus detachRegion(RegionRef regRef);

        // support function to maintain regioninfo ppset bits
        void updateRegionPPSet(RegionRef reg);


        SysStatus noSegMapPageInHAT(
            uval physAddr, uval virtAddr, AccessMode::pageFaultInfo pfinfo,
            AccessMode::mode access, VPNum vp, HATRef hat, RegionRef reg,
            uval offset, uval acckey);

        volatile uval remoteDestoryPageListCount;

        void recordRemoteDeallocPageList() 
            { FetchAndAdd(&remoteDestoryPageListCount, 1); }

        struct  DestroyPageListMsg {
            FCMPartitionedRoot *root;
            FCMPartitioned *rep;
            void init(FCMPartitionedRoot *rt, FCMPartitioned *rp) {
                root=rt; rep=rp; ackVar=0;
            }
            volatile uval ackVar;
            void ack() { ackVar = 1; }
            void waitForAck() { while (ackVar==0); }
        };
        
        static SysStatus DestroyPageListMsgHandler(uval msgUval);

        void remoteDestroyPageList(FCMPartitioned *rep);
        void waitForAllDeallocs(uval remoteCount);
        void locked_destroyPageList();

        SysStatus doDestroy();

        SysStatus destroy();

        

        virtual SysStatusUval isNotInUse() {
            return SysStatusUval(reg==0);
        }

        // all users are gone (except the FR if there is one)
        // should be called anywhere the use counts are seen to be zero
        // default is to destroy, but FCMs with FRs must tell the
        // FR instead
        SysStatus notInUse() {
            tassert(isNotInUse() , err_printf("Caller of notInUse lies\n"));
            if(fr) {
                return DREF(fr)->fcmNotInUse();
            } else {
                return destroy();
            }
        }
    public:
        virtual CObjRep *createRep(VPNum vp);
        DEFINE_GLOBALPADDED_NEW(FCMPartitionedRoot);
    };

    friend class FCMPartitioned::FCMPartitionedRoot;
    FCMPartitionedRoot *root() { return (FCMPartitionedRoot *)myRoot; }

protected:
     FCMPartitioned(uval ps,VPNum vp) : partitionSize(ps), myvp(vp) {}
     FCMPartitioned(uval ps,VPNum vp,uval size, uval skipCnt=0) :
         hashTable(size,size,skipCnt), partitionSize(ps), myvp(vp)
         {}

    ~FCMPartitioned();

    typedef MasterDHashTable<FCMPartPageDesc,FCMPartPageDesc,
                             FCM_PART_ALLOC,
                             FCM_PART_ALLOC> HashTable;
    HashTable hashTable;
    uval partitionSize;
    VPNum myvp;
    
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn) {
	passertMsg(0, "getPage called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    // unlocked previously gotton page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0) {
	passertMsg(0, "releasePage called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    SysStatus ioCompleteInternal(FCMPartPageDesc *pg, SysStatus rc,
                                 PageFaultNotification * skipFn);

    void detectModified(FCMPartPageDesc* pg,
                        AccessMode::mode &access,
                        AccessMode::pageFaultInfo pfinfo);

    // support function for interacting with HAT to map a page
    virtual SysStatus mapPageInHAT(uval physAddr, uval virtAddr,
				   AccessMode::pageFaultInfo pfinfo,
				   AccessMode::mode access, VPNum vp,
				   HATRef hat, RegionRef regRef,
				   FCMPartPageDesc *pg,
				   uval offset);

    SysStatus getFrame(FCMPartPageDesc *pd);

    SysStatusUval
    startFillPage(uval offset, uval paddr, PageFaultNotification* fn,
		  FCMPartPageDesc *ld);

    FCMPartPageDesc *findOrAllocatePageAndLock(uval fileOffset,
                                               SysStatus &rc,
                                               uval *paddr);

    SysStatusUval getPageInternal(uval fileOffset, PageFaultNotification *fn,
                                   FCMPartPageDesc **pd);

    virtual SysStatusUval mapPageInternal(uval fileOffset,
				  uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr,
				  AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef regRef, uval firstAccessOnPP,
                                  PageFaultNotification *fn);

    void destroyPageList();

public:
    virtual SysStatusUval mapPage(uval fileOffset,
				  uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr,
				  AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef regRef, uval firstAccessOnPP,
                                  PageFaultNotification *fn);

    virtual SysStatus attachFR(FRRef frRefArg) {
	tassert(COGLOBAL(fr) == NULL, err_printf("already attached\n"));
	COGLOBAL(fr) = frRefArg;
	return 0;
    }

    // attach to the FCM to map (possibly) part of it
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode) {
        return COGLOBAL(attachRegion(regRef, pmRef, accessMode));
    }

    virtual SysStatus detachRegion(RegionRef regRef) {
        return COGLOBAL(detachRegion(regRef));
    }

    virtual SysStatus getFRRef(FRRef& frRefArg) {
	passertMsg(0, "getFRRef called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    // This should only be called by the owning FR class (not easy to enforce)
    virtual SysStatus destroy() {
        return COGLOBAL(destroy());
    }

    // If something other than a region needs to prevent FCM destruction
    // is must add a reference (count).  see ProcessVPList for example.
    virtual SysStatus addReference() {
	passertMsg(0, "addReferece called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    virtual SysStatus removeReference() {
	passertMsg(0, "removeReferece called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    // force writing of all modified pages
    virtual SysStatus fsync() {
	tassertMsg(0, "fsync called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    // return 1 if FCM has no users
    // N.B. FCM may still have frames cached
    // FIXME:  leaky kludge always say we are in use
    virtual SysStatusUval isNotInUse() {
        return COGLOBAL(isNotInUse());
    }

    virtual SysStatusUval isEmpty() {
	passertMsg(0, "isEmpty called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    // unmap and throw away all pages - even dirty ones!
    virtual SysStatus discardCachedPages() {
	passertMsg(0, "discardCachedPages called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments() {
        return 0;
    }
	
    virtual SysStatus printStatus(uval kind) {
	passertMsg(0, "printStatus called FCMPartitioned\n");
        return _SERROR(2711, 0, EINVAL);
    }

    DEFINE_LOCALSTRICT_NEW(FCMPartitioned);

    static SysStatus Create(FCMRef &ref, uval pSize,
                            PageListDisposition pld, FRRef fr=NULL);

    static SysStatus Create(FCMRef &ref, uval pSize, 
                            PageListDisposition pld, uval initSize,
                            uval skipCount, FRRef fr=NULL);
};
#endif /* #ifndef __FCM_PARTITIONED_H_ */
