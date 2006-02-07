/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000,2001
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: blkTest.C,v 1.15 2005/06/28 19:48:44 rosnbrg Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <io/MemTrans.H>
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ProcessWrapper.H>
#include <io/MemTrans.H>
#include <stub/StubMemTrans.H>
#include <scheduler/Scheduler.H>
#include <cobj/TypeMgr.H>
#include <usr/ProgExec.H>

#include <io/FileLinux.H>
#include <stub/StubBlockDev.H>

#include <alloc/MemoryMgrPrimitive.H>
extern "C" int main(int argc, char *argv[]);





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
	printf("Got message: %s\n",__func__);
	Scheduler::Unblock(owner);
    }
    virtual SysStatus allocRing(MemTransRef mtr, XHandle otherMT, RingID id) {
	other = otherMT;
	ring = id;
	AtomicAdd(&counter,1);
	printf("Got message: %s\n",__func__);
	Scheduler::Unblock(owner);
	return 0;
    }
    virtual uval pokeRing(MemTransRef mtr, RingID id) {
	ring = id;
	AtomicAdd(&counter,1);
	printf("Got message: %s\n",__func__);
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

MTHandlerSrv mts(Scheduler::NullThreadID);


int
main(int argc, char *argv[])
{
    NativeProcess();

    SysStatus rc=0;
    const char *optlet = "acAt:d:";
    extern char* optarg;
    char* dev = "/dev/sda2";
    int c;
    int server=0;
    int alloc_test=0;
    int  timing = 0;
    int p=1;
    while ((c = getopt(argc, argv, optlet)) != EOF) {
	switch (c) {
	    case 'd':
		dev = optarg;
		break;
	    case 't':
		timing=1;
		p = atoi(optarg);
		break;
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

    FileLinuxRef flr;

    rc = FileLinux::Create(flr,dev,0,0);

    _IF_FAILURE_RET(rc);

    ObjectHandle oh;

    DREF(flr)->getOH(oh);

    if (!oh.valid()) {
	printf("Bad OH: %016lx %016lx\n",oh.commID(),oh.xhandle());
	return -1;
    }

    StubObj sobj(StubObj::UNINITIALIZED);
    sobj.setOH(oh);

    ObjectHandle newOH;
    rc = sobj._giveAccess(newOH,DREFGOBJ(TheProcessRef)->getPID(),
			  MetaObj::controlAccess|MetaObj::read|MetaObj::write,
			  MetaObj::none,
			  StubBlockDev::typeID());

    _IF_FAILURE_RET(rc);

    StubBlockDev sbd(StubObj::UNINITIALIZED);
    sbd.setOH(newOH);

    ObjectHandle memTransOH;
    rc = sbd._getMemTrans(memTransOH);

    _IF_FAILURE_RET(rc);
    mts.owner = Scheduler::GetCurThread();
    XHandle partner;

    MemTransRef mtr;
    rc = MemTrans::Create(mtr, memTransOH, 64* PAGE_SIZE, partner, &mts);
    if (_FAILURE(rc)) {
	printf("MemTrans::Create: %016lx\n",rc);
	return -1;
    }

    RingID rid;
    uval size =512;
    rc = DREF(mtr)->allocRing(size, 1, partner);
    if (_FAILURE(rc)) {
	printf("MemTrans::AllocRing: %016lx\n",rc);
	return -1;
    }

    rid = rc;

    uval uaddr;
    if (timing==1) {
	size=p*4096;
	struct timeval start;
	struct timeval end;
	gettimeofday(&start,NULL);
	rc = sbd._getBlock(uaddr, size, 0);
	gettimeofday(&end,NULL);
	end.tv_usec-= start.tv_usec;
	end.tv_sec -= start.tv_sec;
	if (end.tv_usec < 0) {
	    end.tv_usec += 1000000;
	    --end.tv_sec ;
	}



	printf("Done: %016lx -> %ld %ld.%06ld\n",rc,size,
	       end.tv_sec, end.tv_usec);
	rc = DREF(mtr)->allocPagesLocal(uaddr, size);

	_IF_FAILURE_RET(rc);
	char *area = (char*)uaddr;
	for (int x=0; x<p ; ++x) {
	    memset(area+4096*x,'a'+x,40);
	}
	gettimeofday(&start,NULL);
	rc = sbd._putBlock(DREF(mtr)->localOffset(uaddr),size,0);
	gettimeofday(&end,NULL);
	end.tv_usec-= start.tv_usec;
	end.tv_sec -= start.tv_sec;
	if (end.tv_usec < 0) {
	    end.tv_usec += 1000000;
	    --end.tv_sec ;
	}
	printf("Done: %016lx -> %ld %ld.%06ld\n",rc,size,
	       end.tv_sec, end.tv_usec);
    } else {
	size = 60 * 4096;
	rc = DREF(mtr)->allocPagesLocal(uaddr, size);
	uval pages = size/4096;

	_IF_FAILURE_RET(rc);
	char *area = (char*)uaddr;
	for (uval x=0; x<pages ; ++x) {
	    memset(area+4096*x,'a'+x,40);
	}
	struct timeval start;
	struct timeval end;
	gettimeofday(&start,NULL);
	rc = sbd._putBlock(DREF(mtr)->localOffset(uaddr),size,0);
	gettimeofday(&end,NULL);
	end.tv_usec-= start.tv_usec;
	end.tv_sec -= start.tv_sec;
	if (end.tv_usec < 0) {
	    end.tv_usec += 1000000;
	    --end.tv_sec ;
	}
	printf("Done: %016lx -> %ld %ld.%06ld\n",rc,size,
	       end.tv_sec, end.tv_usec);

	_IF_FAILURE_RET(rc);

	rc = sbd._getBlock(uaddr, size, 0);
	area = (char*)DREF(mtr)->remotePtr(uaddr, partner);
	pages = size/4096;
	for (uval i=0; i<pages; ++i) {
	    if (area[i*4096]!=(char)('a'+i)) {
		printf("error at offset %ld\n", i);
		return -1;
	    }
	}
    }

#if 0
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
#endif /* #if 0 */
    return 0;
}
