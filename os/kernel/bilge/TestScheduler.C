/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: TestScheduler.C,v 1.7 2003/06/04 14:17:10 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:  Class for tweaking kernel scheduling parameters.
 ***************************************************************************/

#include "kernIncs.H"
#include "bilge/TestScheduler.H"
#include "meta/MetaTestScheduler.H"
#include "stub/StubTestScheduler.H"
#include "exception/ExceptionLocal.H"
#include <cobj/CObjRootSingleRep.H>

void
TestScheduler::ClassInit(VPNum vp)
{
    if (vp != 0) return;
    MetaTestScheduler::init();
}

TestScheduler::TestScheduler()
{
}

/*virtual*/ SysStatus
TestScheduler::init()
{
    CObjRootSingleRep::Create(this);
    return 0;
}

/*static*/ SysStatus
TestScheduler::_Create(ObjectHandle &tsOH, __CALLER_PID caller)
{
    TestScheduler *ts = new TestScheduler;

    if (ts == NULL) {
	return -1;
    }
    ts->init();

    return(ts->giveAccessByServer(tsOH, caller));
}
