/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: dcofcm.C,v 1.17 2005/06/28 19:48:44 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for dyn-switch.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "misc/testSupport.H"
#include <stub/StubRegionDefault.H>
#include <stub/StubRegionReplicated.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <stub/StubTestSwitch.H>
#include <stub/StubWire.H>
#include <sys/systemAccess.H>

//#define SKIP_VP0 1
#define VP0_UNMAP_ALL 0
#define DISTINCT_PAGETOUCH 1
#define IC_RETOUCH_ALL 1
#define TIME_ENTIRE_IC_LOOP 1
#define USE_FCMDEFAULT 0
#if USE_FCMDEFAULT
#define CACHE_FR 0
#else
#define CACHE_FR 1
#endif
#define WITH_SINGLE_PROC_STAGE 0
#define USE_REGION_REPLICATED 1

#define SWITCH_NUMVP 2

#define STUB_FROM_OH(ltype, var, oh) \
	ltype var(StubObj::UNINITIALIZED); \
	var.setOH(oh);

const uval NUMPROC = DREFGOBJ(TheProcessRef)->ppCount();

struct graphPt { uval x, y; };

const uval icItersTest = 1000;
#define IC_ITER_MAX 1001
graphPt graphAll[6][256];

graphPt graphIC[3][32];
graphPt graphConcTest[3][32];

const uval pagePerVP = 8;

VPNum NumVP = 1;			// total vp to use for tests

enum TestParam {
    T_PARTITIONED = 0,
    T_SHARED,
    T_DYNAMIC
};

const char *const gStrings[] = {
    "IC_WITH_UNMAP",
    "FR",
    "IC",
    "CREATE_FR",
    "CREATE",
    "FULL_TOTAL"
};

const char *const tStrings[] = {
    "partitioned",
    "shared",
    "dynamic"
};

struct AutoThreadDeactivation {
    DEFINE_NOOP_NEW(AutoThreadDeactivation);
    AutoThreadDeactivation() { Scheduler::DeactivateSelf(); }
    ~AutoThreadDeactivation() { Scheduler::ActivateSelf(); }
};

inline void chkByte(TestStructure *ts, char c, uval doLoop)
{
#if 0
    switch ((TestParam)ts->test) {
    case T_PARTITIONED:
	passert(c == 0xfe, ;);
	break;
    case T_SHARED:
	passert(c == 0, ;);
	break;
    case T_DYNAMIC:
	passert(c == 0xfe || c == 0, ;);
	break;
    }
#else
    passert(c == 0, ;);
#endif
    //Scheduler::DelayMicrosecs(300);

    //Scheduler::YieldProcessor();

#define LOOP_IT 0
//#define LOOP_IT 0
    if (doLoop) {
	for (volatile uval loop = 0; loop < LOOP_IT; loop++) {
	}
	//uval iters = baseRandom() % 50000;
	//Scheduler::DelayMicrosecs(LOOP_IT);  // FIXME
	//Scheduler::DelayMicrosecs(((baseRandom()%20)+1)*LOOP_IT);
    }
}

//volatile sval entryCount = 0;
static ObjectHandle frOH;
static ObjectHandle tsOH;
static StubTestSwitch *pStubTS = 0;

volatile char *volatile memory = 0;
ProcessID myPID;
volatile SysTime totalCreate = 0;

#define NUM_REOPENS 1

