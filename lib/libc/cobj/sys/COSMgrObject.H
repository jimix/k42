#ifndef __COSMGR_OBJECT_H_
#define __COSMGR_OBJECT_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSMgrObject.H,v 1.27 2005/03/02 05:27:56 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: The Clustered Object System Manager (which itself is a
 *    Clustered Object.  There is one rep per vp.  which manges the local trans
 *    cache and the vp's portion of the main trans table.
 * **************************************************************************/
#include "TransPageDescList.H"
#include "COVTable.H"
#include <scheduler/Scheduler.H>
#include <scheduler/VPSet.H>
#include <trace/traceClustObj.h>
#include <misc/ListSimple.H>
#include <misc/ListArraySimple.C>
#include <misc/ListSimpleKey.H>
#include <cobj/CObjRoot.H>

//#define COSKEEPCOUNTS 1
#define CLEANUP_DAEMON 1

class MemoryMgrPrimitive;

class COSMgrObject : public COSMgr {
protected:
    class WorkerThreadReset: public MPMsgMgr::MsgAsync{
    protected:
        CORef target;
    public:
        DEFINE_GLOBAL_NEW(WorkerThreadReset);
        WorkerThreadReset(CORef t): target(t){}
        virtual void handle(){
            DREFGOBJ(TheCOSMgrRef)->resetLocalTransEntry(target);
        }
    };

    class COSMgrObjectRoot : public CObjRoot {
    protected:
	volatile COSMgrObject *repList;
	volatile COSMgrObject *repTail;
	void initMembers();
    public:
        /* Default Object for all processors */
        COSDefaultObject   theDefaultObject;

        // FIXME:  May need to revisit if assumptions turn out to be
        //         wrong.
        // We managed the pinned entires globally.
        // We expect little use of these entries and predominately 
        // accessed at startup on one vp
	TransPageDescList  pinnedPageDescList;

	// dummy clustered object that generates a breakpoint for all calls
	COSBreakpointObject   theBreakpointObject;
	COSBreakpointObject  *theBreakpointObjectP; // dummy cobj entry

	// FIXME: Fix padding to be more generic
	struct {
	    uval filler1[8];
	    uval genCount;
	    uval filler2[8];
	} global;

#ifdef CLEANUP_DAEMON
#define CLEANUP_DAEMON_THRESHOLD 10;
	uval cleanupThreshold;
#define CLEANUP_DAEMON_DELAY_TIME_MSEC 2000
	uval cleanupDelay;
#endif

	void updateGlobalGenCount();
	void stutterGlobalGenCount();

	COSMgrObjectRoot();
	// Define a new which does not rely on standard allocators
	DEFINE_PRIMITIVE_NEW(COSMgrObject::COSMgrObjectRoot);


	void init(VPNum vp, COSMgrObject *rep);

	// COSMgrObjectRoot is a special case which defines its own version
	// of the standard  root functionality to use the circular
	// chain of reps it maintians in an efficient manner avoiding
	// the global lock, rather than using CORootMultiRepBase
	virtual SysStatus handleMiss(COSTransObject * &co,
				     CORef ref, uval methodNum);
	virtual SysStatus cleanup(COSMissHandler::CleanupCmd cmd);
	virtual VPSet getTransSet();
	virtual VPSet getVPCleanupSet();
	virtual CObjRep *getRepOnThisVP();
    };
    friend class COSMgrObject::COSMgrObjectRoot;

    COSMgrObjectRoot *root() { return (COSMgrObjectRoot *)myRoot; }

    SysStatus  destroyCOInternal(CORef ref, COSMissHandler *obj, uval refOnly);
protected:
    /* Define a new which does not rely on standard allocators */
    DEFINE_PRIMITIVE_NEW(COSMgrObject);

    /* The vp this rep is associated with */
    VPNum vp;

