#ifndef __COBJ_REP_H_
#define __COBJ_REP_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: CObjRep.H,v 1.13 2001/10/16 05:31:48 okrieg Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description: base clustered object
 * **************************************************************************/
#include <cobj/CObjRoot.H>

class CObjRoot;

class CObjRep : public COSTransObject {
protected:
    CObjRoot *myRoot;
public:
    void* operator new(size_t size);
    CObjRep() : myRoot(0) { /* empty body */ }
    CObjRep(CObjRoot *root) : myRoot(root) { /* empty body */ }

    // FIXME: These calls should not really be public
    void setRoot(CObjRoot *root) { myRoot = root; }
    // FIXME: If the poor perf of a method call here is a problem
    //        we then need to consider caching the ref in the rep
    //        and returning it directly with out call the root.
    RepRef getRef() { return (RepRef) myRoot->getRef(); }

    // cleanup should only be called by a root
    // FIXME:  unfortunately there is no nice way to express this
    virtual SysStatus cleanup()  { delete this; return 1; }
};


#endif /* #ifndef __COBJ_REP_H_ */
