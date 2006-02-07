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
 * $Id: Obj.H,v 1.6 2003/10/08 19:09:03 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Defines the fundamental interface all IO
 * objects support
 * **************************************************************************/

#include <KFSDebug.H>

/*
 * We define a core class with a virtual function to ensure that the virtual
 * function table pointer is the first word of any Obj instance.
 * (stolen from lib/libc/cobj/XBaseObj.H)
 */
class ObjCore {
public:
    virtual void ___dummy() { /* empty body: bogus function*/ }
};

class Obj;
typedef Obj                  **ObjRef;

/*
 * This class tries to imitate K42's BaseObj. All we have here is a
 * reference to the object that extends this class.
 */
class Obj : public ObjCore {
    Obj *ref;
protected:
    Obj() {
	ref = (Obj *)this;
//	KFS_DPRINTF(DebugMask::LINUX,"(Obj::Obj) Obj construtor this=0x%p, ref=0x%p\n", this, ref);
    }
public:
    ObjRef getRef() {
//	KFS_DPRINTF(DebugMask::LINUX,"(Obj::getRef) getting reference for 0x%p\n", ref);
	return &ref;
    }

};

#endif /* #ifndef __OBJ_H_ */
