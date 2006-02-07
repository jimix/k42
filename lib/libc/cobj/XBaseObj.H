#ifndef __XBASE_OBJ_H_
#define __XBASE_OBJ_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: XBaseObj.H,v 1.36 2003/12/03 18:52:47 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    Definition of external Object
 * **************************************************************************/

#include <cobj/BaseObj.H>
#include <cobj/ObjectRefs.H>
#include <cobj/sys/COVTable.H>

/*
 * We define a core class with a virtual function to ensure that the virtual
 * function table pointer is the first word of any XBaseObj instance.
 */
class XBaseObjCore {
public:
    virtual void ___dummy() { /* empty body: bogus function*/ }
};

class XBaseObj : public XBaseObjCore {
public:

    // Use "X" to distinguish these names from "typeName" and "typeID"
    // methods present in Meta* classes, which inherit from XBaseObj
    virtual SysStatus XtypeName(char* buf, uval buflen){return 0;};
    virtual TypeID XtypeID(){return 0;};
protected:
    // an XObj is accessed by assembly
    friend class DispatcherDefault;
    friend class DispatcherDefaultExp;
    friend class XHandleTrans;
    friend class XObjectListObj;
    friend class XObjectListProc;
    friend void genConstants();

    enum {LOG_SIZE_IN_UVALS = 3};	// XBaseObj padded to 8 uvals.

    /*
     * the methods declared here are not valid for PPC
     * the first PPC method number is recorded below
     * by experiment.
     * We make this public because its used in test cases (at least)
     * and keeping it protected just causes people to copy the
     * current value.
     */
public:
#if __GNUC__ < 3
    enum {FIRST_METHOD = 4};
#else
    enum {FIRST_METHOD = 3};
#endif

protected:
    // exactly 7 uvals long - with vtable pointer this is 64 bytes
    ObjRef       __iobj;

    uval8        __nummeth;
    uval8        seqNo;
    uval8        filler0;
    uval8        filler1;
    // should be ProcessID - but that's not been converted to uval32 yet
    uval32       __matchPID;

    AccessRights __mrights;
    AccessRights __urights;

    /*
     * WARNING
     * at remove time, we first call removeMatched, thus
     * making the onProc union available.
     * We then call removeExported, at which time the
     * user may use setBeingFreed.
     * So the pairing of beingFreed and onProc is necessary
     * If beingFreed was paired with onObj, the user might
     * accidently overwrite onObj before calling Obj::removeExported
     */
    union {
	// destroy call back when on free list
	void (*beingFreed) (XHandle xhandle);
	// proc chain, using indexes, when valid
	struct { uval32 next, prev; } onProc;
    };

    union {
	// free list when not valid
	XBaseObj   *nextFree;
	// obj chain, using indexes, when valid
	struct { uval32 next, prev; } onObj;
    };

    uval clientData;

    static COVTableEntry **FTableAddr(XBaseObj *xobj) {
	// VTable pointer is first word of object.
        return (COVTableEntry **) xobj;
    }

    static void SetFTable(XBaseObj *xobj, COVTableEntry *vtbl) {
	(*FTableAddr(xobj)) = vtbl;
    }

    static COVTableEntry *GetFTable(XBaseObj *xobj) {
	xobj->___dummy();	// make sure vtable ptr is initialized
        return *FTableAddr(xobj);
    }

    ObjRef      getObjRef() { return __iobj; }
    XBaseObj*   getNextFree() { return nextFree; }
    void        setNextFree(XBaseObj *nf) {nextFree = nf; }
    uval        getClientData() { return clientData; }
    void        setClientData(uval value) { clientData = value; }

    // returns 0 if does not match
    uval match(ProcessID id, AccessRights mrights, AccessRights urights) {
	if ((__matchPID == id)   &&
	    (__mrights  == mrights) &&
	    (__urights  == urights)) {
	    return 1;
	}
	return 0;
    }

    ProcessID getOwnerProcessID() { return __matchPID; }

    void getRights(ProcessID& id,
		   AccessRights& mrights,
		   AccessRights& urights) {
	id = __matchPID;
	mrights = __mrights;
	urights = __urights;
    }

    uval isValid() { return __nummeth; }

    void setBeingFreed(void (*callback)(XHandle xhandle)) {
	beingFreed = callback;
    }

    void callBeingFreed() {
	if (beingFreed) (*beingFreed)(getXHandle());
    }

    /*
     * NOTE:  This method is called by stub-compiled code, so we want it to
     *        be inlined.  But its implementation requires XHandleTrans.H,
     *        which can't be included here without introducing a circularity.
     *        Therefore the implementation itself is in XHandleTrans.H, and
     *        files that want to use getXHandle() will have to include
     *        that header.
     */
    XHandle getXHandle();

public:

    XBaseObj() { }  // for intermediate compilation and initialization
};

#endif /* #ifndef __XBASE_OBJ_H_ */
