/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MemTrans.C,v 1.40 2004/07/11 21:59:24 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of basic share-memory transport (SMT)
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "MemTrans.H"
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubFRComputation.H>
#include <stub/StubProcessClient.H>
#include <stub/StubRegionDefault.H>
#include <sys/ProcessWrapper.H>
#include <sys/ProcessSet.H>
#include <mem/Access.H>
#include <cobj/ObjectRefs.H>
#include <meta/MetaProcessClient.H>
#include <misc/AutoList.I>
volatile uval allocDump =0;

/* virtual */ void
MemTrans::MTEvents::recvConnection(MemTransRef mtr, XHandle otherMT)
{
    other = otherMT;
}

/* virtual */ SysStatus
MemTrans::MTEvents::allocRing(MemTransRef mtr, XHandle otherMT)
{
    other = otherMT;
    return 0;
}

/* virtual */ uval
MemTrans::MTEvents::pokeRing(MemTransRef mtr, XHandle otherMT)
{
    uval val;
    uval ret = 0;
    while (_SUCCESS(DREF(mtr)->consumeFromRing(otherMT,val))) {
	DREF(mtr)->freePage(val, 0);
	ret = 1;
    }
    return ret;
}


/* define this because we may not have libc to define it */
#define abs(n) (((n) < 0) ? (-(n)) : (n))

#if 0
#define dprintf(args...) if (debug) err_printf(args);
#else /* #if 0 */
#define dprintf(args...)
#endif /* #if 0 */

/* virtual */ SysStatus
MemTrans::giveAccessSetClientData(ObjectHandle &oh,
				  ProcessID toProcID,
				  AccessRights match,
				  AccessRights nomatch,
				  TypeID type)
{
    SysStatus rc;
    ClientData *clientData = new ClientData(toProcID);
    rc =  giveAccessInternal(oh, toProcID, match, nomatch,
			     type, (uval)clientData);
    if (_FAILURE(rc)) {
	delete clientData;
    }

    // Remember the process binding
    BaseProcessRef pref;

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(toProcID,pref);

    _IF_FAILURE_RET_VERBOSE(rc);

    XHandle remote = oh.xhandle();

    //Add ref count for xhandle
    incRefCount();

    rc = DREF(pref)->addSMT(getRef(), remote, key);

    _IF_FAILURE_RET(rc);

    clientData->givenOH = oh;
    cdHash.add(toProcID, clientData);
    dprintf("%s: add xh: %lx -> %ld\n",__func__,oh.xhandle(),toProcID);
    return rc;
}

void
MemTrans::ClientData::init(ObjectHandle FR, ObjectHandle MT,
			   uval base, uval size)
{
    remoteFR.setOH(FR);
    remoteMT.setOH(MT);
    remoteBase = base;
    remoteSize = size;
    controlRing = NULL;
}



// A cludge to type cheat XBaseObj clientData to ClientData
inline /* static */
MemTrans::ClientData* MemTrans::clnt(XHandle xhandle) {
    MemTrans::ClientData* retvalue;
    retvalue = (MemTrans::ClientData *)
	(XHandleTrans::GetClientData(xhandle));
    return retvalue;
}


/* virtual */ SysStatus
MemTrans::incRefCount()
{
    uval count;
    do {
	count = refCount;
	dprintf("Grabbing object: %lx refcount: %ld\n",getRef(),count);
	if (count==~0ULL) {
	    return _SERROR(1999, 0, EBUSY);
	}
    } while (!CompareAndStoreSynced(&refCount, count, count+1));
    return 0;
}

/*static*/ void
MemTrans::ClassInit(VPNum vp)
{
    if (vp!=0) return;

    MetaProcessClient::
	createXHandle((ObjRef)GOBJ(TheProcessRef),
		      _KERNEL_PID,
		      MetaObj::controlAccess|MetaProcessClient::destroy
		      |MetaProcessClient::search |MetaObj::globalHandle,
		      MetaProcessClient::search);




    createLock.init();
}

