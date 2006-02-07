#ifndef __USR_TST_H_
#define __USR_TST_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: UsrTst.H,v 1.12 2001/10/16 05:32:16 okrieg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Provides simple test interface to a user program
 * **************************************************************************/


// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class   UsrTst;
typedef UsrTst 	     **UsrTstRef;

#include "meta/MetaUsrTst.H"
#include <cobj/TypeMgr.H>

class UsrTst : public Obj {
    UsrTst() { /* empty body */ }
public:
    DEFINE_REFS(UsrTst);
    DEFINE_GLOBAL_NEW(UsrTst);
    DEFINE_ACCESS_SERVER(UsrTst,0,0);

public:
    static UsrTstRef Create();

__xpublic:
    virtual SysStatus gotYa(__in uval i);

};

#endif /* #ifndef __USR_TST_H_ */
