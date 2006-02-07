#ifndef __TEST_SWITCH_H_
#define __TEST_SWITCH_H_
/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TestSwitch.H,v 1.11 2002/10/10 13:08:32 rosnbrg Exp $
 *****************************************************************************/

#include <cobj/CObjRootSingleRep.H>
#include <meta/MetaTestSwitch.H>
class TestSwitch;

// FIXME: fix stub-compiler grammer so don't need to do this
#ifdef _IN_STUBGEN
#undef DEFINE_ACCESS_SERVER
#define DEFINE_ACCESS_SERVER(CLASS,matchDefault,nomatchDefault)
#endif /* #ifdef _IN_STUBGEN */

// FIXME: should probably introduce an intermediate Region class
class TestSwitch : public Obj {

protected:
    FCMRef fcmRef; // ref of FCM to switch from
    RegionRef regionRef; // ref of Region to switch from (potentially)
    ProcessRef processRef; // ref of Process to switch from (potentially)
    uval regionSize; // size of region

    DEFINE_GLOBAL_NEW(TestSwitch);
    virtual SysStatus init();

    TestSwitch();
    //static SysStatus fcmXfer(CObjRoot *oldRoot, CObjRoot *newRoot);
    static SysStatus fcmXferTrivial(CObjRoot *oldRoot, CObjRoot *newRoot);
    static SysStatus regionXfer(CObjRoot *oldRoot, CObjRoot *newRoot);

public:
    DEFINE_ACCESS_SERVER(
	TestSwitch,MetaObj::controlAccess|MetaObj::attach,MetaObj::none);

    static void ClassInit(VPNum vp);

    virtual SysStatus storeRefs(FCMRef locFcmRef, RegionRef locRegRef,
				ProcessRef locProcessRef, uval regionSize);

__xpublic:
    static SysStatus _Create(__out ObjectHandle &tsOH, __CALLER_PID caller);

    virtual SysStatus _startSwitch();

};

#endif /* #ifndef __TEST_SWITCH_H_ */