/* virtual */ SysStatus
MemTrans::init(ProcessID partner, XHandle &remoteX,
	       uval size, MTEvents *handler)
{
    debug = 0;
    SysStatus rc = 0;
    CObjRootSingleRep::Create(this);
    uval addr = 0;
    ObjectHandle myFRInt;
    ObjectHandle localOH;
    ObjectHandle remoteOH;
    localSize = size;

    //Ref count is 1 --- for whoever created this object
    refCount = 1;

    allocLock.init();

    cbs = handler;

    // Lock out allocations until we're done initialization
    AutoLock<LockType> al(&allocLock);

    // Give other process access to this MT
    rc = giveAccessByServer(localOH, partner);

    _IF_FAILURE_RET(rc);

    remoteX = localOH.xhandle();


    usedPages.append(new PageUse(localSize/PAGE_SIZE, 0));

    // Create FR for local-side memory area (that will be exported)
    rc = StubFRComputation::_Create(myFRInt);

    _IF_FAILURE_RET_VERBOSE(rc);

    localFR.setOH(myFRInt);

    rc = StubRegionDefault::_CreateFixedLenExt(
	localBase, localSize, 0, myFRInt, 0, AccessMode::writeUserWriteSup, 0,
	RegionType::K42Region);

    _IF_FAILURE_RET_VERBOSE(rc);

    // Create an OH to a global object in the other process and
    // look for a matching MemTrans there

    // But, if this is the kernel, we never attempt this ---
    // Can't trust the other process to not block us on this
    // So we always wait for the other process to call _swapHandle

    if (DREFGOBJ(TheProcessRef)->getPID() == _KERNEL_PID) {
	return 0;
    }

    ObjectHandle SMTDB;
    SMTDB.initWithPID(partner,XHANDLE_MAKE_NOSEQNO(CObjGlobals::ProcessIndex));

    StubProcessClient stubPC(StubObj::UNINITIALIZED);

    stubPC.setOH(SMTDB);

    rc = stubPC._getMemTrans(remoteOH, key);

    if (_SUCCESS(rc)) {
	StubMemTrans remote(StubObj::UNINITIALIZED);
	remote.setOH(remoteOH);


	uval remoteSize;
	ObjectHandle frOH;

	// Give access to this area to the other process
	// FIXME: give access for read only
	ObjectHandle myFRExt;
	rc = localFR._giveAccess(myFRExt, partner);

	_IF_FAILURE_RET_VERBOSE(rc);


	// Give remote process OH's to our FR and MT

	rc = remote._swapHandle(localOH, myFRExt, localSize,
				remoteSize, frOH);

	_IF_FAILURE_RET_VERBOSE(rc);

	rc = StubRegionDefault::_CreateFixedLenExt(
	    addr, remoteSize, 0, frOH, 0, AccessMode::writeUserWriteSup, 0,
	    RegionType::K42Region);

	_IF_FAILURE_RET_VERBOSE(rc);

	ClientData *cd = clnt(remoteX);
	cd->init(frOH, remoteOH, addr, remoteSize);
	remote._completeInit();

    }


    return 0;
}


/* virtual */ SysStatus
MemTrans::_swapHandle(ObjectHandle callerMT, //Caller's MT
		      ObjectHandle callerFR, //Caller's FR
		      uval callerSize,	     //Size of region
		      uval& sizeRemote,
		      ObjectHandle &remoteFR,
		      __XHANDLE xhandle,
		      __CALLER_PID pid)
{
    ClientData *cd = clnt(xhandle);
    tassertMsg(cd,"No clientData defined\n");
    uval addr=0;
    SysStatus rc = 0;

    rc = StubRegionDefault::_CreateFixedLenExt(
	addr, callerSize, 0, callerFR, 0, AccessMode::writeUserWriteSup, 0,
	RegionType::K42Region);

    ObjectHandle frOH;
    if (_SUCCESS(rc)) {
	cd->init(callerFR, callerMT, addr, callerSize);

	rc = localFR._giveAccess(frOH, pid);

    } else {
	return rc;
    }


    remoteFR = frOH;
    sizeRemote = callerSize;


    return rc;
}

/* virtual */ SysStatus
MemTrans::_completeInit(__XHANDLE xhandle)
{
    tassertMsg({ ClientData *cd = clnt(xhandle); cd ;},
	       "No clientData defined\n");

    if (cbs) {
	cbs->recvConnection(getRef(),xhandle);
    }


    return 0;
}

/* virtual */ void*
MemTrans::remotePtr(uval offset, XHandle xh)
{
    ClientData *cd = clnt(xh);
    if (offset >= cd->remoteSize) {
	return NULL;
    }
    return (void*)(offset + cd->remoteBase);
}

/* virtual */ SysStatus
MemTrans::CreateObjCache(ObjCache* &oc, uval objSize)
{
    PageSource *ps = new PageSource(getRef());
    oc = new ObjCache(objSize, ps);
    return 0;
}


