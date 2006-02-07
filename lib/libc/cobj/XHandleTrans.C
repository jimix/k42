/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: XHandleTrans.C,v 1.69 2004/07/11 21:59:23 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * support routines for the external object transation/invocation subsystem
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <meta/MetaObj.H>
#include "XHandleTrans.H"
#include "XBaseObj.H"
#include <cobj/XObjectList.H>
#include "ObjectRefs.H"
#include <misc/hardware.H>
#include <sys/ProcessSet.H>
#include "CObjRootSingleRep.H"
#include <alloc/PageAllocator.H>
#include <scheduler/DispatcherMgr.H>

/* static */ XBaseObj *XHandleTrans::XHandleTable;

/* Define an unused obj class with which we populate the XhandleTransTable
 * Accesses to any <unused entry> will be handled by this missHandle
 * object.
 * We have to think about whether this has to be done in multiple stages
 * when removing an entry
 */

/* static */ void
XHandleTrans::InitXHandleRange(XBaseObj *table, uval oldLimit, uval newLimit,
			       XBaseObj*& top, XBaseObj*& bottom)
{
    XBaseObj *free;
    /*
     * Clear the new table chunk.  We depend on the __nummeth fields
     * being cleared to detect invalid entries.  Everything else
     * (vtable pointer, etc.) is cleared for sanity's sake.
     */
    memset((void *) (&table[oldLimit]),
	   0, (newLimit - oldLimit) * sizeof(XBaseObj));
    /*
     * String the new entries together on a list to be returned.
     */
    free = NULL;
    for (uval i = newLimit - 1; i >= oldLimit; i--) {
	table[i].setNextFree(free);
	free = &table[i];
    }
    top = free;
    bottom = &table[newLimit - 1];
}

/* static */ SysStatus
XHandleTrans::ClassInit(VPNum vp)
{
    SysStatus rc;
    uval ptr;
    XHandleTrans *theXHandleTransPtr;

    if (vp != 0) {
//	tassertWrn(0, "processor %d sharing xhandle table\n", vp);
	return 0;
    }

    /*
     * We allocate virtual memory for the entire quasi-infinite xhandle table,
     * but we initialize just the first chunk of the table.  Subsequent chunks
     * are initialized as needed.
     */
    rc = DREFGOBJ(ThePageAllocatorRef)->
		    allocPagesAligned(ptr, XHANDLE_TABLE_MAX*sizeof(XBaseObj),
				      sizeof(XBaseObj));
    tassert(_SUCCESS(rc), err_printf("xhandle table allocation failed.\n"));

    /*
     * We initialize the first chunk from 1 rather than 0 to avoid putting
     * XHANDLE_NONE on the free list.
     */
    tassert(XHANDLE_NONE_IDX == 0, err_printf("XHANDLE_NONE_IDX not 0?\n"));
    XBaseObj *free, *dummy;

    // We reserve a bunch of XHandle slots for global clustered objects
    // This provides free xhandles for global clustered objects
    InitXHandleRange((XBaseObj *) ptr, CObjGlobals::numReservedEntries,
		     XHANDLE_TABLE_INCREMENT,
		     free, dummy);

    // Must manually zero the reserved entries for global objects
    // since they are not part of the regular initialzation
    memset((void *)ptr, 0, CObjGlobals::numReservedEntries * sizeof(XBaseObj));

    theXHandleTransPtr = new XHandleTrans;

    theXHandleTransPtr->xhandleTable = (XBaseObj *) ptr;
    theXHandleTransPtr->xhandleTableLimit = XHANDLE_TABLE_INCREMENT;
    theXHandleTransPtr->freeEntries = free;
    theXHandleTransPtr->oldEntries.init(0);
    theXHandleTransPtr->removedEntries = 0;
    theXHandleTransPtr->numberRemoved = 0;

    new CObjRootSingleRep(theXHandleTransPtr,
			  RepRef(GOBJ(TheXHandleTransRef)));

    /*
     * Facts about the xhandle table are shadowed in the static XHandleTable
     * and in all the dispatchers.
     */
    XHandleTable = theXHandleTransPtr->xhandleTable;
    rc = DREFGOBJ(TheDispatcherMgrRef)->
		    publishXHandleTable(theXHandleTransPtr->xhandleTable,
					theXHandleTransPtr->xhandleTableLimit);
    tassert(_SUCCESS(rc), err_printf("publishXHandleTable failed.\n"));

    return 0;
}