static void
testFCMWorker(TestStructure *ts)
{
#if SKIP_VP0
    if (Scheduler::GetVP() == 0) return;
#endif

    AutoThreadDeactivation atd;
    const VPNum myvp = Scheduler::GetVP();
    const uval Size = ts->size;
    // Rounding down to an exact number of pages
    const uval numPages = (Size/PAGE_SIZE)/NUMPROC;
    //char **memory = (char **)ts->ptr;
    //const char *p = (char *)ts->ptr + (myvp * PAGE_SIZE * numPages);
    volatile char *cp;
    SysTime start1 = 0, end1 = 0;
    SysTime start2 = 0, end2 = 0;
    SysTime unmapTimeStart = 0, unmapTimeTotal = 0;
    SysTime barrierStart = 0, barrierTotal = 0;
    //uval iStart = 0, iEnd = 0;
    const uval icIters = ts->iters;
    SysStatus rc;
    uval region;
    SysTime totalFR = 0, totalIC = 0;
    volatile char ch;

    unmapTimeStart = unmapTimeStart; // gets rid of warning...

#if 0
    err_printf("%ld: testFCMWorker started: p %p, numPages=%ld\n",
	    myvp, p, numPages);
#endif

    if (myvp == 0) {
	memory = 0;
	totalCreate = 0;
	//entryCount = NumVP;
    }
    Scheduler::DelayMicrosecs(5000);
    ts->bar->enter();
    err_printf("%ld Touching pages... ",myvp);
    Scheduler::DelayMicrosecs(500000);
    ts->bar->enter();

    start1 = Scheduler::SysTimeNow();
    //iStart = getInstrCount();

    for (uval reopens = 0; reopens < NUM_REOPENS; reopens++) {
	if (myvp == 0) {
	    region = 0;
	    //err_printf("A0");
	    switch (ts->test) {
	    case T_PARTITIONED:
		start2 = Scheduler::SysTimeNow();
#if USE_REGION_REPLICATED
		rc = StubRegionReplicated::_CreateFixedLenExtKludge(
		    region, Size, SEGMENT_SIZE,
		    0, (uval)(AccessMode::writeUserWriteSup), 0, 1,
		    RegionType::K42Region);
#else
		rc = StubRegionDefault::_CreateFixedLenExtKludge(
		    region, Size, SEGMENT_SIZE,
		    0, (uval)(AccessMode::writeUserWriteSup), 0, 1,
		    RegionType::K42Region););
#endif
		end2 = Scheduler::SysTimeNow();
		totalCreate += (end2 - start2);
		passert(_SUCCESS(rc), err_printf("woops\n"));
		break;

#if USE_FCMDEFAULT
	    case T_SHARED:
#if !CACHE_FR
		rc = StubFRComputation::_Create(frOH, myPID);
		passert(_SUCCESS(rc), err_printf("woops\n"));
#endif
		start2 = Scheduler::SysTimeNow();
		rc = StubRegionDefault::_CreateFixedLenExt(
		    region, Size, SEGMENT_SIZE,
		    frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0
		    RegionType::K42Region);
		end2 = Scheduler::SysTimeNow();
		totalCreate += (end2 - start2);
		passert(_SUCCESS(rc), err_printf("woops\n"));
		break;
#else
	    case T_SHARED:
		start2 = Scheduler::SysTimeNow();
		rc = StubRegionDefault::_CreateFixedLenExtKludge(
		    region, Size, SEGMENT_SIZE,
		    0, (uval)(AccessMode::writeUserWriteSup), 0, 0,
		    RegionType::K42Region);
		end2 = Scheduler::SysTimeNow();
		totalCreate += (end2 - start2);
		passert(_SUCCESS(rc), err_printf("woops\n"));
		break;
#endif

#if USE_FCMDEFAULT
	    case T_DYNAMIC:
#if !CACHE_FR
		rc = StubFRComputation::_Create(frOH, myPID);
		passert(_SUCCESS(rc), err_printf("woops\n"));
#endif
		start2 = Scheduler::SysTimeNow();
		rc = StubRegionDefault::_CreateFixedLenExtKludgeDyn(
		    region, Size, SEGMENT_SIZE,
		    frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0, tsOH,
		    RegionType::K42Region);
		end2 = Scheduler::SysTimeNow();
		totalCreate += (end2 - start2);
		passert(_SUCCESS(rc), err_printf("woops\n"));
		break;
#else
	    case T_DYNAMIC:
		start2 = Scheduler::SysTimeNow();
		rc = StubRegionDefault::_CreateFixedLenExtKludgeDyn(
		    region, Size, SEGMENT_SIZE,
		    frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0, tsOH,
		    RegionType::K42Region);
		end2 = Scheduler::SysTimeNow();
		totalCreate += (end2 - start2);
		passert(_SUCCESS(rc), err_printf("woops\n"));
		break;
#endif
	    default: passert(0,;);
	    }

	    //err_printf("Ax");
	    memory = (char *)region;

#if WITH_SINGLE_PROC_STAGE
	    start2 = Scheduler::SysTimeNow();
	    for (uval mapIter = 0; mapIter < icIters; mapIter++) {
		//err_printf("unmapRange(0x%lx, 0x%lx)...", uval(p), Size);
		rc = DREFGOBJ(TheProcessRef)->unmapRange(uval(memory),
							 Size/NUMPROC);
		//passert(_SUCCESS(rc),;);
		//err_printf("done.\n");
		//err_printf("unmapRange time: %ld\n",
		//	       uval(Scheduler::SysTimeNow() - unmapTimeStart));
#if IC_RETOUCH_ALL
		for (uval pagenum=0; pagenum<Size/PAGE_SIZE/NUMPROC; pagenum++) {
#else
		for (uval pagenum=0; pagenum < 1; pagenum++) {
#endif
		    //err_printf("(%ld)",pagenum + (myvp * numPages));
		    cp = (char *)((uval)memory + (pagenum * PAGE_SIZE));
		    ch = *cp; // page fault -- in-core
		    //chkByte(ts, ch, 1);
		}
	    }
	    end2 = Scheduler::SysTimeNow();
	    totalCreate += (end2 - start2); // FIXME: use another vble

#endif

	} else {
#if 0
	    //err_printf("An");
	    uval hmm = 0;
	    while (memory == 0) {
		hmm++;
		if (hmm & 0xffff) Scheduler::YieldProcessor();
	    }
#endif
	}
	ts->bar->enter();

#if DISTINCT_PAGETOUCH
	const char *p = (char *)memory + (myvp * PAGE_SIZE * numPages);
#else
	const char *p = (char *)memory/* + (myvp * PAGE_SIZE * numPages)*/;
#endif

	//err_printf("VP%ld enters FR\n", uval(myvp));
	// FR stage: PF serviced from FR
	//start2 = Scheduler::SysTimeNow();
#if IC_RETOUCH_ALL
	for (uval pagenum=0; pagenum < numPages; pagenum++) {
#else
	for (uval pagenum=0; pagenum < 1; pagenum++) {
#endif
	    //err_printf("(%ld)",pagenum + (myvp * numPages));
	    //err_printf("(%ld)",pagenum);
	    cp = (char *)((uval)p + (pagenum * PAGE_SIZE));

	    start2 = Scheduler::SysTimeNow();
	    ch = *cp; // page fault -- load page from FR
	    end2 = Scheduler::SysTimeNow();
	    totalFR += (end2 - start2);

	    chkByte(ts, ch, 1);
	}
	//end2 = Scheduler::SysTimeNow();
	//totalFR += (end2 - start2);

	//err_printf("VP%ld enters IC\n", uval(myvp));
	// IC stage: In-core PF (unmaps for iters)
	uval switched = 0;
#if TIME_ENTIRE_IC_LOOP
	start2 = Scheduler::SysTimeNow();
#endif
	for (uval mapIter = 0; mapIter < icIters; mapIter++) {
	    // unmap range
#if TIME_ENTIRE_IC_LOOP
	    unmapTimeStart = Scheduler::SysTimeNow();
#endif
#if VP0_UNMAP_ALL
	    ts->bar->enter();
	    if (myvp == 0) {
		//err_printf("unmapRange(0x%lx, 0x%lx)...", uval(p), Size);
		rc = DREFGOBJ(TheProcessRef)->unmapRange(uval(p), Size);
		passert(_SUCCESS(rc),;);
		//err_printf("done.\n");
	    }
	    ts->bar->enter();
#else
	    //err_printf("[VP%ld:unmapRange(0x%lx, 0x%lx)]\n", uval(myvp),
	    //	    uval(p), Size/NUMPROC);
	    rc = DREFGOBJ(TheProcessRef)->unmapRange(uval(p), Size/NUMPROC);
	    //err_printf("[VP%ld:unmapRange(0x%lx, 0x%lx) DONE]\n", uval(myvp),
	    //	    uval(p), Size/NUMPROC);
#endif
	    //err_printf("unmapRange time: %ld\n",
	    //	       uval(Scheduler::SysTimeNow() - unmapTimeStart));
#if TIME_ENTIRE_IC_LOOP
	    unmapTimeTotal += (Scheduler::SysTimeNow() - unmapTimeStart);
#endif
#if IC_RETOUCH_ALL
	    for (uval pagenum=0; pagenum < numPages; pagenum++) {
#else
	    for (uval pagenum=0; pagenum < 1; pagenum++) {
#endif
		//err_printf("(%ld)",pagenum + (myvp * numPages));
		cp = (char *)((uval)p + (pagenum * PAGE_SIZE));
#if !TIME_ENTIRE_IC_LOOP
		start2 = Scheduler::SysTimeNow();
#endif
		ch = *cp; // page fault -- in-core
#if !TIME_ENTIRE_IC_LOOP
		end2 = Scheduler::SysTimeNow();
		totalIC += (end2 - start2);
#endif
		chkByte(ts, ch, 1);
	    }
	    if (mapIter == 1 && !switched && myvp == 0 &&
		NumVP >= SWITCH_NUMVP && ts->test == T_DYNAMIC) {
		// switch here instead??
#if 1 // FIXME: rethink location and procedure
		//err_printf("start switch...\n");
		//SysTime siStart = Scheduler::SysTimeNow();
		//STUB_FROM_OH(StubTestSwitch, stubTS, tsOH);
		passert(pStubTS, ;);
		pStubTS->_startSwitch();
		//SysTime siEnd = Scheduler::SysTimeNow();
		//err_printf("Switch init time: %ld\n", uval(siEnd-siStart));
		switched = 1;
#endif
	    }
	}
	// FIXME: problem above -- we are timing the Scheduler::YieldProcessor
	// which adds a lot of noise...
#if TIME_ENTIRE_IC_LOOP
	end2 = Scheduler::SysTimeNow();
	totalIC += (end2 - start2);
#endif

	barrierStart = Scheduler::SysTimeNow();
	// Region Destroy stage
#if 0
	FetchAndAddSignedSynced(&entryCount, -1);
	while (entryCount > 0) {}
#else
	ts->bar->enter();
	//barrierTotal += (Scheduler::SysTimeNow() - barrierStart);
#endif

	if (myvp == 0) {
	    // destroy the data region from my address space
	    //err_printf("DREFGOBJ(TheProcessRef)->regionDestroy(region);\n");
	    DREFGOBJ(TheProcessRef)->regionDestroy(region);
	    memory = 0;
	}

	//barrierStart = Scheduler::SysTimeNow();
	ts->bar->enter();
#if 0
	if (myvp == 0) {
	    entryCount = NumVP;
	}
#endif
	barrierTotal += (Scheduler::SysTimeNow() - barrierStart);

    }

    //iEnd = getInstrCount();
    end1 = Scheduler::SysTimeNow();

    ts->bar->enter();

    const SysTime localTotal = end1 - start1 - barrierTotal - unmapTimeTotal;
    err_printf("%ld: All Pages Read (total: %lld)\n", myvp, localTotal);

    ts->bar->enter();

    ts->time2 = totalFR;			// FR
    ts->time3 = totalIC - unmapTimeTotal;	// IC
    ts->time4 = totalCreate;	// Create
    ts->time1 = totalIC;	// IC with unmaps all combined
    //ts->time1 = ts->time2 + ts->time3 + ts->time4;
    ts->time5 = end1 - start1;

#if 1
    err_printf("%ld: testFCMWorker done: %lld %lld %lld %lld %lld\n", myvp,
	       ts->time5, ts->time1, ts->time2, ts->time3, ts->time4);
#if TIME_ENTIRE_IC_LOOP
    err_printf("%ld: testFCMWorker done: IC with unmap = %lld\n", myvp,
	       totalIC);
    err_printf("%ld: testFCMWorker done: unmapTimeTotal = %lld\n", myvp,
	       unmapTimeTotal);
#endif
    //err_printf("%ld: testFCMWorker done: barrierTotal = %lld\n", myvp,
	       //barrierTotal);
#endif
    //Scheduler::DelayMicrosecs(100000);
}

void
doFCMTest(uval param, uval pagePerVP, uval icIters, uval ptIdx)
{
    SysStatusProcessID pidrc;
#if SKIP_VP0
    BlockBarrier bar(NumVP-1);
#else
    BlockBarrier bar(NumVP);
#endif
    TestStructure *ts = 0;
    SysTime start = 0, end = 0;
    //volatile char *memory = 0;

    //uval region;

    /*const*/ uval regionSize = pagePerVP * NUMPROC * PAGE_SIZE;

    err_printf("Doing FCM test:\n");
    switch (param) {
    case T_PARTITIONED:
	err_printf("Partitioned");
	break;
    case T_SHARED:
	err_printf("Shared");
	break;
    case T_DYNAMIC:
	err_printf("Dynamic");
	break;
    default:
	passert(0,;);
    }
    err_printf(" FCM (%ld distinct pages per VP,", pagePerVP);
    err_printf(" %ld IC iters)\n", icIters);

    pidrc = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(pidrc), err_printf("woops\n"));
    myPID = _SGETPID(pidrc);

#if CACHE_FR
    SysStatus rc = StubFRComputation::_Create(frOH);
    tassert(_SUCCESS(rc), err_printf("woops\n"));
#endif

    ts = TestStructure::Create(
	NumVP, regionSize/*size*/, icIters,
	uval(param)/*test*/, 0/*misc*/, 0/*ptr*/, &bar);

    start = Scheduler::SysTimeNow();
    DoConcTest(NumVP, SimpleThread::function(testFCMWorker), ts);
    end = Scheduler::SysTimeNow();

#if 0
    err_printf("#workers %ld\n", uval(NumVP));
    for (uval i = 0; i < NumVP; i++) {
	err_printf("vp%ld: CacheLoad(%ld) In-core(%ld,perIter=%ld)\n", i,
		uval(ts[i].time1), uval(ts[i].time2), uval(ts[i].time2/iters));
    }
#endif
    err_printf("Total elapsed time = %lld\n", end-start);
    // do avg
    SysTime avg;
    uval vp;

#define DOAVG(GIDX, FIELD) \
    avg = 0;						\
    for (vp = 0; vp < NumVP; vp++) {			\
	avg += (FIELD);				\
    }							\
    avg /= NumVP;					\
    graphAll[GIDX][ptIdx].x = icIters;			\
    graphAll[GIDX][ptIdx].y = avg;

#if 1
    DOAVG(0, ts[vp].time1); // TOTAL
    DOAVG(1, ts[vp].time2); // FR
    DOAVG(2, ts[vp].time3); // IC
    DOAVG(3, ts[vp].time4 + ts[vp].time2); // reg create + FR
    DOAVG(4, ts[vp].time4); // reg create
    DOAVG(5, ts[vp].time5); // full total
#else
    graphAll[0][ptIdx].x = icIters;
    graphAll[0][ptIdx].y = ts[0].time1; // TOTAL
    graphAll[1][ptIdx].x = icIters;
    graphAll[1][ptIdx].y = ts[0].time2; // FR
    graphAll[2][ptIdx].x = icIters;
    graphAll[2][ptIdx].y = ts[0].time3; // IC
    graphAll[3][ptIdx].x = icIters;
    graphAll[3][ptIdx].y = ts[0].time4 + ts[0].time2; // create + FR
    graphAll[4][ptIdx].x = icIters;
    graphAll[4][ptIdx].y = ts[0].time4; //create
    graphAll[5][ptIdx].x = icIters;
    graphAll[5][ptIdx].y = ts[0].time5; //full total
#endif

    delete[] ts;

#if 0
    if (1) {
	// destroy the data region from my address space
	DREFGOBJ(TheProcessRef)->regionDestroy(region);
    }
#endif

    err_printf("All done FCM test\n\n");
}

int test_main(VPNum numvp)
{
    uval it;

    StubTestSwitch stubTS(StubObj::UNINITIALIZED);
    SysStatus rc = StubTestSwitch::_Create(tsOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));
    stubTS.setOH(tsOH);
    pStubTS = &stubTS;

    //NumVP = NUMPROC;
    NumVP = numvp;
    err_printf("dcofcm: NumVP is %ld\n", uval(NumVP));

    //AutoThreadDeactivation atd;

#if 0
    for (it = 0; it < 2; it++) {
	doFCMTest(T_SHARED, 2, 1);
	doFCMTest(T_PARTITIONED, 2, 1);
	doFCMTest(T_DYNAMIC, 2, 1);
    }
#endif

#if 1
    sval icIters = icItersTest;
    for (uval testType = 0; testType < 3; testType++) {
	// warm-up
	doFCMTest(testType, pagePerVP, 50, 0);
	uval ptIdx = 0;
	//for (icIters = IC_ITER_MAX-1; icIters < IC_ITER_MAX; icIters += 10) {
	//for (icIters = IC_ITER_MAX; icIters >= 0; icIters -= 4) {
	    for (it = 0; it < 1; it++) {
		doFCMTest(testType, pagePerVP, icIters, ptIdx++);
#if 0
		doFCMTest((sval)testType, 2, icIters);
		doFCMTest((sval)testType, 4, icIters);
		doFCMTest((sval)testType, 8, icIters);
		doFCMTest((sval)testType, 16, icIters);
		doFCMTest((sval)testType, 32, icIters);
		doFCMTest((sval)testType, 64, icIters);
		doFCMTest((sval)testType, 128, icIters);
		//doFCMTest((sval)testType, 256, icIters);
#endif
	    }
	//}
	for (uval itout = 0; itout < 6; itout++) {
	    err_printf("\n# Graph: %s.%s.dcofcm (NumVP=%ld, LOOP_IT=%d):\n",
		       tStrings[testType], gStrings[itout], NumVP, LOOP_IT);
	    for (it = 0; it < ptIdx; it++) {
		if ((it == 0) ||
		    graphAll[itout][it].x || graphAll[itout][it].y) {
		    err_printf("%ld %ld\n",
			    graphAll[itout][it].x, graphAll[itout][it].y);
		}
	    }
	}
	graphIC[testType][NumVP-1].x = NumVP;
	graphIC[testType][NumVP-1].y = graphAll[2][0].y;
	graphConcTest[testType][NumVP-1].x = NumVP;
	graphConcTest[testType][NumVP-1].y = graphAll[5][0].y;
	Scheduler::DelayMicrosecs(500000);
    }
#endif

    return 0;
}

