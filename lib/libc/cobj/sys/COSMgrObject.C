/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: COSMgrObject.C,v 1.60 2005/08/20 20:45:38 bseshas Exp $
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
#include <sys/sysIncs.H>
#include "COSMgrObject.H"
#include <cobj/DataTransfer.H>
#include <cobj/DTType.H>
#include <cobj/sys/ActiveThrdCnt.H>
#include "cobj/CObjRootMediator.H"
#include <trace/traceClustObj.h>
#include <sys/KernelInfo.H>
#include <stub/StubRegionPerProcessor.H>
#include <stub/StubFRLTransTable.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <cobj/CObjRootSingleRep.H>

/*
 * We activate before calling new, and de-activate after
 */
#define ACTIVATE_PRE_ALLOC
#define DEACTIVATE_POST_ALLOC
/*
#if 1
#define ACTIVATE_PRE_ALLOC \
	tassertMsg(!Scheduler::GetCurThreadPtr()->isActive(), "is active\n");\
        Scheduler::ActivateSelf();
#define DEACTIVATE_POST_ALLOC \
        Scheduler::DeactivateSelf();
#else
#define ACTIVATE_PRE_ALLOC \
	uval __wasActive = Scheduler::GetCurThreadPtr()->isActive();\
        if (!__wasActive) Scheduler::ActivateSelf();
#define DEACTIVATE_POST_ALLOC \
    if (!__wasActive) Scheduler::DeactivateSelf();
#endif
*/
/*  The following long winded comment maybe completely useless but leaving  *
 *  for the moment.  To be cleaned up later                                 */

/******************************************************************************
   Layout of Clustered Object Translation Tables

        Global Clustered Object Translation Table Layout
  -------------------------------------------------------------------------
  || 0th proc partition || 1st proc partition || 2nd proc partition || . . .
  -------------------------------------------------------------------------
     Total Size is : GCOTRANTABLESIZE
     Partition Size: GCOPARITIONSIZE=GCOTRANTABLESIZE/NUMVP
     Total number of Entries: NUMTRANTABLEENTRIES=GCOTRANTABLESIZE /
                              sizeof(GTranEntry)
     Size of Clustered Object Table Page: COTRANTABLEPAGEZIE
     Number of Entries per page: COTRANTABLEPAGESIZE/sizeof(GTranEntry)

     ith Processor's Global Clustered Object Translation Table Layout
           -------------------------------------------
           || Pinned Entries |  Pagable Entries     ||
           -------------------------------------------
     Total Number of Pinned Pages  : PINNEDPAGESPERPART
     Size of Pinned Area           : PINNEDPAGESPERPART * GCOTRANSTABLEPAGESIZE
     Total Number of Pinned Entries: NUMCOPINNEDENTRIES=PINNEDPAGESPERPART/
                                     sizeof(GTranEntry)
     Note:  On the 0th processor the first few entries are used for the
            special reserved well-known objects.
            Each processor maintains two separate lists to manage its
            free pinned and paged entries (pages used for pinned entries
            must obviously be pinned).  Entries allocated on a given processor
            are assigned from its own partition.
     Note:  At userlevel there are no pinned entries

                 Local Clustered Object Translation Table Layout
                 (same layout on all processors)
   ------------------------------------------------------------------------
   | Pinned Entries |      Pagable Entries                                |
   ------------------------------------------------------------------------
     Total Number Entries : NUMTRANTABLEENTRIES as defined above
     Total Size           : sizeof(LTransEntry) * NUMTRANSTABLEENTRIES
     Total Number of Pinned Pages : PINNEDPAGESPERPART as defined above
     Note:  At userlevel there are no pinned entries.
            Pagable pages must use a special region which is automatically
            initialized with entries which point to the processor's default
            object.

FIXME: !!!! For the moment we are managing the global table in a shared
            fashion.  The lists that manage the entries are in the root
            and are globally locked.  Should be a simple change to move
	    to per-processor lists and local management.
******************************************************************************/

/*****************************************************************************
 * TODO in near term:
 *    1) Fix sync issues with passing of Token
 *    2) check with Marc about fork implications to global.genCount
 *    3) Manage ref allocation and deallocation on a per rep basis (remove
 *       global lists
 *****************************************************************************/

#ifdef COSKEEPCOUNTS
#define UPDATEDBGCOUNTER(STMT)                                            \
                            {                                             \
                              STMT;                                       \
                            }

#define UPDATEPINNEDDBGCOUNTER(ref, stmt) 				    \
   {                                                                        \
     tassertMsg( (((uval)ref >= (uval)lTransTablePinned) &&                 \
               ((uval)ref < ((uval)lTransTablePinned + (uval)gPinnedSize))) \
            ||((uval)ref >= (uval)lTransTablePaged &&                       \
               ((uval)ref < ((uval)lTransTablePaged +                       \
                             (uval)gTransTablePagableSize)))                \
            , "That's odd ref does not seem to be in the ltrans ranges");   \
     if ((uval)ref < ((uval)lTransTablePinned + (uval)gPinnedSize)) {       \
      	stmt;							            \
     }                                                                      \
   }

#else /* #ifdef COSKEEPCOUNTS */

#define UPDATEDBGCOUNTER(STMT)
#define UPDATEPINNEDDBGCOUNTER(ref, stmt)

#endif /* #ifdef COSKEEPCOUNTS */


#include <defines/template_bugs.H>
#ifdef EXPLICIT_TEMPLATE_INSTANTIATION
// TEMPLATE INSTANTIATION
template
  void
  ListArraySimple<COSMgrObject::TransferRec, AllocLocalStrict>::
  add(COSMgrObject::TransferRec);
template
  void
  ListArraySimple<COSMgrObject::TransferRec, AllocLocalStrict>::
  grow(void);
template
  void *
  ListArraySimple<COSMgrObject::TransferRec, AllocLocalStrict>::
  next(void *, COSMgrObject::TransferRec &);
#endif /* #ifdef EXPLICIT_TEMPLATE_INSTANTIATION */


GTransEntry *COSMgr::gTransTablePinned;
GTransEntry *COSMgr::gTransTablePaged;
LTransEntry *COSMgr::lTransTablePinned;
LTransEntry *COSMgr::lTransTablePaged;
RepRef CObjGlobals::theBreakpointObjectRef;

SysStatus
COSBreakpoint()
{
    breakpoint();
    return -1;
}

// FIXME: Optimize this
inline void
COSMgrObject::COSMgrObjectRoot::updateGlobalGenCount()
{
    uval oldGblCnt;
    do {
	oldGblCnt = global.genCount;
	tassert((oldGblCnt + 1),
		err_printf("global generation Count rollover not handled\n"));
    } while (!CompareAndStoreSynced(&(global.genCount),
				    oldGblCnt, oldGblCnt + 1));
    TraceOSClustObjGlobalCnt(Scheduler::GetCurThread(),
		   (uval64)(oldGblCnt + 1));

//    err_printf("COSMgrObject::COSMgrObjectRoot::updateGlobalGenCount:"
//               "global.genCount = %ld\n", global.genCount);
}

// FIXME: Optimize this
inline void
COSMgrObject::COSMgrObjectRoot::stutterGlobalGenCount()
{
    uval oldGblCnt;
    do {
	oldGblCnt = global.genCount;
	tassert((oldGblCnt - 1),
		err_printf("global generation Count rollover not handled\n"));
    }  while (!CompareAndStoreSynced(&(global.genCount),
				     oldGblCnt, oldGblCnt - 1));
}

COSMgrObject::COSMgrObjectRoot::COSMgrObjectRoot()
    : CObjRoot((RepRef)GOBJ(TheCOSMgrRef), CObjRoot::skipInstall),
      pinnedPageDescList(AllocPool::STATIC)
{
    initMembers();
}

void
COSMgrObject::COSMgrObjectRoot::initMembers()
{
    repList = 0;
    repTail = 0;
    // to provide a buffer we start the global genCount at
    // VPLimit.  Thus the only way to get a global gen count
    // is if it rolls over
    global.genCount = Scheduler::VPLimit;
}

