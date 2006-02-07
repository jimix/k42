/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: AsyncTester.C,v 1.34 2004/07/11 21:59:27 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 * **************************************************************************/

#include "kernIncs.H"
#include <misc/macros.H>
#include "AsyncTester.H"
#include <meta/MetaAsyncTester.H>
#include <stub/StubAsyncTester.H>
#include "Timer.H"
#include "proc/Process.H"
#include <sync/SLock.H>
#include "cobj/CObjRootSingleRep.H"

//static  sval  holder = -1;

void
asyncLockTest()
{
    /*
     * SLock
     */
    SLock sLock;

    sLock.acquire();
    sLock.release();
    sLock.init();
    if (sLock.tryAcquire()) {
	if (sLock.isLocked()) {
	    sLock.release();
	}
    }
}

const uval timerSetValue = 0x11111;   // test startup

void
testAsync()
{
    static  StubAsyncTester sasyncer = StubAsyncTester(StubObj::UNINITIALIZED);
    SysStatus rc;

    cprintf("testAsync: Initializing\n");

    // allocate the asyncer, register in object table and create extern objref
    AsyncTester *asyncer  = new AsyncTester();
    ObjRef oref =(ObjRef) CObjRootSingleRep::Create(asyncer);
    XHandle xhandle = MetaAsyncTester::createXHandle(oref, GOBJ(TheProcessRef),
						     0, 0);

    sasyncer.initOHWithPID(DREFGOBJK(TheProcessRef)->getPID(), xhandle);

#if 0
    asyncLockTest();

    Timer::init();
    Timer::setTicks(timerSetValue);
#endif

    cprintf("About to sync spawn\n");
    rc = sasyncer.testSync(99);

    cprintf("About to async spawn 1st\n");
    rc = sasyncer.testAsync(1);
    cprintf("testAsync(1) returned 0x%lx\n", rc);

    cprintf("About to async spawn 2nd\n");
    rc = sasyncer.testAsync(2);
    cprintf("testAsync(2) returned 0x%lx\n", rc);

    cprintf("async spawn done\n");

#if 0
    for(;;) {
	uval acq;
//	uval ticks = Timer::getTicks();
	if ((acq = sLock.tryAcquire()))
	    holder = 0;

//	cprintf("==> Master dec-reg=<0x%lx> #-ints=%ld \n",
//			ticks,exceptionLocal.num_dec);
//	threadYield();
//	sasyncer.testSync(99);  // try to do ppc while here

	cprintf("==> Master  acq=%ld H=%ld\n",acq,holder);
	if (acq) {
	    threadYield();  // release and let somebody try to acquire
	    holder = -1;
	    sLock.release();
	    cprintf("==> Master  release\n");
	    threadYield();  // release and let somebody try to acquire
	}
    }
#endif
}


SysStatus
AsyncTester::testAsync(uval arg)
{
    // cannot do cprintfs disabled
    err_printf("AsyncTester::testASync thread is running %ld\n",arg);
#if 0
    for(;;) {
	uval acq;
//	uval ticks = Timer::getTicks();
	if ((acq = sLock.tryAcquire()))
	    holder = arg;

//	cprintf("==> Slave_%ld dec-reg=<0x%lx> #-ints=%ld acq=%ld\n",
//		arg,ticks,exceptionLocal.num_dec,acq);
	cprintf("==> Slave_%ld acq=%ld H=%ld\n",arg,acq,holder);
	if (acq) {
	    threadYield();  // release and let somebody try to acquire
	    holder = -1;
	    sLock.release();
	    cprintf("==> Slave_%ld release\n",arg);
	    threadYield();  // release and let somebody try to acquire
	}
    }
#endif
    return(0);
}

SysStatus
AsyncTester::testSync(uval arg)
{
    cprintf("AsyncTester::testSync thread is running %ld\n",arg);
    return(0);
}

ObjRef
AsyncTester::getRef()
{
    return BaseObj::getRef();
}
