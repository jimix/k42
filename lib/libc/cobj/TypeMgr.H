#ifndef __TYPE_MGR_H_
#define __TYPE_MGR_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TypeMgr.H,v 1.36 2001/10/16 05:31:50 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *      Definition of the TypeMgr
 ****************************************************************************/

#include <stub/StubObj.H>
#include "alloc/AllocPool.H"
#include "misc/ListSimpleLocked.H"

#define TYPE_MGR_HASHNUM (32)
#define TYPEID_NONE (1<<24)

class StubTypeMgrServer;
class TypeFactory;

struct TypeMgrEntryString {
    uval32	length;
    uval32	size;
    char 	*name;
};

struct TypeMgrEntry {
    TypeID             id;
    ObjectHandle       oh;
    uval               signature;
    uval	       localPtr;
    TypeMgrEntryString string;
    TypeMgrEntry       *parent;
    TypeMgrEntry       *next;

    TypeFactory        *factory;
    ListSimpleLocked<TypeMgrEntry *, AllocGlobal> childList;

    DEFINE_GLOBAL_NEW(TypeMgrEntry);

    TypeMgrEntry() {
	string.length = string.size = 0;
	string.name = NULL; }
};

class TypeMgr : public Obj {
protected:
    friend class TypeMgrServer;
    // init creates the only TypeMgr for a process as a static
    TypeMgr() { /* empty body */ }
    BLock lock;
    TypeMgrEntry* freeEntries;
    TypeMgrEntry* hashtab[TYPE_MGR_HASHNUM];
    StubTypeMgrServer*  backupServer; // server in different address space

    TypeID          hash(const char* name, uval hv);
    TypeMgrEntry* locate(TypeID id);

    SysStatus locked_typeName(const TypeID id, char *buf, const uval buflen);

    DEFINE_GLOBALPADDED_NEW(TypeMgr);

public:
    static void ClassInit(VPNum vp, StubTypeMgrServer *backup);

    virtual SysStatus getTypeMgrOH(ObjectHandle& returnOh);
    virtual SysStatus registerType(const TypeID parentid, const char  *clsname,
				   uval signature, TypeID &id);
    virtual SysStatus registerTypeHdlr(const TypeID id,
				       ObjectHandle oh, uval localPtr=0);
    virtual SysStatus getTypeHdlr(const TypeID id, ObjectHandle &oh);
    virtual SysStatusUval getTypeLocalPtr(const TypeID id);
    virtual SysStatus resetTypeHdlr(const TypeID id);
    virtual SysStatus isDerived(const TypeID derivedId, const TypeID baseId);
    virtual SysStatus typeName(const TypeID id, __outbuf(*:buflen) char *buf,
			       const uval buflen);
    virtual SysStatus dumpTree(const uval global = 0);

    virtual SysStatus locateType(const char *name, TypeID &id);
    virtual SysStatus locateName(const TypeID id, char *name, uval nameLen);
    virtual SysStatus hasType(const TypeID id);
    virtual SysStatusUval locateParent(const TypeID id);
    virtual SysStatusUval locateFactoryID(const TypeID id);
    virtual SysStatus registerFactory(const TypeID id, uval factoryID);
    virtual SysStatusUval locateFactory(const char *typeName);
    virtual SysStatusUval locateFactory(const TypeID id);
    virtual SysStatus updateChildren(const TypeID id);
    virtual SysStatus getChildren(const TypeID id, uval64 *children,
                                  const uval arraySize, uval &outSize);
};

#endif /* #ifndef __TYPE_MGR_H_ */
