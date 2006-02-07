#ifndef __FCMPARTITIONED_TRIVIAL_H_
#define __FCMPARTITIONED_TRIVIAL_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPartitionedTrivial.H,v 1.24 2004/10/20 18:10:29 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *****************************************************************************/
#include "mem/FCM.H"
#include <misc/SSACSimplePartitionedArray.H>
#include "FCMCache.H"

class TestSwitch;

class FCMPartitionedTrivial : public FCM {
    friend class TestSwitch;

    class FCMPartitionedTrivialRoot : public CObjRootMultiRep {
    public:
	SSACSimplePartitionedArrayRoot SSACSPARoot;
	SSACSimplePartitionedArrayRoot *_SSACSimplePartitionedArrayRoot()
	    {
	      return &SSACSPARoot;
	    }

	typedef BLock LockType;
	LockType      lock;	 //Lock only used to serialize attachRegion
                                 //To protect assignment of reg and pm
	RegionRef reg;
	PMRef     pm;
	FRRef     frRef;

	SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
			       AccessMode::mode accessMode);

	FCMPartitionedTrivialRoot(const uval indexPartSize,
				  const uval numHashQsPerRep,
				  const uval assoc);
	FCMPartitionedTrivialRoot(const uval indexPartSize,
				  const uval numHashQsPerRep, const uval assoc,
				  RepRef ref,
				  CObjRoot::InstallDirective idir);

	CObjRep *createRep(VPNum vp);

#if 1 || SERIOUS_KLUDGE_FOR_PAPER // detachRegion NYI but I need to free memory
	static SysStatus
	CleanupMsgHandler(uval repUval)
	{
	    FCMPartitionedTrivial *const theRep =
		(FCMPartitionedTrivial *) repUval;
	    delete theRep->cache;
	    theRep->cache = 0;
	    return 0;
	}

	static SysStatus
	CleanupRep(VPNum theVP, FCMPartitionedTrivial *rep)
	{
	    SysStatus retvalue;
	    retvalue = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
					   SysTypes::DSPID(0, theVP),
					   CleanupMsgHandler, uval(rep));
	    return(retvalue);
	}

	SysStatus freeCacheKludge() {
	    passert(0, err_printf("FIXME!!!!!\n"));
#if 0
	    //invoke cleanup of representatives
	    //We do this as a remote call so that resources
	    //get freed to the appropriate processors
	    uval mask=repmask;
	    uval myvp=Scheduler::GetVP();
	    uval localrep=mask & (1UL << myvp);

	    lockReps();
	    mask &= ~( 1UL << myvp);
	    for (int vp=0; mask!=0;mask>>=1,vp++) {
		if (mask & 0x1) {
		    //	    RFUNC0(vp,_ref,CObjRepBase::cleanup);
		    CleanupRep(vp,(FCMPartitionedTrivial *)findRepOn(vp));
		}
	    }
	    // Clean local rep if present

	    if (localrep) {
		//	DREF(_ref)->cleanup();
		CleanupMsgHandler(uval(findRepOn(myvp)));
	    }
	    unlockReps();
#endif /* #if 0 */
	    return 0;
	}
#endif /* #if 1 || SERIOUS_KLUDGE_FOR_PAPER // ... */

	DEFINE_GLOBALPADDED_NEW(FCMPartitionedTrivialRoot);
    };

    friend class FCMPartitionedTrivial::FCMPartitionedTrivialRoot;
    FCMPartitionedTrivialRoot *root()
	{return((FCMPartitionedTrivialRoot *)myRoot);}

    SSACSimplePartitionedArray<FCMCacheEntry<FCMCacheId>,FCMCacheId,
                               FCMPartitionedTrivial> *cache;
    SSACSimplePartitionedArray<FCMCacheEntry<FCMCacheId>,FCMCacheId,
	                       FCMPartitionedTrivial>
    *_SSACSimplePartitionedArray() {return(cache);}

    friend class SSACSimplePartitionedArray<FCMCacheEntry<FCMCacheId>,FCMCacheId,
                                            FCMPartitionedTrivial>;


    FCMPartitionedTrivial(const uval indexPartSize, const uval numHashQsPerRep,
			  const uval assoc)
	: cache(new
		SSACSimplePartitionedArray<FCMCacheEntry<FCMCacheId>,FCMCacheId,
		FCMPartitionedTrivial>(indexPartSize, numHashQsPerRep, assoc))
	{
	    /* empty body */
	}

    SysStatus loadCacheEntry(FCMCacheEntry<FCMCacheId> *ce);

    SysStatus saveCacheEntry(FCMCacheEntry<FCMCacheId> *ce);

    SysStatus insertPageDesc(PageDesc *pageDesc);

    SysStatus createRepOn(uval vp);

    static SysStatus createRepOnMsgHandler(uval refUval);

    // Used by createRepOnMsgHandler to cause rep to be created
    virtual SysStatus init() { return 1;}

protected:

    // called by FCMCRW, returns a locked page if it exists
    // if value returned > 0, then async == 0 done, < 0 error
    virtual SysStatusUval getPage(uval fileOffset, void *&ptr,
				  PageFaultNotification *fn) {return -1;}

    // unlocked previously gotton page
    virtual SysStatus releasePage(uval fileOffset, uval dirty=0) 
        {
            tassertMsg(dirty==0, "NYI");
            return(-1);
        }

public:
    static SysStatus Create(FCMRef &ref, const uval regionSize,
			    const uval numHashQsPerRep,
			    const uval assoc);
    static SysStatus Create(CObjRoot * &root, FCMRef ref, const uval regionSize,
			    const uval numHashQsPerRep, const uval assoc);

    virtual SysStatusUval mapPage(uval fileOffset, uval regionVaddr,
				  uval regionSize,
				  AccessMode::pageFaultInfo pfinfo,
				  uval vaddr, AccessMode::mode access,
				  HATRef hat, VPNum vp,
				  RegionRef reg, uval firstAccessOnPP,
				  PageFaultNotification *fn);



    DEFINE_LOCALSTRICT_NEW(FCMPartitionedTrivial);

    // attach to the FCM to map (possibly) part of it
    virtual SysStatus attachRegion(RegionRef regRef, PMRef pmRef,
				   AccessMode::mode accessMode);

    virtual SysStatus detachRegion(RegionRef regRef) {
	// FIXME: temp kludge to free memory
	return COGLOBAL(freeCacheKludge());
    }

    virtual SysStatus destroy() {return(-1);}

    // when an fr/fcm pair is build externally, the fr must connect
    virtual SysStatus attachFR(FRRef frRefArg){
	tassert(COGLOBAL(frRef) == NULL, err_printf("already attached\n"));
	COGLOBAL(frRef) = frRefArg;
	return 0;
    }

    virtual SysStatus getFRRef(FRRef& frRefArg) {
	frRefArg = COGLOBAL(frRef); return 0;}

    virtual SysStatusUval isNotInUse() {return(0);}

    virtual SysStatusUval isEmpty() { return 0; }

    virtual SysStatus discardCachedPages() {
	passertMsg(0, "NYI\n");
	return 0;
    }

    // non zero if this fcm supports shared segments
    virtual SysStatus sharedSegments() {
	return 0;
    }

    // used for debugging
    virtual SysStatus printStatus(uval kind);
};

#endif /* #ifndef __FCMPARTITIONED_TRIVIAL_H_ */
