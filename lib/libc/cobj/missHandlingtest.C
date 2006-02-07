/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: missHandlingtest.C,v 1.38 2004/10/08 21:40:06 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "CObjRootSingleRep.H"
#include <misc/ListSimpleKey.H>
#include "CObjRootMultiRep.H"
#include <misc/ListArraySimple.H>
#include <scheduler/Scheduler.H>
#include <misc/testSupport.H>
#include "sys/COSMgrObject.H"
#include <cobj/CObjRepArbiter.H>
#include <misc/DHashTable.H>
#include <misc/BaseRandom.H>

class IntegerCounter : public BaseObj {
    IntegerCounter() { /* empty body */ }
protected:
    sval *resourceCount;
    IntegerCounter(sval *count) {
//	err_printf("IntegerCounter::IntegerCounter this=%p\n",
//		   this);
	resourceCount=count;
    	FetchAndAddSignedSynced(resourceCount,1);
	cprintf("+");
    }
public:

    virtual SysStatus inc()=0;
    virtual SysStatus dec()=0;
    virtual SysStatus value(sval &count)=0;
    virtual ~IntegerCounter() {
	FetchAndAddSignedSynced(resourceCount,-1);
	cprintf("-");
    }
};

typedef IntegerCounter **IntegerCounterRef;

class SharedIntegerCounter : public IntegerCounter {
protected:
    sval _value;
    DEFINE_GLOBAL_NEW(SharedIntegerCounter);
public:
    SharedIntegerCounter(sval *count) : IntegerCounter(count), _value(0) {
	/* empty body */
    }
    virtual SysStatus inc();
    virtual SysStatus dec();
    virtual SysStatus value(sval &count);
    virtual SysStatus cleanup();
    static SysStatus Create(IntegerCounterRef &ref, sval *count) {
	ref=(IntegerCounterRef)
	    (CObjRootSingleRep::Create(new SharedIntegerCounter(count)));
	return 0;
    }
#if 0
    static SysStatus ClassInit(IntegerCounterRef ref);
#endif /* #if 0 */
};

class SharedIntegerCounterPinned: public SharedIntegerCounter {
    DEFINE_PINNEDGLOBAL_NEW(SharedIntegerCounterPinned);
public:
    SharedIntegerCounterPinned(sval *count) : SharedIntegerCounter(count) {
	/* empty body */
    }
    static SysStatus Create(IntegerCounterRef &ref, sval *count) {
	ref = (IntegerCounterRef)
	    (CObjRootSingleRepPinned::Create(new SharedIntegerCounter(count)));
	return 0;
    }
};

#if 0
/* static */ SysStatus
SharedIntegerCounter::ClassInit(IntegerCounterRef ref)
{
    // In a real case we expect the ref to be a well know ref
    // And it would not need to be passed in
    static SharedIntegerCounter theCounter;
    static CObjRootSingleRep root( &theCounter, (RepRef)ref );
    return 0;
}
#endif /* #if 0 */

SysStatus
SharedIntegerCounter::inc()
{
    FetchAndAddSignedSynced(&(_value),1);
//    cprintf("      SharedIntegerCounter::inc(): _value=%ld myRoot=%p\n",
//	    _value, myRoot);
    return 1;
}

SysStatus
SharedIntegerCounter::dec()
{
    FetchAndAddSignedSynced(&(_value),-1);
//    cprintf("SharedIntegerCounter::dec(): _value=%d\n",_value);
    return 1;
}

SysStatus
SharedIntegerCounter::value(sval &count)
{
    count=_value;
//    cprintf("SharedIntegerCounter::value(): _value=%d\n",_value);
    return 1;
}

SysStatus
SharedIntegerCounter::cleanup()
{
//    cprintf("SharedIntegerCounter::cleanup(): _value=%d: *** GOODBYE!!!\n",
//	    _value);
    delete this;
    return 0;
}

#if 0
// This nolonger works the way it is as the cleanup method
// of the CObjRootSingleRep explicilty calls clenaup on the
// rep and then deletes itself.  This off course can not
// work if it is embedded in the rep.  A new root class
// which is embeddable is required.  This class can be made
// by subclassing CObjRootSingleRep but overiding the cleanup
// method.
/*
 * PINNED shared rep with embedded root
 */
class SICERoot : public IntegerCounter {
int _value;
    CObjRootSingleRep myRoot;  //overshadows inherited myRoot
    IntegerCounterRef getRef()
    {
      return (IntegerCounterRef) myRoot.getRef();
    }
    DEFINE_PINNEDGLOBAL_NEW(SICERoot);
    SICERoot(sval *rcount) : IntegerCounter(rcount), _value(0),myRoot(this) { }
public:
    virtual SysStatus inc();
    virtual SysStatus dec();
    virtual SysStatus value(sval &count);
    static SysStatus Create(IntegerCounterRef &ref, sval *rcount) {
	ref = (new SICERoot(rcount))->getRef();
	return 0;
    }
};

SysStatus
SICERoot::inc()
{
    _value++;
    cprintf("SICERoot::inc(): _value=%d\n",_value);
    return 1;
}

SysStatus
SICERoot::dec()
{
    _value--;
    cprintf("SICERoot::dec(): _value=%d\n",_value);
    return 1;
}

SysStatus
SICERoot::value(sval &count)
{
    count=_value;
    cprintf("SICERoot::value(): _value=%d\n",_value);
    return 1;
}

#endif /* #if 0 */
/*
 * PAGED multirep case
 */
class IntegerCounterReplicated : public IntegerCounter {
    class ICRRoot : public CObjRootMultiRep {
	sval *resourceCount;
    public:
	virtual CObjRep * createRep(VPNum vp) {
	    CObjRep *rep=(CObjRep *)new
		IntegerCounterReplicated(resourceCount);
	    err_printf("ICRRoot::createRep() : New rep created rep=%p vp=%ld\n",
                       rep,vp);
	    return rep;
	}
	ICRRoot(sval *rcount) {
	    resourceCount = rcount;
	    FetchAndAddSignedSynced(resourceCount,1);
	    cprintf("+");
	}

	ICRRoot(sval *rcount, RepRef ref):CObjRootMultiRep(ref) {
	    resourceCount = rcount;
	    FetchAndAddSignedSynced(resourceCount,1);
	    cprintf("+");
	}

