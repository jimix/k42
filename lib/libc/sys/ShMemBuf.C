/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2003.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ShMemBuf.C,v 1.6 2005/07/05 15:19:59 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implements shared memory transport interface
 * **************************************************************************/
#include <sys/sysIncs.H>
#include "ShMemBuf.H"
#include <cobj/CObjRootSingleRep.H>
#include <stub/StubFRComputation.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>

uval exportsSize = 0;
ShMemBuf::ExpHash *ShMemBuf::exports = NULL;
void
ShMemBuf::ClassInit(VPNum vp)
{
    if (vp) return;
    exports = new ShMemBuf::ExpHash;
}

/*static*/ void
ShMemBuf::PostFork()
{
    if (!exports) {
	return;
    }
    uval restart = 0;
    ProcessID key;
    ShMemBufRef smb;
    while (exports->removeNext(key, smb, restart)) {
	exportsSize--;
	DREF(smb)->forkDestroy();
	restart = 0;
    }
}

/* virtual */ SysStatus
ShMemBuf::forkDestroy()
{
    SysStatus rc;
    //These are no longer valid due to fork (we're a different process)
    frOH.init();

    rc = lockIfNotClosingExportedXObjectList();
    tassertMsg(_SUCCESS(rc), "shouldn't be closing\n");

    XHandle xhandle = getHeadExportedXObjectList();
    while (xhandle != XHANDLE_NONE) {
	uval clientData;
	unlockExportedXObjectList();
	rc = XHandleTrans::Demolish(xhandle, clientData);
	if (_SUCCESS(rc)) {
	    ClientData *cd = (ClientData*)clientData;
	    delete cd;
	}
	rc = lockIfNotClosingExportedXObjectList();
	tassertMsg(_SUCCESS(rc), "shouldn't be closing\n");
	xhandle = getHeadExportedXObjectList();
    }
    unlockExportedXObjectList();

    uval k;
    AllocElement *entry;
    uval restart = 0;
    //The "entry" pointers point into a region we don't own anymore
    while (allocs.removeNext(k, entry, restart));

    return destroyUnchecked();
}

SysStatus
ShMemBuf::Fetch(ProcessID pid, ObjectHandle &oh,
		ShMemBufRef &smbRef, uval minObjSize)
{
    SysStatus rc = 0;
  retry:
    exports->acquireLock();
    if (!exports->locked_find(pid,smbRef)) {
	exports->locked_add(pid, NULL);
	exports->releaseLock();

	rc = Create(smbRef,pid, minObjSize);
	if (_FAILURE(rc)) goto abort;

	rc = DREF(smbRef)->giveAccessByServer(oh, pid);
	if (_FAILURE(rc)) goto abort;

	exports->acquireLock();
	ShMemBufRef ref;
	exports->locked_remove(pid,ref);
	tassertMsg(ref==NULL,"ref is not null: %p\n",ref);
	exports->locked_add(pid, smbRef);
	exportsSize++;
    } else {

	// if NULL, then somebody is in the process of making it
	if (smbRef==NULL) {
	    exports->releaseLock();
	    Scheduler::Yield();
	    goto retry;
	}
	rc = DREF(smbRef)->getOH(pid, oh);
    }
    exports->releaseLock();
abort:
    return rc;
}

/* virtual */ SysStatus
ShMemBuf::addClient(ProcessID pid, ObjectHandle &oh)
{
    SysStatus rc = 0;
    ShMemBufRef smbRef;
    exports->acquireLock();
    if (exports->locked_find(pid,smbRef)) {
	rc = _SERROR(2592, 0, EALREADY);
    } else {
	exports->locked_add(pid, smbRef);
	rc = giveAccessByServer(oh, pid);
	exportsSize++;
    }
    exports->releaseLock();
    return rc;
}

SysStatus
ShMemBuf::Create(ShMemBufRef &ref, ProcessID pid, uval minObjSize)
{
    SysStatus rc = 0;
    ShMemBuf *smb = new ShMemBuf(minObjSize);
    rc = smb->init(pid);

    if (_SUCCESS(rc)) {
	CObjRootSingleRep::Create(smb);
    }

    if (_FAILURE(rc)) {
	delete smb;
    } else {
	ref = smb->getRef();
    }
    return rc;
}