/* virtual */ SysStatusUval
MemTrans::_allocRing(uval localLoc, uval &remoteLoc, __XHANDLE xhandle)
{
    //Note localOffset is caller's local, remoteOffset is caller's remote
    uval size =512;
    ControlRing *r = __allocRing(size);

    if (!r) {
	return _SERROR(1946, 0, ENOMEM);
    }
    r->cd = clnt(xhandle);
    r->remote = localLoc;
    remoteLoc = localOffset((uval)r);

    const ControlRing *remote = remoteAddr(r->cd->remoteBase, r->remote);

    r->highMark  = remote->highMark;
    if (r->highMark > r->size) {
	r->highMark = r->size;
    }

    r->lowMark  = remote->lowMark;
    if (r->lowMark > r->size) {
	r->lowMark = r->size;
    }

    // Let the callback see if this should go through.
    if (cbs) {
	SysStatus rc = cbs->allocRing(getRef(), xhandle);
	if (_FAILURE(rc)) {
	    return rc;
	}
	cbs->ring = r;
	r->cd->controlRing = r;
    }
    return 0;
}

/* virtual */ SysStatus
MemTrans::setThreshold(XHandle xhandle, uval32 low, uval32 high)
{
    ClientData *cd = clnt(xhandle);
    ControlRing *r = cd->controlRing;

    if (!r) {
	return _SERROR(2003, 0, EINVAL);
    }
    if (r->size < high) {
	high = r->size;
    }

    if (r->size < low) {
	low = r->size;
    }

    r->lowMark = low;
    r->highMark = high;

    return 0;

}


/* virtual */ SysStatusUval
MemTrans::_pokeRing(__XHANDLE xhandle)
{
    ClientData *cd = clnt(xhandle);
    ControlRing *r = cd->controlRing;

    if (!r) {
	return _SERROR(2074, 0, EINVAL);
    }

    if (cbs) {
	return cbs->pokeRing(getRef(),xhandle);
    }
    if (!r->cd) breakpoint();
    return 0;
}

/* virtual */ SysStatus
MemTrans::dumpRingStatus(XHandle xhandle)
{
    ClientData *cd = clnt(xhandle);
    ControlRing *r = cd->controlRing;

    if (!r->cd) breakpoint();

    if (!r) {
	return _SERROR(2084, 0, EINVAL);
    }

    const ControlRing *remote = remoteAddr(r->cd->remoteBase, r->remote);
    err_printf("PID: %ld xh: %lx lH: %d lT: %d rH: %d rT: %d\n",
	       DREFGOBJ(TheProcessRef)->getPID(),xhandle,
	       r->localHead, remote->remoteTail,
	       remote->localHead, r->remoteTail);

    return 0;
}


/* virtual */ MemTrans::ControlRing *
MemTrans::__allocRing(uval &size)
{
    AutoLock<LockType> al(&allocLock);

    size = (512 - sizeof(ControlRing)) / sizeof(uval);

    uval y = PAGE_SIZE;
    uval addr;
    SysStatus rc = locked_allocPagesLocal(addr, y, 0);


    if (_FAILURE(rc)) {
	return NULL;
    }
    ControlRing *r = (ControlRing*)addr;
    r->size = size;
    r->localHead = 0;
    r->remoteTail = 0;
    r->readLock.init();
    r->writeLock.init();
    return r;
}

/* virtual */ SysStatusUval
MemTrans::allocRing(uval &size, uval32 low, uval32 high, XHandle remote)
{
    ClientData *cd = clnt(remote);
    tassertMsg(cd,"Should have clientData!!!\n");
    if (!cd) {
	return _SERROR(1948, 0, EINVAL);
    }
    if (cd->controlRing) {
	return _SERROR(1946, 0, EALREADY);

    }

    cd->controlRing = __allocRing(size);

    tassertMsg(cd->controlRing,"MemTrans allocation failure\n");

    ControlRing *r = cd->controlRing;

    if (low > r->size) {
	low = r->size;
    }
    r->lowMark = low;

    if (high > r->size) {
	high = r->size;
    }
    r->highMark = high;

    SysStatusUval rc = cd->remoteMT._allocRing(localOffset((uval)r),
					       r->remote);

    _IF_FAILURE_RET_VERBOSE(rc);

    r->cd = cd;
    if (cbs) {
	cbs->ring = r;
    }
    return 0;
}

