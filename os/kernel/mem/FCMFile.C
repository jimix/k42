/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: FCMFile.C,v 1.4 2005/08/30 01:31:18 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared FCM services for mapping, unmapping,
 * getting/releasing for copy for FCM's attached to FR's (files).
 * **************************************************************************/
#include "kernIncs.H"
#include "FCMFile.H"
#include "mem/FCMDefaultMultiRep.H"
#include "mem/SyncService.H"
#include <trace/traceMem.h>
#include <trace/traceClustObj.h>

/* static */ SysStatus
FCMFile::CreateDefault(FCMRef &ref, FRRef frRefArg,
		       uval pageable, uval preFetchPages,
		       uval maxPages) {
    SysStatus rc;
    if (KernelInfo::ControlFlagIsSet(KernelInfo::USE_MULTI_REP_FCMS)) {
	rc = FCMDefaultMultiRep::Create(ref, frRefArg, pageable);
    } else {
	rc = FCMFile::Create(ref, frRefArg, pageable, preFetchPages,
			     maxPages);
    }
    return rc;
}

/* static */ SysStatus
FCMFile::Create(FCMRef &ref, FRRef frRefArg, uval pageable, 
		uval preFetchPages, uval maxPages)
{
    SysStatus rc;
    FCMFile *fcm;

    fcm = new FCMFile;
    if (fcm == NULL) return -1;

    rc = fcm->init(ref, frRefArg, PAGE_SIZE, pageable, 0, preFetchPages, 
		   maxPages);

#ifdef ENABLE_SYNCSERVICE
    /** @todo Orran - we shouldn't need to detect this. It looks like we're
     *        getting FRPlaceHolders backing FCMFiles, which cause problems
     *        when the SyncService calls fsync(). -jk */
    if (pageable) {
	DREFGOBJK(TheSyncServiceRef)->attachFCM(fcm->getRef());
    } else {
	tassertWrn(0, "Didn't attach FCMFile to SyncService - not pageable\n");
    }
#endif /* ENABLE_SYNCSERVICE */

    TraceOSMemFCMDefaultCreate((uval)ref);
    return rc;
}



/**
 * Go through all modified pages and unmap and write each one
 * Initial implementation syncronous but that's not good enough
 */
/* virtual */ SysStatus
FCMFile::fsync(uval force)
{
    SysStatus rc;
    PageDesc* pg;

    // FIXME: GET RID OF backedbyswap, whole benefit of this class
    if (backedBySwap) return 0;		// nothing to do

    // we sweep through the pagelist collecting up to listSize pages to
    // writeback, release locks do write, and start again if there is
    // more left

    //N.B. these lists are on the stack, so must not be too big
    const uval listSize = 32;
    uval paddrList[listSize];
    uval offsetList[listSize];
    uval i, numCollected;

    do {
	lock.acquire();

	numCollected = 0;
	pg = pageList.getFirst();
	while (pg) {
	    uval wasmapped = 0;
	    // if doingIO then is already being written back
	    if (pg->dirty && !pg->doingIO) {
		if (numCollected >= listSize) break;
		/* 
		 * FIXME: in the non-force case, we should not be
		 * paying the cost to unmap at all, we should only
		 * unmap when the app explicitly fsyncs...
		 *
		 * Linux semantics are actually don't unmap it.
		 * So... we could do the same, but currently we have
		 * an invarient in K42 that a page that is doing I/O 
		 * is not mapped. Its not clear which is better, since
		 * we avoid potential extra writes linux must do, since
		 * by not unmapping its not unsetting dirty bit in the 
		 * page tables.  
		 */
		if (pg->mapped) {
		    unmapPage(pg);
		    wasmapped = 1;
		} 
		/*
		 * if not forced, then write out only unmapped dirty pages
		 * else, write out all the dirty pages
		 */
		if ((wasmapped == 0) || force) {
		    pg->doingIO = PageDesc::SET;
		    DILMA_TRACE_PAGE_DIRTY(this,pg,-1);
		    pg->dirty = PageDesc::CLEAR;
		    paddrList[numCollected] = pg->paddr;
		    offsetList[numCollected] = pg->fileOffset;
		    //err_printf("Collected %lx/%lx - %ld\n", pg->paddr,
		    //   pg->fileOffset, numCollected);
		    numCollected++;
		}
	    }
	    pg = pageList.getNext(pg);
	}

	//err_printf("Collected %ld pages\n", numCollected);

	lock.release();

	for (i = 0; i < numCollected; i++) {
	    // err_printf("startput for %lx/%lx - %ld\n",
	    //             paddrList[i], offsetList[i], i);
	    /* 
	     * FIXME: we should queue up our IORestartRequest
	     * and resume the fsync when we get called back
	     */
	    rc = DREF(frRef)->startPutPage(paddrList[i], offsetList[i]);
	    // what should we do on errors; we need to at least remove doingIO
	    // flag and wake up anyone waiting
	    tassert( _SUCCESS(rc), err_printf("putpage failed\n"));
	}

	//err_printf("Done startputs: %lx %ld\n", pg, numCollected);
	/* if pg != NULL, then we didn't finish scanning the entire pagecache;
	 * we restart from the beginning, just to be safe since we release
	 * locked; we'll skip those that we already did i/o since their state
	 * has been changed
	 */
	tassert(pg == NULL || numCollected == listSize, err_printf("woops\n"));
    } while (pg != NULL);

    return 0;
}

SysStatus
FCMFile::doDestroy()
{
#ifdef ENABLE_SYNCSERVICE
    DREFGOBJK(TheSyncServiceRef)->detachFCM(getRef());
#endif /* ENABLE_SYNCSERVICE */
    return FCMDefault::doDestroy();
}
