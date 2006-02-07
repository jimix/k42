/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMPartitionedTrivial.C,v 1.22 2004/10/08 21:40:08 jk Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include <misc/SSACSimplePartitionedArray.C>
#include "mem/PageAllocatorKernPinned.H"
#include "mem/PageDesc.H"
#include "mem/FCMPartitionedTrivial.H"
#include "mem/Region.H"
#include "mem/PM.H"
#include <trace/traceMem.h>
#include "mem/PageCopy.H"
#include "mem/PerfStats.H"

template class FCMCacheEntry<FCMCacheId>;
template class SSAC<FCMCacheEntry<FCMCacheId>, FCMCacheId, FCMPartitionedTrivial>;
template class SSACSimplePartitionedArray<FCMCacheEntry<FCMCacheId>, FCMCacheId,
                                    FCMPartitionedTrivial>;


FCMPartitionedTrivial::FCMPartitionedTrivialRoot
::FCMPartitionedTrivialRoot(const uval partsz,
			    const uval numhashqs, const uval assoc)
    : SSACSPARoot(partsz, numhashqs, assoc)
{
    /* empty body */
}

FCMPartitionedTrivial::FCMPartitionedTrivialRoot
::FCMPartitionedTrivialRoot(const uval partsz,
			    const uval numhashqs, const uval assoc, RepRef ref,
			    CObjRoot::InstallDirective idir)
    : CObjRootMultiRep(ref, 1, idir), SSACSPARoot(partsz, numhashqs, assoc)
{
    /* empty body */
}

CObjRep *
FCMPartitionedTrivial::FCMPartitionedTrivialRoot::createRep(VPNum vp)
{
    CObjRep *rep=(CObjRep *)new FCMPartitionedTrivial(
	SSACSPARoot.indexPartitionSize,	SSACSPARoot.numHashQsPerRep,
	SSACSPARoot.associativity);

    //err_printf("FCMPartitionedTrivialRoot::createRep()"
	//       ": vp=%d  New rep created rep=%p for ref=%p\n",
	//       Scheduler::GetVP(), rep, _ref);
    return rep;
}

/* static */ SysStatus
FCMPartitionedTrivial::Create(FCMRef &ref, const uval regionSize,
			      const uval numHashQsPerRep, const uval assoc)
{
    FCMPartitionedTrivialRoot *newFCMRoot;

    // FIXME:  Quick Kludge
    tassert(((regionSize >> LOG_PAGE_SIZE) % NUMPROC) == 0,
	    err_printf("Only supports paritions which are integral with respect"
		       " to number of current processors=%ld\n", NUMPROC));

    // Calculate the partition size given the regionsize
    // Assumes that it is expressed in an integral of PAGE_SIZE
    uval partSize = (regionSize >> LOG_PAGE_SIZE) / NUMPROC;

#if _DO_ERR_PRINTF
    err_printf("FCMPartitionedTrivial::Create regionSize=%ld yeilding partition"
               "size in pages of %ld for %ld processors and numHashQsPerRep"
	       "=%ld assoc=%ld\n", regionSize, partSize, NUMPROC,
	       numHashQsPerRep, assoc);
#endif /* #if _DO_ERR_PRINTF */

    newFCMRoot = new FCMPartitionedTrivialRoot(partSize, numHashQsPerRep, assoc);
    if (newFCMRoot == NULL) return -1;

    newFCMRoot->pm      = 0;
    newFCMRoot->reg     = 0;

    ref = (FCMRef)newFCMRoot->getRef();
    TraceOSMemFCMPartitionedTrivialCreate((uval)ref);

    return 0;
}

/* static */ SysStatus
FCMPartitionedTrivial::Create(CObjRoot * &root, FCMRef ref, const uval
	regionSize, const uval numHashQsPerRep, const uval assoc)
{
    FCMPartitionedTrivialRoot *newFCMRoot;

    // FIXME:  Quick Kludge
    tassert(((regionSize >> LOG_PAGE_SIZE) % NUMPROC) == 0,
	    err_printf("Only supports paritions which are integral with respect"
		       " to number of current processors=%ld\n", NUMPROC));

    // Calculate the partition size given the regionsize
    // Assumes that it is expressed in an integral of PAGE_SIZE
    uval partSize = (regionSize >> LOG_PAGE_SIZE) / NUMPROC;

#if _DO_ERR_PRINTF
    err_printf("FCMPartitionedTrivial::Create regionSize=%ld yeilding partition"
               "size in pages of %ld for %ld processors and numHashQsPerRep"
	       "=%ld assoc=%ld\n", regionSize, partSize, NUMPROC,
	       numHashQsPerRep, assoc);
#endif /* #if _DO_ERR_PRINTF */

    newFCMRoot = new FCMPartitionedTrivialRoot(partSize, numHashQsPerRep, assoc,
					       (RepRef)ref,
					       CObjRoot::skipInstall);
    if (newFCMRoot == NULL) return -1;

    newFCMRoot->pm      = 0;
    newFCMRoot->reg     = 0;

    root = newFCMRoot;

    return 0;
}

/* static */ SysStatus
FCMPartitionedTrivial::createRepOnMsgHandler(uval refUval)
{
    //FIXME: implement this correctly
#if 1
    FCMPartitionedTrivial **ref=(FCMPartitionedTrivial **)refUval;
    DREF(ref)->init();
    return 1;
#else /* #if 1 */
    FCMPartitionedTrivialRoot *root=(FCMPartitionedTrivialRoot *)refUval;
    root->getRepOnThisVP();
    return 1;
#endif /* #if 1 */
}

