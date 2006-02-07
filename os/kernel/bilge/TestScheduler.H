#ifndef __TEST_SCHEDULER_H_
#define __TEST_SCHEDULER_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TestScheduler.H,v 1.8 2003/06/04 14:17:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for tweaking kernel scheduling parameters.
 ***************************************************************************/

#include <meta/MetaTestScheduler.H>
#include <cobj/TypeMgr.H>
class TestScheduler;

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

class TestScheduler : public Obj {

protected:
    DEFINE_GLOBAL_NEW(TestScheduler);
    virtual SysStatus init();

    TestScheduler();

public:
    DEFINE_ACCESS_SERVER(
	TestScheduler,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    static void ClassInit(VPNum vp);

__xpublic:
    static SysStatus _Create(__out ObjectHandle &tsOH, __CALLER_PID caller);
};

#endif /* #ifndef __TEST_SCHEDULER_H_ */