/* virtual */ SysStatus
COSMgrObject::COSMgrObjectRoot::handleMiss(COSTransObject * &co,
					   CORef ref, uval methodNum)
{
    LTransEntry *lte=(LTransEntry *)ref;
    VPNum myvp=Scheduler::GetVP();
    COSMgrObject *cur = (COSMgrObject *)repList;

    if (cur == NULL) {
	tassert(0, err_printf("What the COSMgr replist is empty\n"));
    }

    while (cur->vp != myvp && cur->nextRep != repList) {
	cur = cur->nextRep;
    }

    if (cur->vp == myvp) {
	//Install the new representative in the Translation table of this vp.
	lte->setCO(cur);
	co = cur;
	return 0;
    }

    tassert(0, err_printf("Huh could not find COSMgr Rep for vp=%ld, "
			  "We expect to have one rep per vp created at"
			  "vp Construction time\n",myvp));
    // Should never reach hear
    return 1;
}

/* virtual */ SysStatus
COSMgrObject::COSMgrObjectRoot::cleanup(COSMissHandler::CleanupCmd cmd)
{
    tassert(0, err_printf("unexpected call to cleanup on COSMgr\n"));
    return 0;
}

/* virtual */ CObjRep *
COSMgrObject::COSMgrObjectRoot::getRepOnThisVP()
{
    tassert(0, err_printf("Unexpected use.\n"));
    return 0;
}

/* virutal */ VPSet
COSMgrObject::COSMgrObjectRoot::getTransSet()
{
    VPSet dummy;
    tassert(0, err_printf("Unexpected use.\n"));
    return dummy;
}

/* virutal */ VPSet
COSMgrObject::COSMgrObjectRoot::getVPCleanupSet()
{
    VPSet dummy;
    tassert(0, err_printf("Unexpected use.\n"));
    return dummy;
}

// Atomically add a rep to the circular chain of Reps
// Note for this to work repTail should only be used in this
// method.  It should not be used during traversals.
void
COSMgrObject::COSMgrObjectRoot::init(VPNum vp, COSMgrObject *rep)
{
    volatile COSMgrObject  *oldTail;
    rep->vp = vp;

    // Take care of initial case when no reps are in the list
    if (repList == 0 || repTail == 0) {
	if (repList == 0 &&
	    CompareAndStoreSynced((volatile uval *)&repList, 0, (uval)rep))
	{
	    rep->nextRep = rep;
	    repTail = rep;
	    return;
	} else {
	    while (repList == 0 || repTail == 0 );
	}
    }
    // List already has some reps
    rep->nextRep = (COSMgrObject *)repList;
    // update tail pointer atomically
    do {
	oldTail = repTail;
    } while (!CompareAndStoreSynced((volatile uval *)&repTail,
				    (uval)oldTail, (uval)rep));
    // Once we know that rep is the only one to be
    // chained to the representative pointed to by old tail then
    // actually add us to the chain.  Prior to this repTail and the actual
    // chain maybe out of sync. repTail may point to the new rep which is
    // not actually part of the chain. This is not a problem as long
    // as repTail is only used for this purpose of adding reps to the
    // chain.
    oldTail->nextRep = rep;

    // Since we have just added a rep we must stutter the global gen Count
    // So that the current circulation is ignored.
    stutterGlobalGenCount();
}

COSMgrObject::COSMgrObject()
    : pagablePageDescList(AllocPool::STATIC)
{
    /* COSMgrObject is a special case in which the MissHandler has been *
     * informed not to install it into the translation table as it's    *
     * normal process is to use the COSMgrObject to do this.  So we     *
     * must explicity install.                                          */
    nextRep = NULL;
}


/* COSMgrObject requires special care in initialization as no other *
 * well know Clustered Objects including the allocators are         *
 * available.  Both the root and the reps for each processor are    *
 * created by hand and installed explicitly.                        */
/* static */ SysStatus
COSMgrObject::ClassInit(VPNum vp, MemoryMgrPrimitive *pa)
{
    static COSMgrObject::COSMgrObjectRoot *root;
    COSMgrObject *rep;

    passertMsg(COSMAXVPS == Scheduler::VPLimit, "FIXME!!!!! Do this right!!\n");
    // Create the root on the boot processor
    if (vp == 0) root = new(pa) COSMgrObjectRoot();
    //   err_printf("COSMgrObject::ClassInit root=%p vp=%ld\n",root, vp);
    // Create a rep on this processor
    rep = new(pa) COSMgrObject();
    rep->setRoot(root);

    // Create/Map Translation tables
    vpMapCOTransTables(vp, &(root->theDefaultObject),pa);

    // do per rep vp initialization.  Avoid going through the LTransTable
    rep->vpInit(vp, CObjGlobals::numReservedEntries,pa);

    // On boot processor set the root into the Global TransTable
    if (vp == 0) {
	rep->initTransEntry((CORef)GOBJ(TheCOSMgrRef), root);
	root->theBreakpointObjectP = &root->theBreakpointObject;
	CObjGlobals::theBreakpointObjectRef
            = (RepRef)&root->theBreakpointObjectP;
    }

    // Explicitly add the new rep to the root as the rep for this vp
    root->init(vp, rep);
    // Note it is now safe to access the COSMgrObject via the trans table on
    // this vp.  A normal miss will occur.
    return 1;
}

// Map memory for Global and Pinned Local Clustered Object Translation Tables
/* static */ SysStatus
COSMgrObject::vpMapCOTransTables(VPNum vp, COSDefaultObject *TheDefaultObject,
			   MemoryMgrPrimitive *pa)
{
    if (vp == 0) {
	// Map Global Cluster Object  Translation Table
        // Pin enough pages for Pinned Entries
	uval ptr;

	pa->alloc(ptr, gPinnedSize, gTransPageSize);
	gTransTablePinned = (GTransEntry *)ptr;
        // Paged Portion is allocated later when the page allocator is
        // ready.  See vpMaplTransTablePaged.  But to catch errors
        // we initialized to 0.
        gTransTablePaged  = 0;

	lTransTablePinned = lTransTableLocal;
        // Paged Portion is allocated later when the page allocator is
        // ready.  See vpMaplTransTablePaged.  But to catch errors
        // we initialized to 0.
        lTransTablePaged  = 0;
    }

    //Must now initialize the pinned portion of the local table
    LTransEntry *lte;
    for (uval i = 0; i < gPinnedSize/sizeof(TransEntry); i++) {
	lte = &lTransTablePinned[i];
	lte->setToDefault(TheDefaultObject);
    }
//    err_printf("vp=%ld lTransTablePinned=%p pinnedend=%p\n",
//               vp, lTransTablePinned, (uval *)(lTransTablePinned +
//                                               GCOPARTPINNEDSIZE));
    return 1;
}

/* virtual */ SysStatus
COSMgrObject::vpInit(VPNum vp, uval numRes, MemoryMgrPrimitive *pa)
{
    // Create a Translation Paged Descriptor List to managed the pages of
    // Translation entries in the vp's portion of the Global Translation Table
    // For the moment we only have one partition shared by all processors
    if (vp == 0) {
	TransPageDesc *firstPinnedTransPageDesc =
	    new(pa) TransPageDesc(AllocPool::STATIC);
	COGLOBAL(pinnedPageDescList).init((uval) gTransTablePinned,
					  gPinnedSize,
					  firstPinnedTransPageDesc,
					  numRes);
    }
    gcInit(vp);
    return 1;
}

/*
 * called by crtInit for new processes (e.g. exec) and fork children.
 * for exec, forkChild will be 0, and we create new pageable
 * local and global translation tables.  the global table must be copied
 * to fork children, so we register it with the fork manager.
 * in the fork child case, the global table is recreated by the fork
 * manager, but a new local table is made at the same virtual address that
 * it occupied in the parent.  object miss handling will repopulate that
 * table as required.
 */