	~ICRRoot() {
	    FetchAndAddSignedSynced(resourceCount,-1);
	    cprintf("-");
	}

	DEFINE_GLOBAL_NEW(ICRRoot);
	// simple case, create a root for a single object with default pools...
	static IntegerCounterRef Create(sval *rcount, void *ref=NULL) {
  	    IntegerCounterRef retvalue;
	    if (ref==NULL) {
		retvalue = (IntegerCounterRef)
		    ((new ICRRoot(rcount))->getRef());
	    } else {
		retvalue = (IntegerCounterRef)
		    (new ICRRoot(rcount, (RepRef)ref))->getRef();
	    }
	    return(retvalue);
	}
    };

    friend class IntegerCounterReplicated::ICRRoot;
    friend class IntegerCounterReplicatedPinned;

    ICRRoot *root()
    {
      return (ICRRoot *)myRoot;
    }

    sval _val;
    IntegerCounterReplicated(sval *rcount): IntegerCounter(rcount), _val(0) {
	/* empty body */
    }
    ~IntegerCounterReplicated() {
//	cprintf("**** ~IntegerCounterReplicated: GoodBye Cruel World\n");
    }
public:
    static SysStatus Create(IntegerCounterRef &ref, sval *rcount);
    virtual SysStatus value(sval &count);
    virtual SysStatus inc();
    virtual SysStatus dec();
    DEFINE_LOCALSTRICT_NEW(IntegerCounterReplicated);
};

/*
 * PINNED multirep case
 */
class IntegerCounterReplicatedPinned : public IntegerCounterReplicated {
    class ICRRoot : public CObjRootMultiRep {
	sval *resourceCount;
    public:
	virtual CObjRep * createRep(VPNum vp) {
	    CObjRep *rep=(CObjRep *)new
		IntegerCounterReplicatedPinned(resourceCount);
//	    err_printf("ICRRoot::createRep() : New rep created rep=%p\n",rep);
	    return rep;
	}
	ICRRoot(sval *rcount) {
	    resourceCount = rcount;
	    FetchAndAddSignedSynced(resourceCount,1);
	    cprintf("+");
	}
	ICRRoot(sval *rcount, RepRef ref):CObjRootMultiRep(ref) {
	    resourceCount = rcount;
	    FetchAndAddSignedSynced(resourceCount,1);
	    cprintf("+");
	}
	~ICRRoot() {
	    FetchAndAddSignedSynced(resourceCount,-1);
	    cprintf("-");
	}
	DEFINE_PINNEDGLOBAL_NEW(ICRRoot);
	// simple case, create a root for a single object with default pools...
	static IntegerCounterRef Create(sval *rcount, void *ref=NULL) {
  	    IntegerCounterRef retvalue;
	    if (ref==NULL) {
		retvalue = (IntegerCounterRef)(new ICRRoot(rcount))->getRef();
	    } else {
		retvalue = (IntegerCounterRef)
		    (new ICRRoot(rcount, (RepRef)ref))->getRef();
	    }
	    return(retvalue);
	}
    };
    friend class IntegerCounterReplicatedPinned::ICRRoot;

    ICRRoot *root()
    {
      return (ICRRoot *)myRoot;
    }
    ~IntegerCounterReplicatedPinned() {
//	cprintf("~IntegerCounterReplicatedPinned: GoodBye Cruel World\n");
    }
    IntegerCounterReplicatedPinned(sval *rcount) :
	IntegerCounterReplicated(rcount) {
	/* empty body */
    }
public:
    DEFINE_PINNEDGLOBAL_NEW(IntegerCounterReplicatedPinned);
    static SysStatus Create(IntegerCounterRef &ref, sval *rcount);
};

/*Static*/ SysStatus
IntegerCounterReplicated :: Create(IntegerCounterRef &ref,sval *rcount)
{
    ref=ICRRoot::Create(rcount);
    return 0;
}

/*Static*/ SysStatus
IntegerCounterReplicatedPinned :: Create(IntegerCounterRef &ref, sval *rcount)
{
    ref=ICRRoot::Create(rcount);
    return 0;
}

SysStatus
IntegerCounterReplicated :: value(sval &count)
{
        IntegerCounterReplicated *rep=0;
        count=0;
	root()->lockReps();
        for (void *curr=root()->nextRep(0,(CObjRep *&)rep);
             curr; curr=root()->nextRep(curr,(CObjRep *&)rep)) {
	    count+=rep->_val;
	}
	root()->unlockReps();
//	cprintf("IntegerCounterReplicated::value(): _val=%ld this=%p\n",_val,
//		this);
	return 1;
}

SysStatus
IntegerCounterReplicated :: inc()
{
    FetchAndAddSignedSynced(&(_val),1);
//    cprintf("IntegerCounterReplicated::inc(): _val=%ld myRoot=%p\n",_val,
//	    myRoot);
    return 1;
}

SysStatus
IntegerCounterReplicated :: dec()
{
    FetchAndAddSignedSynced(&(_val),-1);
//    cprintf("IntegerCounterReplicated::dec(): _val=%ld this=%p\n",_val,
//	    this);
    return 1;
}

void
printRef(RepRef ref)
{
    LTransEntry *lte=(LTransEntry *)ref;
    GTransEntry *gte=COSMgr::localToGlobal(lte);
    COSMissHandler *mh=gte->getMH();
    CObjRootSingleRep *root=(CObjRootSingleRep *)mh;
    struct rootDetails {
	uval vtab;
	uval8 pool;
	uval ref;
	uval therep;
	uval thevp;
	uval vpMask;
    } *details=(rootDetails *)root;

    cprintf("  ref=%p *ref=%p **ref=%lx\n",ref,*ref,*((uval *)*ref));
    cprintf("  gte=%p mh=%p CObjRootSingleRep=%p vtab=%lx pool=%d ref=%lx"
	    " therep=%lx therep.vtab=%lx\n",
	    gte, mh, root, details->vtab, details->pool, details->ref,
	    details->therep, *((uval *)details->therep));

}

