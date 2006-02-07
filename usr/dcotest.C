/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dcotest.C,v 1.15 2005/06/28 19:48:44 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for dyn-switch.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "misc/testSupport.H"
#include "intctr.H"
#include <stub/StubWire.H>
#include <sys/systemAccess.H>

#define DCOTEST_MAXPROCS	16
#define DO_ALL			0
#define REVERSE_PHASES		0

#define ACT(x) {Scheduler::ActivateSelf(); x ; Scheduler::DeactivateSelf(); myYield(); }


VPNum NumVP = 1;			// total vp to use for tests

uval ITERS_PER_STAGE = 1000;
uval NUM_COUNTERS = 1;
uval SPIN_COUNT = 0;

const uval baseTimes[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

// IntCtr CO tests

enum {
#if !REVERSE_PHASES
    TEST_METHOD_INC = 0,
#else
    TEST_METHOD_INC = 1,
#endif
    TEST_METHOD_VALUE = (1 - TEST_METHOD_INC)

};

enum CtrCOTestParam {
    CT_PARTITIONED,
    CT_SHARED,
    CT_DYNAMIC
};

struct AutoThreadDeactivation {
    DEFINE_NOOP_NEW(AutoThreadDeactivation);
    AutoThreadDeactivation() { Scheduler::DeactivateSelf(); }
    ~AutoThreadDeactivation() { Scheduler::ActivateSelf(); }
};

static inline void myYield()
{
    for (volatile uval i = 0; i < SPIN_COUNT; i++) {}
    Scheduler::Yield();
    //Scheduler::DelayMicrosecs(1);
}

static void
multiCOCtrTestWorker(TestStructure *p)
{
    AutoThreadDeactivation atd;

    const uval maxCounters = 10;
    const uval numCounters = p->size;
    const uval iters  = p->iters;
    SysTime start = 0, end = 0;
    uval iStart = 0, iEnd = 0;
    SysTime totalStart = 0, totalEnd = 0;

    //err_printf("  COCtrWorker(%ld) started\n", uval(Scheduler::GetVP()));
    tassert(maxCounters >= numCounters, err_printf("doh!\n"));

    IntCtrRef counters[maxCounters];

    memcpy(counters, p->ptr, sizeof(IntCtrRef)*numCounters);
    p->bar->enter();

    totalStart = Scheduler::SysTimeNow();
    for (uval stage = 0; stage < 2; stage++) {
	switch (stage) {
	case TEST_METHOD_INC:
#if 0
	    // do a couple of calls to get things going
	    for (uval j = 0; j < numCounters; j++) {
		//err_printf("---INC:DREF(ctr[%ld]) = %p\n", j, DREF(counters[j]));
		DREF(counters[j])->inc();
		//err_printf("---INC:DREF(ctr[%ld]) = %p\n", j, DREF(counters[j]));
		ACT(DREF(counters[j])->dec());
		ACT(DREF(counters[j])->dec());
		ACT(DREF(counters[j])->inc());
	    }

	    p->bar->enter();
#endif

	    // FIXME - do we need these things?
	    //DREFGOBJ(TheProcessRef)->perfMon(BaseProcess::BEGIN_PERF_MON,0);

	    //iStart = getInstrCount();
	    start = Scheduler::SysTimeNow();

	    // we now do actual work that is to be timed
	    for (uval i = 0; i < iters; i++) {
		for (uval j = 0; j < numCounters; j++) {
		    ACT(DREF(counters[j])->inc());
		}
	    }

	    //iEnd = getInstrCount();
	    end = Scheduler::SysTimeNow();

	    //DREFGOBJ(TheProcessRef)->perfMon(BaseProcess::END_PERF_MON,0);

	    p->time1 = end - start;
	    p->instr1 = iEnd - iStart;

	    //p->bar->enter();
	    break;
	case TEST_METHOD_VALUE:
	    sval val;
	    // do a couple of calls to get things going
#if 0

	    for (uval j = 0; j < numCounters; j++) {
		//err_printf(">>>VAL DREF(ctr[%ld]) = %p\n", j, DREF(counters[j]));
		ACT(DREF(counters[j])->value(val));
		//err_printf(">>>VAL DREF(ctr[%ld]) = %p\n", j, DREF(counters[j]));
		//breakpoint();
		ACT(DREF(counters[j])->value(val));
	    }

	    p->bar->enter();
#endif

	    //DREFGOBJ(TheProcessRef)->perfMon(BaseProcess::BEGIN_PERF_MON,0);

	    //iStart = getInstrCount();
	    start = Scheduler::SysTimeNow();

	    // we now do actual work that is to be timed
	    for (uval i = 0; i < iters; i++) {
		for (uval j = 0; j < numCounters; j++) {
		    ACT(DREF(counters[j])->value(val));
		}
	    }

	    //iEnd = getInstrCount();
	    end = Scheduler::SysTimeNow();


	    //DREFGOBJ(TheProcessRef)->perfMon(BaseProcess::END_PERF_MON,0);

	    p->time2 = end - start;
	    //p->instr2 = iEnd - iStart;

#ifndef NDEBUG
	    p->bar->enter();
#endif
	    tassert(uval(val) == iters * NumVP,
		    err_printf("Incorrect value! val = %ld, expected %ld\n",
			val, iters * NumVP));
	    break;
	default:
	    tassert(0, err_printf("Unhandled test param\n"));
	    break;
	}

#if 1
	if (stage == 0) {
	    p->bar->enter();
	    for (uval i = 0; i < numCounters; i++) {
		if (p->test == CT_DYNAMIC && Scheduler::GetVP() == 0) {
#if REVERSE_PHASES
		    SysStatus rc = SharedIntCtr::Root::SwitchImpl(
			    (CORef)counters[i], 1);
#else
		    SysStatus rc = PartitionedIntCtr::Root::SwitchImpl(
			    (CORef)counters[i], 1);
#endif
		    tassert(_SUCCESS(rc),;);
		}
	    }
	}
#endif
	//err_printf("Done stage %ld.\n", stage+1);
    }
    totalEnd = Scheduler::SysTimeNow();

    p->time3 = totalEnd - totalStart;

    //err_printf("  COCtrWorker(%ld) finished\n", uval(Scheduler::GetVP()));
}

struct graphPt { uval x, y; };

uval gindex = 0;
graphPt graphAll[3][32][DCOTEST_MAXPROCS];

void
doCtrCOTest(CtrCOTestParam param, uval iters)
{
    // First a bunch of inc(), then a bunch of value()
    uval i;
    SysStatus rc = 0;
    const uval numIters = iters;
    const uval numCounters = NUM_COUNTERS;
    BlockBarrier bar(NumVP);
    IntCtrRef *counters =
	(IntCtrRef *) allocGlobal(numCounters*sizeof(IntCtrRef));

    for (i = 0; i < numCounters; i++) {
	switch (param) {
	case CT_PARTITIONED:
	    rc = PartitionedIntCtr::Create(counters[i]);
	    break;
	case CT_SHARED:
	    rc = SharedIntCtr::Create(counters[i]);
	    break;
	case CT_DYNAMIC:
#if REVERSE_PHASES
	    rc = SharedIntCtr::Create(counters[i]);
#else
	    rc = PartitionedIntCtr::Create(counters[i]);
#endif
	    break;
	default:
	    tassert(0, err_printf("Doh!\n"));
	    break;
	}
	tassert(rc == 0, err_printf("Doh!\n"));
    }

    // array of teststructs (one per worker) is allocated here
    TestStructure *ts = TestStructure::Create(
	NumVP, numCounters, numIters,
	param/*test*/, 0, counters/*ptr*/, &bar);

    switch (param) {
    case CT_PARTITIONED:
	err_printf("----- PartitionedIntCtr -----\n");
	break;
    case CT_SHARED:
	err_printf("----- SharedIntCtr -----\n");
	break;
    case CT_DYNAMIC:
	err_printf("----- DynamicIntCtr -----\n");
	break;
    }

    tstEvent_starttest();

    DoConcTest(NumVP, (SimpleThread::function)multiCOCtrTestWorker, ts);

    tstEvent_endtest();

    for (i = 0; i < NumVP; i++) {
	graphAll[0][gindex][i].x = NumVP;
	//graphAll[0][gindex][i].y = ts[i].time1;
	//graphAll[0][gindex][i].y = ts[i].time2;
	graphAll[0][gindex][i].y = ts[i].time3;
    }

    delete[] ts;
    freeGlobal(counters, numCounters*sizeof(IntCtrRef));
    err_printf("********* doCtrCOTest() done.\n");
    err_printf("**** doCtrCOTest() time taken: %lld.\n\n", ts[0].time3);
    Scheduler::DelayMicrosecs(100000);
}

int test_main(int argc, char *argv[])
{
    uval avg0[DCOTEST_MAXPROCS+1], avg1[DCOTEST_MAXPROCS+1], avg2[DCOTEST_MAXPROCS+1];
    const uval realNumVP = DREFGOBJ(TheProcessRef)->ppCount();
    uval ymax[3][DCOTEST_MAXPROCS+1], ymin[3][DCOTEST_MAXPROCS+1];

    err_printf("BEGIN %s\n", argv[0]);
    err_printf("# ITERS_PER_STAGE = %ld\n", ITERS_PER_STAGE);
    err_printf("# NUM_COUNTERS = %ld\n", NUM_COUNTERS);
    err_printf("# SPIN_COUNT = %ld\n", SPIN_COUNT);

    Scheduler::DelayMicrosecs(100000);

#if 1
    err_printf("%s: full warm-up\n", argv[0]);
    NumVP = realNumVP;
    gindex = 0;
    doCtrCOTest(CT_PARTITIONED, 100);
    gindex = 1;
    doCtrCOTest(CT_SHARED, 100);
    gindex = 2;
    doCtrCOTest(CT_DYNAMIC, 100);
#endif

    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%s: NumVP is %ld\n", argv[0], uval(NumVP));

	for (uval it = 0; it < 1; it++) {
	    gindex = 0;
	    doCtrCOTest(CT_PARTITIONED, ITERS_PER_STAGE);
	    gindex = 1;
	    doCtrCOTest(CT_SHARED, ITERS_PER_STAGE);
	    gindex = 2;
	    doCtrCOTest(CT_DYNAMIC, ITERS_PER_STAGE);
	}

	uval sum0 = 0, sum1 = 0, sum2 = 0;
	ymax[0][NumVP] = 0;
	ymax[1][NumVP] = 0;
	ymax[2][NumVP] = 0;
	ymin[0][NumVP] = uval(-1);
	ymin[1][NumVP] = uval(-1);
	ymin[2][NumVP] = uval(-1);
	err_printf("#PARTITIONED nums:\n");
	for (uval i = 0; i < NumVP; i++) {
	    err_printf("%ld ", graphAll[0][0][i].y);
	    sum0 += graphAll[0][0][i].y;
	    if (ymax[0][NumVP] < graphAll[0][0][i].y) {
		ymax[0][NumVP] = graphAll[0][0][i].y;
	    }
	    if (ymin[0][NumVP] > graphAll[0][0][i].y) {
		ymin[0][NumVP] = graphAll[0][0][i].y;
	    }
	}
	err_printf("\n\n");
	err_printf("#SHARED nums:\n");
	for (uval i = 0; i < NumVP; i++) {
	    err_printf("%ld ", graphAll[0][1][i].y);
	    sum1 += graphAll[0][1][i].y;
	    if (ymax[1][NumVP] < graphAll[0][1][i].y) {
		ymax[1][NumVP] = graphAll[0][1][i].y;
	    }
	    if (ymin[1][NumVP] > graphAll[0][1][i].y) {
		ymin[1][NumVP] = graphAll[0][1][i].y;
	    }
	}
	err_printf("\n\n");
	err_printf("#DYNAMIC nums:\n");
	for (uval i = 0; i < NumVP; i++) {
	    err_printf("%ld ", graphAll[0][2][i].y);
	    sum2 += graphAll[0][2][i].y;
	    if (ymax[2][NumVP] < graphAll[0][2][i].y) {
		ymax[2][NumVP] = graphAll[0][2][i].y;
	    }
	    if (ymin[2][NumVP] > graphAll[0][2][i].y) {
		ymin[2][NumVP] = graphAll[0][2][i].y;
	    }
	}
	err_printf("\n\n");

	avg0[NumVP] = sum0 / NumVP / ITERS_PER_STAGE - baseTimes[NumVP];
	avg1[NumVP] = sum1 / NumVP / ITERS_PER_STAGE - baseTimes[NumVP];
	avg2[NumVP] = sum2 / NumVP / ITERS_PER_STAGE - baseTimes[NumVP];
	ymax[0][NumVP] = ymax[0][NumVP] / ITERS_PER_STAGE - baseTimes[NumVP];
	ymax[1][NumVP] = ymax[1][NumVP] / ITERS_PER_STAGE - baseTimes[NumVP];
	ymax[2][NumVP] = ymax[2][NumVP] / ITERS_PER_STAGE - baseTimes[NumVP];
	ymin[0][NumVP] = ymin[0][NumVP] / ITERS_PER_STAGE - baseTimes[NumVP];
	ymin[1][NumVP] = ymin[1][NumVP] / ITERS_PER_STAGE - baseTimes[NumVP];
	ymin[2][NumVP] = ymin[2][NumVP] / ITERS_PER_STAGE - baseTimes[NumVP];

    }

    err_printf("# %s\n", argv[0]);
    err_printf("# ITERS_PER_STAGE = %ld\n", ITERS_PER_STAGE);
    err_printf("# NUM_COUNTERS = %ld\n", NUM_COUNTERS);
    err_printf("# SPIN_COUNT = %ld\n", SPIN_COUNT);
    err_printf("\n");
    err_printf("# Partitioned results:\n");
    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%ld %ld\t%ld %ld\n", uval(NumVP), avg0[NumVP],
		ymin[0][NumVP], ymax[0][NumVP]);
    }

    err_printf("\n# Shared results:\n");
    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%ld %ld\t%ld %ld\n", uval(NumVP), avg1[NumVP],
		ymin[1][NumVP], ymax[1][NumVP]);
    }

    err_printf("\n# Dynamic results:\n");
    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%ld %ld\t%ld %ld\n", uval(NumVP), avg2[NumVP],
		ymin[2][NumVP], ymax[2][NumVP]);
    }

    err_printf("END %s\n", argv[0]);
    Scheduler::DelayMicrosecs(1000000);
    return 0;
}
#if 0
int test_main(int argc, char *argv[])
{
    uval avg0[DCOTEST_MAXPROCS], avg1[DCOTEST_MAXPROCS], avg2[DCOTEST_MAXPROCS];
    const uval realNumVP = DREFGOBJ(TheProcessRef)->ppCount();

    err_printf("BEGIN %s\n", argv[0]);
    err_printf("# ITERS_PER_STAGE = %ld\n", ITERS_PER_STAGE);
    err_printf("# NUM_COUNTERS = %ld\n", NUM_COUNTERS);
    err_printf("# SPIN_COUNT = %ld\n", SPIN_COUNT);

    Scheduler::DelayMicrosecs(100000);

#if 1
    err_printf("%s: full warm-up\n", argv[0]);
    NumVP = realNumVP;
    gindex = 0;
    doCtrCOTest(CT_PARTITIONED, 100);
    gindex = 1;
    doCtrCOTest(CT_SHARED, 100);
    gindex = 2;
    doCtrCOTest(CT_DYNAMIC, 100);
#endif

    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%s: NumVP is %ld\n", argv[0], uval(NumVP));

	for (uval it = 0; it < 1; it++) {
	    gindex = 0;
	    doCtrCOTest(CT_PARTITIONED, ITERS_PER_STAGE);
	    gindex = 1;
	    doCtrCOTest(CT_SHARED, ITERS_PER_STAGE);
	    gindex = 2;
	    doCtrCOTest(CT_DYNAMIC, ITERS_PER_STAGE);
	}

	uval sum0 = 0, sum1 = 0, sum2 = 0;
	err_printf("#PARTITIONED nums:\n");
	for (uval i = 0; i < NumVP; i++) {
	    err_printf("%ld ", graphAll[0][0][i].y);
	    sum0 += graphAll[0][0][i].y;
	}
	err_printf("\n\n");
	err_printf("#SHARED nums:\n");
	for (uval i = 0; i < NumVP; i++) {
	    err_printf("%ld ", graphAll[0][1][i].y);
	    sum1 += graphAll[0][1][i].y;
	}
	err_printf("\n\n");
	err_printf("#DYNAMIC nums:\n");
	for (uval i = 0; i < NumVP; i++) {
	    err_printf("%ld ", graphAll[0][2][i].y);
	    sum2 += graphAll[0][2][i].y;
	}
	err_printf("\n\n");

	avg0[NumVP] = sum0 / NumVP / ITERS_PER_STAGE;
	avg1[NumVP] = sum1 / NumVP / ITERS_PER_STAGE;
	avg2[NumVP] = sum2 / NumVP / ITERS_PER_STAGE;

    }

    err_printf("# %s\n", argv[0]);
    err_printf("# ITERS_PER_STAGE = %ld\n", ITERS_PER_STAGE);
    err_printf("# NUM_COUNTERS = %ld\n", NUM_COUNTERS);
    err_printf("# SPIN_COUNT = %ld\n", SPIN_COUNT);
    err_printf("\n");
    err_printf("# Partitioned results:\n");
    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%ld %ld\n", uval(NumVP), avg0[NumVP]);
    }

    err_printf("\n# Shared results:\n");
    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%ld %ld\n", uval(NumVP), avg1[NumVP]);
    }

    err_printf("\n# Dynamic results:\n");
    for (NumVP = 1; NumVP < realNumVP+1; NumVP++) {
	err_printf("%ld %ld\n", uval(NumVP), avg2[NumVP]);
    }

    err_printf("END %s\n", argv[0]);
    Scheduler::DelayMicrosecs(1000000);
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    NativeProcess();

    //AutoThreadDeactivation atd;

    StubWire::SuspendDaemon();
    Scheduler::DelayMicrosecs(100000);