/* virtual */ SysStatus
COSMgrObject::vpMaplTransTablePaged(VPNum vp, uval forkChild)
{
    ObjectHandle gtOH,ltOH;
    SysStatus rc;

    if (vp == 0) {
        rc = StubFRLTransTable::_Create(ltOH,
                                        *(uval*)&COGLOBAL(theDefaultObject));
        passertMsg(_SUCCESS(rc),
                   "Allocation failure in COSMgr initialization\n");

        if (forkChild == 0) {
            uval regionVaddr;
            // It is now time to get ourselves a global pagable table
            rc=StubFRComputation::_Create(gtOH);
            passertMsg(_SUCCESS(rc),
                       "Allocation failure in COSMgr initialization\n");

            rc=StubRegionDefault::_CreateFixedLenExt(
                regionVaddr, gTransTablePagableSize, 0, gtOH, 0,
                (uval)(AccessMode::writeUserWriteSup),(XHandle)0,
		RegionType::ForkCopy+RegionType::KeepOnExec);

            passertMsg(_SUCCESS(rc),
                       "Allocation failure in COSMgr initialization\n");

            Obj::ReleaseAccess(gtOH);

            // record the start of pagable poriton of the global table
            gTransTablePaged = (GTransEntry*)regionVaddr;

            // Now map the local trans table in the address space
	    rc=StubRegionPerProcessor::_CreateFixedLenExt(
		regionVaddr, gTransTablePagableSize , ltOH, 0,
		(uval)(AccessMode::writeUserWriteSup),(XHandle)0,
		RegionType::K42Region);

            passertMsg(_SUCCESS(rc),
                       "Allocation failure in COSMgr initialization\n");

            lTransTablePaged=(LTransEntry *)regionVaddr;
        } else {
            rc=StubRegionPerProcessor::_CreateFixedAddrLenExt(
                (uval)lTransTablePaged, gTransTablePagableSize , ltOH, 0,
                (uval)(AccessMode::writeUserWriteSup),(XHandle)0,
		RegionType::K42Region);

            passertMsg(_SUCCESS(rc),
                       "Allocation failure in COSMgr initialization\n");
        }

        Obj::ReleaseAccess(ltOH);
    }

    // If this is a fork do not reinitialize the data structures
    if (forkChild==0) {
        // Setup managment of this vps portion of the global table
        pagablePageDescList.init((uval)gTransTablePaged +
                                 (uval)(vp * gPartPagableSize),
                                 gPartPagableSize);
    }
    return 0;
}


/* virtual */ SysStatus
COSMgrObject::alloc(COSMissHandler *mh, CORef &ref, uval pool)
{
    SysStatus rc;
    GTransEntry *gte;

    // Common case fast path
    if (pool == AllocPool::PAGED) {
        while (!(gte = pagablePageDescList.getFreeTransEntry())) {
            pagablePageDescList.addTransPageDesc(new(AllocPool::PAGED)
                                           TransPageDesc(AllocPool::PAGED));
        }
        rc = initTransEntry(gte, mh, ref);
        if (rc) return rc;
        pagablePageDescList.returnFreeTransEntry(gte);
		TraceOSClustObjAlloc((uval)gte, (uval)mh, (uval)ref);
        return rc;
    }

    tassert(pool == AllocPool::PINNED,
            err_printf("UnSupported pool type pool=%ld\n", pool));
    // Code duplicate in order to simplify the common case above
    while (!(gte = COGLOBAL(pinnedPageDescList).getFreeTransEntry())) {
        COGLOBAL(pinnedPageDescList).addTransPageDesc(new(AllocPool::PINNED)
                              TransPageDesc(AllocPool::PAGED));
    }
    rc = initTransEntry(gte, mh, ref);
    if (rc) return rc;
    COGLOBAL(pinnedPageDescList).returnFreeTransEntry(gte);
	TraceOSClustObjAlloc((uval)gte, (uval)mh, (uval)ref);
    return rc;
}

/* virtual */ SysStatus
COSMgrObject::dealloc(CORef ref)
{
    LTransEntry *const lte = (LTransEntry *)ref;
    GTransEntry *const gte = localToGlobal(lte);

    // Assume it is a pagable entry and try free it on the pagable list first.
    if (!pagablePageDescList.returnFreeTransEntry(gte)) {
	// If this fails then it must be a pinned entry.  Should be very rare
        // if ever? Otherwise trying to dealloc on wrong vp.
        passert(((uval)gte - (uval)((uval)gTransTablePinned) <
                 (uval)gPinnedSize),
                err_printf("!!!!deallocating on %p on wrong vp=%ld\n",ref,vp));
	if ((!COGLOBAL(pinnedPageDescList).returnFreeTransEntry(gte))) {
	    passert(0, err_printf("oops unable to free ref=%p gte=%p\n",
				  ref, gte));
		TraceOSClustObjDealloc((uval)lte, (uval)gte);
	    return 0;
	}
    }
    return 1;
}

/*static*/ SysStatus
COSMgrObject::substitute(CORef target, COSMissHandler*& curMH,
                         COSMissHandler* newMH)
{
    LTransEntry *const lteTarget = reinterpret_cast<LTransEntry*>(target);
    GTransEntry *const gteTarget = localToGlobal(lteTarget);

    curMH = gteTarget->getMH();
    COSMissHandler* targetRoot = curMH;

    // this prevents simultaneous switching
    if (!gteTarget->mhSwitchEntry()) {
	return -1; // FIXME: proper error code?
    }

    // locate existing reps and blow them away
    VPSet reps = curMH->getTransSet();
    for (VPNum vp = reps.firstVP(); vp != VPSet::VPLimit; vp = reps.nextVP(vp)) {
        if (vp == Scheduler::GetVP()) {
            // blow away LTE entry on this VP and reset to default
            resetLocalTransEntry(target);
        }
        else {
            // blow away remote LTE
            WorkerThreadReset msg = WorkerThreadReset(target);
            msg.send(SysTypes::DSPID(0, vp));
        }
    }

    // Originally this did something different, then I remembered that I can't
    // actually find out the correct miss handler from the CORef when switching
    // back.
    gteTarget->setMH(newMH);

    // unlock the gte
    gteTarget->mhWriteComplete();

    if (gteTarget->mhSwitchExit() == GTransEntry::DESTROYREQUIRED) {
        // safe to call this multiple times
        // FIXME: is this right??
        // Raymond says: I have no clue what this does; I copied it from some
        // other code that probably copied it from some other code... maybe
        // Craig knows, or maybe Kevin knows, or... To me it seems strange
        // since the miss handler doesn't really need to be part of the
        // clustered object (I think!).
        destroyCO(target, targetRoot);
    }

    return 0;
}

/*static*/ SysStatus
COSMgr::switchCObj(CORef ref, CObjRoot *newRoot,
                   SwitchDispDir dir, // = DESTROY_WHEN_DONE
                   sval medClusterSize, // = 1
		   Callback *cb // = NULL
    )
{
    LTransEntry *const lte = (LTransEntry *)ref;
    GTransEntry *const gte = localToGlobal(lte);
    CObjRoot *const oldRoot = (CObjRoot *)gte->getMH();

    //tassert(Scheduler::GetCurThreadPtr()->isActive(),
	//    err_printf("Thread not active!\n"));  // FIXME

    // negotiation
    DTType dtt = DataTransferObject::negotiate(oldRoot, newRoot);

    if (dtt == DTT_ERROR) {
	// negotiation failed... cannot switch between these 2 cobjs
	// since they are of incompatible types
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	    err_printf("COSMgrObject::switchCObj(): DT negotiation failed.\n");
	}
	// FIXME: move this down after the mhSwitchEntry
	// and unlock before the fail-return (maybe?)
	if (cb != NULL) {
	    cb->complete(-1);
	}
	return -1;
    }

    // this prevents simultaneous switching
    // GTE is locked so that a GTE MH lookup is pretected during switching
    // (so that the old MH is not used once the new switching root is
    // installed). GTE lock is released when the root is swung to the
    // mediator root.
    if (!gte->mhSwitchEntry()) { // locks the GTE if succeeded
	// someone else has started switching for this root
	if (!KernelInfo::ControlFlagIsSet(KernelInfo::RUN_SILENT)) {
	  err_printf("COSMgrObject::switchCObj: switch initiated elsewhere\n");
	}
	if (cb != NULL) {
	    cb->complete(-1);
	}
	return -1; // FIXME: proper error code?
    }
    // Install the mediator root.
    // Note that we have no control over the original root miss handling
    // code until after we switch the MH pointer in the GTE.
    // Should not install it yet until the initialization for the root is
    // complete.
    CObjRootMediator *const swRoot =
	new CObjRootMediator(oldRoot, newRoot, dtt, dir, cb, medClusterSize);
    // C'tor acquires the phase lock, which will be released in switchImpl
    // when the initialization is done.
    // switchImpl() is not called in the c'tor since it may need to reset
    // the translation table entries. Doing it after the constructor call
    // guarantees that the translation entry writes are final.
    return swRoot->switchImpl();
}

/* virtual */ SysStatus
COSMgrObject::resetLocalTransEntry(CORef ref)
{
    LTransEntry *lte=(LTransEntry *)ref;
    lte->setToDefault(&COGLOBAL(theDefaultObject));
    return 1;
}

