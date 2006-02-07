/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000,2001
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: smtTest.C,v 1.13 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <io/MemTrans.H>
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ProcessWrapper.H>
#include <io/MemTrans.H>
#include <stub/StubMemTrans.H>
#include <scheduler/Scheduler.H>
#include <cobj/TypeMgr.H>
#include <usr/ProgExec.H>

#include <alloc/MemoryMgrPrimitive.H>
extern "C" int main(int argc, char *argv[]);




void
startupSecondaryProcessors()
{
    SysStatus rc;
    VPNum n, vp;
    n = DREFGOBJ(TheProcessRef)->ppCount();

    if ( n<=1) {
        err_printf("base Servers - number of processors %ld\n", n);
        return ;
    }

    err_printf("base Servers - starting %ld secondary processors\n",n-1);
    for (vp = 1; vp < n; vp++) {
        rc = ProgExec::CreateVP(vp);
        passert(_SUCCESS(rc),
		err_printf("ProgExec::CreateVP failed (0x%lx)\n", rc));
        err_printf("base Servers - vp %ld created\n", vp);
    }
}


struct MTHandlerSrv : public MemTrans::MTEvents {
    MTHandlerSrv(ThreadID id):owner(id),counter(0), last(0) { /* empty body */ }
    ThreadID owner;
    XHandle other;
    RingID ring;
    volatile uval counter;
    volatile uval last;
    virtual void recvConnection(MemTransRef mtr, XHandle otherMT) {
	other = otherMT;
	AtomicAdd(&counter,1);
	printf("Got message: %s from %lx\n",__func__, (uval)otherMT);
	Scheduler::Unblock(owner);
    }
    virtual SysStatus allocRing(MemTransRef mtr, XHandle otherMT) {
	other = otherMT;
	AtomicAdd(&counter,1);
	printf("Got message: from %s\n",__func__, (uval)otherMT);
	Scheduler::Unblock(owner);
	return 0;
    }
    virtual uval pokeRing(MemTransRef mtr, XHandle otherMT) {
	ring = id;
	AtomicAdd(&counter,1);
	printf("Got message: %s from %lx\n",__func__,(uval)otherMT);
	Scheduler::Unblock(owner);
	return 1;
    }
    virtual void waitForNext() {
	uval curr = last;
	printf("Waiting: %ld %ld %ld\n",last,curr,counter);
	while (curr == counter) {
	    Scheduler::Block();
	}
	++last;
    }
};



int
main(int argc, char *argv[])
{
    NativeProcess();

    const char *optlet = "acA";
    int c;
    int server=0;
    int alloc_test=0;
    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	    case 'c':
		server=0;
		break;
	    case 'a':
		server=1;
		break;
	    case 'A':
		alloc_test =1;
		break;
	}
    }

    ProcessID pid = DREFGOBJ(TheProcessRef)->getPID();
    if (server) {
	printf("Pid is:%ld\n",pid);
	startupSecondaryProcessors();
	MetaObj::init();
	MetaMemTrans::init();
	ObjectHandle fake;
	XHandle partner;
	MemTransRef mtr;
	fake.init();

	MTHandlerSrv mts(Scheduler::GetCurThread());

	SysStatus rc = MemTrans::Create(mtr, fake, 64*PAGE_SIZE, partner, &mts);
	printf("MemTrans::Create: %016ld\n",rc);

	if (alloc_test) {

#define A(spot,count) (((spot)<<12) | (count))
#define D(spot,count) ((1<<11) | ((spot)<<12) | (count))
	    uval sequence[15] = { A(0,9), A(1,9), A(2,9), A(3,9), A(4,9),
				  A(5,9), A(6,9),
				  D(1,9), D(3,9), D(2,9),
				  A(1,25), A(2,1), A(3,1),0};

	    void *allocs[8]={NULL,};
	    uval slot = 0;
	    while (sequence[slot]!=0) {

		uval spot = (sequence[slot] & ~((1<<12) - 1))>>12;
		uval count= sequence[slot] & ((1<<11) - 1);
		if ( sequence[slot] & 1<<11 ) {
		    DREF(mtr)->freePagePtr(allocs[spot]);
		    printf("Free: %ld %p %ld\n",spot,allocs[spot],count);
		} else {
		    uval addr;
		    DREF(mtr)->allocPagesLocal(addr, count);
		    allocs[slot]=(void*)addr;
		    printf("Alloc: %ld %p %ld\n",spot,allocs[slot],count);
		}

		++slot;
	    }
	}
	mts.waitForNext();
	printf("Received connection: %016lx\n",mts.other);

	mts.waitForNext();
	printf("Received ring: %016lx %ld\n",mts.other,mts.ring);

	for (uval i=0; i<1001; ++i) {
	    while (1) {
		rc = DREF(mtr)->insertToRing(mts.ring, i);
		if (_FAILURE(rc)) {
		    printf("insert failure: %016lx\n",rc);
		    mts.waitForNext();
		} else {
		    break;
		}
	    }
	}
    } else {
	ObjectHandle oh;
	SysStatus rc=0;
	MTHandlerSrv mts(Scheduler::GetCurThread());
	XHandle partner;

	rc = StubMemTrans::_getHandle(oh);
	printf("got OH: %016lx:%016lx rc: %016lx\n",oh._commID,oh._xhandle,rc);


	MemTransRef mtr;
	rc = MemTrans::Create(mtr, oh, 64*PAGE_SIZE, partner, &mts);
	if (_FAILURE(rc)) {
	    printf("MemTrans::Create: %016lx\n",rc);
	    return -1;
	}
	RingID rid;
	uval size =512;
	rc = mts.allocRing(size, partner);
	if (_FAILURE(rc)) {
	    printf("MemTrans::AllocRing: %016lx\n",rc);
	    return -1;
	}
	rid = rc;

	uval ringVal = 0 ;
	while (ringVal!=1000) {
	    do {
		rc = DREF(mtr)->consumeFromRing(rid, ringVal);
		if (_SUCCESS(rc)) {
		    break;
		} else if (_SGENCD(rc)==EWOULDBLOCK) {
		    mts.waitForNext();
		}
	    } while (1);
	    printf("Consume: %016lx %ld\n",rc, ringVal);
	}
    }
    return 0;
}