/*
 * We validity check these conversion calls to make sure the
 * xobject hasn't been freed.  Of course, the xobject could be
 * freed right after the check.  But in that case, the values in
 * the xobject will persist until the token circulates.
 *
 * In order to synchronize the check with the creation of a valid
 * xobject, we follow the selfIfValid link first.  Creation sets this
 * link last.
 */
/* virtual */ SysStatus
XHandleTrans::xhandleToXObj(XHandle xhandle, XBaseObj *&xobj)
{
    uval seqNo = XHANDLE_SEQNO(xhandle);
    uval idx   = XHANDLE_IDX(xhandle);
    // use a temp - compiler can't tell that sync doesn't change xobj
    XBaseObj *thisXobj;
    uval nummeth;
    if (idx < xhandleTableLimit) {
	xobj = thisXobj = &xhandleTable[idx];
	nummeth = thisXobj->__nummeth;
	SyncAfterAcquire();		// nummeth is the "lock"
	if (nummeth && seqNo == thisXobj->seqNo) return 0;
    }

    // Identifying the error here is useless, the error is actually
    // in the process of the caller.
    return _SERROR(1509, 0, EINVAL);
}

/* virtual */ SysStatus
XHandleTrans::xhandleValidate(XHandle xhandle)
{
    uval seqNo = XHANDLE_SEQNO(xhandle);
    uval idx   = XHANDLE_IDX(xhandle);
    XBaseObj *thisXobj;
    uval nummeth;
    if (idx < xhandleTableLimit) {
	thisXobj = &xhandleTable[idx];
	nummeth = thisXobj->__nummeth;
	SyncAfterAcquire();		// nummeth is the "lock"
	if (nummeth && seqNo == thisXobj->seqNo) return 0;
    }

    tassertWrn(0, "invalid xhandle=0x%lx\n", xhandle);
    return _SERROR(1162, 0, EINVAL);
}


/* static */ SysStatus
XHandleTrans::XHToInternal(XHandle xh, ProcessID pid, AccessRights rights,
			   ObjRef &ref, TypeID &type)
{
    XBaseObj *xobj;
    SysStatus rc = DREFGOBJ(TheXHandleTransRef)->xhandleToXObj(xh, xobj);
    tassertWrn(_SUCCESS(rc), "woops\n");
    if (!_SUCCESS(rc)) return rc;

    ref = xobj->__iobj;
    if (((pid == xobj->__matchPID) &&
	 ((rights & xobj->__mrights) == rights)) ||
	((rights & xobj->__urights) == rights)) {
	return DREF(ref)->getType(type);
    }
    tassertWrn(0, "Attempt to get internal ref  on xh %lx failed\n"
	       "\t req: pid %lx, rights %lx\n"
	       "\t obj: matchPid %x mr %lx ur %lx\n", xh,
	       pid, rights, xobj->__matchPID, xobj->__mrights,
	       xobj->__urights);
    return _SERROR(1161, 0, EPERM);
}

/* static */ SysStatus
XHandleTrans::GetInfo(XHandle xhandle,
		      TypeID& xhType,
		      AccessRights& match,
		      AccessRights& nomatch)
{
    SysStatus rc;
    XBaseObj *xobj;
    ProcessID matchPID;

    rc = DREFGOBJ(TheXHandleTransRef)->xhandleToXObj(xhandle, xobj);
    _IF_FAILURE_RET(rc);

    xhType = xobj->XtypeID();
    xobj->getRights(matchPID, match, nomatch);

    return 0;
}