    /* list to manage this reps portion of the clustered object id space */
    TransPageDescList  pagablePageDescList;
#if 0
    //FIXME: Old version here until I am convinced new version does the same
    uval isRefLocal(CORef ref) {
        uval start=(uval)lTransTablePaged + (uval)(vp * gPartPagableSize);
        // if in vp partition or pinned portion return true
        if ((((uval)ref >= start) &&
             ((uval)ref < (start + (uval)gPartPagableSize)))
            || (((uval)ref >= (uval)lTransTablePinned) && 
                ((uval)ref <  ((uval)lTransTablePinned + gPinnedSize)))) {
            return 1;
        }
        return 0;
    }
#endif

    uval isRefLocal(CORef ref) {
        tassert(isRef(ref), ;);
        // Internally to the COSmgr we treat a pinned entry as being local to
        // all vps for the sake of destruction since we managed the pinned 
        // portion of the table with a common list.
        // Note refToVP maps pinned entries to vp zero so we must explicity 
        // handle this case first!
        if (((uval)ref >= (uval)lTransTablePinned) && 
            ((uval)ref <  ((uval)lTransTablePinned + gPinnedSize))) {
            return 1;
        }
        return (refToVP(ref)==vp);
    }

    /* Destruction data structures */
    typedef BLock LockType;
    // used to serialize calls to checkCleanup and the methods it depends on
    LockType lock;
    // used to serialize fork and garbage collection since the
    // gc thread does NOT survive fork
    LockType forkLock;

#ifdef CLEANUP_DAEMON
    // id of the cleanup daemon thread associated with this rep
    ThreadID cleanupDaemonID;
    // keep track of the number of times destroy has been invoked
    // since last explicit kick of the cleanupDameon
    uval destroyCount;
#endif /* #ifdef CLEANUP_DAEMON */

    // used to serialize access to stage1 lists
    LockType stage1Lock;
    // strictly local obj. from curr. gen, waiting for gen swap
    ListSimple<COSMissHandler*, AllocLocalStrict> stage1LocalList;
#ifdef COSKEEPCOUNTS
    uval stage1LocalCount;
#endif /* #ifdef COSKEEPCOUNTS */
    // non-local obj, added since last had token
    ListSimple<COSMissHandler*, AllocLocalStrict> stage1RemoteList;
#ifdef COSKEEPCOUNTS
    uval stage1RemoteCount;
#endif /* #ifdef COSKEEPCOUNTS */

    // list of objects which need to have there cleanup method invoked on this
    // vp.  This list is processed on every invokation of ckeckCleanup
    ListSimple <COSMissHandler *, AllocLocalStrict> vpCleanupList;
#ifdef COSKEEPCOUNTS
    uval cleanupCount;
#endif /* #ifdef COSKEEPCOUNTS */

    // list of references to be reclaimed (deallocated when the token arrives)
    ListSimple<CORef, AllocLocalStrict> reclaimRefList;
#ifdef COSKEEPCOUNTS
    uval reclaimRefCount;
    uval reclaimRefPinnedCount;
#endif /* #ifdef COSKEEPCOUNTS */
    // list of references associated with local objects which where destroyed
    // while we did not have the token and must be added to the reclaimRefList
    // when we have the token so that they can be reclaimed the next time we
    // get the token
    ListSimple<CORef, AllocLocalStrict> waitingForTokenReclaimLocalRefList;
    ListSimple<CORef, AllocLocalStrict> waitingForTokenReclaimRemoteRefList;

#ifdef COSKEEPCOUNTS
    uval waitingForTokenReclaimLocalRefCount;
    uval waitingForTokenReclaimRemoteRefCount;
    uval waitingForTokenReclaimRefPinnedCount;
#endif /* #ifdef COSKEEPCOUNTS */
    // The array of cos which need to keep circulating
    struct TransferRec {
	VPSet          transSet;
	VPSet          vpCleanupSet;
	COSMissHandler *root;
        CORef           ref;
    };
    ListArraySimple<TransferRec, AllocLocalStrict> transferArray;
#ifdef COSKEEPCOUNTS
    uval transferArrayCount;
#endif /* #ifdef COSKEEPCOUNTS */
    struct DeleteSet {
	ThreadMarker marker;
	ListSimple<COSMissHandler*, AllocLocalStrict> list;
	struct DeleteSet *next;
#ifdef COSKEEPCOUNTS
	uval count;
#endif /* #ifdef COSKEEPCOUNTS */
	DEFINE_NEW(DeleteSet,AllocLocalStrict);
    };