SysStatus
FCMPartitionedTrivial::createRepOn(uval vp)
{
    SysStatus rc;
    rc=MPMsgMgr::SendSyncUval(Scheduler::GetEnabledMsgMgr(),
			      SysTypes::DSPID(0, vp),
			      createRepOnMsgHandler,
			      uval(COGLOBAL(getRef())),
			      rc);
    tassert(_SUCCESS(rc), err_printf("failure in remote call to "
				     "createRepOnMsgHandler\n"));
    return rc;
}

/* virtual */SysStatusUval
FCMPartitionedTrivial::mapPage(uval offset,
		       uval regionVaddr,
		       uval regionSize,
		       AccessMode::pageFaultInfo pfinfo,
		       uval vaddr, AccessMode::mode access,
		       HATRef hat, VPNum vp,
		       RegionRef /*reg*/, uval /*firstAccessOnPP*/,
		       PageFaultNotification * /*fn*/)
{
    SysStatus rc, putbackRc;
    ScopeTime timer(MapPageTimer);
    FCMCacheEntry<FCMCacheId> *entry;

    tassert(COGLOBAL(pm) != 0, err_printf("FCMPartitionedTrivial: no pm\n"));

    offset += vaddr - regionVaddr;

//    breakpoint();

#define _PROFILING 0
#if _PROFILING
    static SysTime getMax = 0;
    SysTime getStart = Scheduler::SysTimeNow();
#endif /* #if _PROFILING */
    rc = cache->getAndLock(this, offset, entry);
#if _PROFILING
    SysTime getTotal = Scheduler::SysTimeNow() - getStart;
    if (getTotal > getMax) {
	getMax = getTotal;
	err_printf("\n\n>>>>>>>>>>> NEW get MAX: %lld <<<<<<<<<<\n", getMax);
    }
#endif /* #if _PROFILING */

    tassert(_SUCCESS(rc),
	    err_printf("No free entry found: Implement retry logic\n"));

    rc = DREF(hat)->mapPage(entry->paddr(), vaddr, PAGE_SIZE,
			    pfinfo, access, vp, 1);

    tassert(_SUCCESS(rc), err_printf("OOPS HAT::mapPage failed\n"));

    // FIXME: Go back and figure out what is up with rc from HAT see
    //        check in ProcessReplicated.C: 215 which looks for
    //        specific return values to determine if the fault
    //        was handled in-core.  For the moment we just
    //        pass the rc fron the hat::mapPage call directly
    putbackRc = cache->putbackAndUnlock(this, entry);
    tassert(_SUCCESS(putbackRc),
	    err_printf("OOPS putbackAndUnlocked failed!!!\n"));

    return rc;
}

/* virtual */SysStatus
FCMPartitionedTrivial::attachRegion(RegionRef regRef, PMRef pmRef,
				    AccessMode::mode accessMode)
{
    return COGLOBAL(attachRegion(regRef, pmRef, accessMode));
}

SysStatus
FCMPartitionedTrivial::FCMPartitionedTrivialRoot::attachRegion(
    RegionRef regRef, PMRef pmRef, AccessMode::mode accessMode)
{
    AutoLock<LockType> al(lock); // locks now, unlocks on return

#if _DO_ERR_PRINTF
    err_printf("FCMPartitionedTrivial::attachRegion(): reg=%p pm=%p regRef=%p "
	       "pmRef=%p\n", reg, pm, regRef, pmRef);
#endif /* #if _DO_ERR_PRINTF */
    tassert(reg == 0,
	    err_printf("FCMPartitionedTrivial second attach\n"));
    reg = regRef;
    pm  = pmRef;
    return 0;
}


SysStatus
FCMPartitionedTrivial::loadCacheEntry(FCMCacheEntry<FCMCacheId> *ce)
{
    SysStatus rc;
    uval tmp;

    if (ce->paddr() == 0) {
	//err_printf("X");
	// no page allocated for this offset; allocate it
	// FIXME: DANGER: This needs to be looked at carefully as it could
	//        cause a dead lock with the pm
	// MARK non pageable to avoid blocking in PM
	rc = DREF(COGLOBAL(pm))->allocPages(getRef(), tmp, PAGE_SIZE, 
					    0 /* nonpageable */);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
	PageCopy::Memset0((void *)tmp, PAGE_SIZE);
	ce->setPaddr(PageAllocatorKernPinned::virtToReal(tmp));
    } else {
	// FIXME remove assert -- only here for testing
	tassert(0, ;);
	err_printf("x");
	tmp=PageAllocatorKernPinned::realToVirt(ce->paddr());
	PageCopy::Memset0((void *)tmp, PAGE_SIZE);
    }
    return 1;
}

SysStatus
FCMPartitionedTrivial::saveCacheEntry(FCMCacheEntry<FCMCacheId> *ce)
{
    err_printf("FCMPartitionedTrivial::saveCacheEntry(): nop\n");
    return 1;
}

SysStatus
FCMPartitionedTrivial::insertPageDesc(PageDesc *pageDesc)
{
    SysStatus rc = 0;
    FCMCacheEntry<FCMCacheId> *ce = 0;

    rc = cache->insertPageDesc(this, pageDesc->fileOffset, ce);
    tassert(_SUCCESS(rc) && ce,
	    err_printf("[dcofcm test] should get a CacheEntry.\n"));
    ce->setPD(*pageDesc);

    return rc;
}

SysStatus
FCMPartitionedTrivial::printStatus(uval kind)
{
    switch (kind) {
    case PendingFaults:

	break;
    default:
	err_printf("kind %ld not known in printStatus\n", kind);
    }
    return 0;
}

