/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: XObjectList.C,v 1.5 2001/11/01 01:09:13 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Manage a list of XObjs and their. XObjs entered
 *    can be found by searched by <ProcessID,matched-right,unmatched-rights>
 * **************************************************************************/


#include <sys/sysIncs.H>
#include "XHandleTrans.H"
#include "XBaseObj.H"
#include "XObjectList.H"

/* static */ inline uval
XObjectListObj::GetPrev(uval index)
{
    return XHandleTrans::XHandleTable[index].onObj.prev;
}

/* static */ inline uval
XObjectListObj::GetNext(uval index)
{
    return XHandleTrans::XHandleTable[index].onObj.next;
}

/* static */ inline void
XObjectListObj::SetPrev(uval target, uval newPrev)
{
    XHandleTrans::XHandleTable[target].onObj.prev = newPrev;
}

/* static */ inline void
XObjectListObj::SetNext(uval target, uval newNext)
{
    XHandleTrans::XHandleTable[target].onObj.next = newNext;
}

XHandle
XObjectListObj::getHead()
{
    IndexBits indexBits;
    tassert(headBLock.isLocked(),
	    err_printf("XObjectList called without lock %p\n", this));
    headBLock.get(indexBits);
    // don't try to get seq no right
    return XHANDLE_MAKE_NOSEQNO(indexBits.index());
}

XHandle
XObjectListObj::getNext(XHandle xhandle)
{
    IndexBits indexBits;
    uval head, next;
    tassert(headBLock.isLocked(),
	    err_printf("XObjectList called without lock %p\n", this));
    headBLock.get(indexBits);
    head = indexBits.index();
    next = GetNext(XHANDLE_IDX(xhandle));
    if (next == head) return XHANDLE_NONE;
    // don't try to get seq no right
    return XHANDLE_MAKE_NOSEQNO(next);
}

void
XObjectListObj::locked_add(XHandle xhandle)
{
    IndexBits indexBits;
    uval xobjindex = XHANDLE_IDX(xhandle);
    tassert(headBLock.isLocked(),
	    err_printf("XObjectList called without lock %p\n", this));
    headBLock.get(indexBits);

    if (indexBits.index() != XHANDLE_NONE_IDX) {
	SetNext(xobjindex, indexBits.index());
	SetPrev(xobjindex, GetPrev(indexBits.index()));
	SetNext(GetPrev(indexBits.index()), xobjindex);
	SetPrev(indexBits.index(), xobjindex);
    } else {
	SetNext(xobjindex, xobjindex);
	SetPrev(xobjindex, xobjindex);
	indexBits.index(xobjindex);
	headBLock.set(indexBits);
    }
}

void
XObjectListObj::locked_remove(XHandle xhandle)
{
    IndexBits indexBits;
    uval xobjindex = XHANDLE_IDX(xhandle);
    tassert(headBLock.isLocked(),
	    err_printf("XObjectList called without lock %p\n", this));
    headBLock.get(indexBits);

    if (GetNext(xobjindex) == xobjindex) {
	// am only one on queue, so head must point to me
	indexBits.index(XHANDLE_NONE_IDX);
	headBLock.set(indexBits);
	return;
    }
    SetPrev(GetNext(xobjindex), GetPrev(xobjindex));
    SetNext(GetPrev(xobjindex), GetNext(xobjindex));

    // change head unconditionally
    indexBits.index(GetNext(xobjindex));
    headBLock.set(indexBits);
}

SysStatus
XObjectListObj::lockIfNotClosing()
{
    IndexBits indexBits;
    headBLock.acquire(indexBits);
    if (indexBits.closing()) {
	headBLock.release();
	return _SERROR(1528, 0, ENXIO);
    }
    return 0;
}

uval
XObjectListObj::isClosed()
{
    IndexBits indexBits;
    headBLock.get(indexBits);
    return indexBits.closing();
}

uval
XObjectListObj::isEmpty()
{
    IndexBits indexBits;
    headBLock.get(indexBits);
    return (indexBits.index() == XHANDLE_NONE_IDX) | (indexBits.closing());
}

void
XObjectListObj::unlockIfNotClosing()
{
    IndexBits indexBits;
    headBLock.get(indexBits);
    if (!indexBits.closing()) {
	if (!headBLock.isLocked()) {
	    tassertWrn( (headBLock.isLocked()), 
			"locked not held even though !closing, "
			"Orran doesn't "
			"understand this code, ask Marc\n");
	    return;
	}
	headBLock.release();
    } else {
	tassert( (!headBLock.isLocked()), 
		 err_printf("locked held even though closing, Orran doesn't "
			    "understand this code, ask Marc\n"));
    }
}

