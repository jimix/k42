/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TypeMgrServer.C,v 1.9 2004/07/11 21:59:23 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of the KernelTypeMgr
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <cobj/TypeMgr.H>
#include "TypeMgrServer.H"
#include <cobj/ObjectRefs.H>
#include "CObjRootSingleRep.H"
#include <meta/MetaObj.H>
#include <meta/MetaTypeMgrServer.H>

/* static */ void
TypeMgrServer::ClassInit(VPNum vp)
{
    if (vp!=0) return;			// nothing to do on second proc
    TypeMgrServer *typeMgrPtr;

    typeMgrPtr = new TypeMgrServer();

    // initialize the TypeMgr structure
    typeMgrPtr->lock.init();
    typeMgrPtr->freeEntries = NULL;
    for (uval i= 0; i<TYPE_MGR_HASHNUM; i++) {
	typeMgrPtr->hashtab[i] = NULL;
    }

    typeMgrPtr->backupServer = 0;

    // now register ourselves in the global object table
    CObjRootSingleRepPinned::Create(typeMgrPtr, GOBJ(TheTypeMgrRef));
    // kernel initialization path
    // now register the default types immediately by accessing their
    // typeid
    (void)StubObj::typeID();
    (void)MetaObj::typeID();

    XHandle xhandle =
	MetaTypeMgrServer::createXHandle(typeMgrPtr->getRef(),
					 GOBJ(TheProcessRef), 0, 0);
    typeMgrPtr->oh.initWithMyPID(xhandle);
}

/* virtual */ SysStatus
TypeMgrServer::_registerType(__in       const TypeID parentid,
				__inbuf(*) const char  *clsname,
				__in             uval   signature,
				__out            TypeID &id)
{
    return registerType(parentid, clsname, signature, id);
}

/* virtual */ SysStatus
TypeMgrServer::_registerTypeHdlr(__in const TypeID id,
				    __in ObjectHandle oh)
{
    return registerTypeHdlr(id, oh);
}

/* virtual */ SysStatus
TypeMgrServer::getTypeMgrOH(ObjectHandle& returnOh)
{
    returnOh = oh;
    return 0;
}

/* virtual */ SysStatus
TypeMgrServer::_getTypeHdlr(__in const TypeID id,
			       __out ObjectHandle &oh)
{
    return getTypeHdlr(id, oh);
}

/* virtual */ SysStatus
TypeMgrServer::_isDerived(__in const TypeID derivedId,
			     __in const TypeID baseId)
{
    return isDerived(derivedId, baseId);
}

/* virtual */ SysStatus
TypeMgrServer::_typeName(__in const TypeID id,
			    __outbuf(*:buflen) char *buf,
			    __in const uval buflen)
{
    return typeName(id, buf, buflen);
}

/* virtual */ SysStatus
TypeMgrServer::_dumpTree(__in const uval global)
{
    return dumpTree(global);
}

/* virtual */ SysStatus
TypeMgrServer::_locateType(__inbuf(*) const char *name,
                           __out TypeID &id)
{
    return locateType(name, id);
}

/* virtual */ SysStatus
TypeMgrServer::_locateName(__in const TypeID id,
                           __outbuf(*:buflen) char *name,
                           __in const uval nameLen)
{
    return locateName(id, name, nameLen);
}

/* virtual */ SysStatus
TypeMgrServer::_hasType(__in const TypeID id)
{
    return hasType(id);
}

/* virtual */ SysStatusUval
TypeMgrServer::_locateParent(__in const TypeID id)
{
    return locateParent(id);
}

/* virtual */ SysStatusUval
TypeMgrServer::_locateFactoryID(__in const TypeID id)
{
    return locateFactoryID(id);
}

/* virtual */ SysStatusUval
TypeMgrServer::_registerFactory(__in const TypeID id,
                                __in const uval factoryID)
{
    return registerFactory(id, factoryID);
}

/* virtual */ SysStatus
TypeMgrServer::_getChildren (__in const TypeID id,
                             __outbuf(outSize:arraySize) uval64 *children,
                             __in const uval arraySize,
                             __out uval &outSize)
{
    return getChildren(id, children, arraySize, outSize);
}