/* virtual */ SysStatus
ShMemBuf::init(ProcessID pid)
{

    SysStatus rc = 0;
    lock.init();
    key = pid;
    rc = StubFRComputation::_Create(frOH);

    _IF_FAILURE_RET(rc);

    rc=StubRegionDefault::_CreateFixedLenExt(addr, bufSize, SEGMENT_SIZE,
					     frOH, 0,
					     AccessMode::writeUserWriteSup,0,
					     RegionType::K42Region);
    _IF_FAILURE_RET(rc);

    free = new AllocElement(0, bufSize/minSize);
    return 0;
}

/* virtual */ SysStatus
ShMemBuf::_registerClient(ObjectHandle &oh, uval &size,
			  __XHANDLE xh, __CALLER_PID pid)
{
    SysStatus rc = 0;
    StubFRComputation stubFR(StubObj::UNINITIALIZED);
    stubFR.setOH(frOH);
    rc = stubFR._giveAccess(oh, pid);
    if (_SUCCESS(rc)) {
	size = bufSize;
    }
    return rc;
}

/* virtual */ SysStatus
ShMemBuf::giveAccessSetClientData(ObjectHandle &oh,
				  ProcessID toProcID,
				  AccessRights match,
				  AccessRights nomatch,
				  TypeID type)
{
    SysStatus rc;
    ClientData *clientData = new ClientData();
    rc = giveAccessInternal(oh, toProcID, match, nomatch,
			    type, (uval)clientData);

    if (_SUCCESS(rc)) {
	clientData->oh = oh;
	clientData->pid = toProcID;
    }
    return rc;
}

/* virtual */ SysStatus
ShMemBuf::shareExisting(ProcessID pid, uval sharedAddr, uval offset)
{
    SysStatus rc = 0;

    if (sharedAddr!=0) {
	offset = sharedAddr - addr;
    }
    if (offset >= bufSize) {
	return _SERROR(2587, 0, EINVAL);
    }

    AutoLock<LockType> al(&lock);
    ClientData *cd;
    rc = locked_getProcInfo(pid, cd);
    _IF_FAILURE_RET(rc);

    AllocElement *ae;
    if (!allocs.find(offset, ae)) {
	return _SERROR(2588, 0, EINVAL);
    }

    tassertMsg(ae->offset==offset, "Offset mismatch\n");
    ++ae->refCount;
    cd->mine.add(offset, ae);
    return 0;

}

/* virtual */ SysStatus
ShMemBuf::locked_getProcInfo(ProcessID pid, ShMemBuf::ClientData* &cd)
{
    // now traverse list of clients and tell them
    if (_FAILURE(lockIfNotClosingExportedXObjectList())) {
	return _SERROR(2590, 0, EINVAL);
    }
    XHandle xhandle = getHeadExportedXObjectList();
    while (xhandle != XHANDLE_NONE) {
	ProcessID p=~0ULL;
	AccessRights x,y;
	XHandleTrans::GetRights(xhandle, p, x, y);
	if (p == pid) {
	    cd = (ClientData*)XHandleTrans::GetClientData(xhandle);
	    unlockExportedXObjectList();
	    return 0;
	}
	xhandle = getNextExportedXObjectList(xhandle);
    }
    unlockExportedXObjectList();

    return _SERROR(2591, 0, EINVAL);
}

/* virtual */ SysStatus
ShMemBuf::getOH(ProcessID pid, ObjectHandle &oh)
{
    SysStatus rc = 0;
    AutoLock<LockType> al(&lock);
    ClientData *cd;
    rc = locked_getProcInfo(pid, cd);
    _IF_FAILURE_RET(rc);
    oh = cd->oh;
    return rc;
}

/* virtual */ SysStatusUval
ShMemBuf::shareAlloc(ProcessID pid, uval &offset,
		     uval &shaddr, uval size)
{
    AllocElement* curr = free;
    AllocElement** prev= &free;
    uval neededBlocks = (size + (minSize-1))/minSize;
    SysStatus rc = 0;

    AutoLock<LockType> al(&lock);
    ClientData *cd;
    rc = locked_getProcInfo(pid, cd);
    _IF_FAILURE_RET(rc);

    rc = _SRETUVAL(neededBlocks * minSize);
    do {
	if (curr->blocks > neededBlocks) {
	    AllocElement* tmp = new AllocElement(
		curr->offset + (neededBlocks * minSize),
		curr->blocks - neededBlocks);

	    tmp->next=curr->next;
	    curr->next = tmp;
	    curr->blocks = neededBlocks;
	}
	if (curr->blocks == neededBlocks) {
	    *prev = curr->next;
	    curr->next = NULL;
	    break;
	}
	curr = curr->next;
	if (curr == NULL) {
	    return _SERROR(2586, 0 , ENOMEM);
	}
    } while (1);

    // 1 for this process, one for the other
    curr->refCount = 2;
    cd->mine.add(curr->offset, curr);
    allocs.add(curr->offset, curr);
    shaddr = curr->offset + addr;
    offset = curr->offset;
    return rc;
}

