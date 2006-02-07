#ifndef __TYPE_MGR_SERVER_H_
#define __TYPE_MGR_SERVER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TypeMgrServer.H,v 1.9 2001/10/05 21:47:22 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *      Definition of the server side of TypeMgr
 ****************************************************************************/

#include <cobj/TypeMgr.H>

class TypeMgrServer : public TypeMgr __xbase(Obj) __xa(none) {
    ObjectHandle     oh;           // entry for external handle

    virtual SysStatus getTypeMgrOH(ObjectHandle& returnOh);

    DEFINE_GLOBAL_NEW(TypeMgrServer);

__xprivate:
    virtual SysStatus _registerType     (__in       const TypeID parentid,
					 __inbuf(*) const char  *clsname,
					 __in             uval   signature,
					 __out            TypeID &id);
    virtual SysStatus _registerTypeHdlr (__in const TypeID id,
					 __in ObjectHandle oh);
    virtual SysStatus _getTypeHdlr      (__in const TypeID id,
					 __out ObjectHandle &oh);
    virtual SysStatus _isDerived        (__in const TypeID derivedId,
					 __in const TypeID baseId);
    virtual SysStatus _typeName         (__in const TypeID id,
					 __outbuf(*:buflen) char *buf,
					 __in const uval buflen);
    virtual SysStatus _dumpTree         (__in const uval global = 0);
    virtual SysStatus _locateType       (__inbuf(*) const char *name,
                                         __out TypeID &id);
    virtual SysStatus _locateName       (__in const TypeID id,
                                         __outbuf(*:nameLen) char *name,
                                         __in const uval nameLen);
    virtual SysStatus _hasType          (__in const TypeID id);
    virtual SysStatusUval _locateParent (__in const TypeID id);
    virtual SysStatusUval _locateFactoryID (__in const TypeID id);
    virtual SysStatus _registerFactory  (__in const TypeID id,
                                         __in const uval factoryID);
    virtual SysStatus _getChildren      (__in const TypeID id,
                                         __outbuf(outSize:arraySize)
                                             uval64 *children,
                                         __in const uval arraySize,
                                         __out uval &outSize);

public:
    static void ClassInit(VPNum vp);
};

#endif /* #ifndef __TYPE_MGR_SERVER_H_ */