int main(int argc, char *argv[])
{
    NativeProcess();

#if 0
    test_main(1);
    if (NUMPROC > 1) {
	test_main(2);
	if (NUMPROC > 2) test_main(NUMPROC);
    }
#else
    StubWire::SuspendDaemon();
    Scheduler::DelayMicrosecs(100000);
    err_printf("\n*** BEGIN %s\n", argv[0]);

    SysTime tickTime;
    tickTime = Scheduler::SysTimeNow();
    Scheduler::DelayMicrosecs(1000000);
    tickTime = Scheduler::SysTimeNow() - tickTime;
    err_printf("1000000 uSec == %lld ticks\n", tickTime);
    tickTime = Scheduler::SysTimeNow();
    Scheduler::DelayMicrosecs(10);
    tickTime = Scheduler::SysTimeNow() - tickTime;
    err_printf("10 uSec == %lld ticks\n", tickTime);
    tickTime = Scheduler::SysTimeNow();
    Scheduler::DelayMicrosecs(100000);
    tickTime = Scheduler::SysTimeNow() - tickTime;
    err_printf("100000 uSec == %lld ticks\n", tickTime);
    tickTime = Scheduler::SysTimeNow();
    Scheduler::DelayMicrosecs(10);
    tickTime = Scheduler::SysTimeNow() - tickTime;
    err_printf("10 uSec == %lld ticks\n", tickTime);
    Scheduler::DelayMicrosecs(100000);

    VPNum numvp;
    //for (numvp = 1; numvp <= NUMPROC; numvp++) {
    // FIXME: temp hack to avoid starting too many VPs
    for (numvp = NUMPROC > 8 ? 8 : NUMPROC; numvp > 0; numvp--) {
	test_main(numvp);
	Scheduler::DelayMicrosecs(100000);
    }
    for (uval testType = 0; testType < 3; testType++) {
	err_printf("\n# %s\n", argv[0]);
	err_printf("# In-core PF (numproc,ticks/icPF) type(%s)\n",
		   tStrings[testType]);
	err_printf("# DISTINCT_PAGETOUCH=%d\n", DISTINCT_PAGETOUCH);
	err_printf("# IC_RETOUCH_ALL=%d\n", IC_RETOUCH_ALL);
	err_printf("# LOOP_IT(spin)=%d\n", LOOP_IT);
	err_printf("# TIME_ENTIRE_IC_LOOP=%d\n", TIME_ENTIRE_IC_LOOP);
	err_printf("# USE_FCMDEFAULT=%d\n", USE_FCMDEFAULT);
	err_printf("# USE_REGION_REPLICATED=%d\n", USE_REGION_REPLICATED);
	for (numvp = 0; numvp < NUMPROC; numvp++) {
#if IC_RETOUCH_ALL
	    err_printf("%ld %ld\n", graphIC[testType][numvp].x,
		    graphIC[testType][numvp].y/(icItersTest*pagePerVP));
#else
	    err_printf("%ld %ld\n", graphIC[testType][numvp].x,
		    graphIC[testType][numvp].y/icItersTest);
#endif
	}
    }
    for (uval testType = 0; testType < 3; testType++) {
	err_printf("\n# ConcTest (%ld icPF, pagePerVP=%ld) type(%s)\n",
		icItersTest, pagePerVP, tStrings[testType]);
	err_printf("# DISTINCT_PAGETOUCH=%d\n", DISTINCT_PAGETOUCH);
	err_printf("# IC_RETOUCH_ALL=%d\n", IC_RETOUCH_ALL);
	err_printf("# TIME_ENTIRE_IC_LOOP=%d\n", TIME_ENTIRE_IC_LOOP);
	err_printf("# LOOP_IT(spin)=%d\n", LOOP_IT);
	err_printf("# USE_FCMDEFAULT=%d\n", USE_FCMDEFAULT);
	err_printf("# USE_REGION_REPLICATED=%d\n", USE_REGION_REPLICATED);
	for (numvp = 0; numvp < NUMPROC; numvp++) {
	    err_printf("%ld %ld\n", graphConcTest[testType][numvp].x,
		    graphConcTest[testType][numvp].y);
	}
    }
    err_printf("\n*** END %s\n", argv[0]);
    StubWire::RestartDaemon();
#endif
}