/* virtual */ SysStatus
ShMemBuf::unShare(uval offset, ProcessID pid)
{
    ClientData *cd;
    SysStatus rc;
    lock.acquire();
    rc = locked_getProcInfo(pid, cd);
    lock.release();
    _IF_FAILURE_RET(rc);

    return _unShare(offset,cd->oh.xhandle());
}

/* virtual */ SysStatus
ShMemBuf::_unShare(uval offset, __XHANDLE xh)
{
    AutoLock<LockType> al(&lock);
    AllocElement *ae = NULL;

    if (xh!=XHANDLE_NONE) {
	ClientData *cd = (ClientData*)XHandleTrans::GetClientData(xh);
	if (!cd || !cd->mine.remove(offset, ae)) {
	    return _SERROR(2589, 0, EINVAL);
	}
    } else {
	if (!allocs.find(offset, ae)) {
	    tassertMsg(ae==NULL,"Should have found AllocElement\n");
	    return _SERROR(2601, 0, EINVAL);
	}
	tassertMsg(ae,"Should have found AllocElement\n");

    }
    --ae->refCount;

    if (ae->refCount) {
	return 0;
    }
    allocs.remove(offset, ae);

    AllocElement* curr = free;
    AllocElement* prev = NULL;

    // FIXME:  use a plug-replacement allocator that doesn't suffer
    //         O(n) scan costs on the free list

    // Locate spot in the free list

    if (!free || free->offset > ae->offset) {
	ae->next = free;
	curr = free;
	free = ae;
	prev = free;
    } else {
	while (curr && curr->offset < ae->offset) {
	    prev = curr;
	    curr = curr->next;
	}
	ae->next = curr;
	prev->next = ae;
    }

    // Coalesce before and after the newly inserted element

    while (prev->next &&
	   prev->offset + prev->blocks * minSize == prev->next->offset) {
	prev->blocks += prev->next->blocks;
	AllocElement *tmp = prev->next;
	prev->next = prev->next->next;
	delete tmp;
    }
    while (curr->next &&
	   curr->offset + curr->blocks * minSize == curr->next->offset) {
	curr->blocks += curr->next->blocks;
	AllocElement *tmp = curr->next;
	curr->next = curr->next->next;
	delete tmp;
    }
    return 0;
}

/* virtual */ SysStatus
ShMemBuf::handleXObjFree(XHandle xh)
{
    ClientData *cd = (ClientData*)XHandleTrans::GetClientData(xh);
    AllocElement* ae;
    uval k;

    while (cd->mine.getFirst(k, ae)) {
	_unShare(k, xh);
    }
    XHandleTrans::SetBeingFreed(xh, BeingFreed);
    return 0;
}

/* static */ void
ShMemBuf::BeingFreed(XHandle xh) {
    ClientData *cd = (ClientData*)XHandleTrans::GetClientData(xh);
    delete cd;
}

/*virtual*/ SysStatus
ShMemBuf::exportedXObjectListEmpty()
{
    return destroy();
}

/* virtual */ SysStatus
ShMemBuf::destroy()
{
    ShMemBufRef ref;

    // remove all ObjRefs to this object
    SysStatus rc = closeExportedXObjectList();
    // most likely cause is that another destroy is in progress
    // in which case we return success
    if (_FAILURE(rc)) {
	err_printf("In ShMemBuf::destroy(): already being destroyed\n");
	return _SCLSCD(rc) == 1 ? 0 : rc;
    }

    exports->remove(key, ref);

    --exportsSize;
    StubFRComputation stubFR(StubObj::UNINITIALIZED);
    stubFR.setOH(frOH);
    stubFR._releaseAccess();

    uval k;
    AllocElement *ae;
    while (allocs.getFirst(k, ae)) {
	_unShare(k, XHANDLE_NONE);
    }

    allocs.destroy();

    DREFGOBJ(TheProcessRef)->regionDestroy(addr);

    destroyUnchecked();
    return 0;
}
