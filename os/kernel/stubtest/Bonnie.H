#ifndef __BONNIE_H_
#define __BONNIE_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: Bonnie.H,v 1.39 2001/10/05 21:50:19 peterson Exp $
 *****************************************************************************/
/******************************************************************************
 * This file is derived from Tornado software developed at the University
 * of Toronto.
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *   Definition of a elaborate STUBGENERATOR test class
 * **************************************************************************/

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

#include "meta/MetaBonnie.H"

#ifndef EXPORT_BONNIE
#define EXPORT_BONNIE

struct ohh { unsigned long x, y; };

#endif /* #ifndef EXPORT_BONNIE */

class Bonnie : public Obj
    __xbase(Obj)
    __xadef(read:write:kaka:soso:yippie)
    __xa(processOnly)
{

    uval state;

public:
    Bonnie();
    DEFINE_GLOBAL_NEW(Bonnie);
    DEFINE_ACCESS_SERVER(Bonnie,0,0);
    void init(uval var) { state = var; }
    static void init();

__xpublic:
    static SysStatus construct(__out ObjectHandle & oh,
			       __CALLER_PID processID);
    static SysStatus construct(__out ObjectHandle & oh,
			       __in  int a,
			       __in  int b,
			       __CALLER_PID processID);
    static SysStatus construct(__out ObjectHandle & oh,
			       __in int a,
			       __in int b,
			       __in int c,
			       __CALLER_PID processID);

    static SysStatus aStaticFunc(const long arg);

    virtual SysStatus null   ();
    virtual SysStatus simple (int a1, int a2, int a3, int a4 = 4, int a5 = 5);
    virtual SysStatus simple (__in int arg);
    virtual SysStatus reftest(__inout long & arg);
    virtual SysStatus reftest(__inout long & arg1,
			      __out   long & arg2,
			      __in    long   arg3);

    virtual SysStatus steve  (long arg1,
			      __inout ohh * arg,
			      __inout ohh & arg3);
    virtual SysStatus func1  (char arg1, char arg2, unsigned short arg3);

    virtual SysStatus cohhr  (ohh & arg);
    virtual SysStatus cohhp  (ohh * arg);
    virtual SysStatus m2ohh  (__inout ohh& arg, __out uval &ni);

    /* simple stringtable */
    virtual SysStatus string0 (__inbuf(*)          char* arg1);

    virtual SysStatus string1 (__inbuf(*)          char* arg1,
			       __inbuf(*)          char* arg2);

    virtual SysStatus string2 (__inbuf(*)          char* arg1,
			       __inoutbuf(*:*:20)  char* arg2);

    virtual SysStatus string3 (__inbuf(*)          char* arg1,
			       __inoutbuf(*:*:20)  char* arg2,
			       __outbuf(*:30)      char* arg3);

    virtual SysStatus string4 (__inoutbuf(*:*:20)  char* arg1,
			       __outbuf(*:25)      char* arg2,
			       __inoutbuf(*:*:30) char* arg3,
			       __outbuf(*:35)     char* arg4);

    virtual SysStatus string5 (__outbuf(*:25)       char* arg1,
			       __outbuf(*:30)       char* arg2,
			       __outbuf(*:35)       char* arg3,
			       __outbuf(*:40)       char* arg4);

    virtual SysStatus arraytest (__inbuf(3)         uval *arg1,
				 __outbuf(5:5)      uval *arg2);

    virtual SysStatus inout     (__inbuf(*)          char* lsArgs,
				 __outbuf(*:1024)    char* outputBuf);

    virtual SysStatus varray    (__inbuf(len)        uval *vax,
				 __in                uval  len,
				 __inbuf(*)          char* sayhello);

    virtual SysStatus garray    (__inoutbuf(len:len:lenmax)  uval *va,
				 __inout               uval &len,
				 __in                  uval  lenmax);

    virtual SysStatus varray    (__outbuf(len3:1024)  uval *va1,
				 __inbuf(len2)        uval *va2,
				 __outbuf(*:len2)     char *va3,
			  	 __inbuf(*)           char *va4,
				 __inoutbuf(3:4:100)  uval *va5,
				 __inoutbuf(len1:len2:32)   uval *va6,
				 __in                 uval  len1,
				 __inout              uval *len2,
				 __out                uval &len3,
				 __inbuf(20)          uval *stuff
				);
    virtual SysStatus overflow  (__inoutbuf(*:*:len)  char *va,
				 __inout              uval *len
	                       );

    virtual SysStatus ppc_overflow(__inoutbuf(ilen:olen:mlen) uval *va,
				   __in                       uval ilen,
				   __out                      uval *olen,
			           __in                       uval mlen);

    virtual SysStatus getName(__outbuf(*:len) char *va,
			      __in            uval  len=256);

    virtual SysStatus testBadge(uval x, __CALLER_PID processID);

};

class BonnieChild : public Bonnie  {
public:
    virtual SysStatus getName(__outbuf(*:len) char *va,
			      __in            uval  len=256);
};


class BonnieGrandChild : public BonnieChild __xbase(Bonnie) __xadef(yobaby) {
__xpublic:
    static SysStatus construct(__out ObjectHandle & oh);

};

/* This class is to tests the authentication mechanism
 * Objects are created with certain different rights and then]
 * we run test functions with different required access rights against
 * these objects
 */

class BadgeTester : public Obj  __xbase(Obj) __xadef(read:write:changeXH) {
public:
    DEFINE_GLOBAL_NEW(BadgeTester);
    DEFINE_ACCESS_SERVER(BadgeTester,0,0);
__xpublic:
    static  SysStatus construct    (__out ObjectHandle & oh,
				    __in  AccessRights rights,
				    __CALLER_PID processID);
    virtual SysStatus testRead     () __xa(read);
    virtual SysStatus testWrite    () __xa(write);
    virtual SysStatus testReadWrite() __xa(read:write);
    virtual SysStatus testAny      () __xa(none);
};

#endif /* #ifndef __BONNIE_H_ */