uval
allocdeallocTest()
{
    COSMissHandler *mh=(COSMissHandler *)0xdeadbeaf;
    CORef refs[COSMgr::numGTransEntriesPerPage+1], oldref;
    uval i;

    cprintf("Test of Allocation and Deallocation of TransEntries\n");

    cprintf("  Testing allocation of Paged Entries crossing at least one"
	    " Page Boundary of TransEntries\n");
    // cross at least one page boundary
    DREFGOBJ(TheCOSMgrRef)->alloc(mh,refs[0],AllocPool::PAGED);
    cprintf("  The startingref is : startingref=%p\n",refs[0]);
    for (i=1;
	 i<COSMgr::numGTransEntriesPerPage;
	 i++) {
	DREFGOBJ(TheCOSMgrRef)->alloc(mh,refs[i],AllocPool::PAGED);
    }

    cprintf("  last ref=%p. OK here goes nothing, allocating an entry that"
	    " must be on a new page...\n",refs[i-1]);

    DREFGOBJ(TheCOSMgrRef)->alloc(mh,refs[i],AllocPool::PAGED);
    cprintf("  The paged Entry is : ref=%p\n",refs[i]);

    cprintf("  Lets test deallocation of Paged Entries by cleanup previous"
	    " allocations\n");
    oldref = refs[i];
    // deallocate the last ref
    DREFGOBJ(TheCOSMgrRef)->dealloc(refs[i]);
    // reallocate it and see if we get it back
    DREFGOBJ(TheCOSMgrRef)->alloc(mh,refs[i],AllocPool::PAGED);
    tassert(oldref == refs[i],
	    err_printf("oops we did not get the same oldref=%p ref=%p\n",
		       oldref, refs[i]));

    // lets remember what the fist rep allocated was
    oldref = refs[0];

    // ok now deallocate everything
    for (i=COSMgr::numGTransEntriesPerPage;
	 i>0;
	 i--) {
	DREFGOBJ(TheCOSMgrRef)->dealloc(refs[i]);
    }

    // now deallocate the first ref
    DREFGOBJ(TheCOSMgrRef)->dealloc(refs[0]);
    //one last check

    // lets try and allocate a ref.  Because we deallocated in reverse order
    // we should get back the same ref that we started with.
    DREFGOBJ(TheCOSMgrRef)->alloc(mh,refs[0],AllocPool::PAGED);
    tassert(oldref == refs[0],
	    err_printf("oops we did not get the same oldref=%p "
		       "startingref=%p\n", oldref, refs[0]));
    // ok now deallocate if for real
    DREFGOBJ(TheCOSMgrRef)->dealloc(refs[0]);

    cprintf("  Lets try allocating and deallocating a Pinned Entry\n");
    DREFGOBJ(TheCOSMgrRef)->alloc(mh,refs[0],AllocPool::PINNED);
    cprintf("  The pinned entry is : ref=%p\n",refs[0]);
    oldref = refs[0];

    DREFGOBJ(TheCOSMgrRef)->dealloc(refs[0]);

    DREFGOBJ(TheCOSMgrRef)->alloc(mh,refs[0],AllocPool::PINNED);
    tassert(oldref == refs[0], err_printf("oops we did not get the same pinned"
				      " entry back oldref=%p ref=%p", oldref,
				      refs[0]));
    DREFGOBJ(TheCOSMgrRef)->dealloc(refs[0]);
    cprintf("Test Allocation and Deallocation of TransEntries passed\n");

    return 1;
}
#if 0
void
staticCOTestSetup(IntegerCounterRef *theIref)
{
    COSMissHandler *mh=(COSMissHandler *)0xdeadbeaf;

    cprintf("  Creating the static counter");
    // Dummy alloc to get us a pinned ref to use for the static counter
    // In a real scenario this would not be necessary as the ref for the
    // static would be well known.
    DREFGOBJ(TheCOSMgrRef)->alloc(mh,(CORef &)(*theIref),
					AllocPool::PINNED);
    cprintf(" ref=%p ...", *theIref);
    SharedIntegerCounter::ClassInit(*theIref);
}

