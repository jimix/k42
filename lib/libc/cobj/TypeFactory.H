#ifndef __TYPE_FACTORY_H_
#define __TYPE_FACTORY_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TypeFactory.H,v 1.2 2001/10/05 21:47:22 peterson Exp $
 *****************************************************************************/

#include "TypeMgr.H"

class TypeFactory {
protected:
    uval         id;      // FIXME: temp hack without shared libraries
    TypeMgrEntry *entry;  // the associated type entry

    // the list of instances created by this factory
    ListSimple<uval, AllocGlobal> instList;

public:
    // FIXME: part of no-shared-library hack
    virtual uval getID(void) { return id; }

    // Each factory must be able to create objects of the associated type
    virtual SysStatus Create(uval *ref) = 0;

    // Need to be able to set the associated type entry
    virtual SysStatus setEntry(TypeMgrEntry *e) {
        entry = e;
        return 0;
    }
    DEFINE_NOOP_NEW(TypeFactory);
    virtual ~TypeFactory() { }
    // FIXME: interface to track/locate/swap instances created by factory
};


// FIXME: This is a temporary hack at creating factories in an
//        application's address space on demand until shared library
//        support is in place.
#include "Example.H"

class FactoryTable {
public:
    static TypeFactory *getFact(uval factoryID) {
        switch(factoryID) {
        case(1):
            return (new ExampleFactory);
            break;
        case(2):
            return (new ExampleAFactory);
            break;
        case(3):
            return (new ExampleBFactory);
            break;
        }

        return NULL;
    }
};

#endif /* #ifndef __TYPE_FACTORY_H_ */