// FIXME: Optimize this
// returns a mask with the bits corresponding to the cluster
// to which vp belongs set.
/* static */ uval
COSMgr:: clusterSetMask(VPNum vp, VPNum clustersize)
{
    tassert(Scheduler::VPLimit <= 64,
	    err_printf("ooops this code does not work with more"
		       "than 64 vps\n"));
    tassert(clustersize != 0,
	    err_printf("clustersize of 0 does not make sense\n"));
    return ((1<<clustersize)-1) << (vp/clustersize);
}

/* static */ SysStatus
COSMgrObject::refMarkCleaned(CORef ref)
{
    LTransEntry *lte = (LTransEntry *)ref;
    GTransEntry *gte = localToGlobal(lte);

    gte->mhMarkCleaned();
    gte->setMH(NULL);
    return 1;
}

/* static */ SysStatus
COSMgr::refIsCleaned(CORef ref)
{
    LTransEntry *lte = (LTransEntry *)ref;
    GTransEntry *gte = localToGlobal(lte);

    return gte->mhIsMarkedCleaned();
}

/* virtual */ SysStatus
COSMgrObject::initTransEntry(GTransEntry *gte, COSMissHandler *mh, CORef &ref)
{
    LTransEntry *lte = globalToLocal(gte);

    gte->setMH(mh);
    gte->resetBits(); // resets the mhcount and switching/writing bits
    tassertMsg(lte->co == (COSTransObject *)&(lte->tobj),
	       "allocating unfree lte\n");
    ref = (CORef)lte;
    return 1;
}

/* virtual */ SysStatus
COSMgrObject::initTransEntry(CORef ref, COSMissHandler *mh)
{
    LTransEntry *lte = (LTransEntry *)ref;
    GTransEntry *gte = localToGlobal(lte);

    gte->setMH(mh);
    gte->resetBits(); // resets the mhcount and switching/writing bits
    tassertMsg(lte->co == (COSTransObject *)&(lte->tobj),
	       "allocating unfree lte\n");
    return 1;
}

/* static */ SysStatus
COSMgr::swingRoot(CORef ref, COSMissHandler *mh)
{
    LTransEntry *lte=(LTransEntry *)ref;
    GTransEntry *gte=localToGlobal(lte);
#ifndef NDEBUG
    TransEntry::GTransData gd;
    gd.data = gte->gteData.data;
    tassert(gd.switching(), err_printf("Should be switching.\n"));
#endif /* #ifndef NDEBUG */
    gte->setMH(mh);
    return 1;
}

/* static */ SysStatus
COSMgr::gteWriteComplete(CORef ref)
{
    LTransEntry *lte=(LTransEntry *)ref;
    GTransEntry *gte=localToGlobal(lte);
    gte->mhWriteComplete();
    return 1;
}

/* static */ SysStatus
COSMgr::gteSwitchComplete(CORef ref)
{
    LTransEntry *lte=(LTransEntry *)ref;
    GTransEntry *gte=localToGlobal(lte);

    //WARNING:  The order of the next few lines is important!!!!
    //          Get the root prior to releasing the switching bit.
    //          This ensures that no destroy can sneak in
    //          before we can read the mh from the gte entry.
    COSMissHandler *root=gte->getMH();

    if (gte->mhSwitchExit() == GTransEntry::DESTROYREQUIRED) {
	// safe to call this multiple times
	DREFGOBJ(TheCOSMgrRef)->destroyCO(ref, root);
    }
    return 1;
}

/* Garbage Collection Methods */
inline void
COSMgrObject::handoffToken()
{
    Token = 0;
    nextRep->Token = this;
}

inline uval
COSMgrObject::haveToken()
{
    return (Token != NULL);
}

inline COSMgrObject *
COSMgrObject::prevRep()
{
    return (COSMgrObject *)Token;
}

// FIXME:  Talk to Marc to figure out what really should
//         and can be done here.
/* virtual */SysStatus
COSMgrObject::postFork()
{
    VPNum myvp = Scheduler::GetVP();

    tassert(myvp == 0, err_printf("huh vp=%ld\n",myvp));

    tassert(nextRep == this,
	    err_printf("huh multi-vp fork not supported\n"));

    vpMaplTransTablePaged(myvp, 1);

    // We are going to simply strand garbage for the moment.
    gcInit(myvp);

#if CLEANUP_DAEMON
    startPeriodicGC(myvp);
#endif /* #if CLEANUP_DAEMON */

    return 1;
}

void
COSMgrObject::gcInit(VPNum vp)
{
    lock.init();
    forkLock.init();
    stage1Lock.init();
    // we are initialized before the scheduler, so we just assume current
    // generation is zero (we are the only ones that ever change it)
    genCount              = 0;
    localDeleteSets.head  = NULL;
    localDeleteSets.tail  = NULL;
    remoteDeleteSet.next  = NULL;
    // if this is the first rep then give it the Token with itself being
    // the previous holder of the token.
    if (vp == 0)   {
	Token  = this;
    }  else {
	Token  = NULL;
    }
#if CLEANUP_DAEMON
    cleanupDaemonID       = Scheduler::NullThreadID;
    destroyCount          = 0;
#endif /* #if CLEANUP_DAEMON */
    seenToken             = 0;

    // Initialize Debug counters
    UPDATEDBGCOUNTER(localDeleteSets.setCount       = 0);
    UPDATEDBGCOUNTER(localDeleteSets.objCount       = 0);
    UPDATEDBGCOUNTER(remoteDeleteSet.count          = 0);
    UPDATEDBGCOUNTER(stage1LocalCount               = 0);
    UPDATEDBGCOUNTER(stage1RemoteCount              = 0);
    UPDATEDBGCOUNTER(reclaimRefCount                = 0);
    UPDATEDBGCOUNTER(reclaimRefPinnedCount          = 0);
    UPDATEDBGCOUNTER(cleanupCount                   = 0);
    UPDATEDBGCOUNTER(waitingForTokenReclaimLocalRefCount = 0);
    UPDATEDBGCOUNTER(waitingForTokenReclaimRemoteRefCount = 0);
    UPDATEDBGCOUNTER(waitingForTokenReclaimRefPinnedCount = 0);
}

#ifdef CLEANUP_DAEMON
/* virtual */ SysStatus
COSMgrObject::setCleanupDelay(uval msecs) {
    COGLOBAL(cleanupDelay)=msecs*SchedulerTimer::TicksPerSecond()/1000;
    return 0;
}

/* virtual */ SysStatus
COSMgrObject::startPeriodicGC(VPNum vp)
{
    SysStatus rc = 0;
    SysStatusProcessID pidrc;
    ProcessID myPID;
    ThreadID myThreadID;

    // use rep lock to protect daemon creation
    AutoLock<LockType> al(&lock);

    if (cleanupDaemonID != Scheduler::NullThreadID) {
      return rc;
    }

    pidrc = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(pidrc), err_printf("woops\n"));
    myPID = _SGETPID(pidrc);

    myThreadID = Scheduler::GetCurThread();

    if (vp == 0) {
	COGLOBAL(cleanupThreshold) = CLEANUP_DAEMON_THRESHOLD;
	COGLOBAL(cleanupDelay) = CLEANUP_DAEMON_DELAY_TIME_MSEC *
	    SchedulerTimer::TicksPerSecond() / 1000;
    }

    // create cleanup daemon
    //err_printf("COSMgrObject::startPeriodicGC: %ld:%lx:%lx called "
    //	       "threshold=%ld, delay=%ld this=%p\n",
    //	       vp, myPID, myThreadID, cleanupThreshold, cleanupDelay, this);

    rc = Scheduler::ScheduleFunction(CleanupDaemon, (uval)this,
				     cleanupDaemonID);

    tassert(_SUCCESS(rc), err_printf("oops: failed to create cleanup"
				     " daemon\n"));
    return rc;
}

/* static */void
COSMgrObject::CleanupDaemon(uval ptr)
{
    ((COSMgrObject *)ptr)->cleanupDaemon();
}

void
COSMgrObject::cleanupDaemon()
{
    SysStatusProcessID pidrc;
    ProcessID myPID;
    ThreadID myThreadID;


    pidrc = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(pidrc), err_printf("woops\n"));
    myPID = _SGETPID(pidrc);

    myThreadID = Scheduler::GetCurThread();


    // err_printf("COSMgrObject::cleanupDaemon: %ld:%lx:%lx Started\n",
    //            vp, myPID, myThreadID);

    Scheduler::DeactivateSelf();
    while (1) {
	//FIXME: ensure that this will do the right thing
	//       if woken up before timeout expires
	Scheduler::BlockWithTimeout(
	    COGLOBAL(cleanupDelay), TimerEvent::relative);
#if 0
	err_printf("COSMgrObject::cleanupDaemon %ld:%lx:%lx token=%p "
	           "this=%p\n",vp, myPID, myThreadID, Token, this);
#endif /* #if 0 */
	forkLock.acquire();
	checkCleanup();
	forkLock.release();
    }
}
#endif /* #ifdef CLEANUP_DAEMON */