uval
staticCOTest(IntegerCounterRef theIref)
{
    sval v=10;
    cprintf("Testing a Pinned Static Clustered Object with a"
            " CObjRootSingleRep\n");

    DREF(theIref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(theIref)->inc();
    DREF(theIref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(theIref)->dec();
    DREF(theIref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(*theIref)->inc();

    return 1;
}
#endif /* #if 0 */

void
pinnedCOTestSetup(IntegerCounterRef *iref, sval *count)
{
    cprintf("  Creating the SharedIntegerCounterPinned Counter\n");
    SharedIntegerCounterPinned::Create(*iref, count);
}

uval
pinnedCOTest(IntegerCounterRef iref)
{
    sval v=10;

    cprintf("Testing a Pinned Cluster Object which uses a"
	    " CObjRootSingleRep\n");

    cprintf("  iref=%p ... ", iref);

    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->dec();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);

    return 1;
}

void
pagedCOTestSetup(IntegerCounterRef *iref, sval *count)
{
    cprintf("  Creating the SharedIntegerCounter counter\n");
    SharedIntegerCounter::Create(*iref, count);
}

uval
pagedCOTest(IntegerCounterRef iref)
{
    sval v=10;

    cprintf("\nTesting a Paged ClusteredObject which uses a"
            " CObjRootSingleRep\n");

    cprintf("  iref1=%p ... ", iref);

    DREF(iref)->value(v);
    cprintf("v=%ld .. ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->dec();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);

    return 1;
}

#if 0
void
pinnedEmbeddedRootCOTestSetup(IntegerCounterRef *iref, sval *count)
{
    cprintf("  Creating the pinnedEmbeddedRootCO counter\n");
    SICERoot::Create(*iref, count);
}

uval
pinnedEmbeddedRootCOTest(IntegerCounterRef iref)
{
    sval v=10;

    cprintf("\nTesting a Pinned ClusteredObject which embeddes a"
	    " CObjRootSinglRep\n");

    cprintf("  iref=%p ...", iref);

    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->dec();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);

    return 1;
}
#endif /* #if 0 */

void
pagedReplicatedCOTestSetup(IntegerCounterRef *iref, sval *count)
{
    cprintf("  Creating the replicated counter\n");
    IntegerCounterReplicated::Create(*iref, count);
}

uval
pagedReplicatedCOTest(IntegerCounterRef iref)
{
    sval v=10;

    cprintf("\nTesting a Paged ClusteredObject which is replicated\n");
    cprintf("  iref=%p ... ", iref);

    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->dec();
    DREF(iref)->value(v);
    cprintf("v=%ld ... ",v);

    DREF(iref)->inc();
    DREF(iref)->value(v);

    return 1;
}

uval
threadMarkerTest()
{
    COSMgr::ThreadMarker marker;
    COSMgr::MarkerState  mstate;
    cprintf("Testing COSMgr ThreadMarkers:\nCreating Marker ...");

    DREFGOBJ(TheCOSMgrRef)->setVPThreadMarker(marker);

    cprintf("  marker=%ld\n",marker);

    cprintf("  First tests of marker should return ACTIVE:\n");
    for (uval i=0; i<5; i++)
    {
	cprintf("    %ld: Testing marker=%ld: ",i,marker);
	DREFGOBJ(TheCOSMgrRef)->updateAndCheckVPThreadMarker(marker,
								   mstate);
	switch (mstate) {
	case COSMgr::ACTIVE : cprintf("ACTIVE\n"); break;
	case COSMgr::ELAPSED:
	    cprintf("ELAPSED\n");
	    tassert(0,
		    err_printf("marker has elapsed when it was not expected to"
			       " marker=%ld",
			       marker));
	    break;
	default:
	    tassert(0, err_printf("Unknown VPThreadMarker marker = %ld "
				  "State = %d",marker, mstate));
	}
    }

    cprintf("  Deactiveting self (removed from active count)\n");
    Scheduler::DeactivateSelf();

    cprintf("  Second tests of marker should return ELAPSED:\n");
    for (uval i=0; i<5; i++)
    {
	cprintf("    %ld: Testing marker=%ld: ",i,marker);
	DREFGOBJ(TheCOSMgrRef)->updateAndCheckVPThreadMarker(marker,
								   mstate);
	switch (mstate) {
	case COSMgr::ACTIVE : cprintf("ACTIVE\n"); break;
	case COSMgr::ELAPSED: cprintf("ELAPSED\n"); break;
	default:
	    tassert(0, err_printf("Unknown VPThreadMarker marker = %ld "
				  "State = %d",marker, mstate));
	}
    }

    Scheduler::ActivateSelf();
    cprintf("  Reactivated self: Put this thread back in active count\n");

    return 1;
}

uval
simpleDestTest(IntegerCounterRef iref0, IntegerCounterRef iref1,
	       IntegerCounterRef iref2, IntegerCounterRef iref3,
	       IntegerCounterRef iref4)
{
    VPNum myvp = Scheduler::GetVP();
    cprintf("Simple Destruction test with no destroyed objects\n");

    cprintf("  Deactiveting self (removed from active count)\n");
//    Scheduler::DeactivateSelf();

    cprintf("  To get our self in a bit of a known state we call"
	    " checkCleanup a few times\n");

    cprintf("  First call to checkCleanup\n");
    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
    cprintf("  Second call to checkCleanup\n");
    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
    cprintf("  Third call to checkCleanup\n");
    DREFGOBJ(TheCOSMgrRef)->checkCleanup();

    if (myvp == 0) {
	if (iref0!=0) {
	    cprintf("Calling Destroying iref0=%p\n",iref0);
	    DREF(iref0)->destroy();
	}

	if (iref1!=0) {
	    cprintf("Calling Destroying iref1=%p\n",iref1);
	    DREF(iref1)->destroy();
	}

	if (iref2!=0) {
	    cprintf("Calling Destroying iref2=%p\n",iref2);
	    DREF(iref2)->destroy();
	}

	if (iref3!=0) {
	    cprintf("Calling Destroying iref3=%p\n",iref3);
	    DREF(iref3)->destroy();
	}

	if (iref4!=0) {
	    cprintf("Calling Destroying iref4=%p\n",iref4);
	    DREF(iref4)->destroy();
	}
    }

// Take this thread out of the Active count
    Scheduler::DeactivateSelf();

    cprintf("  Simple Destruction test with one destroyed objects\n");
    cprintf("    First call to checkCleanup\n");
    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
    cprintf("    Second call to checkCleanup\n");
    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
    cprintf("    Third call to checkCleanup\n");
    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
    // Put this thread back into the active count
    Scheduler::ActivateSelf();

//    Scheduler::ActivateSelf();
    cprintf("  Reactivated self: Put this thread back in active count\n");

    return 1;
}

struct COTestArgs {
    VPNum numWorkers;
    uval numlocal;
    uval numnonrandom;
    uval numrandom;
    sval localCount;
    sval nonrandomCount;
    sval randomCount;
    sval simpleCount;
    BlockBarrier *bar;
    IntegerCounterRef iref0;
    IntegerCounterRef iref1;
    IntegerCounterRef iref2;
    IntegerCounterRef iref3;
    IntegerCounterRef iref4;
    IntegerCounterRef *sharedobjects;
};

enum COAccessType { NonRandom = 0, Random = 1 };

void
allocateCO(VPNum numWorkers, IntegerCounterRef *refs, uval numRefs,
	   COAccessType access, sval *count)
{
    IntegerCounterRef ref;
    uval j;
    uval32 randomState;
    VPNum myvp = Scheduler::GetVP();

    for (uval i = 0; i < numRefs; i++) {
	switch (i%3) {
	case 0:
            if (myvp==0) {
                SharedIntegerCounterPinned::Create(ref, count);
            } else {
                IntegerCounterReplicated::Create(ref, count);
            }
	    break;
	case 1:
	    SharedIntegerCounter::Create(ref, count);
	    break;
	case 2:
	    IntegerCounterReplicated::Create(ref, count);
	    break;
	}

        tassert(COSMgrObject::isRef((CORef)ref),
                err_printf("ERROR: vp=%ld isRef(%p) is false\n",myvp, ref));

        tassert(myvp == COSMgrObject::refToVP((CORef)ref),
                err_printf("ERROR: vp=%ld != refToVP(%p) = %ld\n",myvp,ref,
                COSMgrObject::refToVP((CORef)ref)));

	if (access == NonRandom) {
	    refs[i] = ref;
	} else {
	    do {
		j = BaseRandom::GetLC(&randomState) %
		    (numRefs * numWorkers * 10);
	    } while (!CompareAndStoreSynced((uval *)&refs[j], 0, (uval)ref));
//	    cprintf("      allocateCO %ld: %ld: objects[%ld]=%p\n",
//		    myvp, i, j, ref);
	}
    }
}

void
accessCO(VPNum numWorkers, IntegerCounterRef *refs, uval numRefs,
	 COAccessType access, sval *count)
{
//    VPNum myvp = Scheduler::GetVP();
    for (uval i = 0; i < numRefs; i++) {
	sval v=10;
	if (refs[i] != 0) {
//	    cprintf("      accessCO %ld, objects[%ld]=%p\n", myvp, i, refs[i]);
	    if (_FAILURE(DREF(refs[i])->value(v))) {
		cprintf("      OOPS: failed access on %p\n", refs[i]);
	    }
	    if (_FAILURE(DREF(refs[i])->inc())) {
		cprintf("    OOPS: failed access on %p\n", refs[i]);
	    }
	    if (_FAILURE(DREF(refs[i])->value(v))) {
		cprintf("      OOPS: failed access on %p\n", refs[i]);
	    }
	    if (_FAILURE(DREF(refs[i])->dec())) {
		cprintf("      OOPS: failed access on %p\n", refs[i]);
	    }
	    if (_FAILURE(DREF(refs[i])->value(v))) {
		cprintf("      OOPS: failed access on %p\n", refs[i]);
	    }
	    if (_FAILURE(DREF(refs[i])->inc())) {
		cprintf("      OOPS: failed access on %p\n", refs[i]);
	    }
	    if (_FAILURE(DREF(refs[i])->value(v))) {
		cprintf("      OOPS: failed access on %p\n", refs[i]);
	    }
	}
    }
}

void
deallocateCO(VPNum numWorkers, IntegerCounterRef *refs, uval numRefs,
	     COAccessType access, sval *count)
{
//    Scheduler::DeactivateSelf();
    IntegerCounterRef ref;
//    VPNum myvp = Scheduler::GetVP();

    for (uval i = 0; i < numRefs; i++) {
	ref=refs[i];
	if (ref != 0)	{
	    if (CompareAndStoreSynced((uval *)&refs[i], (uval)ref, 0)) {
//	      cprintf("      deallocateCO %ld, objects[%ld]=%p\n",
//		    myvp, i, refs[i]);
		if (_FAILURE(DREF(ref)->destroy())) {
		cprintf("      OOPS: attempted to delete %p more than once\n",
			refs[i]);
		}
	    }
	}

    }

//    Scheduler::ActivateSelf();
}

void
simpleTests(VPNum numWorkers, Barrier *bar, IntegerCounterRef *iref0,
	    IntegerCounterRef *iref1, IntegerCounterRef *iref2,
	    IntegerCounterRef *iref3, IntegerCounterRef *iref4,
	    sval *count)
{

    VPNum myvp = Scheduler::GetVP();
    sval v=10;

    if (myvp == 0) {
	pinnedCOTestSetup(iref0, count);
	pagedCOTestSetup(iref1, count);
//	pinnedEmbeddedRootCOTestSetup(iref2, count);
	*iref2=0;
	pagedReplicatedCOTestSetup(iref3, count);

        tassert(COSMgrObject::isRef((CORef)*iref0),
                err_printf("ERROR: vp=%ld isRef(%p) is false\n",myvp, *iref0));

        tassert(COSMgrObject::isRef((CORef)*iref1),
                err_printf("ERROR: vp=%ld isRef(%p) is false\n",myvp, *iref1));

        tassert(COSMgrObject::isRef((CORef)*iref3),
                err_printf("ERROR: vp=%ld isRef(%p) is false\n",myvp, *iref3));

	// staticCOTestSetup(iref4);
	*iref4=0;
    }

    bar->enter();

    tassert(0 == COSMgrObject::refToVP((CORef)*iref0),
            err_printf("ERROR: vp=%ld != refToVP(%p) = %ld\n",myvp, *iref0,
            COSMgrObject::refToVP((CORef)*iref0)));

    tassert(0 == COSMgrObject::refToVP((CORef)*iref1),
            err_printf("ERROR: vp=%ld != refToVP(%p) = %ld\n",myvp,*iref1,
                COSMgrObject::refToVP((CORef)*iref1)));

    tassert(0 == COSMgrObject::refToVP((CORef)*iref3),
            err_printf("ERROR: vp=%ld != refToVP(%p) = %ld\n",myvp,*iref3,
            COSMgrObject::refToVP((CORef)*iref3)));

    pinnedCOTest(*iref0);

    pagedCOTest(*iref1);
#if 0
    // See comments at class defintion
    pinnedEmbeddedRootCOTest(*iref2);
#endif /* #if 0 */
    pagedReplicatedCOTest(*iref3);

#if 0
    staticCOTest(*iref4);
#endif /* #if 0 */

    bar->enter();

    DREF(*iref0)->value(v);
    tassert(VPNum(v) == numWorkers,
	    err_printf("CO failure v=%ld is should be 1\n",v));

    DREF(*iref1)->value(v);
    tassert(VPNum(v) == numWorkers,
	    err_printf("CO failure v=%ld is should be 1\n",v));

#if 0
    DREF(*iref2)->value(v);
    tassert(VPNum(v) == numWorkers,
	    err_printf("CO failure v=%ld is should be 1\n",v));
#endif /* #if 0 */
    DREF(*iref3)->value(v);
    tassert(VPNum(v) == numWorkers,
	    err_printf("CO failure v=%ld is should be 1\n",v));

#if 0
    DREF(*iref4)->value(v);
    tassert(VPNum(v) == numWorkers,
	    err_printf("CO failure v=%ld is should be 1\n",v));
#endif /* #if 0 */
    simpleDestTest(*iref0, *iref1, *iref2, *iref3, *iref4);
}

void
COTestWorker(TestStructure *ts)
{
    VPNum myvp = Scheduler::GetVP();
    struct COTestArgs *args = (struct COTestArgs *)ts->ptr;
    IntegerCounterRef *objects;
    volatile sval *rCount;

//    breakpoint();

    cprintf("COTestWorker: vp=%ld\n",myvp);

#if 0
    // err_printf("\nStarting GC Daemon\n");
    ((COSMgrObject *)DREFGOBJ(TheCOSMgrRef))
	->startPeriodicGC(myvp);
#endif /* #if 0 */

    args->bar->enter();

    simpleTests(args->numWorkers, args->bar, &(args->iref0), &(args->iref1),
		&(args->iref2),	&(args->iref3), &(args->iref4),
		&(args->simpleCount));

    rCount=&(args->localCount);
    if (args->numlocal > 0) {
	cprintf("  COTestWorker %ld: local object test: numlocal=%ld\n",
		myvp, args->numlocal);
	IntegerCounterRef localobjects[args->numlocal];
	allocateCO(args->numWorkers, localobjects, args->numlocal, NonRandom,
		   (sval *)rCount);
	cprintf("    After allocate resourceCount=%ld\n",*rCount);
	accessCO(args->numWorkers, localobjects, args->numlocal, NonRandom,
		 (sval *)rCount);
	cprintf("    After access resourceCount=%ld\n",*rCount);
	deallocateCO(args->numWorkers, localobjects, args->numlocal, NonRandom,
		     (sval *)rCount);
	cprintf("    After deallocate resourceCount=%ld\n",*rCount);
#ifdef CLEANUP_DAEMON
	cprintf("    waiting for GC to cleanup rCount = %ld\n", *rCount);
#endif /* #ifdef CLEANUP_DAEMON */
	Scheduler::DeactivateSelf();
	while (*rCount != 0) {
#ifndef CLEANUP_DAEMON
	    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
#endif /* #ifndef CLEANUP_DAEMON */
	    Scheduler::DelayMicrosecs(1000000);
	}
	Scheduler::ActivateSelf();
    }

    rCount=&(args->nonrandomCount);
    objects = args->sharedobjects;
    if (args->numnonrandom > 0) {
	args->bar->enter();
	cprintf("  COTestWorker %ld: nonrandom shared object test: "
		"numnonrandom=%ld\n", myvp,
		args->numnonrandom);
	allocateCO(args->numWorkers, &(objects[myvp * args->numnonrandom]),
		   args->numnonrandom, NonRandom, (sval *)rCount);
	args->bar->enter();
	cprintf("    After allocate resourceCount=%ld\n", *rCount);
	accessCO(args->numWorkers, objects,
		 args->numnonrandom * args->numWorkers,
		 NonRandom, (sval *)rCount);
	args->bar->enter();
	cprintf("    After accessCO resourceCount=%ld\n", *rCount);
	deallocateCO(args->numWorkers, &(objects[myvp * args->numnonrandom]),
		     args->numnonrandom, NonRandom, (sval  *)rCount);
	cprintf("    After deallocate resourceCount=%ld\n",*rCount);

#ifdef CLEANUP_DAEMON
	cprintf("    waiting for GC to cleanup rCount = %ld\n", *rCount);
#endif /* #ifdef CLEANUP_DAEMON */

	Scheduler::DeactivateSelf();
	while (*rCount != 0) {
#ifndef CLEANUP_DAEMON
	    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
#endif /* #ifndef CLEANUP_DAEMON */
	    // Delay to make life more bareable on the simulator
	    Scheduler::DelayMicrosecs(1000000);
	}
	Scheduler::ActivateSelf();
    }

    rCount=&(args->randomCount);
    objects=&(args->sharedobjects[args->numnonrandom]);
    if (args->numrandom > 0) {
	cprintf("  COTestWorker %ld: random shared object test: "
		"random=%ld\n", myvp,
		args->numrandom);
	allocateCO(args->numWorkers, objects, args->numrandom, Random,
		   (sval *)rCount);
	cprintf("    After allocate resourceCount=%ld\n",*rCount);
	accessCO(args->numWorkers, &(objects[(myvp * args->numrandom * 10)]),
		 args->numrandom * 10, NonRandom, (sval *)rCount);
	cprintf("    After access resourceCount=%ld\n",*rCount);
	while (*rCount != 0) {
	    deallocateCO(args->numWorkers,
			 &(objects[(myvp * args->numrandom * 10)]),
			 args->numrandom * 10, NonRandom, (sval *)rCount);
	    Scheduler::DeactivateSelf();
#ifndef CLEANUP_DAEMON
	    DREFGOBJ(TheCOSMgrRef)->checkCleanup();
#endif /* #ifndef CLEANUP_DAEMON */
	    Scheduler::DelayMicrosecs(1000000);
	    Scheduler::ActivateSelf();
	}
    }
    cprintf("  COTestWork %ld: done!\n", myvp);
}

void
COTest(VPNum numVP)
{
    BlockBarrier bar(numVP);
    BlockBarrier tstBar(numVP);
    struct COTestArgs args;
    SysStatusProcessID pidrc;
    ProcessID myPID;

    pidrc = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(pidrc), err_printf("woops\n"));
    myPID = _SGETPID(pidrc);

    args.numWorkers    = numVP;
#if 0
    args.numlocal       = 0;
    args.numnonrandom   = 4;
    args.numrandom      = 0;
#else /* #if 0 */
    args.numlocal       = 21;
    args.numnonrandom   = 21;
    args.numrandom      = 21;
#endif /* #if 0 */
    args.localCount     = 0;
    args.nonrandomCount = 0;
    args.randomCount    = 0;
    args.simpleCount    = 0;
    args.bar            = &tstBar;

    args.sharedobjects = (IntegerCounterRef *)
	allocGlobal(sizeof(CORef) * numVP * (args.numnonrandom +
					     (args.numrandom * 10)));

    for (uval i=0; i<(args.numrandom*numVP*10); i++)
	args.sharedobjects[args.numnonrandom+i]=0;

    cprintf("\nMissHandling and Clustered Object tests myPID=%lx"
	    " NumVP=%ld\n",myPID, numVP);

//    breakpoint();
    TestStructure *ts = TestStructure::Create(
	numVP, 0/*size*/, 0/*iters*/,
	0/*test*/, 0/*misc*/, &args/*ptr*/, &bar);

    DoConcTest(numVP, SimpleThread::function(COTestWorker), ts);

    delete[] ts;
    freeGlobal((void *)args.sharedobjects,
	       sizeof(CORef) * numVP *
	       (args.numnonrandom + (args.numrandom * 10)));
}

// --------------------------------------------------------------------------
class TestHashData {
    uval key;
    uval data;
    BLock lck;
    static const uval INVALID = uval(~0);
public:
    uval getData() { return data; }
    void setData(uval d) { data = d; }
public:
    // needed to make it work with DHash
    inline uval isEmpty() { return key == INVALID; }
    inline uval getKey()  { return key; }
    inline void setKey(uval k)  { key = k; }
    inline void lock() { lck.acquire(); }
    inline void unlock() { lck. release(); }
    inline uval isLocked() {
	return lck.isLocked();
    }
    VPNum firstReplica() {
	return Scheduler::VPLimit;
    }
    VPNum nextReplica(VPNum vp) {
	passertMsg(0, "look");
	return 0;
    }
    void print() {
	cprintf("this %p, key %ld, data %ld\n", this, key, data);
    }
    void clearEmpty() {
	passertMsg(0, "what?");
    }
    void setEmpty(DHashTableBase::OpArg dummy) {
	key = INVALID;
    }
    void init() { lck.init(); }
    static uval Hash(uval key) { return key; }
};

class TestHashCO;
typedef TestHashCO** TestHashCORef;

class TestHashCO : public BaseObj {
    typedef MasterDHashTable<TestHashData,TestHashData,AllocGlobal,
	AllocGlobal> HashTable;
    HashTable hashTable;
public:
    typedef enum {FOUND, NOT_FOUND} AllocateStatus;

    DEFINE_GLOBAL_NEW(TestHashCO);
    virtual SysStatus findOrAddAndLock(uval k,  TestHashData **d,
				       AllocateStatus *st) {
	HashTable::AllocateStatus astat =
	    hashTable.findOrAllocateAndLock(k, d);
	if (astat == HashTable::FOUND) {
	    *st = FOUND;
	} else {
	    *st = NOT_FOUND;
	}
	return 0;
    }
    virtual SysStatus findAndLock(uval k, TestHashData **d) {
	*d = hashTable.findAndLock(k);
	return 0;
    }
    virtual SysStatus removeData(uval key) {
	hashTable.doEmpty(key, 0);
	return 0;
    }
    virtual SysStatus print() {
	hashTable.print();
	return 0;
    }
    static  SysStatus Create(TestHashCORef &ref) {
	TestHashCO* obj = new TestHashCO();
	if (obj) {
	    ref = (TestHashCORef) CObjRootSingleRep::Create(obj);
	    return 0;
	} else {
	    return _SERROR(2715, 0, ENOMEM);
	}
    }
    ~TestHashCO() {
	hashTable.cleanup();
    }
};

//TEMPLATEDHASHTABLE(TestHashData,AllocGlobal,TestHashData,AllocGlobal)
template class DHashTable<TestHashData,AllocGlobal>;
template class MasterDHashTable<TestHashData,TestHashData,AllocGlobal,
    AllocGlobal>;

void
hashTableTestWorker(TestStructure *ts)
{
    VPNum myvp = Scheduler::GetVP();
    TestHashCORef ref = (TestHashCORef)(ts->ptr);
    TestHashData *he;

    cprintf("%ld: testDhashTableWorker started: ref=%p\n", myvp, ref);

    ts->bar->enter();

    err_printf("At Start on vp=%ld localTable:\n", myvp);

    // Testing findAndLock when element is not there, and it has never
    // been there
    (void)DREF(ref)->findAndLock(0xabababab, &he);
    tassertMsg(he == NULL, "?");

    for(uval i = myvp*(ts->size); i < ((myvp*(ts->size))+(ts->size)); i++) {
	TestHashCO::AllocateStatus astat;
	(void)DREF(ref)->findOrAddAndLock(i, &he, &astat);
	tassertMsg(he, "ooops he=0\n");
	tassertMsg(he->getKey() == i, "no match i=%ld key=%ld\n", i,
		   he->getKey());
	tassertMsg(!(he->isEmpty()), "he is empty\n");
	tassertMsg(he->isLocked(), "he is not locked");
	if (astat == TestHashCO::FOUND) {
	    // here we could fill up other fields in the entry (do real work)
	} else {
	    tassertMsg(astat == TestHashCO::NOT_FOUND, "?");
	    he->setData(i);
	}
	he->unlock();
    }

    // The following, in a scenario where we have replicated tables,
    // ensures that the data would have multiple replicas now
    for(uval i = 0; i < (ts->misc)*(ts->size); i++) {
	TestHashCO::AllocateStatus astat;
	(void)DREF(ref)->findOrAddAndLock(i, &he, &astat);
	tassertMsg(he, "ooops he=0\n");
	tassertMsg(he->getKey() == i, "no match i=%ld key=%ld\n", i,
		   he->getKey());
	tassertMsg(!(he->isEmpty()), "he is empty\n");
	tassertMsg(he->isLocked(), "he is not locked");
	if (astat == TestHashCO::FOUND) {
	    // here we got something from the table
	} else {
	    tassertMsg(astat == TestHashCO::NOT_FOUND, "?");
	    he->setData(i);
	}
	he->unlock();
    }

    ts->bar->enter();

    if (myvp == 0) {
	// remove everything on its range
	for(uval i = myvp*(ts->size); i < ((myvp*(ts->size)) + (ts->size));
	    i++) {
	    (void)DREF(ref)->removeData(i);
	}
    }

    ts->bar->enter();

    // Testing findAndLock when element is not there now, but it has been
    // there before
    (void)DREF(ref)->findAndLock(0, &he);
    tassertMsg(he == NULL, "?");

    cprintf("%ld: testDHashTableWorker all done\n", myvp);
}

uval
hashTableTest(VPNum numVP)
{
    BlockBarrier bar(numVP);

    TestHashCORef ref = 0;

    cprintf("Doing DHashTable test:\n");

    TestHashCO::Create(ref);

    cprintf("   ref=%p\n", ref);

    cprintf("At Start MasterTable:\n");
    DREF(ref)->print();

    TestStructure *ts = TestStructure::Create(
	numVP, 10/*size*/, 0/*iters*/,
	0/*test*/, numVP/*misc*/, ref/*ptr*/, &bar);

    DoConcTest(numVP, SimpleThread::function(hashTableTestWorker), ts);

    cprintf("At End MasterTable:\n");
    DREF(ref)->print();

    delete[] ts;

    DREF(ref)->destroy();

    cprintf("All DHashTable test\n");
 
    return 0;
}

#include <CObjRepArbiterCallCounter.H>

class fib : public BaseObj{
public:
    // recursive calls to self using COS
    virtual SysStatus calcR(fib** myRef, uval num, uval* res, double fpparam){
        if(num < 1){
            *res = 1;
            return 0;
        }
        SysStatus r = DREF(myRef)->calcR(myRef, num - 1, res, fpparam);
        *res *= num;
        return r;
    }
    // doesn't use COS recursion
    virtual SysStatus calc(fib** myRef, uval num, uval* res, double fpparam){
        if(num < 1){
            cprintf("myRef is %lx; this %lx\n", (uval)myRef, (uval)this);
            *res = 1;
            return 0;
        }
        SysStatus r = calc(myRef, num - 1, res, fpparam);
        *res *= num;
        return r;
    }
    static fib** Create() {
	return reinterpret_cast<fib**>(CObjRootSingleRep::Create(new fib()));
    }
protected:
    DEFINE_GLOBAL_NEW(fib);
};

typedef fib** FibRef;

struct ArbiterTestRefs{
    cocccRef com1;
    cocccRef com2;
    FibRef fibber;
};

void arbiterTestWorker(TestStructure* ts){

    uval res;
    sval counts[256];
    VPNum myvp = Scheduler::GetVP();

    cocccRef com1 = (reinterpret_cast<ArbiterTestRefs*>(ts->ptr))->com1;
    cocccRef com2 = (reinterpret_cast<ArbiterTestRefs*>(ts->ptr))->com2;
    FibRef fibber = (reinterpret_cast<ArbiterTestRefs*>(ts->ptr))->fibber;

    cprintf("starting arbiter multi on %ld\n", myvp);
    ts->bar->enter();
    err_printf("At Start on vp=%ld localTable:\n", myvp);

    if(myvp == 0)
        DREF(com1)->captureTarget(reinterpret_cast<CORef>(fibber));

    DREF(fibber)->calc(fibber, 3, &res, 8.9932);
    DREF(com1)->getCallCount(counts);
    if(myvp == 0){
        cprintf("first call count (com1)\n");
        for(int i = 0; i < 256; i++)
            if(counts[i])
                cprintf("fnum %d: %ld\n", i, counts[i]);
    }
    ts->bar->enter();
    if(myvp == 0)
        DREF(com2)->captureTarget(reinterpret_cast<CORef>(fibber));
    ts->bar->enter();
    DREF(fibber)->calcR(fibber, 3, &res, 8.9932);
    DREF(com1)->getCallCount(counts);
    if(myvp == 0){
        cprintf("second call count (com1)\n");
        for(int i = 0; i < 256; i++)
            if(counts[i])
                cprintf("fnum %d: %ld\n", i, counts[i]);
        DREF(com2)->getCallCount(counts);
        cprintf("first call count (com2)\n");
        for(int i = 0; i < 256; i++)
            if(counts[i])
                cprintf("fnum %d: %ld\n", i, counts[i]);
    }

    ts->bar->enter();
    if(myvp == 0){
        DREF(com2)->releaseTarget();
        DREF(com1)->releaseTarget();
    }
}

uval
arbiterTest(VPNum numVP)
{
    BlockBarrier bar(numVP);

    cocccRef com1 = CORepArbiterCallCounter::Create();
    cocccRef com2 = CORepArbiterCallCounter::Create();
    FibRef fibber = fib::Create();

    ArbiterTestRefs ctr;
    ctr.com1 = com1;
    ctr.com2 = com2;
    ctr.fibber = fibber;

    cprintf("starting arbiter single-processor tests\n");
    uval res;
    sval counts[256];

    DREF(com1)->captureTarget(reinterpret_cast<CORef>(fibber));
    DREF(fibber)->calc(fibber, 3, &res, 8.9932);
    DREF(com1)->getCallCount(counts);
    cprintf("first call count (com1)\n");
    for(int i = 0; i < 256; i++)
        if(counts[i])
            cprintf("fnum %d: %ld\n", i, counts[i]);
    DREF(com2)->captureTarget(reinterpret_cast<CORef>(fibber));
    DREF(fibber)->calcR(fibber, 3, &res, 8.9932);
    DREF(com1)->getCallCount(counts);
    cprintf("second call count (com1)\n");
    for(int i = 0; i < 256; i++)
        if(counts[i])
            cprintf("fnum %d: %ld\n", i, counts[i]);
    DREF(com2)->getCallCount(counts);
    cprintf("first call count (com2)\n");
    for(int i = 0; i < 256; i++)
        if(counts[i])
            cprintf("fnum %d: %ld\n", i, counts[i]);
    DREF(com2)->releaseTarget();
    DREF(com1)->releaseTarget();

    TestStructure *ts = TestStructure::Create(
	numVP, 10/*size*/, 0/*iters*/,
	0/*test*/, numVP/*misc*/, &ctr/*ptr*/, &bar);

    cprintf("starting arbiter multi-processor tests\n");

    DoConcTest(numVP, SimpleThread::function(arbiterTestWorker), ts);

    delete[] ts;

    DREF(com1)->destroy();
    DREF(com2)->destroy();
    DREF(fibber)->destroy();

    cprintf("completed arbiter tests\n");
 
    return 0;
}

// --------------------------------------------------------------------------
uval
mhTest()
{
    cprintf("\nMissHandling and Clustered Object tests\n");

    // allocdeallocTest();

    // threadMarkerTest();

    COTest((DREFGOBJ(TheProcessRef)->ppCount()));

    hashTableTest((DREFGOBJ(TheProcessRef)->ppCount()));
    arbiterTest((DREFGOBJ(TheProcessRef)->ppCount()));

    return 1;
}