    struct {
	struct DeleteSet *head;
	struct DeleteSet *tail;
#ifdef COSKEEPCOUNTS
	uval setCount;
	uval objCount;
#endif /* #ifdef COSKEEPCOUNTS */
    } localDeleteSets;

    struct DeleteSet remoteDeleteSet;

    uval genCount;
    uval prevGen(){ return !curGen(); }      // previous (non-curr) generation
    uval curGen() { return genCount & uval(1); }  // current generation


    volatile COSMgrObject *Token;
    COSMgrObject          *nextRep;
    uval                  seenToken;

    inline void          handoffToken();
    inline uval          haveToken();
    inline COSMgrObject *prevRep();
    /* Construction and initialization Methods */
    COSMgrObject();
    // For the moment protected but May need to expose more of the creation
    // later
    virtual SysStatus vpInit(VPNum vp, uval numRes, MemoryMgrPrimitive *pa);
    static  SysStatus vpMapCOTransTables(VPNum vp,
					 COSDefaultObject *TheDefaulObject,
					 MemoryMgrPrimitive *pa);
    virtual SysStatus vpMaplTransTablePaged(VPNum vp, uval forkChild=0);

    void gcInit(VPNum vp);
    uval updateTheGen();
    MarkerState checkVPThreadMarker(ThreadMarker marker);
    SysStatus locked_updateAndCheckVPThreadMarker(ThreadMarker marker,
						  MarkerState  &state);
    void createNewLocalDeleteSet();
    uval processLocalDeleteSets(ListSimple<COSMissHandler*, AllocLocalStrict>
			   *tmpList);
    void createRemoteDeleteSet();
    uval processRemoteDeleteSet(ListSimple<COSMissHandler*, AllocLocalStrict>
				*tmpList);
#if 0
    uval cleanupObj(COSMissHandler *obj);
#else /* #if 0 */
    void checkCleanupObj(COSMissHandler *obj, VPSet *transSet, VPSet *repSet);
#endif /* #if 0 */
    static  SysStatus   refMarkCleaned(CORef ref);
#ifdef CLEANUP_DAEMON
    static void CleanupDaemon(uval ptr);
    void cleanupDaemon();
#endif /* #ifdef CLEANUP_DAEMON */
public:
    static SysStatus ClassInit(VPNum vp, MemoryMgrPrimitive *pa);
    virtual SysStatus pausePeriodicGC() {
	forkLock.acquire();
	return 0;
    }
    virtual SysStatus unpausePeriodicGC() {
	forkLock.release();
	return 0;
    }

    static SysStatus getTypeToken(COSMissHandler *mh, uval &typeToken);

    static uval isRef(CORef ref) {
        return (   (((uval)ref >= (uval)lTransTablePinned) && 
                    ((uval)ref <  ((uval)lTransTablePinned  
                                   + gPinnedSize)))
                  ||((uval)ref >= (uval)lTransTablePaged) &&
                    ((uval)ref <  ((uval)lTransTablePaged
                                   + gTransTablePagableSize)));
    }

    static VPNum refToVP(CORef ref) {
        // FIXME: May want to fix this later but for the moment
        //        report to callers that pinned entries are mapped to 
        //        vp 0
        tassert(isRef(ref), ;);
        if (((uval)ref >= (uval)lTransTablePinned) && 
                ((uval)ref < ((uval)lTransTablePinned + gPinnedSize))) {
            return (VPNum)0;
        } else {
            return (VPNum)(((uval)ref - (uval)lTransTablePaged) 
                           >> logGPartPagableSize);
        }
    }

#ifdef CLEANUP_DAEMON
    virtual SysStatus setCleanupDelay(uval msecs);
    virtual SysStatus   startPeriodicGC(VPNum vp);
#endif /* #ifdef CLEANUP_DAEMON */
    /* Translation Table Entry Management Methods */
    virtual SysStatus   resetLocalTransEntry(CORef ref);
    virtual SysStatus   initTransEntry(GTransEntry *gte, COSMissHandler *mh,
				       CORef &ref);
    virtual SysStatus   initTransEntry(CORef ref, COSMissHandler *mh);
    virtual SysStatus   alloc(COSMissHandler *mh, CORef &ref, uval pool);
    // FIXME only here for testing really can have protected access
    virtual SysStatus   dealloc(CORef ref);