class DummyRoot : public COSMissHandler {
    VPSet transSet;
    DummyRoot() {}
public:
    DEFINE_GLOBAL_NEW(DummyRoot);

    DummyRoot(CORef ref, VPSet tset)  { myRef = ref; transSet = tset; }
    DummyRoot(CORef) { transSet.init(); }
    virtual SysStatus handleMiss(COSTransObject * &co,
        CORef ref, uval methodNum) {
        passertMsg(0, "ooops miss on DummyRoot\n");
        return -1;
    }
    virtual SysStatus cleanup(CleanupCmd cmd) { delete this; return 0; }
    virtual VPSet   getTransSet() { return transSet; }
    virtual VPSet   getVPCleanupSet() { VPSet emptyVPSet; return emptyVPSet; }
};

/* virtual */ SysStatus
COSMgrObject::reclaimRef(CORef ref, COSMissHandler *obj)
{
    return destroyCOInternal(ref, obj, 1);
}

/* virtual */ SysStatus
COSMgrObject::destroyCO(CORef ref, COSMissHandler *obj)
{
    return destroyCOInternal(ref, obj, 0);
}

SysStatus
COSMgrObject::destroyCOInternal(CORef ref, COSMissHandler *obj, uval refOnly)
{
    LTransEntry *lte = (LTransEntry *)ref;
    GTransEntry *gte = localToGlobal(lte);
    VPNum myvp        = Scheduler::GetVP();
#ifdef CLEANUP_DAEMON
    uval pokeCleanupDaemon = 0;
#endif /* #ifdef CLEANUP_DAEMON */
    GTransEntry::DestroyStatus  dStat;
    
    // Wait for all current misses to complete and mark entry as destroying
    // to turn away any new misses.
    dStat = gte->mhDestroyEntry();
    if (dStat == GTransEntry::DESTROYINPROGRESS) {
	err_printf("* oops: called destroyCO multiple times\n");
	return _SERROR(2576, 0xF0,1);
    }

    if (dStat == GTransEntry::DESTROYPENDING) {
	err_printf("* oops: switching in progress destroy pending\n");
	return _SERROR(2577, 0xF1,1);
    }

    // Hack to support only reclaiming of ref ... eg. give up your ref safely,
    // threads who may be accessing it will see old value after all threads
    // in flight have passed away then the ref will be reset.  We cheat
    // for the moment and simply alloc a dummy root :-)
    if (refOnly) {
        obj=new DummyRoot(ref,obj->getTransSet());
    }
    // FIXME: For the moment using a lock to protect lists may want to
    //        try harder to do additions and checkcleanups without locking.
    stage1Lock.acquire();

    // Just a quick sanity check.  Of course in the furture we may actually
    // want to support CO's which have never been accessed.  But for the
    // moment we expect that all COS have suffered at least one miss.
    // When this is nolonger true we need to remove any dependences that may
    // exist.
    //FIXME maa removed assert - seems to work for simple case but
    // needs a detailed look.  simple case occurs when trying
    // to create 0 length region.
#if 0
    tassert( ! obj->getTransSet().isEmpty(),
	    err_printf("oops found a CO who's trans set is empty\n"));
#endif /* #if 0 */

    // Use vpmask of object to see if this is the only processor on which
    // it has been accessed.  This is safe as we expect that by
    // this point any new requests on other processors would be automatically
    // turned away. By deleted checks in misshandling path
    if (obj->getTransSet().isEmpty() || obj->getTransSet().isOnlySet(myvp)) {
	stage1LocalList.add(obj);
//	err_printf("COSMgrObject::add(%p) local add vp=%ld\n", obj, myvp);
	UPDATEDBGCOUNTER(stage1LocalCount++);
    } else {
	stage1RemoteList.add(obj);
//	err_printf("COSMgrObject::add(%p) remote add vp=%ld\n", obj, myvp);
	UPDATEDBGCOUNTER(stage1RemoteCount++);
    }
#ifdef CLEANUP_DAEMON
    destroyCount++;
    if (cleanupDaemonID != Scheduler::NullThreadID &&
	(destroyCount > COGLOBAL(cleanupThreshold))) {
	// May want to make cleanup a global flag which is
	// atomically changed to allow one rep to influence
	// the rate cleanup on all reps
	pokeCleanupDaemon = 1;
	destroyCount      = 0;
    }
#endif /* #ifdef CLEANUP_DAEMON */
    stage1Lock.release();

#ifdef CLEANUP_DAEMON
    // To avoid a initialization race we condition on cleanupDaemonID
    if (pokeCleanupDaemon == 1) {
	Scheduler::Unblock(cleanupDaemonID);
    }
#else /* #ifdef CLEANUP_DAEMON */
    checkCleanup();
#endif /* #ifdef CLEANUP_DAEMON */
    return 1;
}

/* virtual */ SysStatus
COSMgrObject::setVPThreadMarker(ThreadMarker &marker)
{
    marker = genCount; // ThreadMarker is really just the generation Count at
                       // the time of creation.
    TraceOSClustObjMarkSet((uval64)(&marker),
		   Scheduler::GetCurThread());
    return 1;
}

SysStatus
COSMgrObject::locked_updateAndCheckVPThreadMarker(ThreadMarker marker,
						  MarkerState &state)
{
    tassert(lock.isLocked(),
	    err_printf("oops we don't have the COSMgrObject::lock\n"));
    /*
     * Two generations must pass to ensure all threads with respect
     * to a given starting generation have terminated.
     */
    updateTheGen();
    // A thread set is really just a starting generation count
    state=checkVPThreadMarker(marker);
    return 1;
}

/* virtual */ SysStatus
COSMgrObject::updateAndCheckVPThreadMarker(ThreadMarker marker,
					   MarkerState &state)
{
    SysStatus rc;
    lock.acquire();
    rc = locked_updateAndCheckVPThreadMarker(marker, state);
    lock.release();
    return rc;
}

COSMgrObject::MarkerState
COSMgrObject::checkVPThreadMarker(ThreadMarker marker)
{
    tassert(genCount >= marker,
	    err_printf("previous greater than current gen\n"));

    if ((genCount - marker) >= 2) {
	TraceOSClustObjMarkElapsed((uval64)(&marker),
		       Scheduler::GetCurThread());
	return ELAPSED;
    }

    TraceOSClustObjMarkActive((uval64)(&marker),
		   Scheduler::GetCurThread());
    return ACTIVE;
}

/* virtual */ SysStatus
COSMgrObject::setGlobalThreadMarker(ThreadMarker &marker)
{
    marker = COGLOBAL(global.genCount); // ThreadMarker is really just the
                                        // generation Count at
                                        // the time of creation.
    return 1;
}

/* virtual */ SysStatus
COSMgrObject::checkGlobalThreadMarker(ThreadMarker marker, MarkerState &state)
{
/*
 *FIXME maa
 *this assert is bogus because every time a new vp starts, the
 *stutter logic redues the global.genCount by 1, and can cause
 *a perfectly good marker to assert.
 */
#if 0
    // check for roll over
    tassert(COGLOBAL(global.genCount) >= marker,
	    err_printf("marker greater than current count\n"));
#endif
    /*
     * because marker can be greater than genCount if a stutter
     * has happened, and these are unsigned numbers, the check
     * must be done carefully.  If you subtrace marker from genCount
     * you may get an underflow which results in a very large unsigned
     * number.
     */
    if (COGLOBAL(global.genCount) >= (marker + 2)) state = ELAPSED;
    else state = ACTIVE;

    return 1;
}

uval
COSMgrObject::updateTheGen()
{
    tassert(lock.isLocked(),
	    err_printf("oops we don't have the COSMgrObject::lock\n"));

    // optimistically attempts to advance the generation twice

    if (ActiveThrdCnt::Advance()) {
	genCount++;
	tassert(genCount, err_printf("genCount rolled over: not handled"));
	if (ActiveThrdCnt::Advance()) {
	    genCount++;
	    tassert(genCount, err_printf("genCount rolled over: not handled"));
	}
	return 1;
    }

    return 0;
}

