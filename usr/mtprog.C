/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: mtprog.C,v 1.14 2005/06/28 19:48:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "misc/testSupport.H"
#include <usr/ProgExec.H>
#include <scheduler/Scheduler.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubRegionReplicated.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <stdio.h>
#include <sys/systemAccess.H>

uval8 ProcsCreated[Scheduler::VPLimit];		// bool for which vps created
uval NumVP;

struct WorkerStruct {
    Barrier *bar;
    void *ptr;
    uval size;
};

void
MakeMP(VPNum numVP)
{
    SysStatus rc;

    passert(numVP <= Scheduler::VPLimit, {});

    for (VPNum vp = 1; vp < numVP; vp++) {
	if (!ProcsCreated[vp]) {
	    rc = ProgExec::CreateVP(vp);
	    passert(_SUCCESS(rc), ;);
	    ProcsCreated[vp] = 1;
	}
    }
}

void
MTTest(VPNum numvp, SimpleThread::function func)
{
    SysStatus rc;
    BlockBarrier bar(numvp);
    int i;
    SimpleThread *threads[2*numvp];
    WorkerStruct ws;
    uval region = 0;
    const uval size = numvp * PAGE_SIZE * 8;
    ObjectHandle frOH;

    ws.bar = &bar;

    rc = StubFRComputation::_Create(frOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));
#if 0
    rc = StubRegionDefault::_CreateFixedLenExt(
	region, size, SEGMENT_SIZE,
	frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0
	RegionType::K42Region);
#else
    rc = StubRegionReplicated::_CreateFixedLenExtKludge(
	region, size, SEGMENT_SIZE,
	0, (uval)(AccessMode::writeUserWriteSup), 0, 1,
	RegionType::K42Region);
#endif
    ws.ptr = (void *)region;
    ws.size = size;

    MakeMP(numvp);
    for (i=0; i < int(numvp); i++) {
	threads[i] = SimpleThread::Create(func, &ws, SysTypes::DSPID(0, i));
	passert(threads[i]!=0, err_printf("Thread create failed\n"));
    }
    for (i=0; i < int(numvp); i++) {
	SimpleThread::Join(threads[i]);
	//err_printf("----Join(threads[%ld])----\n\n", uval(i));
    }
}


static SysStatus
testWorker(void *p)
{
    VPNum myvp = Scheduler::GetVP();
    SysStatus rc;
    uval i;
    WorkerStruct *ws = (WorkerStruct *)p;
    BlockBarrier *bar = (BlockBarrier *)ws->bar;
    volatile char *region = (char *)ws->ptr;
    uval mychunk = uval(region) + myvp * ws->size / NumVP;
    volatile char ch;

    printf(" Worker(%lx) started\n", uval(Scheduler::GetCurThread()));
    bar->enter();

    for (i = 0; i < 1000; i++) {
	for (volatile char *page = (char *)mychunk; page < region + (myvp+1) *
		ws->size/NumVP; page += PAGE_SIZE) {
	    //err_printf("(%p)", page);
	    ch = *page;
	}
	//err_printf("DREFGOBJ(TheProcessRef)->unmapRange(%p, %ld);\n",
	//	   (char *)mychunk, ws->size/NumVP);
	rc = DREFGOBJ(TheProcessRef)->unmapRange(mychunk, ws->size/NumVP);
	//err_printf("DREFGOBJ(TheProcessRef)->unmapRange(%p, %ld); DONE\n",
	//	   (char *)mychunk, ws->size/NumVP);
    }

    printf(" W(%lx) loop done\n", uval(Scheduler::GetCurThread()));

    bar->enter();

    //printf("  Worker(%lx) finished\n", uval(Scheduler::GetCurThread()));
    return 0xbeefbeefbeefbeef;
}

int main()
{
    NativeProcess();

    NumVP = DREFGOBJ(TheProcessRef)->ppCount();
    printf("test: NumVP is %ld\n", uval(NumVP));
    MTTest(NumVP, (SimpleThread::function)testWorker);
    printf("Test done.\n");

    return 0;
}
