#ifndef __EXAMPLE_H_
#define __EXAMPLE_H_

/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Example.H,v 1.2 2001/10/05 21:47:19 peterson Exp $
 *****************************************************************************/

#include <misc/ListSimpleLocked.H>
#include <cobj/CObjRootSingleRep.H>

class TypeFactory;

class Example;
typedef Example **ExampleRef;

class ExampleFactory : public TypeFactory {
public:
    DEFINE_GLOBAL_NEW(ExampleFactory);

    ExampleFactory() {
        id = 1;
    }

    SysStatus Create(uval *ref) {
        void *ptr;
        TypeMgrEntry *e;// , *pick;
        TypeFactory *factory;

        // XXX - go through the list of sub-factories
        DREFGOBJ(TheTypeMgrRef)->updateChildren(entry->id);
        ptr = entry->childList.next(NULL, e);
        while(ptr) {
            ptr = entry->childList.next(ptr, e);
        }

        // call the one with the highest version number
        factory = (TypeFactory *)DREFGOBJ(TheTypeMgrRef)->locateFactory(e->id);
        return factory->Create(ref);
    }
};

class Example : public Obj {
public:
    virtual int math(int a, int b) = 0;
};




class ExampleA;
typedef ExampleA **ExampleARef;

class ExampleA : public Example {
protected:
    DEFINE_GLOBAL_NEW(ExampleA);

public:
    static SysStatus Create(ExampleARef *r) {
        ExampleA *a;

        a = new ExampleA;
        (*r) = (ExampleARef)CObjRootSingleRep::Create(a);

        return 0;
    }

    int math(int a, int b) {
        return a + b;
    }
};

class ExampleAFactory : public TypeFactory {
public:
    DEFINE_GLOBAL_NEW(ExampleAFactory);

    ExampleAFactory() {
        id = 2;
    }

    SysStatus Create(uval *ref) {
        ExampleARef aRef;

        // create a reference to a ExampleA instance
        ExampleA::Create(&aRef);
        *ref = (uval)aRef;
        instList.add(*ref);

        // FIXME: setup tracking to the instance

        return 0;
    }
};




class ExampleB;
typedef ExampleB **ExampleBRef;

class ExampleB : public Example {
protected:
    DEFINE_GLOBAL_NEW(ExampleB);

public:
    static SysStatus Create(ExampleBRef *r) {
        ExampleB *b;

        b = new ExampleB;
        (*r) = (ExampleBRef)CObjRootSingleRep::Create(b);

        return 0;
    }

    int math(int a, int b) {
        return a - b;
    }
};

class ExampleBFactory : public TypeFactory {
public:
    DEFINE_GLOBAL_NEW(ExampleBFactory);

    ExampleBFactory() {
        id = 3;
    }

    SysStatus Create(uval *ref) {
        ExampleBRef bRef;

        // create a reference to a ExampleB instance
        ExampleB::Create(&bRef);
        *ref = (uval)bRef;
        instList.add(*ref);

        // FIXME: setup tracking to the instance

        return 0;
    }
};

#endif /* #ifndef __EXAMPLE_H_ */