/* virtual */ SysStatusXHandle
XHandleTrans::alloc(ObjRef oref, BaseProcessRef pref,
		    AccessRights matched, AccessRights unmatched,
		    COVTableEntry* vtbl, uval8 numMethods,
		    uval clientData)
{
    SysStatus rc = 0;
    XBaseObj *te;
    SysStatusProcessID matchPID;

    // lock the obj and process lists
    // we munch the static function to be a member function so to speak

    uval idx = CObjGlobals::GlobalXHandleIdx(oref);
    if (idx && (matched & MetaObj::globalHandle)) {
	te = &xhandleTable[idx];
    } else do {

	/*
	 * because these objects are managed with ReadCopyUpdate
	 * its safe to do a non-blocking pop.  If the top object
	 * disappears, it cant re-appear will this thread completes
	 */

	while ((te = freeEntries)) {
	    XBaseObj* next;
	    next=te->getNextFree();
	    if (CompareAndStoreSynced(
		(uval*)(&freeEntries), (uval)te, (uval)next)) break;
	}

	if (te == 0) {
	    // Free list is empty.  See if any previous of frees can
	    // be completed.
	    processToken();
	    if (freeEntries == 0) {
		// Free list is still empty.  Allocate a new chunk of
		// the table, guarded by oldEntries lock
		oldEntries.acquire();
		// check after getting the lock to avoid duplicate extends
		if (FetchAndNop((uval*)&freeEntries) == 0) {
		    XBaseObj *top, *bottom, *oldFree;
		    uval const newLimit = xhandleTableLimit +
			XHANDLE_TABLE_INCREMENT;

		    passertMsg(newLimit <= XHANDLE_TABLE_MAX,
			       "Quasi-infinite xhandle table exhausted.\n");
		    InitXHandleRange(xhandleTable,
					  xhandleTableLimit, newLimit,
					  top, bottom);
		    xhandleTableLimit = newLimit;
		    rc = DREFGOBJ(TheDispatcherMgrRef)->
			publishXHandleTable(xhandleTable, xhandleTableLimit);
		    tassertMsg(_SUCCESS(rc),"publishXHandleTable failed.\n");
		    // must not make new entries available until limit is moved
		    do {
			oldFree = freeEntries;
			bottom->setNextFree(oldFree);
		    } while (!(CompareAndStoreSynced(
			(uval*)(&freeEntries), (uval)oldFree, (uval)top)));

		}
		oldEntries.release();
	    }
	}
    } while (te == 0);

    matchPID = DREF(pref)->getPID();
    tassertMsg(_SUCCESS(matchPID), "bad process ref passed in alloc %lx\n",rc);

    if (_FAILURE(rc = DREF(oref)->publicLockExportedXObjectList())) goto exit1;
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
    if (matchPID != _KERNEL_PID) {
	// we know kernel will not go away, don't insert in process list,
	// since this ProcessWrapper becomes a hot spot in file systems
	if (_FAILURE(rc = DREF(pref)->lockMatchedXObjectList())) goto exit2;
    }
#else
    if (_FAILURE(rc = DREF(pref)->lockMatchedXObjectList())) goto exit2;
#endif

    // populate the translation entry, make sure vtable is last
    // do not modify the seqNo which is incremented on free
    te->__iobj      = oref;
    te->__matchPID  = _SGETPID(matchPID);
    te->__mrights   = matched | MetaObj::processOnly;
    te->__urights   = unmatched;
    te->clientData  = clientData;

    XHandle xhandle;
    xhandle = te->getXHandle();

    // add to process and object lists - can't fail, lock is held
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
    if (matchPID != _KERNEL_PID) {
	DREF(pref)->addMatchedXObj(xhandle);
    }
#else
    DREF(pref)->addMatchedXObj(xhandle);
#endif
    DREF(oref)->addExportedXObj(xhandle);

    // now stick in the vtable pointer
    XBaseObj::SetFTable(te, vtbl);

    // now sync and then make the xobject valid
    // N.B. nummeth is valid flag
    SyncBeforeRelease();
    te->__nummeth = numMethods;
    tassertMsg(numMethods != 0, "numMethods is 0 for xhandle 0x%lx\n", xhandle);

#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
    if (matchPID != _KERNEL_PID) {
	DREF(pref)->unlockMatchedXObjectList();
    }
#else
    DREF(pref)->unlockMatchedXObjectList();
#endif
    DREF(oref)->publicUnlockExportedXObjectList();

    return xhandle;

exit2:
    DREF(oref)->publicUnlockExportedXObjectList();
exit1:
    // Return the entry we acquired to the removed list
    // We don't put it on the free list because we want ReadCopyUpdate
    // guarantees for nonblocking access to freelist.
    XBaseObj *removed;

    te->setBeingFreed(0);		// make sure since its going
					// through removed cycle
    do {
	removed = removedEntries;
	te->setNextFree(removed);
    } while (!CompareAndStoreSynced((uval*)(&removedEntries),
				    (uval)removed, (uval)te));

    numberRemoved++;			// doesn't have to be exact,
					// so don't sync
    return rc;
}