void
COSMgrObject::checkCleanupObj(COSMissHandler *obj, VPSet *transSet,
			      VPSet *vpCleanupSet)
{
  CORef     objref  = obj->getRef();

  tassert(lock.isLocked(),
	  err_printf("oops we don't have the COSMgrObject::lock\n"));

  tassert(!transSet->isEmpty(),
	  err_printf("unCleanVPSet should not be zero\n"));

  // Check if this object has had a translation on this vp during its life time
  if (transSet->isSet(vp)) {
      resetLocalTransEntry(objref);  // reset the local translation table entry
      transSet->removeVP(vp);        // remove this vp from the objects
                                     // translation set.

      if (transSet->isEmpty()) {
	  ACTIVATE_PRE_ALLOC;
	  obj->cleanup(COSMissHandler::STARTCLEANUP);
	  DEACTIVATE_POST_ALLOC;
	  // no DOVPCLEANUP will be issued additionally for this vp
	  return;
      }

      if (vpCleanupSet->isSet(vp)) {
	  // DOVPCLEANUP command is issued on each of these VPs.
	  // see processing of vpCleanupList below.
	  vpCleanupSet->removeVP(vp);
	  ACTIVATE_PRE_ALLOC;
	  vpCleanupList.add(obj);
	  DEACTIVATE_POST_ALLOC;
	  UPDATEDBGCOUNTER(cleanupCount++);
      }
  }
}

COSMgrObject::ManualCleanupOperator::ManualCleanupOperator() :
    theRoot(0)
{
}

void
COSMgrObject::ManualCleanupOperator::start(COSMissHandler *obj)
{
    VPNum vp=Scheduler::GetVP();

    theRoot=obj;
    vpCleanupSet=theRoot->getVPCleanupSet();
    vpCleanupSet.removeVP(vp);
    theRoot->cleanup(COSMissHandler::STARTCLEANUP);
}

uval
COSMgrObject::ManualCleanupOperator::perVPInvocation()
{
    VPNum vp=Scheduler::GetVP();

    if (vpCleanupSet.isSet(vp)) {
        theRoot->cleanup(COSMissHandler::DOVPCLEANUP);
        vpCleanupSet.removeVP(vp);
    }
    return vpCleanupSet.isEmpty();
}

uval
COSMgrObject::ManualCleanupOperator::isDone()
{
    return vpCleanupSet.isEmpty();
}

