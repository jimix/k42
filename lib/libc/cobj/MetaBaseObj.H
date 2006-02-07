#ifndef __META_BASE_OBJ_H_
#define __META_BASE_OBJ_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: MetaBaseObj.H,v 1.10 2001/11/13 18:29:09 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Class Descriptor Object reendering services to
 * be available to all instances. Used in TypeServices and static PPC requests
 * **************************************************************************/

#include <xobj/XBaseObj.H>

class MetaBaseObj : public XBaseObj {
    TYPE_DECL(MetaBaseObj,<InValid>);
public:
    enum { none         = 0x0,
	   globalHandle = 0x1,  // For creating well-known xhandles to
				// global (within this addr space) objects
	   processOnly  = 0x2,
	   _rightsAvail = 0x4,
    };

    // here to catch error, should never declare an instance
    // of a meta object
    MetaBaseObj();

    // need a fake type to allow magic constructions to generate vtable
    enum  FakeType {MAGIC};
    MetaBaseObj(FakeType) { /* empty body */ }
};

#define NO_RIGHTS (MetaBaseObj::any)

#endif /* #ifndef __META_BASE_OBJ_H_ */