/* virtual */ SysStatus
XHandleTrans::free(XHandle xhandle)
{
    XBaseObj *xBaseObj;
    ObjRef objRef;
    BaseProcessRef procRef;
    SysStatus rc;
    SysStatusProcessID processPID;
    // no lock is held, so these pointers can dissapear
    // out from under us.  That's why we copy and test.
    // also, the xhandle may already be free, so all errors
    // just lead to returning

    tassertMsg(XHANDLE_IDX(xhandle) >= CObjGlobals::numReservedEntries,
	       "Freeing global xhandle\n");
    xBaseObj = &xhandleTable[XHANDLE_IDX(xhandle)];
    objRef = xBaseObj->__iobj;
    processPID = xBaseObj->getOwnerProcessID();


#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
    if (processPID != _KERNEL_PID) {
	// we know kernel will not go away, don't insert in process list,
	// since this ProcessWrapper becomes a hot spot in file systems
	rc = DREFGOBJ(TheProcessSetRef)->
	    getRefFromPID(xBaseObj->getOwnerProcessID(), procRef);
	if (_FAILURE(rc)) {
	    tassert(!(xBaseObj->isValid()),
		    err_printf("PID %ld invalid in valid Xobj\n",
			       xBaseObj->getOwnerProcessID()));
	    return 0;
	}
	if (!procRef) return 0;
    }
#else
    rc = DREFGOBJ(TheProcessSetRef)->
	getRefFromPID(xBaseObj->getOwnerProcessID(), procRef);
    if (_FAILURE(rc)) {
	tassert(!(xBaseObj->isValid()),
		err_printf("PID %ld invalid in valid Xobj\n",
			   xBaseObj->getOwnerProcessID()));
	return 0;
    }
    if (!procRef) return 0;
#endif

    if ((!objRef)) return 0;
    if ((rc=DREF(objRef)->publicLockExportedXObjectList())) return 0;
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
    if (processPID != _KERNEL_PID) {
	if ((rc=DREF(procRef)->lockMatchedXObjectList())) {
	    DREF(objRef)->publicUnlockExportedXObjectList();
	    return 0;
	}
    }
#else
    if ((rc=DREF(procRef)->lockMatchedXObjectList())) {
	DREF(objRef)->publicUnlockExportedXObjectList();
	return 0;
    }
#endif
    //once we have the locks, the XObject state can't change out from
    //under us.
    if (xBaseObj->isValid()) {
	/* kill xobject so subsequent calls fail don't need a sync -
	 * token circulates before xobj becomes unsafe for stale
	 * references remove from process and object lists. Remove
	 * holder from lists and reset __nummeth to prevent another
	 * releaseAccess call (inflight) from trying to free this
	 * again.
	 */
	xBaseObj->__nummeth = 0;
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
	if (processPID != _KERNEL_PID) {
	    DREF(procRef)->removeMatchedXObj(xhandle);
	}
#else
	DREF(procRef)->removeMatchedXObj(xhandle);
#endif
	DREF(objRef)->removeExportedXObj(xhandle);

	// release locks before callback
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
	if (processPID != _KERNEL_PID) {
	    DREF(procRef)->unlockMatchedXObjectList();
	}
#else
	DREF(procRef)->unlockMatchedXObjectList();
#endif
	DREF(objRef)->publicUnlockExportedXObjectList();

	xBaseObj->setBeingFreed(0);
	DREF(objRef)->handleXObjFree(xhandle);

	XBaseObj *removed;

	// must to this after removes - same space is used for both
	do {
	    removed = removedEntries;
	    xBaseObj->setNextFree(removed);
	} while (!CompareAndStoreSynced((uval*)(&removedEntries),
				       (uval)removed, (uval)xBaseObj));

	numberRemoved++;		// doesn't have to be exact,
					// so don't sync
    } else {
	// race - someone else freed the XObject
	// release locks using original values.
#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
	if (processPID != _KERNEL_PID) {
	    DREF(procRef)->unlockMatchedXObjectList();
	}
#else
	DREF(procRef)->unlockMatchedXObjectList();
#endif
	DREF(objRef)->publicUnlockExportedXObjectList();
    }

    if ((numberRemoved % XHANDLE_TABLE_GC_INTERVAL) == 0) processToken();
    return 0;
}