/* virtual */ uval
MemTrans::insertToRing(XHandle xhandle, uval *value, uval numVals)
{
    ClientData *cd = clnt(xhandle);
    ControlRing *r = cd->controlRing;

    if (!r) {
	return _SERROR(1947, 0, EINVAL);
    }

    AutoLock<FairBLock> al(&r->writeLock);

    const ControlRing *remote = remoteAddr(r->cd->remoteBase, r->remote);

    volatile uval head = r->localHead;
    volatile uval tail = remote->remoteTail;
    uval used = head - tail;

    tassertMsg(used<=r->size,"Bad ring usage\n");
    if (r->size - used < numVals) {
	return _SERROR(1953, MemTransErr, EBUSY);
    }
    for (uval i=0; i<numVals; ++i) {
	r->data[(r->localHead+i)%r->size] = value[i];
    }
    AtomicAdd32Synced(&r->localHead, numVals);

    head +=numVals;
    tail = remote->remoteTail;
    uval newUsed = head - tail;
    dprintf("Ins: t:%ld s:%ld h:%ld v:%lx\n", tail, r->size, head, value[0]);

    if (newUsed >= remote->highMark && used < remote->highMark) {
	dprintf("poking 2: %ld %ld %ld\n",tail, r->size, head);
	SysStatusUval rc =r->cd->remoteMT._pokeRing();
	tassertWrn(_SUCCESS(rc),"Poke should succeed: %lx\n",rc);
    }
    if (!r->cd) breakpoint();
    return 0;
}

/* virtual */ SysStatus
MemTrans::consumeFromRing(XHandle xhandle, uval *value, uval numVals)
{
    ClientData *cd = clnt(xhandle);
    ControlRing *r = cd->controlRing;

    if (!r) {
	return _SERROR(1952, 0, EINVAL);
    }

    AutoLock<FairBLock> al(&r->readLock);

    const ControlRing *remote = remoteAddr(r->cd->remoteBase, r->remote);

    volatile uval head = remote->localHead;
    volatile uval tail = r->remoteTail;
    uval used = head - tail;

    tassertMsg(used<=r->size,"Bad ring usage\n");

    if ( used < numVals ) {
	return _SERROR(1951, MemTransErr, EBUSY);
    }

    dprintf("Cons: %ld %ld %ld\n",tail, r->size, head);

    for (uval i=0; i<numVals; ++i) {
	value[i] = remote->data[ (r->remoteTail+i) % r->size];
    }
    AtomicAdd32Synced(&r->remoteTail,numVals);

    head = remote->localHead;
    tail+= numVals;
    uval newUsed = head - tail;

    if (newUsed <= remote->lowMark &&  used > remote->lowMark) {
	dprintf("poking 3: %ld %ld %ld\n", tail, r->size, head);
	r->cd->remoteMT._pokeRing();
    }
    if (!r->cd) breakpoint();

    return 0;
}


MemTrans::LockType MemTrans::createLock;

/* virtual */ SysStatus
MemTrans::postFork()
{
    return 0;
}

/* virtual */ SysStatus
MemTrans::detach()
{
    uval count;
    uval old;
    do {
	old = refCount;
	count = refCount - 1;
	if (count==0) {
	    count = ~0ULL;
	}
    } while (!CompareAndStoreSynced(&refCount, old, count));
    if (count==~0ULL) {
	return destroy();
    }
    return 0;
}

/* virtual */ SysStatus
MemTrans::destroy()
{
    dprintf("Destroy: %lx\n",getRef());
    if (refCount!=~0ULL) {
	tassertMsg(0,"Can't destroy busy MemTrans: %ld\n",refCount);
	return _SERROR(2000, 0, EBUSY);
    }

    cbs->nowDead(getRef());
    cbs = NULL;

    {   // remove all ObjRefs to this object
	SysStatus rc=closeExportedXObjectList();
	// most likely cause is that another destroy is in progress
	// in which case we return success
	if (_FAILURE(rc)) return _SCLSCD(rc)==1?0:rc;
    }

    DREFGOBJ(TheProcessRef)->regionDestroy(localBase);

    localFR._releaseAccess();
    cdHash.destroy();
    return destroyUnchecked();
}


/* virtual */ uval
MemTrans::getOH(ProcessID pid, ObjectHandle &oh)
{
    ClientData *cd;
    uval ret = cdHash.find(pid, cd);
    if (ret) {
	oh = cd->givenOH;
    }
    dprintf("%s: got xh: %lx -> %ld\n",__func__,oh.xhandle(),pid);
    return ret;
}