#if DO_ALL
    ITERS_PER_STAGE = 1000;
    NUM_COUNTERS = 1;
    SPIN_COUNT = 0;
    test_main(argc, argv);

    ITERS_PER_STAGE = 1000;
    NUM_COUNTERS = 5;
    SPIN_COUNT = 0;
    test_main(argc, argv);

    ITERS_PER_STAGE = 1000;
    NUM_COUNTERS = 1;
    SPIN_COUNT = 100;
    test_main(argc, argv);

    ITERS_PER_STAGE = 1000;
    NUM_COUNTERS = 5;
    SPIN_COUNT = 100;
    test_main(argc, argv);

    ITERS_PER_STAGE = 10000;
    NUM_COUNTERS = 1;
    SPIN_COUNT = 0;
    test_main(argc, argv);

    ITERS_PER_STAGE = 10000;
    NUM_COUNTERS = 5;
    SPIN_COUNT = 0;
    test_main(argc, argv);

    ITERS_PER_STAGE = 10000;
    NUM_COUNTERS = 1;
    SPIN_COUNT = 100;
    test_main(argc, argv);

    ITERS_PER_STAGE = 10000;
    NUM_COUNTERS = 5;
    SPIN_COUNT = 100;
    test_main(argc, argv);
#else
    test_main(argc, argv);
#endif

    StubWire::RestartDaemon();
}