// returns 1 : If entries are put on the free list (occurs after a global
//             generation of threads has elapsed, token has circulated at
//             least twice).
void
XHandleTrans::processToken()
{
    XBaseObj *top, *bottom, *next, *removed;

    // grab the lock and get list of ones ready to reuse
    oldEntries.acquire(top);

    if (top != NULL) {
	COSMgr::MarkerState stat;
	DREFGOBJ(TheCOSMgrRef)->checkGlobalThreadMarker(marker, stat);
	if (stat == COSMgr::ACTIVE) {
	    // We have entries waiting but the token has not arrived.
	    // Return without doing anything.
	    oldEntries.release();
	    return;
	}
    }

    // get the ones ready to wait for next token
    removed = (XBaseObj *) FetchAndClearSynced((uval *)(&removedEntries));
    numberRemoved = 0;

    DREFGOBJ(TheCOSMgrRef)->setGlobalThreadMarker(marker);

    // We can release the lock and then process the ones ready to reuse.
    // removed becomes the new oldEntries list.
    oldEntries.release(removed);

    // We're done if there was nothing on the old list.
    if (top == NULL) return;

    next = top;
    do {
	next->callBeingFreed();
	bottom = next;
	next = next->getNextFree();
    } while (next != NULL);

    XBaseObj *oldFree;
    do {
	oldFree = freeEntries;
	bottom->setNextFree(oldFree);
    } while (!(CompareAndStoreSynced(
	(uval*)(&freeEntries), (uval)oldFree, (uval)top)));
}


/* virtual */ SysStatus
XHandleTrans::demolish(XHandle xhandle, uval &clientData)
{
    XBaseObj *xBaseObj;
    ObjRef objRef;
    BaseProcessRef procRef;
    SysStatus rc;
    SysStatusProcessID processPID;
    // no lock is held, so these pointers can dissapear
    // out from under us.  That's why we copy and test.
    // also, the xhandle may already be free, so all errors
    // just lead to returning

    tassertMsg(XHANDLE_IDX(xhandle) >= CObjGlobals::numReservedEntries,
	       "Freeing global xhandle\n");
    xBaseObj = &xhandleTable[XHANDLE_IDX(xhandle)];
    objRef = xBaseObj->__iobj;
    processPID = xBaseObj->getOwnerProcessID();

#ifdef __NO_REGISTER_WITH_KERNEL_WRAPPER
#define REGISTER_ALWAYS 0
#else
#define REGISTER_ALWAYS 0
#endif

    if (REGISTER_ALWAYS || (processPID != _KERNEL_PID)) {
	// we know kernel will not go away, don't insert in process list,
	// since this ProcessWrapper becomes a hot spot in file systems
	rc = DREFGOBJ(TheProcessSetRef)->
	    getRefFromPID(xBaseObj->getOwnerProcessID(), procRef);
	if (_FAILURE(rc)) {
	    procRef = NULL;
	}
    }
    uval procLocked = 0;

    tassertMsg(objRef, "No object in XHandleTrans::demolish\n");

    rc=DREF(objRef)->publicLockExportedXObjectList();
    tassertMsg(rc==0, "Can't lock in XHandleTrans::demolish\n");

    if (procRef && (REGISTER_ALWAYS || (processPID != _KERNEL_PID))) {
	procLocked = DREF(procRef)->lockMatchedXObjectList();
    }

    tassertMsg(xBaseObj->isValid(), "Object is invalid\n");

    /* kill xobject so subsequent calls fail don't need a sync -
     * token circulates before xobj becomes unsafe for stale
     * references remove from process and object lists. Remove
     * holder from lists and reset __nummeth to prevent another
     * releaseAccess call (inflight) from trying to free this
     * again.
     */
    xBaseObj->__nummeth = 0;
    if (procRef && (REGISTER_ALWAYS || (processPID != _KERNEL_PID))) {
	DREF(procRef)->removeMatchedXObj(xhandle);
    }
    DREF(objRef)->removeExportedXObj(xhandle);

    // release locks before callback
    if (procLocked && (REGISTER_ALWAYS || (processPID != _KERNEL_PID))) {
	DREF(procRef)->unlockMatchedXObjectList();
    }

    clientData = xBaseObj->getClientData();

    DREF(objRef)->publicUnlockExportedXObjectList();

    xBaseObj->setBeingFreed(0);
//	DREF(objRef)->handleXObjFree(xhandle);

    XBaseObj *removed;

    // must to this after removes - same space is used for both
    do {
	removed = removedEntries;
	xBaseObj->setNextFree(removed);
    } while (!CompareAndStoreSynced((uval*)(&removedEntries),
				    (uval)removed, (uval)xBaseObj));

    numberRemoved++;		// doesn't have to be exact,
				// so don't sync

    if ((numberRemoved % XHANDLE_TABLE_GC_INTERVAL) == 0) processToken();
    return 0;
}
