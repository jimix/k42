#ifndef __ASYNC_TESTER_H_
#define __ASYNC_TESTER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AsyncTester.H,v 1.17 2002/07/02 18:13:05 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Simple Object to test Asynchronous Calls
 * **************************************************************************/

class AsyncTester :  public Obj {
public:
    AsyncTester() { /* empty body */ }
    DEFINE_GLOBAL_NEW(AsyncTester);
    ObjRef getRef();
__xpublic:
    virtual                 SysStatus testSync (uval arg);
    virtual __async         SysStatus testAsync(uval arg);
};

#endif /* #ifndef __ASYNC_TESTER_H_ */
