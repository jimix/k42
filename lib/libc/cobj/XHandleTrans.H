#ifndef __XHANDLE_TRANS_H_
#define __XHANDLE_TRANS_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: XHandleTrans.H,v 1.42 2004/07/11 21:59:23 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *
 * Contains global definitions for external
 * object translation/invocation subsystem.
 * **************************************************************************/

#include <xobj/XBaseObj.H>

#define XHANDLE_MAKE(idx,seqno) \
    (((idx) << _XHANDLE_IDX_SHIFT) | ((seqno) << _XHANDLE_SEQNO_SHIFT))
#define XHANDLE_MAKE_NOSEQNO(idx) \
    ((idx) << _XHANDLE_IDX_SHIFT)
#define XHANDLE_SEQNO(xhandle) \
    (((xhandle)>>_XHANDLE_SEQNO_SHIFT) & ((uval(1)<<_XHANDLE_SEQNO_BITS)-1))
#define XHANDLE_IDX(xhandle) \
    (((xhandle)>>_XHANDLE_IDX_SHIFT) & ((uval(1)<<_XHANDLE_IDX_BITS)-1))
#define XHANDLE_NONE_IDX XHANDLE_IDX(XHANDLE_NONE)

class XHandleTrans : public Obj {

    friend class XObjectListObj;
    friend class XObjectListProc;

protected:
    enum {
	XHANDLE_TABLE_MAX = 0x800000,
	XHANDLE_TABLE_INCREMENT = 0x40,
	XHANDLE_TABLE_GC_INTERVAL = 0x20
    };

    static XBaseObj *XHandleTable;

    XBaseObj *freeEntries;	    // available for reuse
    XBaseObj *removedEntries;	    // freed in this token generation
    PtrBLock<XBaseObj> oldEntries;  // freed in last generation
				    // this is processToken lock
    COSMgr::ThreadMarker marker;    // oldEntries generation marker
    uval numberRemoved;             // aprox number of removedEntries
    XBaseObj *xhandleTable;         // array of x-objects
    uval xhandleTableLimit;	    // current number of entries

    // called internally when processing is needed
    void processToken();

    // called internally to initialize a new chunk of the xhandle table
    static void InitXHandleRange(XBaseObj *table,
				  uval oldLimit, uval newLimit,
				  XBaseObj*& top, XBaseObj*& bottom);

    // Init creates exactly one of these as a static
    XHandleTrans() { /* empty body: null constructor protected */ }

    // only classes gets xobj to xhandle
    friend class DispatcherDefault;
    friend class DispatcherDefaultExp;

    virtual SysStatus demolish(XHandle xhandle, uval &clientData);
    virtual SysStatus xhandleToXObj(XHandle, XBaseObj *&);

    DEFINE_GLOBAL_NEW(XHandleTrans);
public:

    static SysStatus ClassInit(VPNum vp);

    // N.B. this routine takes an index, NOT an xhandle
    static XBaseObj* GetPointer(uval32 index) { return &(XHandleTable[index]);}

    /* used in tasserts
     */
    virtual SysStatus xhandleValidate(XHandle);

    /*
     * If access rights match, returns back object and the type of
     * the object.
     */
    static SysStatus XHToInternal(XHandle xh, ProcessID pid, AccessRights r,
				  ObjRef &ref, TypeID &type);

    /*
     * Check that xhandle is valid, and if so return everything we know
     * about it.
     */
    static SysStatus GetInfo(XHandle xhandle,
			     TypeID& xhType,
			     AccessRights& match,
			     AccessRights& nomatch);

    // functions on local x translation table

    /*
     * Allocates an XObj, attaches to process list, and to object
     * list, and then returns the new xhandle, this is called only by
     * MetaObjects.
     */
    virtual SysStatusXHandle alloc(ObjRef oref, BaseProcessRef matchProc,
				   AccessRights matched,
				   AccessRights unmatched,
				   COVTableEntry* vtbl, uval8 numMethods,
				   uval clientData=0);

    /*
     * Make this XObj no longer active.  It is queued for
     * eventual reuse once the token has circulated.
     * Server can override the removeExportedXObj call to
     * deal with per-client processing.
     * N.B. after free, other calls may still be in progress
     * using this XObj.
     */
    virtual SysStatus free(XHandle xhandle);

    /*
     * static services for operating on XHandles
     * xobjects are not normally exposed
     */
    static XHandle GetXHandle(XBaseObj *xobj, uval8 seqNo) {
 	return XHANDLE_MAKE(xobj - XHandleTable, seqNo);
    }
    static void SetBeingFreed(XHandle xhandle,
			      void (*callback) (XHandle xhandle)) {
	XHandleTable[XHANDLE_IDX(xhandle)].setBeingFreed(callback);
    }

    static void GetRights(XHandle xhandle, ProcessID& id,
			  AccessRights& mrights,
			  AccessRights& urights) {
	XHandleTable[XHANDLE_IDX(xhandle)].getRights(id, mrights, urights);
    }

    static uval GetClientData(XHandle xhandle) {
	return XHandleTable[XHANDLE_IDX(xhandle)].getClientData();
    }

    static void SetClientData(XHandle xhandle, uval value) {
	XHandleTable[XHANDLE_IDX(xhandle)].setClientData(value);
    }

    static ProcessID GetOwnerProcessID(XHandle xhandle) {
	return XHandleTable[XHANDLE_IDX(xhandle)].__matchPID;
    }

    static TypeID GetTypeID(XHandle xhandle) {
	return XHandleTable[XHANDLE_IDX(xhandle)].XtypeID();
    }

    static SysStatus GetTypeName(XHandle xhandle, char* buf, uval buflen) {
	return XHandleTable[XHANDLE_IDX(xhandle)].XtypeName(buf, buflen);
    }
    static XHandle GetGlobalXH(ObjRef ref) {
	if (!CObjGlobals::IsGlobalRef(ref)) return XHANDLE_NONE;
	return XHANDLE_MAKE_NOSEQNO(CObjGlobals::GlobalXHandleIdx(ref));
    }

    // For use after fork, to clean up xhandles in the child.
    // Use this on objects that have no prefork logic, but require
    // post-fork cleanup.  Note that such objects have essentially
    // bogus xhandles (because they're from the parent), demolish
    // is tolerant of such conditions.
    static SysStatus Demolish(XHandle xhandle, uval &clientData) {
	return DREFGOBJ(TheXHandleTransRef)->demolish(xhandle, clientData);
    }
};

/*
 * NOTE:  This method is called by stub-compiled code, so we want it to
 *        be inlined.  But its implementation requires XHandleTrans.H,
 *        which can't be included in XBaseObj.H without introducing a
 *        circularity.  Therefore the implementation itself is here, and
 *        files that want to use getXHandle() will have to include
 *        this header.
 */
inline XHandle XBaseObj::getXHandle()
{
    return XHandleTrans::GetXHandle(this, seqNo);
}

#endif /* #ifndef __XHANDLE_TRANS_H_ */