/* static */ SysStatus
MemTrans::GetMemTrans(MemTransRef &mtr,
		      XHandle &xh,
		      ProcessID pid,
		      uval key)
{
    SysStatus rc = 0;
    BaseProcessRef pref;

    rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(pid, pref);

    _IF_FAILURE_RET_VERBOSE(rc);

    rc = DREF(pref)->getLocalSMT(mtr, xh, key);

    return rc;

}

/* static */ SysStatus
MemTrans::Create(MemTransRef &mtr, ProcessID pid, uval size,
		 XHandle &remote, MTEvents *handler, uval key)
{
    if ( _SUCCESS(GetMemTrans(mtr, remote, pid, key)) ) {

	ObjectHandle tmpOH;
	DREF(mtr)->getOH(pid, tmpOH);
	remote = tmpOH.xhandle();
	return _SERROR(1996, 0, EALREADY);
    }

    createLock.acquire();
    MemTrans *mt = new MemTrans(key);
    SysStatus rc = mt->init(pid, remote, size, handler);

    //init always creates a ref -- it needs it internally
    //thus we have to use destroy() on failure
    mtr = mt->getRef();

    createLock.release();

    if (_FAILURE(rc) && _SGENCD(rc)!=EALREADY) {
	err_printf("Failed creating MemTrans: %lx\n", rc);
	DREF(mtr)->destroy();
    }

    return rc;
}

/* virtual */ SysStatus
MemTrans::allocPagesLocal(uval &addr, uval &size, uval validator)
{
    AutoLock<LockType> al(&allocLock);
    return locked_allocPagesLocal(addr, size, validator);
}

/* virtual */ SysStatus
MemTrans::allocPagesLocalBlocking(uval &addr, uval &size, uval validator)
{
    SysStatus rc = 0;
    uval numPages = size/PAGE_SIZE;
    if (size%PAGE_SIZE) {
	++numPages;
    }
    WaitElement we(Scheduler::GetCurThread(), numPages);
    allocLock.acquire();
    do {

	we.stopBlock = 0;

	waitList.prepend(&we);

	rc = locked_allocPagesLocal(addr, size, validator);

	if (_SUCCESS(rc)) {
	    // Detach we before releasing allocLock
	    break;
	}

	allocLock.release();

	while (!we.stopBlock) {
	    Scheduler::Block();
	}
	allocLock.acquire();
	we.detach();
    } while (1);

    we.detach();
    allocLock.release();
    return rc;
}

/* virtual */ void
MemTrans::dumpAllocList()
{
    if (!allocDump) return;
    err_printf("Dump of: %p ",getRef());
    sval count = 0;
    uval unchangedCount = 0;
    PageUse *pu;
    for (pu = (PageUse*)usedPages.next(), count = pu->pageCount;
	 pu!=NULL; pu = (PageUse*)pu->next()) {
	if (count == pu->pageCount) {
	    ++unchangedCount;
	    continue;
	}
	if (count > 0) {
	    err_printf("(%ld)[%ld] ",count, unchangedCount);
	} else {
	    err_printf("{%ld}[%ld] ",-count, unchangedCount);
	}
	count = pu->pageCount;
	unchangedCount = 1;
    }
    if (count > 0) {
	err_printf("(%ld)[%ld]\n",count, unchangedCount);
    } else {
	err_printf("{%ld}[%ld]\n",-count, unchangedCount);
    }

}


/* virtual */ SysStatus
MemTrans::locked_allocPagesLocal(uval &addr, uval &size, uval validator)
{
    PageUse *best=NULL;
    if (allocDump) {
	err_printf("Alloc: %ld\n",size);
	dumpAllocList();
    }
    uval numPages = size/PAGE_SIZE;
    if (size%PAGE_SIZE) {
	++numPages;
    }
    uval bestPage = 0;
    uval currPage = 0;

    for (PageUse* pu = (PageUse*)usedPages.next(); pu!=NULL;
	 pu = (PageUse*)pu->next()) {
	if (pu->pageCount>=(sval)numPages) {

	    if (pu->pageCount>(sval)numPages) {
		PageUse* leftOver = new PageUse(pu->pageCount - numPages, 0);
		pu->append(leftOver);
	    }
	    pu->pageCount = -numPages;
	    pu->validator = validator;
	    addr = localBase + PAGE_SIZE * currPage;
	    size = numPages*PAGE_SIZE;
	    return 0;
	}
	if ((!best && pu->pageCount>0) ||
	    (best && best->pageCount < pu->pageCount)) {
	    best = pu;
	    bestPage= currPage;

	}
	currPage+= abs(pu->pageCount);
    }


#if 0
    if (best) {
	size = best->pageCount * PAGE_SIZE;
	best->pageCount *= -1;
	best->validator = validator;
	addr = localBase+ PAGE_SIZE * bestPage;
	return 0;
    }
#endif /* #if 0 */

    return _SERROR(1955, MemTransErr, ENOMEM);
}