// FIXME:  For the moment I have used a lot of dynamic allocations
//         and avoided any dependencies on the number of generations
//         (therefore any dependencies on the number of gen swaps required
//          to know that a marker has ELAPSED).  If performance turns out
//         to be an issue that replace list of delete sets with a fixed
//         array and simply merge in the deletes into the array.
/* virtual */ SysStatus
COSMgrObject::checkCleanup()
{
    COSMissHandler *obj;
    CORef objref;
    ListSimple<COSMissHandler*, AllocLocalStrict> tmpList;
    COSMgrObject *previous;
    TransferRec trec;
    SysStatus rtn;

//    err_printf("checkCleanup: this=%p Token=%p:\n", this, Token);
#if 0
    err_printf("  stage1LocalCount=%ld stage1RemoteCount=%ld\n",
	       stage1LocalCount, stage1RemoteCount);
    err_printf("  localDeleteSets.setCount=%ld localDeleteSets.objCount=%ld "
	       "remoteDeleteSet.count=%ld\n",
	       localDeleteSets.setCount, localDeleteSets.objCount,
	       remoteDeleteSet.count);
    err_printf("  reclaimRefCount=%ld "
	       "waitingForTokenReclaimRefCount=%ld transferArrayCount=%ld\n",
	       reclaimRefCount,
	       waitingForTokenReclaimRefCount, transferArrayCount);
#endif /* #if 0 */
    // For uniprocessor serialization acquire local locak.
    // FIXME: may want to optimize via tmp lists to reduce time spent holding
    //        lock (this gets mess tried a few times but decided to leave it
    //        for a latter optimization)
    // FIXME:  May want to revisit the need for this lock.
    lock.acquire();

    updateTheGen();  // Attempt to advance the generation count
                     // for good measure

    // Take care of local objects which have been destroyed
    createNewLocalDeleteSet();

    if (processLocalDeleteSets(&tmpList)==1) {
	// now clean up any local Objects found to be ready
	ACTIVATE_PRE_ALLOC;
	while (tmpList.removeHead(obj)) {
	    DEACTIVATE_POST_ALLOC;
	    objref = obj->getRef();
            if (isRefLocal(objref)) {
		ACTIVATE_PRE_ALLOC;
                waitingForTokenReclaimLocalRefList.add(objref);
		DEACTIVATE_POST_ALLOC;
                UPDATEDBGCOUNTER( waitingForTokenReclaimLocalRefCount++);
                UPDATEPINNEDDBGCOUNTER(objref,
                                   waitingForTokenReclaimRefPinnedCount++);
            } else {
//                err_printf("local delete set ref=%p vp=%ld not Local Ref\n",
//                           objref, vp);
		ACTIVATE_PRE_ALLOC;
                waitingForTokenReclaimRemoteRefList.add(objref);
		DEACTIVATE_POST_ALLOC;
                UPDATEDBGCOUNTER(waitingForTokenReclaimRemoteRefCount++);
                UPDATEPINNEDDBGCOUNTER(objref,
                                 waitingForTokenReclaimRefPinnedCount++);
            }
	    // A local object can be explicity Cleaned immediately
	    resetLocalTransEntry(objref);
	    ACTIVATE_PRE_ALLOC;
	    rtn=obj->cleanup(COSMissHandler::STARTCLEANUP);
	    DEACTIVATE_POST_ALLOC;
	    tassert(_SUCCESS(rtn), err_printf("Local: root=%p ref=%p did not "
					      "successfully cleanup\n", obj,
					      objref));
	    ACTIVATE_PRE_ALLOC;
	}
	DEACTIVATE_POST_ALLOC;
    }

    // invoke cleanup method on objects on cleanup list
    {
	void *prev=NULL;
	void *curr=vpCleanupList.next(NULL, obj);
	while (curr) {
	    ACTIVATE_PRE_ALLOC;
	    if (_SUCCESS(obj->cleanup(COSMissHandler::DOVPCLEANUP))) {
		vpCleanupList.removeNext(prev);
		UPDATEDBGCOUNTER(cleanupCount--);
	    } else {
		prev=curr;
	    }
	    curr=vpCleanupList.next(prev, obj);
	    DEACTIVATE_POST_ALLOC;
	}
    }

    // check to see if we have the token
    if (haveToken()) {
	previous = prevRep();
	// Yes we do so process remote objects
	if (seenToken == 0) {
	    // First time we have noticed that the Token has arrived

	    // if we are the zeroth processor then update the global gen count
	    if (vp == 0) {
		COGLOBAL(updateGlobalGenCount());
	    }

	    // reset my transfer Array
	    transferArray.reinit();
	    UPDATEDBGCOUNTER(transferArrayCount = 0);
	    // Next we take care of refs which can be reclaimed
	    ACTIVATE_PRE_ALLOC;
	    while (reclaimRefList.removeHead(objref)) {
		DEACTIVATE_POST_ALLOC;
		// The very last stage of death is to reclaim the CO reference
		// for reuse
		//err_printf("--reclaiming ref=%p\n", objref);
		dealloc(objref);
		UPDATEDBGCOUNTER(reclaimRefCount--);
		UPDATEPINNEDDBGCOUNTER(objref, reclaimRefPinnedCount--);

#if 0
		err_printf("--pinnedPageDescList->numAllocated = %ld\n",
			   COGLOBAL(pinnedPageDescList).numAllocated());
		err_printf("--pagablePageDescList->numAllocated = %ld\n",
			   pagablePageDescList.numAllocated());
		err_printf("reclaimRefCount = %ld, reclaimRefPinnedCount = "
			"%ld\n", reclaimRefCount, reclaimRefPinnedCount);
#endif /* #if 0 */
		ACTIVATE_PRE_ALLOC;
	    }
	    DEACTIVATE_POST_ALLOC;

	    // now move the refs which where waiting for the token to arrive
	    // on the waiting list the reclaim list so that the next time
	    // the token arrives they can be cleaned up.
            // Local refs waiting for token can be immediately added to our
            // reclaimRefList
	    reclaimRefList = waitingForTokenReclaimLocalRefList;
	    waitingForTokenReclaimLocalRefList.reinit();
	    UPDATEDBGCOUNTER(reclaimRefCount
                             +=waitingForTokenReclaimLocalRefCount);
	    UPDATEDBGCOUNTER(reclaimRefPinnedCount
                             +=waitingForTokenReclaimRefPinnedCount);
            UPDATEDBGCOUNTER(waitingForTokenReclaimLocalRefCount = 0);
            UPDATEDBGCOUNTER(waitingForTokenReclaimRefPinnedCount = 0);
            // however Remote refs waiting for token must go on the tranfer
            // Array.  This should be a very rare case.  CO was only accessed
            // On this processor but was allocated else where.  Note even sure
            // If this is possible.  For the moment assert on this.
	    ACTIVATE_PRE_ALLOC;
	    while (waitingForTokenReclaimRemoteRefList.removeHead(trec.ref)) {
//                err_printf("moving remote ref=%p vp=%ld waiting list\n",
//                           trec.ref, vp);
                trec.root=0;
                transferArray.add(trec);
		UPDATEDBGCOUNTER(waitingForTokenReclaimRemoteRefCount--);
		UPDATEPINNEDDBGCOUNTER(objref, reclaimRefPinnedCount--);
            }
	    DEACTIVATE_POST_ALLOC;
	    // Finally create a delete set for the objects on the remote list
	    createRemoteDeleteSet();
	    seenToken = 1;
	    // Regardless if the delete set is empty or not we
	    // must wait for the newly created marker associated with
	    // this delete set to elapse before we can pass the token
	    // to ensure that the token's sematics are mantained.
	}
	// We already have the token.  Attempt to process remote
	// delete set created when we first got the token.
	// FIXME:  We could add a support to treat the remote deletes
	//         as the local deletes.  That is create a delete
	//         set every time we enter when we have the token
	//         but only process those sets whos markers have elapsed
	//         with respect to the token time.  But this gets
	//         complicated as when we get the token we need
	//         to adjust the time of the markers to reflect the number
	//         of swaps remaining for the delete created the last
	//         time we had the token.  If the calls to check cleanup
	//         are frequent enough we don't expect the length of
	//         time which we have to token to be long with respect
	//         to the length of time we do not have the token.
	if (processRemoteDeleteSet(&tmpList) == 1) {
	    // Ok the remote delete set created when when got the token
	    // is ready to be cleaned up.
	    ACTIVATE_PRE_ALLOC;
	    while (tmpList.removeHead(trec.root)) {
		DEACTIVATE_POST_ALLOC;
		// get the vp mask of the object.
		trec.transSet       = trec.root->getTransSet();
		trec.vpCleanupSet   = trec.root->getVPCleanupSet();
		// Since we are the vp on which the CO was destroyed we
		// take responsiblity for reclaiming the id.
		// FIXME:  Note we end up calling a method of the root
		//         of the CO even if this CO was never accessed
		//         on this vp.  It would be nice to avoid
		//         extra global memory accesses.
		objref     = trec.root->getRef();
                if (isRefLocal(objref)) {
		    ACTIVATE_PRE_ALLOC;
                    reclaimRefList.add(objref);
		    DEACTIVATE_POST_ALLOC;
                    trec.ref = 0;
                } else {
                    trec.ref = objref;
                }
		UPDATEDBGCOUNTER(reclaimRefCount++);
		UPDATEPINNEDDBGCOUNTER(objref, reclaimRefPinnedCount++);
//		err_printf("-cleaning %ldth remote object=%p ref=%p\n",
//			   i, trec.root, objref);
		checkCleanupObj(trec.root, &trec.transSet, &trec.vpCleanupSet);

		// Since this was on our remote list there must be at least
		// one mapping.
		tassert(!trec.transSet.isEmpty(),
			err_printf("remote object has no remote mappings\n"));
		// The co by definition of being remote requires transfer to
		// the next vp to try again
		ACTIVATE_PRE_ALLOC;
		transferArray.add(trec);
		DEACTIVATE_POST_ALLOC;

		UPDATEDBGCOUNTER(transferArrayCount++);
		ACTIVATE_PRE_ALLOC;
	    }
	    DEACTIVATE_POST_ALLOC;

	    // If we have gotten hear we know that all the threads since
	    // we got then token have died (as the remote delete set thread
	    // marker must have elapsed).  So we now process the objects
	    // transfered to us aswell.

	    // avoid bootstrap case in which the first vp
	    // has the token set to itself.
	    // FIXME: find a way to avoid this check on common case
	    if (previous != this) {
		for (void *cur = previous->transferArray.next(0, trec);
		     cur != 0;
		     cur = previous->transferArray.next(cur, trec)) {
		    // Ok lets do the local cleanup for the co's passed to us
		    // via our predecessor's transferArray.
		    // Begin by checking to see if this CO has resources on
		    // this vp. Thus avoid touching the co when not necessary.
                    if (trec.root) {
                        checkCleanupObj(trec.root, &trec.transSet,
                                        &trec.vpCleanupSet);

                        if (trec.ref && isRefLocal(trec.ref)) {
                            // Our ref so add it to our reclaimRefList
			    ACTIVATE_PRE_ALLOC;
                            reclaimRefList.add(trec.ref);
			    DEACTIVATE_POST_ALLOC;
                            UPDATEDBGCOUNTER(reclaimRefCount++);
                            UPDATEPINNEDDBGCOUNTER(trec.ref,
                                                   reclaimRefPinnedCount++);
                            trec.ref=0;
                        }

                        if (trec.transSet.isEmpty()) trec.root=0;

                        // if transSet is not empty or the ref has not
                        // been claimed continue to pass it on
                        if (!trec.transSet.isEmpty() || trec.ref) {
                            // Not all translations have been cleared so
                            // pass object on to the next vp
			    ACTIVATE_PRE_ALLOC;
			    transferArray.add(trec);
			    DEACTIVATE_POST_ALLOC;
			    UPDATEDBGCOUNTER(transferArrayCount++);
                        }
                    } else {
                        if (trec.ref && isRefLocal(trec.ref)) {
                            // Our ref so add it to our reclaimRefList
			    ACTIVATE_PRE_ALLOC;
                            reclaimRefList.add(trec.ref);
			    DEACTIVATE_POST_ALLOC;
                            UPDATEDBGCOUNTER(reclaimRefCount++);
                            UPDATEPINNEDDBGCOUNTER(trec.ref,
                                                   reclaimRefPinnedCount++);
                            trec.ref=0;
                        } else {
                            // Not our ref so move it along
			    ACTIVATE_PRE_ALLOC;
                            transferArray.add(trec);
			    DEACTIVATE_POST_ALLOC;
			    UPDATEDBGCOUNTER(transferArrayCount++);
                        }
                    }
                }
	    }
	    // ok we have had the token long enough for all threads
	    // to pass away so pass the token along to let someone else
	    // have a turn.
	    seenToken = 0;
	    handoffToken();
	}
    }

    lock.release();

    // FIXME: may want to put back retry logic to catch late token arrivals
    return 1;
}


void
COSMgrObject::createRemoteDeleteSet()
{
    tassert(lock.isLocked(),
	    err_printf("oops we don't have the COSMgrObject::lock\n"));

    stage1Lock.acquire();

    // Regardless of the stage1RemoteList being empty we must
    // wait for a marker to elapse to determine when it is ok to
    // pass the token.
    remoteDeleteSet.list = stage1RemoteList;
    stage1RemoteList.reinit();
    UPDATEDBGCOUNTER(remoteDeleteSet.count += stage1RemoteCount);
    UPDATEDBGCOUNTER(stage1RemoteCount = 0);
    stage1Lock.release();

    setVPThreadMarker(remoteDeleteSet.marker);
}


uval
COSMgrObject::processRemoteDeleteSet(ListSimple<COSMissHandler*,
				     AllocLocalStrict> *tmpList)
{

    MarkerState state;

    tassert(lock.isLocked(),
	    err_printf("oops we don't have the COSMgrObject::lock\n"));

    locked_updateAndCheckVPThreadMarker(remoteDeleteSet.marker,state);

    if (state == ELAPSED) {
	*tmpList = remoteDeleteSet.list;
	remoteDeleteSet.list.reinit();
	UPDATEDBGCOUNTER(remoteDeleteSet.count = 0);
	return 1;
    }

    return 0;
}


