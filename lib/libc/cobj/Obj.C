/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Obj.C,v 1.41 2004/02/27 17:05:59 mostrows Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/XHandleTrans.H>
#include <cobj/TypeMgr.H>
#include <stub/StubObj.H>
#include <meta/MetaObj.H>

SysStatus
Obj::_giveAccess(ObjectHandle & oh, ProcessID toProcID,
		 __XHANDLE xhandle)
{
    AccessRights match,nomatch;
    ProcessID dummy;
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    return giveAccessByServer(oh, toProcID, match, nomatch);
}

/*
 * rights requested must be a subset of rights held
 * Again, need controlAccess permission but need not be
 * owner
 */
SysStatus
Obj::_giveAccess(ObjectHandle& oh, ProcessID toProcID,
		 __in AccessRights newmatch, __in AccessRights newnomatch,
		 __XHANDLE xhandle)
{
    AccessRights match,nomatch;
    ProcessID dummy;
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    if ((newmatch&(~match))|(newnomatch&(~nomatch))) {
	return _SERROR(2534, 0, EPERM);
    }
    return giveAccessByServer(oh, toProcID, newmatch, newnomatch);
}

/*
 * Same as above, but specify the type of ObjectHandle to create
 */
SysStatus
Obj::_giveAccess(ObjectHandle& oh, ProcessID toProcID,
		 __in AccessRights newmatch, __in AccessRights newnomatch,
		 __in TypeID type, __XHANDLE xhandle)
{
    AccessRights match,nomatch;
    ProcessID dummy;
    XHandleTrans::GetRights(xhandle, dummy, match, nomatch);
    if ((newmatch&(~match))|(newnomatch&(~nomatch))) {
	return _SERROR(2535, 0, EPERM);
    }

    return giveAccessByServer(oh, toProcID, newmatch, newnomatch, type);
}


SysStatus
Obj::_releaseAccess(__XHANDLE handle)
{
    return releaseAccess(handle);
}

SysStatus
Obj::_asyncReleaseAccess(__XHANDLE handle)
{
    return releaseAccess(handle);
}

/* virtual */ SysStatus
Obj::publicLockExportedXObjectList()
{
    return myRoot->exported.lockIfNotClosing();
};

/* virtual */ SysStatus
Obj::publicUnlockExportedXObjectList()
{
    myRoot->exported.unlockIfNotClosing();
    return 0;
};

/* virtual */ SysStatus
Obj::addExportedXObj(XHandle xhandle)
{
    myRoot->exported.locked_add(xhandle);
    return 0;
}

/* virtual */ SysStatus
Obj::removeExportedXObj(XHandle xhandle)
{
    myRoot->exported.locked_remove(xhandle);
    return 0;
}


// called when list goes empty - override if you're interested
/* virtual */SysStatus
Obj::exportedXObjectListEmpty()
{
    return 0;
}

SysStatus
Obj::releaseAccess(XHandle xhandle)
{
    SysStatus rc = DREFGOBJ(TheXHandleTransRef)->free(xhandle);
    tassertWrn( _SUCCESS(rc), "object already freed?\n");
    if (isEmptyExportedXObjectList()) {
	exportedXObjectListEmpty();
    }
    return rc;
}

/*static*/ SysStatus
Obj::ReleaseAccess(ObjectHandle oh)
{
    StubObj stubObj(StubObj::UNINITIALIZED);
    stubObj.setOH(oh);
    return stubObj._releaseAccess();
}

/*static*/ SysStatus
Obj::AsyncReleaseAccess(ObjectHandle oh)
{
    StubObj stubObj(StubObj::UNINITIALIZED);
    stubObj.setOH(oh);
    return stubObj._asyncReleaseAccess();
}

/*static*/ SysStatus
Obj::GiveAccessByClient(ObjectHandle oh, ObjectHandle &newOh, ProcessID procID)
{
    StubObj stubObj(StubObj::UNINITIALIZED);
    stubObj.setOH(oh);
    return stubObj._giveAccess(newOh, procID);
}

/* virtual */ SysStatus
Obj::_lazyReOpen(__out ObjectHandle & oh, __in ProcessID toProcID,
		 __in AccessRights match, __in AccessRights nomatch,
		 __XHANDLE xhandle)
{
    tassertMsg(0, "lazy reopen called on obj\n");
    return 0;
}

/* virtual */ SysStatus
Obj::_lazyReOpen(__out ObjectHandle & oh, __in ProcessID toProcID,
		 __in AccessRights match, __in AccessRights nomatch,
		 __inoutbuf(datalen:datalen:datalen) char *data,
		 __inout uval& datalen,
		 __XHANDLE xhandle)
{
    tassertMsg(0, "lazy reopen for files called on obj\n");
    return 0;
}

/*
 * Return generic information about the called xhandle.
 */
/* virtual */ SysStatus
Obj::_getInfo(__out TypeID& xhType,
	      __out AccessRights& match,
	      __out AccessRights& nomatch,
	      __XHANDLE xhandle)

{
    return XHandleTrans::GetInfo(xhandle, xhType, match, nomatch);
}