/* virtual */ void
MemTrans::freePagePtr(void *addr)
{
    freePage(localOffset((uval)addr));
}


/* virtual */ void
MemTrans::freePage(uval offset, uval validator)
{
    uval currPage=0;
    uval targetPage = offset/PAGE_SIZE;
    allocLock.acquire();
    if (allocDump) {
	err_printf("Free: %ld\n",targetPage);
	dumpAllocList();
    }

    tassertMsg(offset <= localSize,
	       "De-allocating bad page %016lx [%016lx %016lx]\n",
	       offset, localBase, localBase + localSize);

    PageUse *pu = NULL;
    for (pu = (PageUse*)usedPages.next(); pu!=NULL;
	 pu = (PageUse*)pu->next()) {


	if (currPage >= targetPage) {

	    tassertMsg(pu->validator == validator,
		       "Freeing unmatched validator: %lx vs %lx : %lx",
		       pu->validator,validator,offset);
	    break;
	}
	tassertMsg(currPage < targetPage,
		   "Can't de-allocate bad page %016lx [%016lx %016lx]\n",
		   offset, localBase, localBase+localSize);

	currPage+= abs(pu->pageCount);
    }

    tassertMsg(pu,"Can't de-allocate bad page %016lx [%016lx %016lx]\n",
	       offset, localBase, localBase+localSize);

    tassertMsg(pu->pageCount != abs(pu->pageCount),
	       "Can't de-allocate already free page\n");

    // We found the item in the list representing this page range
    // Now try to see if we can merge it with some others
    pu->pageCount = abs(pu->pageCount);
    if (pu->prev() && ((PageUse*)pu->prev())->pageCount>0) {
	pu->pageCount += ((PageUse*)pu->prev())->pageCount;
	delete pu->prev();
    }

    if (pu->next() && ((PageUse*)pu->next())->pageCount>0) {
	pu->pageCount += ((PageUse*)pu->next())->pageCount;
	delete pu->next();
    }

    // Is there anybody waiting for pages who could use this area?

    uval availPages = pu->pageCount;
    WaitElement *x=NULL;

    for (x=(WaitElement*)waitList.next();
	 x!=NULL && availPages>0 ;
	 x= (WaitElement*)x->next()) {
	if (x->numPages <= availPages) {
	    x->stopBlock = 1;
	    x->lockedDetach();
	    Scheduler::Unblock(x->id);
	    readyList.prepend(x);
	    availPages -= x->numPages;
	}
    }

    allocLock.release();


}

/* static */ void
MemTrans::FreeCDData(XHandle xhandle)
{
    ClientData *cd = clnt(xhandle);

    delete cd;
}

/* virtual */ SysStatus
MemTrans::handleXObjFree(XHandle xhandle)
{
    // We're removing a client


    AutoLock<LockType> al(&createLock);
    ClientData *cd = clnt(xhandle);
    ClientData *cd2;

    // Client may not have connected yet
    if (cd->remoteBase) {
	DREFGOBJ(TheProcessRef)->regionDestroy(cd->remoteBase);
	cd->remoteFR._releaseAccess();
    }


    // Eliminate the process binding
    BaseProcessRef pref;
    SysStatus rc = DREFGOBJ(TheProcessSetRef)->getRefFromPID(cd->pid, pref);

    tassertMsg(_SUCCESS(rc), "Can't find process %ld\n", cd->pid);

    rc = DREF(pref)->removeSMT(getRef());

    tassertMsg(_SUCCESS(rc), "Can't remove SMT\n");

    if (cdHash.remove(XHandleTrans::GetOwnerProcessID(xhandle), cd2)) {

	dprintf("%s: remove xh: %lx -> %ld\n",__func__,xhandle, cd->pid);
	tassertMsg(cd == cd2, "CD's don't match: %p %p\n",cd,cd2);
    }

    //Detach --- one less client around; dec ref count
    detach();


    XHandleTrans::SetBeingFreed(xhandle,MemTrans::FreeCDData);

    return 0;
}
