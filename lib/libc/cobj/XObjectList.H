#ifndef __XOBJECT_LIST_H_
#define __XOBJECT_LIST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: XObjectList.H,v 1.5 2002/04/19 17:31:54 rosnbrg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *     maintain a cirular list of XObj's.
 *     head points to some entry.
 *     entries are double linked in a circle
 *     There are two versions, one to mainatain the proc list,
 *           one to maintain the obj list.
 *     No checking is done.
 * **************************************************************************/

//N.B. these includes are needed because XObjectList is used by Obj.H
//     normally, they are included by sysIncs.
//     Be careful if you need to add other things because of this.
#include <misc/BitStructure.H>
#include <sync/Lock.H>
#include <sync/BLock.H>


class XBaseObj;

class XObjectListObj {
    // only this class adds and removes entries to list
    friend class XHandleTrans;

    struct IndexBits : BitStructure {
	__BIT_FIELD(8*sizeof(uval32), index, BIT_FIELD_START);
	__BIT_FIELD(8, closing, index);
	__BIT_FIELD(24, filler, closing);
	// we expect better code if lock bits are all the way
	// to the right because of mask ops with small masks
	__LOCK_BIT(filler, 0);
	__WAIT_BIT(filler, 1);
    };

    BitBLock<IndexBits> headBLock;	// this lock protects the whole
					// list - must be held even if
					// head is not used.
private:
    /*
     * Internal functions to avoid confusion between indexes inXBaseObj
     * for Proc and Obj lists
     * Operate on indexes, not pointers
     */
    static uval GetPrev(uval index);
    static uval GetNext(uval index);
    static void SetPrev(uval target, uval newPrev);
    static void SetNext(uval target, uval newNext);


public:
    void Init()  { headBLock.init(); /* clears index and closing fields */ };
    XObjectListObj()  {Init(); };

    void locked_add(XHandle xhandle);

    void locked_remove(XHandle xhandle);
public:
    /*
     * lock and unlock the holder
     * lock and unlock returns error if the value of the
     * list is CLOSING
     */
    SysStatus lockIfNotClosing();
    void unlockIfNotClosing();

    // remove all XObjects and set closing state
    // returns error if already closed
    SysStatus close();

    // true if this list is closed
    uval isClosed();

    // true if this list is empty or closed
    uval isEmpty();

    // exported routines to allow traversal of list, must acquire lock first
    XHandle getHead();
    XHandle getNext(XHandle xhandle); // returns XHANDLE_NONE if next is head
};

class XObjectListProc {
    friend class XHandleTrans;

    struct IndexBits : BitStructure {
	__BIT_FIELD(8*sizeof(uval32), index, BIT_FIELD_START);
	__BIT_FIELD(8, closing, index);
	__BIT_FIELD(24, filler, closing);
	// we expect better code if lock bits are all the way
	// to the right because of mask ops with small masks
	__LOCK_BIT(filler, 0);
	__WAIT_BIT(filler, 1);
    };

    BitBLock<IndexBits> headBLock;	// this lock protects the whole
					// list - must be held even if
					// head is not used.

private:
    /*
     * Internal functions to avoid confusion between pointers in XObjHandle
     * for Proc and Obj lists
     * Operates on indexes, not pointers
     */
    static uval GetPrev(uval index);
    static uval GetNext(uval index);
    static void SetPrev(uval target, uval newPrev);
    static void SetNext(uval target, uval newNext);

public:
    void Init()  { headBLock.init(); /* clears index and closing fields */ };

    XObjectListProc()  { Init(); };

    void locked_add(XHandle xhandle);

    void locked_remove(XHandle xhandle);

    // remove all XObjects and set closing state
    // returns error if already closed
    SysStatus close();
public:
    /*
     * lock and unlock the holder
     * lock returns error and does not lock if the value of the
     * list is CLOSING
     */
    SysStatus lockIfNotClosing();
    void unlock();

    // true if this list is closed
    uval isClosed();

    // true if this list is empty or closed
    uval isEmpty();
};

#endif /* #ifndef __XOBJECT_LIST_H_ */