SysStatus
XObjectListObj::close()
{
    IndexBits indexBits;
    uval head;
    ObjRef objRef;
    XBaseObj *xobj;
    while (1) {
	headBLock.acquire(indexBits);
	if (indexBits.closing()) {
	    // Some one else made the closing transition
	    headBLock.release();
	    return _SERROR(1529, 1, ESRCH);
	}
	head = indexBits.index();
	if (head == XHANDLE_NONE_IDX) {
	    indexBits.closing(1);
	    headBLock.release(indexBits);
	    return 0;
	}
	xobj = XHandleTrans::GetPointer(head);
	tassert(xobj->isValid(),
		err_printf("Already freed XObj still on chain\n"));
	objRef = xobj->getObjRef();
	tassert(objRef != NULL,
		err_printf("Valid XObj without object %p\n", this));
	headBLock.release();
	/*
	 * This remove can fail if some one else has also done a remove
	 * at the same time and got there first.  But the XBaseObj itself
	 * is "typesafe memory", it won't be reused until a token circulates.
	 */
	DREF(objRef)->releaseAccess(xobj->getXHandle());
    }
}

/* static */ inline uval
XObjectListProc::GetPrev(uval index)
{
    return XHandleTrans::XHandleTable[index].onProc.prev;
}

/* static */ inline uval
XObjectListProc::GetNext(uval index)
{
    return XHandleTrans::XHandleTable[index].onProc.next;
}

/* static */ inline void
XObjectListProc::SetPrev(uval target, uval newPrev)
{
    XHandleTrans::XHandleTable[target].onProc.prev = newPrev;
}

/* static */ inline void
XObjectListProc::SetNext(uval target, uval newNext)
{
    XHandleTrans::XHandleTable[target].onProc.next = newNext;
}

void
XObjectListProc::locked_add(XHandle xhandle)
{
    tassert(headBLock.isLocked(),
	    err_printf("XObjectList called without lock %p\n", this));

    IndexBits indexBits;
    uval xobjindex = XHANDLE_IDX(xhandle);
    headBLock.get(indexBits);

    if (indexBits.index() != XHANDLE_NONE_IDX) {
	SetNext(xobjindex, indexBits.index());
	SetPrev(xobjindex, GetPrev(indexBits.index()));
	SetNext(GetPrev(indexBits.index()), xobjindex);
	SetPrev(indexBits.index(), xobjindex);
    } else {
	SetNext(xobjindex, xobjindex);
	SetPrev(xobjindex, xobjindex);
	indexBits.index(xobjindex);
	headBLock.set(indexBits);
    }
}

void
XObjectListProc::locked_remove(XHandle xhandle)
{
    IndexBits indexBits;
    uval xobjindex = XHANDLE_IDX(xhandle);
    tassert(headBLock.isLocked(),
	    err_printf("XObjectList called without lock %p\n", this));
    headBLock.get(indexBits);

    if (GetNext(xobjindex) == xobjindex) {
	// am only one on queue, so head must point to me
	indexBits.index(XHANDLE_NONE_IDX);
	headBLock.set(indexBits);
	return;
    }
    SetPrev(GetNext(xobjindex), GetPrev(xobjindex));
    SetNext(GetPrev(xobjindex), GetNext(xobjindex));

    // change head unconditionally
    indexBits.index(GetNext(xobjindex));
    headBLock.set(indexBits);
}

SysStatus
XObjectListProc::lockIfNotClosing()
{
    IndexBits indexBits;
    headBLock.acquire(indexBits);
    if (indexBits.closing()) {
	headBLock.release();
	return _SERROR(1216, 0, ENXIO);
    }
    return 0;
}

uval
XObjectListProc::isClosed()
{
    IndexBits indexBits;
    headBLock.get(indexBits);
    return indexBits.closing();
}

uval
XObjectListProc::isEmpty()
{
    IndexBits indexBits;
    headBLock.get(indexBits);
    return (indexBits.index() == XHANDLE_NONE_IDX) | (indexBits.closing());
}

void
XObjectListProc::unlock()
{
    headBLock.release();
}

SysStatus
XObjectListProc::close()
{
    IndexBits indexBits;
    uval head;
    ObjRef objRef;
    XBaseObj *xobj;
    while (1) {
	headBLock.acquire(indexBits);
	if (indexBits.closing()) {
	    // Some one else made the closing transition
	    headBLock.release();
	    return _SERROR(1221, 0, ESRCH);
	}
	head = indexBits.index();
	if (head == XHANDLE_NONE_IDX) {
	    indexBits.closing(1);
	    headBLock.release(indexBits);
	    return 0;
	}
	xobj = XHandleTrans::GetPointer(head);
	tassert(xobj->isValid(),
		err_printf("Already freed XObj still on chain"));
	objRef = xobj->getObjRef();
	tassert(objRef != NULL,
		err_printf("Valid XObj without object %p\n", this));
	headBLock.release();
	DREF(objRef)->releaseAccess(xobj->getXHandle());
    }
}
