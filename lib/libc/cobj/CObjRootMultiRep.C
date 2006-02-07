/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRootMultiRep.C,v 1.19 2005/03/02 20:21:53 jappavoo Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <alloc/MemoryMgrPrimitive.H>
#include "CObjRootMultiRep.H"

/* virtual */ VPSet
CObjRootMultiRepBase::getTransSet()
{
    return transSet;
}


/* virtual */ VPSet
CObjRootMultiRepBase::getVPCleanupSet()
{
    // We use the repSet to track the vp's on which reps were created on.
    // We then pass this set to the COSMgr to have it invoke cleanup
    // on those processors.  Note it is our responsibility to implement
    // the logic to ensure that cleanup calls do the right thing.
    return repSet;
}

/*virtual*/ CObjRep *
CObjRootMultiRepBase::getRepOnThisVP()
{
    VPNum vp = Scheduler::GetVP();

    AutoLock<BLock> al(&lock);	// locks now, unlocks on return
    return locked_getRep(vp);
}

inline void
CObjRootMultiRepBase::locked_addRep(VPNum vp, CObjRep *rep)
{
    transSet.addVP(vp);
    replistAdd(vp, rep);
}

// Search for the rep in the current mappings if it can not be found
// then create a rep and record a mapping for it
inline CObjRep *
CObjRootMultiRepBase::locked_getRep(VPNum vp)
{
    
    CObjRep *rep=locked_findRepOn(vp);

    if (rep == NULL) {
	// Could not find a rep so we now try and create a rep
	rep=createRep(vp);
	// FIXME: errors should be handled correctly
	// if (!rep) do error handleing.
	rep->setRoot(this);
	// record this vp as owning a rep in the rep set
	repSet.addVP(vp);
	// add it to the list of Reps
	locked_addRep(vp, rep);
    }
    
    return rep;
}


SysStatus
CObjRootMultiRepBase::installRep(CORef ref, VPNum vp, COSTransObject *rep)
{
    LTransEntry *lte=(LTransEntry *)ref;
    
    // Install the new representative in the Translation table of this vp.
    lte->setCO(rep);
    return 0;
}

SysStatus
CObjRootMultiRepBase::handleMiss(COSTransObject * &co, CORef ref,
				 uval methodNum)
{
    CObjRep *rep=(CObjRep *)NULL;
    VPNum myvp=Scheduler::GetVP();
    AutoLock<BLock> al(&lock);	// locks now, unlocks on return

    // Create representative if necessary
    rep = locked_getRep(myvp);

    // Install the new representative in the Translation table of this vp.
    installRep(ref, myvp, rep);

    //pass the pointer to the new representative back to the Object Translation
    // system so that the method for which the miss occurred can be invoked.
    co = rep;

    return 0;
}

/* virtual */ void *
CObjRootMultiRepBase::nextRep(void *curr, CObjRep *&rep)
{
    uval key = 0;
    
    return replistNext(curr,key,rep);
}

/* virtual */ void *
CObjRootMultiRepBase::nextRepVP(void *curr, VPNum &vp, CObjRep *&rep)
{
    return replistNext(curr,vp,rep);
}


// FIXME:  May later want to remove vpSet as it is just used to
//         avoid long searches of the replist.  If we replace the
//         replist with a more efficient data struct (hash table/array)
//         then we may want to just remove vpSet.
// Search for a rep for the specified vp if the cluster size is not 1
// we extend our search to look at other vps in the cluster which the
// specified vp belongs to
CObjRep *
CObjRootMultiRepBase::locked_findRepOn(VPNum vp)
{

    CObjRep *rep      = (CObjRep *)NULL;

    // To be on the safe side always hold lock before traversal.
    // Some traversals could be safe but then would make assumptions
    // about how the list is implemented.  So for the moment go
    // the easy way and assert the lock.
    _ASSERT_HELD(lock)
 
    // If there is a rep on the vp specified we can take the short way out
    if (transSet.isSet(vp)) {
	replistFind(vp, rep);
	tassert(rep!=NULL,
		err_printf("1: oops expected to find a rep for"
			   "vp=%ld\n", vp));
	return rep;
    }

    // If we could not explicity find a rep on the vp specified we
    // look to see if there is a rep in the cluster associated with
    // the vp in question.  (only do this when cluster size is not 1)
    // Note clustersize = 0 implies shared.
    if (clustersize != 1) {
	// if the clustersize is not one then see if there exists a
	// rep on any of the vps in the cluster which this vp belongs to
	VPSet intersection;
	intersection.addAllInUvalMask(COSMgr::clusterSetMask(vp,clustersize));
	intersection.intersect(transSet);
	if (!intersection.isEmpty()) {
	    // found a rep in the cluster which the vp belongs to
	    // record the mapping and return the rep
	    VPNum rvp = intersection.firstVP();
	    tassert(transSet.isSet(rvp), ; );
	    replistFind(rvp, rep);
	    tassert(rep!=NULL, err_printf("2: oops expected to find a rep"
					  "for vp=%ld\n", rvp));
	    // we now explicity establish the mapping for the vp to the rep
	    // for the cluster by adding it to the list and updating the vpset
	    locked_addRep(vp, rep);
	}
    }
    
    return rep;
}

/*
 *FIXME
 * can't use _ref to call reps - its already been swung to
 * point to the deleted object.
 */
#if 0
SysStatus
CObjRootMultiRepBase::cleanup()
{
    //invoke cleanup of representatives
    //We do this as a remote call so that resources
    //get freed to the appropriate processors
    uval  mask     = repmask;
    VPNum myvp     = Scheduler::GetVP();
    uval  localrep = mask & (1UL << myvp);
  
    mask &= ~( 1UL << myvp); 
    for (int vp = 0; mask != 0; mask >>= 1, vp++) {
	if (mask & 0x1) {
	    CleanupRep(vp, findRepOn(vp));
	}
    }
    // Clean local rep if present

    if (localrep) {
	findRepOn(myvp)->cleanup();
    }

    // Clean our selfs up.
    // FIXME : This is not quite right, Should ensure
    //         that this is done on the processor on which root was
    //         allocated.
    delete this;
    return 0;
}
#else

/*
 * CLUSTERED OBJECT MEMORY RECLAIMATION.
 *
 */
/* virtual */ SysStatus
CObjRootMultiRepBase::cleanup(CleanupCmd cmd)
{
    VPNum     myvp = Scheduler::GetVP();
    CObjRep   *rep  = NULL;
    SysStatus  rtn  = -1;
    
//    err_printf("CObjRootMultiRepBase::cleanup called, this=%p, cmd=%d\n",
//	       this, cmd);

    if (cmd == STARTCLEANUP) {
	myRef = 0;
	rtn = 1;
    }
    
    if (myRef == 0) {
	tassert(repSet.isSet(myvp),
		err_printf("cleanup called on vp=%ld but it is not in repSet\n",
			   myvp));
	// Don't need to do this locked as no one can be modifying the
	// rep list now.
	replistFind(myvp, rep);
	tassert(rep!=NULL,
		err_printf("oops expected to find a rep for vp=%ld\n", myvp));
	rep->cleanup();
	if (repSet.atomicRemoveVPAndTestEmpty(myvp)) {
	    // FIXME:  May want to move this up into per vp action but will
	    //         Then have to use locking.
	    while (replistRemoveHead(rep));
	    delete this;
	}
	rtn = 1;
    }
    return rtn;

}
#endif