void
COSMgrObject::createNewLocalDeleteSet()
{
    tassert(lock.isLocked(),
	    err_printf("oops we don't have the COSMgrObject::lock\n"));

    DeleteSet *newset;

    // FIXME:  See about trying to remove necessity to lock list by
    //         coordinating with gcAdd
    stage1Lock.acquire();

    if (stage1LocalList.isEmpty()) {
	stage1Lock.release();
	return;
    }
    ACTIVATE_PRE_ALLOC;
    newset       = new DeleteSet;
    DEACTIVATE_POST_ALLOC;

    newset->next = NULL;
    newset->list = stage1LocalList;
    stage1LocalList.reinit();
    UPDATEDBGCOUNTER(newset->count = stage1LocalCount);
    UPDATEDBGCOUNTER(stage1LocalCount = 0);
    stage1Lock.release();
    UPDATEDBGCOUNTER(localDeleteSets.objCount += newset->count);
    setVPThreadMarker(newset->marker);

    // merge new delete set into previous sets
    // sets are ordered by marker.  Oldest marker (least valued) to newest
    // marker (largest valued).
    // FIXME: We probably need to serial this if there are multiple
    //        checkcleanups happening on a vp.  Lets see what we can do to
    //        avoid this. But for the moment we are locking in checkCleanup
    if (localDeleteSets.tail == NULL) {
	localDeleteSets.tail = newset;
	localDeleteSets.head = newset;
	UPDATEDBGCOUNTER(localDeleteSets.setCount = 1);
    } else {
	tassert(localDeleteSets.tail->marker <= newset->marker,
		err_printf("Oops new marker is older than a previous marker"
			   " did we wrap new=%ld < old=%ld\n",newset->marker,
			   localDeleteSets.tail->marker));
	if ( localDeleteSets.tail->marker == newset->marker ) {
	    // As an optimization we merge in new deletes in to old deletes
	    // as marker is the same
	    newset->list.transferTo(localDeleteSets.tail->list);
	    UPDATEDBGCOUNTER(localDeleteSets.tail->count += newset->count);

	    ACTIVATE_PRE_ALLOC;
	    delete(newset);
	    DEACTIVATE_POST_ALLOC;
	} else {
	    localDeleteSets.tail->next = newset;
	    localDeleteSets.tail       = newset;
	    UPDATEDBGCOUNTER(localDeleteSets.setCount++);
	}
    }
}

uval
COSMgrObject::processLocalDeleteSets(ListSimple<COSMissHandler*,
				     AllocLocalStrict> *tmpList)
{
    DeleteSet *cur = localDeleteSets.head;

    tassert(lock.isLocked(),
	    err_printf("oops we don't have the COSMgrObject::lock\n"));

    updateTheGen();

    while (cur != NULL && checkVPThreadMarker(cur->marker) == ELAPSED) {
	cur->list.transferTo(*tmpList);
	UPDATEDBGCOUNTER(localDeleteSets.objCount -= cur->count);
	localDeleteSets.head = cur->next;

	ACTIVATE_PRE_ALLOC;
	delete(cur);
	DEACTIVATE_POST_ALLOC;

	cur = localDeleteSets.head;
	UPDATEDBGCOUNTER(localDeleteSets.setCount--);
	updateTheGen();
    }
    // if we deleted the last set then reset the tail;
    if (localDeleteSets.head == NULL) localDeleteSets.tail=NULL;

    // if the list is not empty return 1 else return 0
    return ( ! tmpList->isEmpty() );
}

// test routine called from KernelInit
void
runObjGC()
{
    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
}



extern "C" SysStatus GenericDefaultFunc(uval &ths, uval methodNum);

//FIXME:  This code needs a lot of cleanup!!!
SysStatus
GenericDefaultFunc(uval &ths, uval methodNum)
{
    LTransEntry    *lte = (LTransEntry *)(ths-sizeof(uval));
    GTransEntry    *gte = COSMgr::localToGlobal(lte);
    COSMissHandler *mh;
    COSTransObject *co;

    if (gte->mhReadEntry() == 0) {
        // Failed to acquire read lock.  Object is free or is being destroyed.
	return 0;
    }

    // It is now safe to read read the gte and invoke the miss Handler
    mh  = gte->getMH();
    tassert(mh,
	    err_printf("OOPS: Attempted to access an Empty MHOEntry\n"));
    // Invoke appropriate miss Handler
    mh->handleMiss(co, (CORef)lte, methodNum); // invoke missHandler

    // release
    gte->mhReadExit();

    tassert(co,
	    err_printf("OOPS: %p->handleMiss() returns 0\n", mh));

    // We leave it up to the missHandler to have installed a local
    // Entry if it desired to do so.

    // To ensure restarting of method invocation locate method in
    // object returned by miss handler
    COVTable *vtable = *((COVTable **)co);

    // Ensure that the 'this' pointer is pointing at the right object
    ths = (uval)co;
    return (vtable->vt[methodNum].getFunc());
}

SysStatus
COSMgrObject::print()
{
    err_printf("--pinnedPageDescList->numAllocated = %ld\n",
	       COGLOBAL(pinnedPageDescList).numAllocated());

    //FIXME: need to do this across all rep
    err_printf("--pagablePageDescList->numAllocated = %ld\n",
	         pagablePageDescList.numAllocated());
    err_printf("--pinnedPageDescList object vtable addresses:\n");
    COGLOBAL(pinnedPageDescList).printVTablePtrs();

    err_printf("--pagablePageDescList object vtable addresses:\n");
    //FIXME: need to do this across all rep
    pagablePageDescList.printVTablePtrs();
    return 0;
}

/* virtual */ SysStatusUval
COSMgrObject::getCOList(CODesc *objDescs, uval numObjDescs)
{
    uval rtn = 0;
    if (vp == 0) {
        rtn=COGLOBAL(pinnedPageDescList).getCOList(objDescs, numObjDescs);
        if (rtn == numObjDescs) return rtn;
    }
    rtn += pagablePageDescList.getCOList(&(objDescs[rtn]), numObjDescs - rtn);
    return rtn;
}

#if 0
uval
COSMgrObject::isRef(CORef ref) {
    return (   (((uval)ref >= (uval)lTransTablePinned) &&
                ((uval)ref <  ((uval)lTransTablePinned
                               + gPinnedSize)))
               ||((uval)ref >= (uval)lTransTablePaged) &&
               ((uval)ref <  ((uval)lTransTablePaged
                              + gTransTablePagableSize)));
}

VPNum
COSMgrObject::refToVP(CORef ref) {
    // FIXME: May want to fix this later but for the moment
    //        report to callers that pinned entries are mapped to
    //        vp 0
    tassert(isRef(ref), ;);
    if (((uval)ref >= (uval)lTransTablePinned) &&
        ((uval)ref < ((uval)lTransTablePinned + gPinnedSize))) {
        return (VPNum)0;
    } else {
        return (VPNum)(((uval)ref - ((uval)lTransTablePaged))
                       >> logGPartPagableSize);
    }
}
#endif

/* static */ SysStatus 
COSMgrObject::getTypeToken(COSMissHandler *mh, uval &typeToken)
{
    static uval singleRepVTable = 0;
    static uval singleRepPinVTable = 0;
    uval vTable;

    if (mh == NULL) return -1;
    // A non-null miss-handler pointer indicates that the object
    // is not free.

    if (singleRepVTable == 0) {
        // NOTE: A race here should not matter
        // Track down the vtable pointer of a single-rep clustered
        // object.  We use it to recognize other single-rep objects.
        CObjRep dummyRep;
        CObjRootSingleRep *tmpRoot = new 
            CObjRootSingleRep(&dummyRep, NULL,
                              CObjRoot::skipInstall);
        singleRepVTable = *((uval *) tmpRoot);
        delete tmpRoot;
        CObjRootSingleRepPinned *tmpPRoot = 
            new CObjRootSingleRepPinned(&dummyRep, NULL,
                                        CObjRoot::skipInstall);
        singleRepPinVTable = *((uval *) tmpPRoot);
        delete tmpPRoot;
    }

    // Get the vtable pointer of the miss-handler.
    vTable = *((uval *) mh);
    if (vTable == singleRepVTable || vTable == singleRepPinVTable) {
        // The root is of type CObjRootSingleRep, so get the
        // vtable pointer of the rep.        
        CObjRootSingleRep *singleRepRoot = (CObjRootSingleRep *) mh;
        vTable = *((uval *) (singleRepRoot->getRepOnThisVP()));
    }

    typeToken = vTable;
    return 0;
}
