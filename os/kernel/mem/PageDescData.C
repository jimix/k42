/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageDescData.C,v 1.7 2002/05/10 23:14:19 jappavoo Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Shared FCM services for mapping, unmapping,
 * getting/releasing for copy for FCM's attached to FR's (files).
 * **************************************************************************/
#include "kernIncs.H"
#include "PageDescData.H"
#include "PageFaultNotification.H"
#include <mem/CacheSync.H>

void
LocalPageDescData::notify(SysStatus rc=0, PageFaultNotification *skipFn=0)
{
    tassert(isLocked(), err_printf("trying to do a notify with lock\n"));
    PageFaultNotification *notf = pd.fn;
    pd.fn = 0;
    while (notf) {
	PageFaultNotification *nxt = notf->next;
	if (notf != skipFn) {
	    notf->setRC(rc);
	    notf->doNotification();
	}
	notf = nxt;
    }

}


void
LocalPageDescData::setEmpty(DHashTableBase::OpArg ptrEmptyArg)
{
    EmptyArg *ea = (EmptyArg *)ptrEmptyArg;

    tassert(isLocked(), err_printf("trying to set empty but not locked\n"));
    tassert(ea!=0, ;);

    if (ea->doNotify == 1) {
	notify(ea->rc, ea->skipFn);
    }
    empty = 1;
}


// common implementation for both locals and master
DHashTableBase::OpStatus
LocalPageDescData::doIOComplete(DHashTableBase::OpArg ioCompleteArgPtr)
{
    IOCompleteArg *ioCompleteArg =  (IOCompleteArg *)ioCompleteArgPtr;

    if(ioCompleteArg->setDirty != uval(-1)) {
	pd.dirty = ioCompleteArg->setDirty;
        if (ioCompleteArg->setDirty) {
            pd.cacheSynced = PageDesc::SET;
        }
    }
    pd.doingIO = PageDesc::CLEAR;
    // Note fn can be queue on locals or
    // master
    notify(ioCompleteArg->rc, ioCompleteArg->skipFn);

    return DHashTableBase::SUCCESS;
}


#if 0
// common implementation for both locals and master
DHashTableBase::OpStatus
LocalPageDescData::doPut(DHashTableBase::OpArg putArgPtr)
{
    PutArg *putArg =  (PutArg *)putArgPtr;

    if ( putArg->clearDirty ) {
	pd.dirty = PageDesc::CLEAR;
    }
    pd.doingIO = PageDesc::CLEAR;
    notify(putArg->rc, putArg->skipFn); // Note fn can be queue on locals or
                                          // master
    return DHashTableBase::SUCCESS;
}
#endif

DHashTableBase::OpStatus
LocalPageDescData::doSetPAddr(DHashTableBase::OpArg addr)
{
    uval paddr=(uval)addr;
    setPAddr(paddr);
    return DHashTableBase::SUCCESS;
}

DHashTableBase::OpStatus
LocalPageDescData::doSetPAddrAndIOComplete(DHashTableBase::OpArg addr)
{
    IOCompleteArg ioCompleteArg = { 1, //dirty=1 and cacheSync = 1
                                    0, //skipFn=0,
                                    0 }; //rc=0
                                    
    (void)doSetPAddr(addr);
    return doIOComplete((DHashTableBase::OpArg)&ioCompleteArg);
}

DHashTableBase::OpStatus
LocalPageDescData::doSetCacheSync(DHashTableBase::OpArg dummy)
{
    pd.cacheSynced = PageDesc::SET;
    return DHashTableBase::SUCCESS;
}

DHashTableBase::OpStatus
MasterPageDescData::doCacheSync(DHashTableBase::OpArg dummy)
{
    if (pd.cacheSynced == PageDesc::SET) {
	return DHashTableBase::FAILURE;
    } else {
	CacheSync(&pd);
	tassert(pd.cacheSynced == PageDesc::SET, ;);
	return DHashTableBase::SUCCESS;
    }
}

DHashTableBase::OpStatus
LocalPageDescData::doSetDirty(DHashTableBase::OpArg dummy)
{
    pd.dirty = PageDesc::SET;
    return DHashTableBase::SUCCESS;
}

DHashTableBase::OpStatus
MasterPageDescData::doDirty(DHashTableBase::OpArg dummy)
{
    if (pd.dirty == PageDesc::SET) {
	return DHashTableBase::FAILURE;
    } else {
	pd.dirty = PageDesc::SET;
	return DHashTableBase::SUCCESS;
    }
}

DHashTableBase::OpStatus
LocalPageDescData::doSetFreeAfterIO(DHashTableBase::OpArg dummy)
{
    setFreeAfterIO();
    return DHashTableBase::SUCCESS;
}

// Remember the ppset and mapped flag are only kept locally
DHashTableBase::OpStatus
LocalPageDescData::doUnmap(DHashTableBase::OpArg ppsetPtr)
{
    uval *ppset = (uval *)ppsetPtr;
    *ppset |= getPPSet();
    clearPPSet();
    clearMapped();
    return DHashTableBase::SUCCESS;
}

// Nolonger used as dirty bit is now maintained globally
// at the cost of an extra fault.
#if 0
// Remember the dirty bit is only kept locally
DHashTableBase::OpStatus
LocalPageDescData::doCheckDirty(DHashTableBase::OpArg dirtyPtr)
{
    uval *dirty = (uval *)dirtyPtr;
    *dirty |= isDirty();
    return DHashTableBase::SUCCESS;
}
#endif /* #if 0 */

DHashTableBase::OpStatus
LocalPageDescData::doSetDoingIO(DHashTableBase::OpArg dummy)
{
    pd.doingIO = PageDesc::SET;
    return DHashTableBase::SUCCESS;
}

#if 0
DHashTableBase::OpStatus
LocalPageDescData::doSetMapped(DHashTableBase::OpArg dummy)
{
    pd.cacheSynced = PageDesc::CLEAR;
    return DHashTableBase::SUCCESS;
}

DHashTableBase::OpStatus
MasterPageDescData::doSetMapped(DHashTableBase::OpArg dummy)
{

    if (pd.mapped == PageDesc::SET) {
	return DHashTableBase::FAILURE;
    } else {
	pd.mapped = PageDesc::SET;
	PageAllocatorKernPinned::setAccessed(pg->paddr);
	if (pd.free == PageDesc::SET) {
	    fl->dequeue(pd);
	    pg.free = PageDesc::CLEAR;
	}
    }
}
#endif /* #if 0 */
