#ifndef __OBJ_H_
#define __OBJ_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Obj.H,v 1.76 2004/02/27 17:06:00 mostrows Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the fundamental interface all IO
 * objects support
 * **************************************************************************/

#include <cobj/BaseObj.H>
#include <cobj/XObjectList.H>

/*
 * access rights:
 *   controlAccess - specifies that a process can give this access to another
 *      process.  Normally, only owner has giveAccess
 *   attach - specifies that a process has the rights to connect two objects
 *      together ala building blocks,
 *   lookup - used for all naming objects to look up objects and read dirs
 */
class Obj : public BaseObj __xadef(controlAccess:attach:lookup:read:write) {
protected:
    Obj() { /* empty body */ }

public:
    // this is basic implementation of server side remove
    virtual SysStatus releaseAccess(XHandle xhandle);

    // most exported operations are non-virtual and protected
    // these are variants used by XHandleTrans
    virtual SysStatus publicLockExportedXObjectList();

    virtual SysStatus publicUnlockExportedXObjectList();

    // maintains list of XObjects exported by the internal object
    virtual SysStatus addExportedXObj(XHandle xhandle);

    virtual SysStatus removeExportedXObj(XHandle xhandle);

    // override handleXObjFree to deal with user data when xobject is removed
    // use xobj->setBeingFreed to request a second call back when
    // the xobj is freed - use this to delete storage pointed to by
    // the xobj
    virtual SysStatus handleXObjFree(XHandle xhandle) {
	return 0;
    }

protected:
    // when last xobject released, this function is called, you override
    // this function if you want to destroy when list goes empty.
    // Note, the semantics are, if the list ever goes empty, this function is
    // called.  For erronious applications that call give access at the same
    // time as releaseAccess the list may become non-empty again, but
    // a destroy of the object under these conditions seems reasonable.
    virtual SysStatus exportedXObjectListEmpty();


    // N.B. Not virtual
    SysStatus closeExportedXObjectList() {
	return myRoot->exported.close(); }

    SysStatus isClosedExportedXObjectList() {
	return myRoot->exported.isClosed(); }

    SysStatus isEmptyExportedXObjectList() {
	return myRoot->exported.isEmpty(); }

    SysStatus lockIfNotClosingExportedXObjectList() {
	return myRoot->exported.lockIfNotClosing();
    }

    SysStatus unlockExportedXObjectList() {
	myRoot->exported.unlockIfNotClosing(); return 0;
    };

    XHandle getHeadExportedXObjectList() {
	return myRoot->exported.getHead();
    }

    XHandle getNextExportedXObjectList(XHandle xhandle) {
	return myRoot->exported.getNext(xhandle);
    }

public:
    /*
     * these are a convenience function to deal with
     * giveAccess and releaseAccess when
     * there is no other reason to make a stub or wrapper object.
     * N.B. this is NOT a Meta function.  It is executed in the
     * client.
     */
    static SysStatus ReleaseAccess(ObjectHandle oh);
    static SysStatus AsyncReleaseAccess(ObjectHandle oh);
    static SysStatus GiveAccessByClient(ObjectHandle oh,
					ObjectHandle& newOh,
					ProcessID toProcID);

__xprotected:

    // We can't use default argument values in stub-compiled calls,
    // thus we have to replicate these definitions to simulate
    // optional parameters.

    // Duplicate the XObj.
    // This is our mechanism for passing capabilities, it returns
    // an object handle for (possibly) new XObj.
    virtual SysStatus _giveAccess(__out ObjectHandle &oh,
				  __in ProcessID toProcID,
				  __XHANDLE xhandle) __xa(controlAccess);

    // Duplicate the XObj reducing access
    // This is our mechanism for passing capabilities, it returns
    // an object handle for new XObj.
    // It is an error to ask to pass a right that you don't have
    virtual SysStatus _giveAccess(__out ObjectHandle &oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle) __xa(controlAccess);


    // Duplicate the XObj reducing access, and specify the type of
    // ObjectHandle created.
    virtual SysStatus _giveAccess(__out ObjectHandle &oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __in TypeID type,
				  __XHANDLE xhandle) __xa(controlAccess);

    // destroys this XObject
    virtual SysStatus _releaseAccess(__XHANDLE xhandle) __xa(controlAccess);

    virtual __async SysStatus _asyncReleaseAccess(__XHANDLE xh) __xa(controlAccess);

    // call from kernel to re-open this file
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __XHANDLE xhandle);
    virtual SysStatus _lazyReOpen(__out ObjectHandle & oh,
				  __in ProcessID toProcID,
				  __in AccessRights match,
				  __in AccessRights nomatch,
				  __inoutbuf(datalen:datalen:datalen) char *data,
				  __inout uval& datalen,
				  __XHANDLE xhandle);

    /*
     * Return generic information about the called xhandle.
     */
    virtual SysStatus _getInfo(__out TypeID& xhType,
			       __out AccessRights& match,
			       __out AccessRights& nomatch,
			       __XHANDLE xhandle);
};


extern Obj* theDeletedObj;

#endif /* #ifndef __OBJ_H_ */