    /* A utility Class which can be used to Manually cleanup up an objects */
    /* memory if you know that it no longer being accessed                 */
    class ManualCleanupOperator {
        COSMissHandler *theRoot;
        VPSet vpCleanupSet;
    public:
        ManualCleanupOperator();
        /* start should be invoked on one and only one vp to kick of the 
           cleanup */
        void start(COSMissHandler *obj);
        /* After start has been invoked perVPInvocation should be called on all
           vps that the object has been accessed on (well at least the vps for 
           which there have been reps created */
        uval perVPInvocation();
        uval isDone();
    };

    /* Garbage Collection Methods */
    // add an object ready for deletion
    virtual SysStatus  destroyCO(CORef ref, COSMissHandler *obj);
    virtual SysStatus  reclaimRef(CORef ref, COSMissHandler *obj);
    // run the destruction manager to remove memory allocated by these objects
    virtual SysStatus  checkCleanup();

    // Create a marker which can be tested to see if the threads on this
    // vp at the time of invocation have terminated.  Indicated by the marker's
    // state.
    virtual SysStatus  setVPThreadMarker(ThreadMarker &marker);

    // Method to update and test a thread marker created with the above method.
    virtual SysStatus  updateAndCheckVPThreadMarker(ThreadMarker marker,
						    MarkerState  &state);

    // Create a marker which can be tested to see if the threads across
    // the entire system at the time of invocation have terminated.  Indicated
    // by the marker's state
    virtual SysStatus  setGlobalThreadMarker(ThreadMarker &marker);

    // Method to update and test a global thread marker created with above
    // method
    virtual SysStatus  checkGlobalThreadMarker(ThreadMarker marker,
					       MarkerState &state);

    // method to get a list of descriptors for the active objects on a vp basis
    virtual SysStatusUval getCOList(CODesc *coDescs, uval numDescs);

#ifdef CLEANUP_DAEMON
    // called by fork subsystem to reset the COSMgrObject to a sane
    // state in child.
    virtual SysStatus postFork();
#endif /* #ifdef CLEANUP_DAEMON */

    virtual SysStatus substitute(CORef target,
		                 COSMissHandler*& curMH, COSMissHandler* newMH);

#ifdef COSKEEPCOUNTS
    virtual SysStatus  getStage1LocalCount(uval *count) {
	*count = stage1LocalCount; return 0;
    }
    virtual SysStatus  getStage1RemoteCount(uval *count) {
	*count = stage1RemoteCount; return 0;
    }
    virtual SysStatus  getReclaimRefCount(uval *count) {
	*count = reclaimRefCount; return 0;
    }
    virtual SysStatus getWaitingForTokenReclaimLocalRefCount(uval *count) {
	*count = waitingForTokenReclaimLocalRefCount; return 0;
    }
    virtual SysStatus getWaitingForTokenReclaimRemoteRefCount(uval *count) {
	*count = waitingForTokenReclaimRemoteRefCount; return 0;
    }
    virtual SysStatus getTransferArrayCount(uval *count) {
	*count = transferArrayCount; return 0;
    }
    virtual SysStatus getRemoteDeleteSetCount(uval *count) {
	*count = remoteDeleteSet.count; return 0;
    }
    virtual SysStatus getLocalDeleteSetSetCount(uval *count) {
	*count = localDeleteSets.setCount; return 0;
    }
    virtual SysStatus getLocalDeleteSetObjCount(uval *count) {
	*count = localDeleteSets.objCount; return 0;
    }
#endif /* #ifdef COSKEEPCOUNTS */

    // method for debugging - print out statistics
    virtual SysStatus print();
};

#endif /* #ifndef __COSMGR_OBJECT_H_ */
