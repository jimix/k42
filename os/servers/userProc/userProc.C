/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: userProc.C,v 1.174 2005/08/24 14:19:52 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <scheduler/Scheduler.H>
#include <sys/ProcessServer.H>
#include <misc/linkage.H>
#include <stub/StubProcessServer.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include <stub/StubWire.H>
#include <usr/ProgExec.H>
#include <sync/MPMsgMgr.H>
#include <sync/Barrier.H>
#include <mem/Access.H>
#include <io/PathName.H>
#include <io/NameTreeLinux.H>
#include <io/DirBuf.H>
#include <io/FileLinux.H>
#include <io/FileLinuxSocket.H>
#include <io/Socket.H>
#include <stdio.h>
#include <cobj/CObjRootSingleRep.H>
#include <cobj/CObjRootMultiRep.H>
#include "../sample/SampleServiceWrapper.H"
#include <misc/testSupport.H>
#include <misc/BaseRandom.H>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/systemAccess.H>
//#define DEBUG_TEST 1

Barrier *GlobalBarrier;

VPNum NumVP = 1;			// total vp to use for tests
uval runningRegress = 0;

extern "C" void _start();

// FIXME:  This code lives in misc/testSupport.* simpleThread.*
//         For the moment to avoid duplication have ifdef this
//         version out.  When we are sure the other version
//         is correct this stuff can be take completely out of
//         the file
#if 0
/*====================================================================*/

// used to trigger simos events

extern "C" {
    void tstEvent_startworker();
    void tstEvent_endworker();
    void tstEvent_starttest();
    void tstEvent_endtest();
}

void tstEvent_starttest()
{
  /* empty body */
}
void tstEvent_endtest()
{
  /* empty body */
}
void tstEvent_startworker()
{
  /* empty body */
}
void tstEvent_endworker()
{
  /* empty body */
}

// --------------------------------------------------------------------------

uval8 ProcsCreated[Scheduler::VPLimit];		// bool for which vps created
					// vp 0 is always assumed created

static void
MakeMP(VPNum numVP)
{
    VPNum     vp;
    SysStatus rc;

    passert(numVP <= Scheduler::VPLimit, {});

    for (vp = 1; vp < numVP; vp++) {
	if (!ProcsCreated[vp]) {
	    rc = ProgExec::CreateVP(vp);
	    passert(_SUCCESS(rc), {});
	    ProcsCreated[vp] = 1;
	}
    }
}

// --------------------------------------------------------------------------

class MyThread {

public:
    typedef SysStatus (*function)(void *);

private:
    friend class GetRidofGCCWarning;	// gets rid of warning :-)

    SysStatus   rc;			// thread result
    volatile ThreadID joiner;		// thread waiting for join
    volatile uval     done;		// flag signaling thread is done
    function    func;			// thread function
    void       *arg;			// argument for thread

    ~MyThread() { /* null to prevent user delete */ }
    MyThread() { /* null to prevent user new */ }

    struct CreateMyThreadMsg : public MPMsgMgr::MsgAsync {
	MyThread *thread;

	virtual void handle() {
	    MyThread *const t = thread;
	    free();
	    Start(t);
	}
    };
    friend struct CreateMyThreadMsg;

    // called on thread start side from createThread call
    static void Start(MyThread *thread) {
	thread->rc = thread->func(thread->arg);
	thread->done = 1;
	ThreadID j = thread->joiner;
	if (j != Scheduler::NullThreadID) {
	    Scheduler::Unblock(j);
	}
    }

public:

    DEFINE_GLOBAL_NEW(MyThread);

    static MyThread *Create(function func, void *arg,
			    DispatcherID dspid = Scheduler::GetDspID()) {
	SysStatus rc;
	MyThread *t = new MyThread();
	t->func = func;
	t->arg  = arg;
	t->joiner = Scheduler::NullThreadID;
	t->done = 0;
	if (dspid == Scheduler::GetDspID()) {
	    rc = Scheduler::ScheduleFunction(Scheduler::ThreadFunction(Start),
					     uval(t));
	} else {
	    CreateMyThreadMsg *const msg =
		new(Scheduler::GetEnabledMsgMgr()) CreateMyThreadMsg;
	    tassert(msg != NULL, err_printf("message allocation failed.\n"));
	    msg->thread = t;
	    rc = msg->send(dspid);
	}
	if (!_SUCCESS(rc)) {
	    delete t;
	    t = 0;
	}
	return t;
    }

    static SysStatus Join(MyThread *&t) {
	SysStatus val;
	t->joiner = Scheduler::GetCurThread();
	while (!t->done) {
	    Scheduler::DeactivateSelf();
	    Scheduler::Block();
	    Scheduler::ActivateSelf();
	}
	val = t->rc;
	delete t;
	t = 0;
	return val;
    }
};

// --------------------------------------------------------------------------

class teststruct {
public:
    Barrier     *bar;
    uval         workers;
    SysTime      time1, time2;
    uval         instr1, instr2;
    uval         size;
    uval         iters;
    uval         test;
    uval         misc;
    void        *ptr;

    DEFINE_GLOBAL_NEW(teststruct);

    void init(uval w, uval s, uval i, uval t, uval m, void *p, Barrier *b) {
	workers=w; size=s; iters=i; test=t; misc=m; ptr=p; bar=b;
	time1 = time2 = 0;
    }
    static teststruct *Create(uval w, uval s, uval i, uval t, uval m, void *p,
			      Barrier *b) {
	teststruct *ts = new teststruct[w];
	uval j;
	for (j = 0; j < w; j++) {
	    ts[j].init(w,s,i,t,m,p,b);
	}
	return ts;
    }
};

static void
DoConcTest(VPNum numWorkers, MyThread::function func, teststruct *p)
{
    VPNum vp;
    MyThread *threads[numWorkers];

    MakeMP(numWorkers);
    for (vp=0; vp < numWorkers; vp++) {
	threads[vp] = MyThread::Create(func, &p[vp], SysTypes::DSPID(0, vp));
	passert(threads[vp]!=0, err_printf("Thread create failed\n"));
    }
    for (vp=0; vp < numWorkers; vp++) {
	MyThread::Join(threads[vp]);
    }
}
#endif /* #if 0 */
/*====================================================================*/
// testing constructors

typedef struct {
    int test[40];
} testAllocT;

#define TSTRT  "/root/gorp"
PathNameDynamic<AllocGlobal> *rt =0;
uval rtlen = 0;


// Sigh, don't know how to do this in a class
const static char *names[] = {"IPC",
			      "USER_IPC",
			      "USER_IPC2",
			      "USER_IPC3",
			      "USER_IPC4",
			      "Thread",
			      "Page Fault 1",
			      "Page Fault 2",
			      "Page Fault 3",
			      "Page Fault 4"
};

#define MAX_TESTS 100
#define MAX_VPS 32
typedef struct {
    // test names must be first sice we reference arrays based on them
    typedef enum {IPC, USER_IPC, USER_IPC2, USER_IPC3, USER_IPC4, THREAD,
		  PAGE_FAULT1, PAGE_FAULT2, PAGE_FAULT3, PAGE_FAULT4,
		  MAX_EXPER} test;


    uval numbIters[MAX_EXPER];		// number of iterations for an exper
    uval numbReqPerIter[MAX_EXPER];	// requests per interation

    uval testCount, numbTestsToRun;
    SysTime tResults[MAX_EXPER][MAX_VPS][MAX_TESTS];
    uval iResults[MAX_EXPER][MAX_VPS][MAX_TESTS];

    // +1 since we start a 1 vp
    SysTime overallTResults[MAX_EXPER][Scheduler::VPLimit+1];
    uval overallIResults[MAX_EXPER][Scheduler::VPLimit+1];

    void setNumbTestsToRun(uval val) {numbTestsToRun=val;};
    void incTestCount() {testCount++;};
    void enterResult(uval exper, VPNum vp, SysTime timeVal, uval instrVal) {
	tResults[exper][vp][testCount] = timeVal;
	iResults[exper][vp][testCount] = instrVal;
    }
    void printResults(uval which);
    void printOverallResults(uval which);
    void storeOverallResults(VPNum numbVPs);
} stats;

stats Stats;

char *
div3(uval num, uval den, char *res)
{
    sval mylen,len;
    char myres[16];

    if (den == 0) {
	strcpy(res,"NaN");
    } else if (num*1000 <= den) {
	strcpy(res,"too small");
    }
    else {
	sprintf(res, "%ld", num/den);
	sprintf(myres, "000%ld", (num*1000)/den);
	len = strlen(res);
	mylen = strlen(myres);
	res[len]= '.';
	res[len+4] = 0;
	res[len+3] = myres[mylen-1];
	res[len+2] = myres[mylen-2];
	res[len+1] = myres[mylen-3];
    }
    return (res);
}

void
stats::printOverallResults(uval which)
{
    VPNum i;
    uval k;

    char str1[16], str2[16];
    uval startExp, endExp;

    if (which == MAX_EXPER) {
	startExp = 0;
	endExp = MAX_EXPER;
    } else {
	startExp = which;
	endExp = which+1; // used for loops only need to do one iteration
    }

    cprintf("---------------------------------------------------------------");
    cprintf("\npastable results for overall instruction slowdown\n");
    cprintf("numb vps");
    for (k=startExp;k<endExp;k++) cprintf(" \"%s\"", names[k]);
    cprintf("\n");
    cprintf("startGraphResults: overall instruction slowdown\n\n");

    VPNum const numPPs = DREFGOBJ(TheProcessRef)->ppCount();
    for (i=1; i<=numPPs; i++) {
	cprintf("%ld ",i);
	for (k=startExp;k<endExp;k++) {
	    cprintf("%s ",
		    div3(overallIResults[k][i],overallIResults[k][1],str1));
	}
	cprintf("\n");
    }
    cprintf("\nendGraphResults: overall instruction slowdown\n");


    cprintf("\npastable results for overall time slowdown\n");
    cprintf("numb vps ");
    for (k=startExp;k<endExp;k++) cprintf("\" %s \"", names[k]);
    cprintf("\n");
    cprintf("startGraphResults: overall time slowdown\n\n");

    for (i=1; i<=numPPs; i++) {
	cprintf("%ld ",i);
	for (k=startExp;k<endExp;k++) {
	    cprintf("%s ",
		    div3(overallTResults[k][i],overallTResults[k][1],str1));
	}
	cprintf("\n");
    }
    cprintf("\nendGraphResults: overall time slowdown\n");


    cprintf("\nOverall results of tests:\n");

    for (i=1; i<=numPPs; i++) {
	cprintf(" for %ld VPs:\n",i);
	for (k=startExp;k<endExp;k++) {
	    cprintf("  %s instr %ld efficiency %s time %lld efficiency %s\n",
		    names[k],
		    overallIResults[k][i],
		    div3(overallIResults[k][1],overallIResults[k][i],str1),
		    overallTResults[k][i],
		    div3(overallTResults[k][1],overallTResults[k][i],str2));
	}
    }
}

void
stats::storeOverallResults(VPNum numbVPs)
{
    uval i,j, k;
    uval ttotsum[stats::MAX_EXPER], itotsum[stats::MAX_EXPER];

    for (k=0;k<MAX_EXPER;k++) {
	ttotsum[k]=0;
	itotsum[k]=0;
    }


    for (j=0;j<NumVP;j++) {
	for (i=0;i<numbTestsToRun;i++) {
	    for (k=0;k<MAX_EXPER;k++) {
		itotsum[k] += iResults[k][j][i];
		ttotsum[k] += tResults[k][j][i];
	    }
	}
    }

    for (k=0;k<MAX_EXPER;k++) {
	overallTResults[k][numbVPs] =
	    ttotsum[k]/(numbTestsToRun*NumVP*numbIters[k]*numbReqPerIter[k]);
	overallIResults[k][numbVPs] =
	    itotsum[k]/(numbTestsToRun*NumVP*numbIters[k]*numbReqPerIter[k]);
    }
}

void
stats::printResults(uval which)
{
    uval i,j;
    uval ttotsum[MAX_EXPER], itotsum[MAX_EXPER];
    uval tsum[MAX_EXPER], isum[MAX_EXPER];
    uval startExp, endExp;

    if (which == MAX_EXPER) {
	startExp = 0;
	endExp = MAX_EXPER;
    } else {
	startExp = which;
	endExp = which+1; // used for loops only need to do one iteration
    }

    for (uval k=startExp;k<endExp;k++) {
	ttotsum[k]=0;
	itotsum[k]=0;
    }

    cprintf("\nResults for %ld processors on %ld VPs over %ld runs\n",
	    DREFGOBJ(TheProcessRef)->ppCount(),
	    NumVP, numbTestsToRun);

    for (j=0;j<NumVP;j++) {
	for (uval k=startExp;k<endExp;k++) {
	    tsum[k]=0;
	    isum[k]=0;
	}
	for (i=0;i<numbTestsToRun;i++) {
	    for (uval k=startExp;k<endExp;k++) {
		tsum[k]    += tResults[k][j][i];
		isum[k]    += iResults[k][j][i];
		ttotsum[k] += tResults[k][j][i];
		itotsum[k] += iResults[k][j][i];
	    }
	}
	for (uval k=startExp;k<endExp;k++) {
	    cprintf(" average %s instr %ld time %ld on vp %ld\n",
		    names[k],
		    isum[k]/(numbTestsToRun*numbIters[k]*numbReqPerIter[k]),
		    tsum[k]/(numbTestsToRun*numbIters[k]*numbReqPerIter[k]),j);
	}
    }
    cprintf("\n");
    for (uval k=startExp;k<endExp;k++) {
	cprintf(" overall average for %s instr %ld time %ld\n",
		names[k],
		itotsum[k]/(numbTestsToRun*NumVP*numbIters[k]*
			    numbReqPerIter[k]),
		ttotsum[k]/(numbTestsToRun*NumVP*numbIters[k]*
			    numbReqPerIter[k]));
    }

    if (runningRegress) return;
    // now print out results for pasting but print across VPs
    for (uval k=startExp;k<endExp;k++) {
	cprintf("------------------------------------------------------------");
	cprintf("\npastable time results for %s \n", names[k]);
	cprintf("results for each vp starting from 0\n");
	cprintf("startGraphResults\n\n");
	for (j=0;j<NumVP;j++) {
	    tsum[k]=0;

	    for (i=0;i<numbTestsToRun;i++) {
		tsum[k]    += tResults[k][j][i];
	    }
	    cprintf("%ld %ld\n",j,
		    tsum[k]/(numbTestsToRun*numbIters[k]*numbReqPerIter[k]));
	}
	cprintf("endGraphResults\n");
    }
    for (uval k=startExp;k<endExp;k++) {
	cprintf("------------------------------------------------------------");
	cprintf("\npastable instr results for %s \n", names[k]);
	cprintf("results for each vp starting from 0\n");
	cprintf("startGraphResults\n\n");
	for (j=0;j<NumVP;j++) {
	    isum[k]=0;

	    for (i=0;i<numbTestsToRun;i++) {
		isum[k]    += iResults[k][j][i];
	    }
	    cprintf("%ld %ld\n",j,
		    isum[k]/(numbTestsToRun*numbIters[k]*numbReqPerIter[k]));
	}
	cprintf("endGraphResults\n");
    }
}

void
initializeRt()
{
    char buf[1046];
    SysStatus rc;
    printf("*Starting path name mushing\n");

    printf("creating rt from %s\n", TSTRT);
    rtlen = PathNameDynamic<AllocGlobal>::Create(TSTRT, strlen(TSTRT), 0, 0,
						 rt);
    rc = rt->getUPath(rtlen, buf,sizeof(buf));
    tassert(_SUCCESS(rc), err_printf("woops, buf too small\n"));
    printf("unix path got back is %s\n", buf);
}

void
caseZero()
{
    uval pln;
    SysStatus rc;
    char* ret;
    if (!rtlen)
	initializeRt();
    char buf[1024];
    printf("type in a string: ");

    ret = fgets(buf, 1024, stdin);
    tassert(ret, err_printf("fgets failed\n"));

    pln = strlen(buf);
    if (buf[pln-1] == '\n') {
	buf[pln-1] = '\0';
	pln--;
    }
    PathNameDynamic<AllocGlobal> *tmp;
    uval len = PathNameDynamic<AllocGlobal>::Create(buf, pln, rt, rtlen, tmp);

    rc = tmp->getUPath(len,buf,sizeof(buf));
    tassert(_SUCCESS(rc), err_printf("woops, buf too small\n"));

    printf("unix path got back is %s\n", buf);

    printf("splitting path after first element\n");
    PathNameDynamic<AllocGlobal> *ptr = tmp->getNext(len);
    PathNameDynamic<AllocGlobal> *tmp1, *tmp2;
    uval len1, len2;
    tmp->splitCreate(len, ptr, tmp1, len1, tmp2, len2);

    rc = tmp1->getUPath(len1,buf,sizeof(buf));
    tassert(_SUCCESS(rc), err_printf("woops, buf too small\n"));
    printf("unix path first part is %s\n", buf);

    rc = tmp2->getUPath(len2,buf,sizeof(buf));
    tassert(_SUCCESS(rc), err_printf("woops, buf too small\n"));
    printf("unix path second part is %s\n", buf);

    tmp->destroy(len);
    tmp1->destroy(len1);
    tmp2->destroy(len2);
}

void
caseLs()
{
    SysStatus rc;
    char* ret;
    char buf[1024];
    printf("type in a directory name: ");

    ret = fgets(buf, 1024, stdin);
    tassert(ret, err_printf("fgets failed\n"));

    uval pln = strlen(buf);
    if (buf[pln-1] == '\n') {
	buf[pln-1] = '\0';
	pln--;
    }

    FileLinuxRef flr;
    rc = FileLinux::Create(flr, buf, O_RDONLY, 0);
    if (_FAILURE(rc)) {
	err_printf("open of directory %s failed _SERROR =(%lu,%lu,%lu)\n",
		   buf, _SERRCD(rc), _SCLSCD(rc), _SGENCD(rc));
	return;
    }

    for (;;) {
	SysStatusUval rv;
	//Max to comfortably fit in a PPC page
	struct direntk42 dbuf[(PPCPAGE_LENGTH_MAX/sizeof(struct direntk42))];

	rv = DREF(flr)->getDents(dbuf, sizeof(dbuf));
	if (_FAILURE(rv)) {
	    err_printf("read of directory %s failed "
		       "_SERROR =(%lu,%lu,%lu)\n",
		       buf, _SERRCD(rv), _SCLSCD(rv), _SGENCD(rv));
	    DREF(flr)->destroy();
	    return;
	} else if (_SGETUVAL(rv) == 0) {
	    DREF(flr)->destroy();
	    return;
	}

	struct direntk42 *dp = dbuf;
	do {
	    cprintf("- %s - \n", dp->d_name);
	} while (dp->d_off != 0 &&
		 (dp = (struct direntk42 *)((uval)dbuf + dp->d_off)));
    }
}

void
caseOne()
{
    SysStatus rc;
    uval pln;
    char buf[1024];
    printf("appending two strings together\n");

    printf("type in first string: ");

    char *ret = fgets(buf, 1024, stdin);
    tassert(ret, err_printf("fgets failed\n"));

    pln = strlen(buf);
    if (buf[pln-1] == '\n') {
	buf[pln-1] = '\0';
	pln--;
    }

    PathNameDynamic<AllocGlobal> *tmp1;
    uval len1 = PathNameDynamic<AllocGlobal>::Create(buf, pln, 0, 0, tmp1);

    printf("type in second string: ");

    ret = fgets(buf, 1024, stdin);
    tassert(ret, err_printf("fgets failed\n"));

    pln = strlen(buf);
    if (buf[pln-1] == '\n') {
	buf[pln-1] = '\0';
	pln--;
    }
    PathNameDynamic<AllocGlobal> *tmp2;
    uval len2 = PathNameDynamic<AllocGlobal>::Create(buf, pln, 0, 0, tmp2);

    PathNameDynamic<AllocGlobal> *tmp3;
    uval len3 = tmp1->create(len1, tmp2, len2, tmp3);

    rc = tmp3->getUPath(len3,buf,sizeof(buf));
    tassert(_SUCCESS(rc), err_printf("woops, buf too small\n"));
    printf("unix path got back is %s\n", buf);
    tmp1->destroy(len1);
    tmp2->destroy(len2);
    tmp3->destroy(len3);
}


void
misc()
{

#if 0
    asm("int $3");
#endif /* #if 0 */
    printf("Another test %d\n", 19);

    //========================= Test Alloc ==========================
    {
	testAllocT *testAlloc;
	//int rc;

	cprintf("before test alloc\n");

	// allocate from unpinned pool
	testAlloc = (testAllocT *)allocGlobal(sizeof(testAllocT));

	testAlloc->test[3] = 17;
	cprintf("after test alloc\n");

    }

    //========================= TESTING ==========================

    // now starting some console stuff
    cprintf("hello world\n");

    cprintf("thread done\n");
}


SysStatus
threadTestFunc(void *p)
{
    (void)p;
    return 0;
}


void
threadTestWorker(TestStructure *ts)
{
    uval i;
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    SysTime start, end;
    uval iStart, iEnd;
    SimpleThread *t;

    cprintf("threadTestWorker %ld started\n", uval(Scheduler::GetVP()));

    // warm things up a bit
    t = SimpleThread::Create(threadTestFunc,0);
    (void)SimpleThread::Join(t);

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (i = 0; i < numIters; i++) {
	t = SimpleThread::Create(threadTestFunc,0);
	(void)SimpleThread::Join(t);
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

void
threadTest()
{
    uval i;
    const uval numIters = Stats.numbIters[stats::THREAD];
    const uval size = 0;
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, size, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(threadTestWorker), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::THREAD, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters));
    }

    delete[] ts;
}

void
pingpong(volatile uval *p, uval num)
{
    sval tmp;
    uval i, iter;
    uval vp = (uval)Scheduler::GetVP();

    GlobalBarrier->enter();
    for (iter = 0; iter < 4; iter++) {
//	while ((uval(*p) & 0x1) != num) {
//	    Scheduler::Yield();
//	}
	tmp = FetchAndAddVolatile(p, 1);
	for (i = 0; i < vp*100000; i++)
	    ;
	GlobalBarrier->enter();
	cprintf("%ld: %ld\n", Scheduler::GetVP(), tmp);
	GlobalBarrier->enter();
    }
    cprintf("%ld: pingpong all done\n", Scheduler::GetVP());
}

static void
threadFunction(uval data)
{
    volatile uval *p = (volatile uval *)data;
    cprintf("threadFunction: vp %ld, p %p, *p %lx\n",
	    Scheduler::GetVP(), p, *p);
    pingpong(p, 1);
}

uval datum = 3;

struct TestMsg : public MPMsgMgr::MsgAsync {
    uval d1;
    uval d2;

    virtual void handle() {
	cprintf("TestFunction (vp %ld): received d1 %ld, d2 %ld.\n",
		Scheduler::GetVP(), d1, d2);
	free();
	Scheduler::DisabledScheduleFunction(threadFunction,(uval)&datum);
    }
};

void
vpTest()
{
    SysStatus rc;
    VPNum vp;

    MakeMP(NumVP);

    vp = 1;

    GlobalBarrier = new BlockBarrier(2);
    //GlobalBarrier = new SpinBarrier(2);
    //GlobalBarrier = new SpinOnlyBarrier(2);

    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();
    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();
    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();
    cprintf("vptest:  yielding.\n");
    Scheduler::Yield();

    cprintf("vptest: calling sendDisabledAsync().\n");
    //breakpoint();

    TestMsg *const msg = new(Scheduler::GetDisabledMsgMgr()) TestMsg;
    tassert(msg != NULL, err_printf("message allocation failed.\n"));
    msg->d1 = 13;
    msg->d2 = 17;
    rc = msg->send(SysTypes::DSPID(0, vp));
    cprintf("vptest:  send disabled async() returned 0x%lx.\n", rc);

    pingpong(&datum,0);

    delete GlobalBarrier;

    for (uval i=0;i<6;i++) {
	cprintf("CY");
	Scheduler::Yield();
    }
}

// --------------------------------------------------------------------------
/*
 * use regions create/destroy to unmap
 * time whole loop
 */
void
pfTestWorker1(TestStructure *ts)
{
    SysStatus rc;
    uval vaddr, hackvaddr;
    const uval size = ts->size;
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval i, iters;
    uval iStart, iEnd;
    SysTime start, end;
    VPNum vp;
    ObjectHandle frOH;

    vp = Scheduler::GetVP();

    cprintf("pfTestWorker1 %ld started\n", uval(vp));

    ts->bar->enter();

    rc = StubFRComputation::_Create(frOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	hackvaddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);
    passert(_SUCCESS(rc), err_printf("woops2\n"));

    // initialize memory
    for (i = 0; i < size; i += PAGE_SIZE) {
	// for debugging store decorated vp
	*(volatile uval *)(hackvaddr+i) = 0xf00f001000000000+vp;
    }

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::START_MLS_STATS,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
#ifdef DEBUG_TEST
	cprintf("iter %ld\n", iters);
#endif /* #ifdef DEBUG_TEST */

	rc = StubRegionDefault::_CreateFixedLenExt(
	    vaddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	    RegionType::K42Region);
	passert(_SUCCESS(rc), err_printf("woops3\n"));
#ifdef DEBUG_TEST
	cprintf("Bound region at %lx\n", vaddr);
#endif /* #ifdef DEBUG_TEST */

	for (i = 0; i < size; i += PAGE_SIZE) {
	    volatile uval *p = (volatile uval *)(vaddr+i);
	    //cprintf("Accessing %lx...\n", p);
	    *p = 0xf00f002000000000+vp;
	    //cprintf("...Accessed %lx\n", p);
	}

#ifdef DEBUG_TEST
	cprintf("destroying region %llx\n", regionref);
#endif /* #ifdef DEBUG_TEST */
	DREFGOBJ(TheProcessRef)->regionDestroy(vaddr);
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::DUMP_ZERO_MLS_STATS,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("destroying region on %ld\n", uval(Scheduler::GetVP()));
    DREFGOBJ(TheProcessRef)->regionDestroy(hackvaddr);
    Obj::ReleaseAccess(frOH);
    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

/*
 * use regions create/destroy to unmap
 * time whole loop
 * barrier before page touch part of loop
 * barrier delay part of timing
 */
void
pfTestWorker2(TestStructure *ts)
{
    SysStatus rc;
    uval vaddr, hackvaddr;
    const uval size = ts->size;
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval i, iters;
    uval iStart, iEnd;
    SysTime start, end;
    VPNum vp;
    ObjectHandle frOH;

    vp = Scheduler::GetVP();

    cprintf("pfTestWorker2 %ld started\n", uval(vp));

    ts->bar->enter();

    rc = StubFRComputation::_Create(frOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	hackvaddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);
    passert(_SUCCESS(rc), err_printf("woops2\n"));
    // initialize memory
    for (i = 0; i < size; i += PAGE_SIZE) {
	// for debugging store decorated vp
	*(volatile uval *)(hackvaddr+i) = 0xf00f001000000000+vp;
    }

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::START_MLS_STATS,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
#ifdef DEBUG_TEST
	cprintf("iter %ld\n", iters);
#endif /* #ifdef DEBUG_TEST */
	rc = StubRegionDefault::_CreateFixedLenExt(
	    vaddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	    RegionType::K42Region);
	passert(_SUCCESS(rc), err_printf("woops3\n"));
#ifdef DEBUG_TEST
	cprintf("Bound region at %lx\n", vaddr);
#endif /* #ifdef DEBUG_TEST */

	ts->bar->enter();

	for (i = 0; i < size; i += PAGE_SIZE) {
	    volatile uval *p = (volatile uval *)(vaddr+i);
	    //cprintf("Accessing %lx...\n", p);
	    *p = 0xf00f002000000000+vp;
	    //cprintf("...Accessed %lx\n", p);
	}

#ifdef DEBUG_TEST
	cprintf("destroying region\n");
#endif /* #ifdef DEBUG_TEST */
	DREFGOBJ(TheProcessRef)->regionDestroy(vaddr);
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();


    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::DUMP_ZERO_MLS_STATS,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("destroying region on %ld\n", uval(Scheduler::GetVP()));
    DREFGOBJ(TheProcessRef)->regionDestroy(hackvaddr);
    Obj::ReleaseAccess(frOH);
    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

/*
 * use regions create/destroy to unmap
 * time page touches only
 */

void
pfTestWorker3(TestStructure *ts)
{
    SysStatus rc;
    uval vaddr, hackvaddr;
    const uval size = ts->size;
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval i, iters;
    uval fltStart, fltStartIn, fltTotTime=0, fltTotInst=0;
    VPNum vp;
    ObjectHandle frOH;

    vp = Scheduler::GetVP();

    cprintf("pfTestWorker3 %ld started\n", uval(vp));

    ts->bar->enter();


    rc = StubFRComputation::_Create(frOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	hackvaddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);
    passert(_SUCCESS(rc), err_printf("woops2\n"));

    // initialize memory
    for (i = 0; i < size; i += PAGE_SIZE) {
	// for debugging store decorated vp
	*(volatile uval *)(hackvaddr+i) = 0xf00f001000000000+vp;
    }

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::START_MLS_STATS,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::COLLECT_PERF_MON,0);

    for (iters = 0; iters < numIters; iters++) {
#ifdef DEBUG_TEST
	cprintf("iter %ld\n", iters);
#endif /* #ifdef DEBUG_TEST */
	rc = StubRegionDefault::_CreateFixedLenExt(
	    vaddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	    RegionType::K42Region);
	passert(_SUCCESS(rc), err_printf("woops3\n"));
#ifdef DEBUG_TEST
	cprintf("Bound region at %lx\n", vaddr);
#endif /* #ifdef DEBUG_TEST */

	ts->bar->enter();

	DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::START_PERF_MON,0);

	if (triggerit) tstEvent_startworker();
	fltStart = Scheduler::SysTimeNow();
	fltStartIn = getInstrCount();

	for (i = 0; i < size; i += PAGE_SIZE) {
	    volatile uval *p = (volatile uval *)(vaddr+i);
	    //cprintf("Accessing %lx...\n", p);
	    *p = 0xf00f002000000000+vp;
	    //cprintf("...Accessed %lx\n", p);
	}
	fltTotInst += getInstrCount() - fltStartIn;
	fltTotTime += Scheduler::SysTimeNow() - fltStart;
	if (triggerit) tstEvent_endworker();

	DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::STOP_PERF_MON,0);

#ifdef DEBUG_TEST
	cprintf("destroying region\n");
#endif /* #ifdef DEBUG_TEST */
	DREFGOBJ(TheProcessRef)->regionDestroy(vaddr);
    }

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::PRINT_PERF_MON,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::DUMP_ZERO_MLS_STATS,0);

    ts->bar->enter();

    ts->time1 = fltTotTime;
    ts->instr1 = fltTotInst;

    cprintf("destroying region on %ld\n", uval(Scheduler::GetVP()));
    DREFGOBJ(TheProcessRef)->regionDestroy(hackvaddr);
    Obj::ReleaseAccess(frOH);
    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

 /*
 * use unmapRange to unmap pages
 * time whole loop
 */
void
pfTestWorker4(TestStructure *ts)
{
    SysStatus rc;
    uval vaddr;
    const uval size = ts->size;
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval start, end, iStart, iEnd;
    uval i, iters;
    VPNum vp;
    ObjectHandle frOH;

    vp = Scheduler::GetVP();

    cprintf("pfTestWorker4 %ld started\n", uval(vp));

    ts->bar->enter();


    rc = StubFRComputation::_Create(frOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	vaddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);

    passert(_SUCCESS(rc), err_printf("woops2\n"));

    // initialize memory
    for (i = 0; i < size; i += PAGE_SIZE) {
	// for debugging store decorated vp
	*(volatile uval *)(vaddr+i) = 0xf00f001000000000+vp;
    }

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::START_MLS_STATS,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
#ifdef DEBUG_TEST
	cprintf("iter %ld\n", iters);
#endif /* #ifdef DEBUG_TEST */
	DREFGOBJ(TheProcessRef)->unmapRange(vaddr, size);

	for (i = 0; i < size; i += PAGE_SIZE) {
	    volatile uval *p = (volatile uval *)(vaddr+i);
	    //cprintf("Accessing %lx...\n", p);
	    *p = 0xf00f002000000000+vp;
	    //cprintf("...Accessed %lx\n", p);
	}
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::DUMP_ZERO_MLS_STATS,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("destroying region on %ld\n", uval(Scheduler::GetVP()));
    DREFGOBJ(TheProcessRef)->regionDestroy(vaddr);
    Obj::ReleaseAccess(frOH);
    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

void
pfTest1()
{
    uval i;
    const uval numIters = Stats.numbIters[stats::PAGE_FAULT1];
    const uval size = Stats.numbReqPerIter[stats::PAGE_FAULT1]*PAGE_SIZE;
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, size, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(pfTestWorker1), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::PAGE_FAULT1, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld, perPage %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters),
		uval(ts[i].time1/numIters/
		     Stats.numbReqPerIter[stats::PAGE_FAULT1]));

    }

    delete[] ts;
}

void
pfTest2()
{
    uval i;
    const uval numIters = Stats.numbIters[stats::PAGE_FAULT2];
    const uval size = Stats.numbReqPerIter[stats::PAGE_FAULT2]*PAGE_SIZE;
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, size, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(pfTestWorker2), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::PAGE_FAULT2, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld, perPage %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters),
		uval(ts[i].time1/numIters/
		     Stats.numbReqPerIter[stats::PAGE_FAULT2]));

    }

    delete[] ts;
}

void
pfTest3()
{
    uval i;
    const uval numIters = Stats.numbIters[stats::PAGE_FAULT3];
    const uval size = Stats.numbReqPerIter[stats::PAGE_FAULT3]*PAGE_SIZE;
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, size, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(pfTestWorker3), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::PAGE_FAULT3, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld, perPage %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters),
		uval(ts[i].time1/numIters/
		     Stats.numbReqPerIter[stats::PAGE_FAULT3]));

    }

    delete[] ts;
}

void
pfTest4()
{
    uval i;
    const uval numIters = Stats.numbIters[stats::PAGE_FAULT4];
    const uval size = Stats.numbReqPerIter[stats::PAGE_FAULT4]*PAGE_SIZE;
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, size, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(pfTestWorker4), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::PAGE_FAULT4, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld, perPage %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters),
		uval(ts[i].time1/numIters/
		     Stats.numbReqPerIter[stats::PAGE_FAULT4]));

    }

    delete[] ts;
}

// --------------------------------------------------------------------------

void
ipcTestWorker(TestStructure *ts)
{
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval iters;
    uval iStart, iEnd;
    SysTime start, end;

    cprintf("ipcTestWorker %ld started\n", uval(Scheduler::GetVP()));

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
	DREFGOBJ(TheProcessRef)->testIPCPerf();
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

void
ipcTest()
{
    uval i;
    const uval numIters = Stats.numbIters[stats::IPC];
    const uval triggerit = 1;

    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, 0/*size*/, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(ipcTestWorker), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::IPC, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters));
    }

    delete[] ts;
}

// --------------------------------------------------------------------------

SampleServiceRef sampleServiceRef = NULL;

void
userIPCTestWorker(TestStructure *ts)
{
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval iters;
    uval iStart, iEnd;
    SysTime start, end;

    cprintf("userIPCTestWorker %ld started\n", uval(Scheduler::GetVP()));

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
	DREF(sampleServiceRef)->testRequest();
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

void
userIPCTest()
{
    SysStatus rc;

    uval i;
    const uval numIters = Stats.numbIters[stats::USER_IPC];
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, 0/*size*/, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    while (sampleServiceRef == NULL) {
	rc = SampleServiceWrapper::Create(sampleServiceRef);
	cprintf("created sampleServiceRef %p, rc 0x%lx\n",
		sampleServiceRef, rc);
    }

    DoConcTest(NumVP, SimpleThread::function(userIPCTestWorker), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::USER_IPC, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters));
    }

    delete[] ts;
}

void
userIPCTestWorker2(TestStructure *ts)
{
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval iters;
    uval iStart, iEnd;
    SysTime start, end;

    cprintf("userIPCTestWorker2 %ld started\n", uval(Scheduler::GetVP()));

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
	DREF(sampleServiceRef)->testRequestWithIncrement();
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

void
userIPCTest2()
{
    SysStatus rc;

    uval i;
    const uval numIters = Stats.numbIters[stats::USER_IPC2];
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, 0/*size*/, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    while (sampleServiceRef == NULL) {
	rc = SampleServiceWrapper::Create(sampleServiceRef);
	cprintf("created sampleServiceRef %p, rc 0x%lx\n",
		sampleServiceRef, rc);
    }

    DoConcTest(NumVP, SimpleThread::function(userIPCTestWorker2), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::USER_IPC2, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters));
    }

    delete[] ts;
}

void
userIPCTestWorker3(TestStructure *ts)
{
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval iters;
    uval iStart, iEnd;
    SysTime start, end;

    cprintf("userIPCTestWorker3 %ld started\n", uval(Scheduler::GetVP()));

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
	DREF(sampleServiceRef)->testRequestWithLock();
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

void
userIPCTest3()
{
    SysStatus rc;

    uval i;
    const uval numIters = Stats.numbIters[stats::USER_IPC3];
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, 0/*size*/, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    while (sampleServiceRef == NULL) {
	rc = SampleServiceWrapper::Create(sampleServiceRef);
	cprintf("created sampleServiceRef %p, rc 0x%lx\n",
		sampleServiceRef, rc);
    }

    DoConcTest(NumVP, SimpleThread::function(userIPCTestWorker3), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::USER_IPC3, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters));
    }

    delete[] ts;
}

void
userIPCTestWorker4(TestStructure *ts)
{
    const uval numIters = ts->iters;
    const uval triggerit = ts->misc;
    uval iters;
    uval iStart, iEnd;
    SysTime start, end;

    cprintf("userIPCTestWorker4 %ld started\n", uval(Scheduler::GetVP()));

    ts->bar->enter();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);

    if (triggerit) tstEvent_startworker();
    start = Scheduler::SysTimeNow();
    iStart = getInstrCount();

    for (iters = 0; iters < numIters; iters++) {
	DREF(sampleServiceRef)->testRequestWithFalseSharing();
    }

    end = Scheduler::SysTimeNow();
    iEnd = getInstrCount();

    if (triggerit) tstEvent_endworker();

    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);

    ts->bar->enter();

    ts->time1 = end - start;
    ts->instr1 = iEnd - iStart;

    cprintf("All done on %ld\n", uval(Scheduler::GetVP()));
}

void
userIPCTest4()
{
    SysStatus rc;

    uval i;
    const uval numIters = Stats.numbIters[stats::USER_IPC4];
    const uval triggerit = 1;
    BlockBarrier bar(NumVP);
    TestStructure *ts = TestStructure::Create(
	NumVP, 0/*size*/, numIters,
	0/*test*/, triggerit, 0/*ptr*/, &bar);

    while (sampleServiceRef == NULL) {
	rc = SampleServiceWrapper::Create(sampleServiceRef);
	cprintf("created sampleServiceRef %p, rc 0x%lx\n",
		sampleServiceRef, rc);
    }

    DoConcTest(NumVP, SimpleThread::function(userIPCTestWorker4), ts);

    for (i=0; i<NumVP; i++) {
	Stats.enterResult(stats::USER_IPC4, i, ts[i].time1, ts[i].instr1);
	cprintf("%ld: %ld perIter %ld\n", i, uval(ts[i].time1),
		uval(ts[i].time1/numIters));
    }

    delete[] ts;
}

// --------------------------------------------------------------------------

void
pageoutTestWorker(TestStructure *ts)
{
    const uval size = ts->size;
    const uval iters = ts->iters;
    const char *p = (char *)ts->ptr;
    uval numChunks = ts->misc;
    uval i, j;
    VPNum myvp = Scheduler::GetVP();

    BaseRandom random;

    cprintf("%ld: pageoutTestWorker started\n", uval(myvp));

    ts->bar->enter();

    j = random.getVal() % numChunks;
    for (i = 0; i < iters; i++) {
	if ((i % 100) == 0) {
	    err_printf("vp %ld, iter %ld\n", myvp, i);
	}
	if ((i % 4) == 0) {
	    // periodically just do a read of some random page
	    *(volatile char *)(p + size*j);
	    j = random.getVal() % numChunks;
	}
	FetchAndAddVolatile((volatile uval *) (p + size*j), 1);
	Scheduler::DelayMicrosecs(100000);
	if (numChunks > 10) {
	    numChunks--;
	}
	j = random.getVal() % numChunks;
    }

    ts->bar->enter();
}

void
pageoutTest()
{
    SysStatus rc;
    FileLinuxRef flr;
    const char filename[] = "/pageoutfile";
    const uval numChunks = 200 /*1000*/ /*NumVP*/;
    const uval chunkSize = 4096;
    const uval numIters = numChunks * 2 /* 2000 */;
    BlockBarrier bar(NumVP);
    char *p;
    uval i, total;

    // hack to get filelength write (right?)when doing random paging

    rc = FileLinux::Create(flr, filename,
			   O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed\n", filename);
	return;
    }
    for (i = 0; i < numChunks; i++) {
	while (1) {
	    ThreadWait *tw = NULL;
	    rc = DREF(flr)->writeAlloc(chunkSize, p, &tw);
	    if (_FAILURE(rc) && tw) {
		while (!tw->unBlocked()) {
		    Scheduler::Block();
		}
		tw->destroy();
		delete tw;
		tw = NULL;
	    } else {
		break;
	    }
	}

	tassert(_SUCCESS(rc), err_printf("woops\n"));
	memset(p,0,chunkSize);
	rc = DREF(flr)->writeFree(p);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
    }

    rc = DREF(flr)->destroy();
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    // end of hack

    rc = FileLinux::Create(flr, filename, O_RDWR, 0644);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed\n", filename);
	return;
    }

    while (1) {
	ThreadWait *tw = NULL;
	rc = DREF(flr)->writeAlloc(chunkSize*numChunks, p, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    tassert(_SUCCESS(rc), err_printf("woops\n"));

    memset(p,0,chunkSize*numChunks);	// bring all into memory for now

    TestStructure *ts = TestStructure::Create(
	NumVP, chunkSize, numIters/*iters*/,
	0/*test*/, numChunks/*misc*/, p/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(pageoutTestWorker), ts);

    delete[] ts;

    total = 0;
    for (i = 0; i < numChunks; i++) {
	total += *(uval *)(p + i*chunkSize);
    }

    rc = DREF(flr)->writeFree(p);
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    rc = DREF(flr)->destroy();
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    passert(total == NumVP*numIters,
	    err_printf("Bad total: %ld != %ld\n",total,NumVP*numIters));
}


void
pageoutTestFRComputation()
{
    SysStatus rc;
    const uval numChunks = 4*1024 /*NumVP*/;
    const uval chunkSize = 4096;
    const uval numIters = /*numChunks*/2000;
    BlockBarrier bar(NumVP);
    char *p;
    uval vaddr;
    uval i, total;
    SysStatusProcessID pidrc;
    ProcessID myPID;
    ObjectHandle frOH;

    cprintf("pageoutTestFRComputation: numvp %ld, numChunks %ld, chunkSize %ld"
	    " iters %ld\n", NumVP, numChunks, chunkSize, numIters);

    pidrc = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(pidrc), err_printf("woops\n"));
    myPID = _SGETPID(pidrc);

    rc = StubFRComputation::_Create(frOH);

    //The following invokes the create method but does NOT
    //create a stub object.
    rc = StubRegionDefault::_CreateFixedLenExt(
	vaddr, chunkSize*numChunks, PAGE_SIZE, frOH, 0,
	(uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);

    tassert(_SUCCESS(rc), err_printf("woops\n"));
    p = (char *)vaddr;

    cprintf("Initializing memory\n");
    memset(p,0,chunkSize*numChunks);	// bring all into memory for now

    cprintf("Starting mp test\n");
    TestStructure *ts = TestStructure::Create(
	NumVP, chunkSize, numIters/*iters*/,
	0/*test*/, numChunks/*misc*/, p/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(pageoutTestWorker), ts);

    delete[] ts;

    total = 0;
    for (i = 0; i < numChunks; i++) {
	total += *(uval *)(p + i*chunkSize);
    }

    passert(total == NumVP*numIters,
	    err_printf("Bad total: %ld != %ld\n",total,NumVP*numIters));

    // destroy the data region from my address space
    DREFGOBJ(TheProcessRef)->regionDestroy(vaddr);

    // we must delete our own access so FR can eventually be destroyed
    Obj::ReleaseAccess(frOH);

    cprintf("Test completed successfully\n");

}

// --------------------------------------------------------------------------

static uval
SharedFCMIndexToOffset(uval id, uval chunkSize)
{
    return ((id/chunkSize)*SEGMENT_SIZE) + ((id%chunkSize)*PAGE_SIZE);
}

void
testSharedFCMWorker(TestStructure *ts)
{
    const uval chunkSize = ts->size;
    const uval iters = ts->iters;
    const char *p = (char *)ts->ptr;
    volatile uval *sp;
    uval numChunks = ts->misc;
    uval i, j;
    VPNum myvp = Scheduler::GetVP();
    uval32 randomState;

#if 0
    const uval minNumChunks = numChunks/10 > 10 ? numChunks / 10 : 10;
#endif /* #if 0 */

    cprintf("%ld: testSharedFCMWorker started: p %p, cs %lx, numc %lx\n",
	    uval(myvp), p, chunkSize, numChunks);

    randomState = myvp;			// val unique to thread but repeatable

    ts->bar->enter();

    j = 0;
    for (i = 0; i < iters; i++) {
	if ((i % 10) == 0) {
	    cprintf("vp %ld, iter %ld\n", myvp, i);
	}

#if PERIODIC_PAGE_READ
	if ((i % 4) == 0) {
	    // periodically just do a read of some random page
#if RANDOM_CHUNKS
	    j = BaseRandom::GetLC(&randomState) % numChunks;
#else /* #if RANDOM_CHUNKS */
	    j = (j + 1) % numChunks;
#endif /* #if RANDOM_CHUNKS */
	    sp = (volatile uval *)(p + SharedFCMIndexToOffset(j, chunkSize));
	    *sp;
	    //Scheduler::DelayMicrosecs(100000);
	    if (numChunks > minNumChunks) {
		numChunks--;
	    }
	}
#else /* #if PERIODIC_PAGE_READ */
	j = 0;
#endif /* #if PERIODIC_PAGE_READ */

#if RANDOM_CHUNKS
	j = BaseRandom::GetLC(&randomState) % numChunks;
#else /* #if RANDOM_CHUNKS */
	j = (j + 1) % numChunks;
#endif /* #if RANDOM_CHUNKS */

	sp = (volatile uval *)(p + SharedFCMIndexToOffset(j, chunkSize));
//	cprintf("Accessing %p, current val %ld\n", sp, *sp);
	FetchAndAddVolatile(sp, 1);

    }

    ts->bar->enter();

    cprintf("%ld: testSharedFCMWorker all done\n", myvp);
}

void
testSharedFCM(uval deleteFRWhenDone)
{
    SysStatusProcessID pidrc;
    SysStatus rc;
    ProcessID myPID;
    char *p;
    uval i, total;
    BlockBarrier bar(NumVP);

    /*const*/ uval numRegions = /*4*/2;
    /*const*/ uval regionSize = 1/*2*/*SEGMENT_SIZE;
    /*const*/ uval totalSize = regionSize * numRegions;
    /*const*/ uval chunkSize = 512;
    /*const*/ uval numChunks = chunkSize * numRegions *
		  (regionSize / SEGMENT_SIZE);
    /*const*/ uval numIters = 2*numChunks;

    uval regions[numRegions];

    static ObjectHandle frOH;
    static uval frOHinited = 0;

    cprintf("Doing shared FCM test:\ndelete %c, size %lx, numr %ld, rsize %lx"
	    " chunksize %lx numchunks %ld, iters %ld\n",
	    deleteFRWhenDone ? 'y' : 'n',
	    totalSize, numRegions, regionSize, chunkSize, numChunks, numIters);

    pidrc = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(pidrc), err_printf("woops\n"));
    myPID = _SGETPID(pidrc);

    if (!frOHinited) {
	rc = StubFRComputation::_Create(frOH);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
	frOHinited = 1;
    }

    /* we create what seems like one large region, but actually consists of
     * multiple regions back-to-back.  Additionally, each region maps the
     * same FR.
     */
    for (i = 0; i < numRegions; i++) {
	rc = StubRegionDefault::_CreateFixedLenExt(
	    regions[i], regionSize, SEGMENT_SIZE,
	    frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	    RegionType::K42Region);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
	// we want all the regions in a row
	tassert((i == 0) || (regions[i] == regions[i-1]+regionSize),
		err_printf("not addr wanted %lx != %lx\n",
			   regions[i], regions[i-1]+regionSize));
    }
    p = (char *)regions[0];

    // clear words that will be touched; only need to do so for one region
    for (i = 0; i < numChunks/numRegions; i++) {
	uval *sp = (uval *)(p + SharedFCMIndexToOffset(i, chunkSize));
	//cprintf("clearing %p\n", sp);
	*(uval *)(sp) = 0;
    }

    TestStructure *ts = TestStructure::Create(
	NumVP, chunkSize/*size*/, numIters/*iters*/,
	0/*test*/, numChunks/*misc*/, p/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(testSharedFCMWorker), ts);

    delete[] ts;

    err_printf("Checking results\n");

    total = 0;
    for (i = 0; i < numChunks/numRegions; i++) {
	uval *sp = (uval *)(p + SharedFCMIndexToOffset(i, chunkSize));
	total += *(uval *)(sp);
    }

    passert(total == NumVP*numIters,
	    err_printf("Bad total: %ld != %ld\n",total,NumVP*numIters));

    for (i = 0; i < numRegions; i++) {
	// destroy the data region from my address space
	DREFGOBJ(TheProcessRef)->regionDestroy(regions[i]);
    }

    if (deleteFRWhenDone) {
	cprintf("Deleting FR\n");
	// we must delete our own access so FR can eventually be destroyed
	Obj::ReleaseAccess(frOH);
	frOHinited = 0;
    }

    cprintf("All done shared FCM test\n");
}

// --------------------------------------------------------------------------

void
testPartitionedFCMWorker(TestStructure *ts)
{
    VPNum myvp = Scheduler::GetVP();
    const uval Size = ts->size;
    // Rounding down to an exact number of pages
    const uval numPages = (Size/PAGE_SIZE)/ts->workers;
    const unsigned char *p = (unsigned char *)ts->ptr + (myvp * PAGE_SIZE * numPages);
    volatile unsigned char *cp;

    cprintf("%ld: testPartitionedFCMWorker started: p %p, numPages=%ld\n",
	    myvp, p, numPages);

    ts->bar->enter();

    cprintf("%ld Reading page: ",myvp);

    for (uval pagenum=0; pagenum < numPages; pagenum++) {
	cprintf("%ld ",pagenum + (myvp * numPages));
	// read  page and ensure all values are 0
	for (cp = (unsigned char *)((uval)p + (pagenum * PAGE_SIZE));
	     ((uval)cp) < ((uval)p + (pagenum + 1) * PAGE_SIZE);
	     cp++) {
	    tassert(*cp == 0, err_printf("%ld: oops expected 0 found %d",
					 myvp, *cp));
	}
    }

    cprintf("\n%ld: All Pages Read\n",myvp);

    ts->bar->enter();

    cprintf("%ld: testPartitionedFCMWorker all done\n", myvp);
}

void
testPartitionedFCM(uval deleteRegionWhenDone, uval partitioned)
{
    SysStatusProcessID pidrc;
    SysStatus rc;
    ProcessID myPID;
    char *p;
    BlockBarrier bar(NumVP);
    uval replicatedRegion;

    /*const*/ uval regionSize = 512*NumVP*PAGE_SIZE;

    cprintf("Doing replicated FCM test:\n");

    pidrc = DREFGOBJ(TheProcessRef)->getPID();
    tassert(_SUCCESS(pidrc), err_printf("woops\n"));
    myPID = _SGETPID(pidrc);

    rc = StubRegionDefault::_CreateFixedLenExtKludge(
	replicatedRegion, regionSize, SEGMENT_SIZE,
        0, (uval)(AccessMode::writeUserWriteSup), 0, partitioned,
	RegionType::K42Region);
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    p = (char *)replicatedRegion;

    cprintf("Region start address is: %p\n",p);
    TestStructure *ts = TestStructure::Create(
	NumVP, regionSize/*size*/, 0/*iters*/,
	0/*test*/, 0/*misc*/, p/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(testPartitionedFCMWorker), ts);

    delete[] ts;


    if (deleteRegionWhenDone) {
	// destroy the data region from my address space
	DREFGOBJ(TheProcessRef)->regionDestroy(replicatedRegion);
    }

    cprintf("All done Partitioned FCM test\n");
}


// --------------------------------------------------------------------------
void
writeTestWorker(TestStructure *ts)
{
    SysStatus rc;
    const uval size = ts->size;
    const FileLinuxRef flr = (FileLinuxRef)ts->misc;
    char *p;
    VPNum myvp = Scheduler::GetVP();

    cprintf("%ld: writeTestWorker started\n", uval(myvp));

    ts->bar->enter();

    cprintf("%ld: writeAlloc\n", uval(myvp));

    while (1) {
	ThreadWait *tw = NULL;
	rc = DREF(flr)->writeAlloc(size, p, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    tassert(_SUCCESS(rc), err_printf("woops\n"));
    cprintf("%ld: memset\n", uval(myvp));
    memset(p,'A'+myvp,size);
    cprintf("%ld: writeFree\n", uval(myvp));
    rc = DREF(flr)->writeFree(p);
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    ts->bar->enter();
}

void
writeTest()
{
    SysStatus rc;
    FileLinuxRef flr;
    const char filename[] = "/afile";
    const uval chunkSize = 4096;
    BlockBarrier bar(NumVP);
    uval wasRunning;

    rc = StubWire::RestartDaemon();
    passert(_SUCCESS(rc), err_printf("Open failed: %lx\n", rc));
    wasRunning = _SGETUVAL(rc);

    cprintf("driver: opening %s\n", filename);

    rc = FileLinux::Create(flr, filename,
			   O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (_FAILURE(rc)) {
	err_printf("open of file %s failed\n", filename);
	return;
    }
    passert(_SUCCESS(rc), err_printf("Open failed: %lx\n", rc));

    TestStructure *ts = TestStructure::Create(
	NumVP, chunkSize, 0 /*iters*/,
	0/*test*/, uval(flr)/*misc*/, 0/*ptr*/, &bar);

    DoConcTest(NumVP, SimpleThread::function(writeTestWorker), ts);

    delete[] ts;

    cprintf("driver: close\n");
    rc = DREF(flr)->destroy();
    tassert(_SUCCESS(rc), err_printf("woops\n"));

    if (!wasRunning) StubWire::SuspendDaemon();

    cprintf("All done with write test\n");
}

/*------------------------------------------------------------------------*/

void
socketTest()
{
    SysStatus rc;
    FileLinuxRef tref, trefcomm;
    uval port=4567;
    err_printf("connect to port %ld\n", port );

    StubWire::RestartDaemon();

    rc = FileLinuxSocket::Create(tref, AF_INET, SOCK_STREAM, 0);
    tassert((_SUCCESS(rc)),
	    err_printf("socket() failed for port %ld\n", port));

    SocketAddrIn addrIn((uval)0, 6000);

    rc = DREF(tref)->bind((char*)&addrIn, sizeof(addrIn));
    tassert((_SUCCESS(rc)), err_printf("bind() failed\n"));

    rc = DREF(tref)->listen(4);
    tassert((_SUCCESS(rc)), err_printf("listen() failed\n"));

    while (1) {
	ThreadWait *tw = NULL;
	rc = DREF(tref)->accept(trefcomm, &tw);
	if (_FAILURE(rc) && tw) {
	    while (!tw->unBlocked()) {
		Scheduler::Block();
	    }
	    tw->destroy();
	    delete tw;
	    tw = NULL;
	} else {
	    break;
	}
    }

    tassert((_SUCCESS(rc)), err_printf("accept() failed\n"));

    while (1) {
	char buf[256];
	uval len;
	SysStatus rc;
	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(trefcomm)->write("\ntype: ", strlen("type: ")+1,
				       &tw, moreAvail);
	    if (_FAILURE(rc) && tw) {
		while (!tw->unBlocked()) {
		    Scheduler::Block();
		}
		tw->destroy();
		delete tw;
		tw = NULL;
	    } else {
		break;
	    }
	}

	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(trefcomm)->read(buf, 256, &tw, moreAvail);
	    if (_FAILURE(rc) && tw) {
		while (!tw->unBlocked()) {
		    Scheduler::Block();
		}
		tw->destroy();
		delete tw;
		tw = NULL;
	    } else {
		break;
	    }
	}

	tassert( _SUCCESS(rc), err_printf("woops\n"));
	len = _SGETUVAL(rc);
	while (1) {
	    GenState moreAvail;
	    ThreadWait *tw = NULL;
	    rc = DREF(trefcomm)->write(buf, len, &tw, moreAvail);
	    if (_FAILURE(rc) && tw) {
		while (!tw->unBlocked()) {
		    Scheduler::Block();
		}
		tw->destroy();
		delete tw;
		tw = NULL;
	    } else {
		break;
	    }
	}

    }
}


struct TestBitPtr : BitStructure {
    friend class LTestBitPtr;
    __BIT_FIELD(64, all, BIT_FIELD_START);
    __BIT_FIELD(8, count, BIT_FIELD_START);
    __BIT_FIELD(56, pntr, count);
    __LOCK_BIT(pntr, 1);
    __WAIT_BIT(pntr, 0);
};

struct LTestBitPtr : BitBLock<TestBitPtr> {
    void acquire();
    void release();
};
void LTestBitPtr::acquire()
{
    TestBitPtr tmp;
    BitBLock<TestBitPtr>::acquire(tmp);
}

void LTestBitPtr::release()
{
    TestBitPtr tmp;
    tmp.pntr(0xdeadbeef);
    tmp.count(0xdeadbeef);
    BitBLock<TestBitPtr>::release(tmp);
}

//LTestBitPtr    lockTestLock;
BLock          lockTestLock;
//UnFairBLock    lockTestLock;
//SLock          lockTestLock;

uval sharedVal;

void
lockTest()
{
    uval i;
    const uval numIters = 10000;
    SimpleThread *otherThreads[NumVP];
    uval vp;

    sharedVal = 0;

    if (Scheduler::GetVP() == 0) {
	MakeMP(NumVP);
	GlobalBarrier = new BlockBarrier(NumVP);
	for (i = 1; i < NumVP; i++) {
	    otherThreads[i] =
		SimpleThread::Create(SimpleThread::function(lockTest), 0,
				     SysTypes::DSPID(0, i));
	    passert(otherThreads[i]!=0, err_printf("Thread create failed\n"));
	}
    }

    // set here because we don't want to do it inside of timing
    vp = Scheduler::GetVP();

    GlobalBarrier->enter();

    for (i = 0; i < numIters; i++) {
#if 0
	DREFGOBJ(TheBlockedThreadQueuesRef)->addCurThreadToQueue(0);
	for (uval j=0; j < 10000; j++) {}
	DREFGOBJ(TheBlockedThreadQueuesRef)->removeCurThreadFromQueue(0);
#else /* #if 0 */
	lockTestLock.acquire();
	//while (!lockTestLock.tryAcquire()) { Scheduler::Yield(); }
	uval tmp = sharedVal;

	if (i % (numIters/10) == 0) {
	    cprintf("\t%ld: iter %ld\n", Scheduler::GetVP(), i);
	}
	sharedVal = tmp + 1;
	lockTestLock.release();
	if (i % (numIters/10) == 0) {
	    cprintf("%ld: iter %ld\n", Scheduler::GetVP(), i);
	}
#endif /* #if 0 */
    }

    GlobalBarrier->enter();

    if (sharedVal != numIters*NumVP) {
	cprintf("Ooops, wrong value at end %ld\n", sharedVal);
	breakpoint();
    }

    if (Scheduler::GetVP() == 0) {
	for (i = 1; i < NumVP; i++) {
	    (void)SimpleThread::Join(otherThreads[i]);
	}
	delete GlobalBarrier;
    }

}

/* ------------------------------------------------------------------------ */

void
AllocTest()
{
    extern void ConcTestAlloc(uval pool, uval numTests);
    // pre-create the vps needed, and then just call allocator test

    MakeMP(DREFGOBJ(TheProcessRef)->ppCount());

    ConcTestAlloc(AllocPool::DEFAULT, 10000);
}

/* --------------------------------------------------------------------------
 * Derived from Tornado tstglock.C
 * FIXME:  Convert to using stub and XObjects objects
 */
class TestObj;
typedef TestObj **TestObjRef;

class TestObj : public BaseObj {
    TestObjRef ref() { return (TestObjRef)getRef(); }
    TestObj() { /* empty body */ }
public:
    DEFINE_GLOBALPADDED_NEW(TestObj);
    static TestObjRef Create() {
        TestObjRef retvalue;
	retvalue = (TestObjRef)
	    (CObjRootSingleRep::Create(new TestObj));
	return (retvalue);
    }
    virtual SysStatus  func1(uval *data, uval it);
    virtual SysStatus  func2(uval *data, uval it);
    virtual SysStatus  func3(uval *data, uval it);
};


SysStatus
TestObj :: func1(uval *data, uval it)
{
    passert( *data == it, err_printf("*data!=it\n") );
    Scheduler::Yield();
    passert( *data == it, err_printf("*data %lx, it %lx\n",
				    *data, it) );
    return 0;
}

SysStatus
TestObj :: func2(uval *data, uval it)
{
    passert( *data == it, err_printf("*data!=it\n") );
    Scheduler::Yield();
    passert( *data == it, err_printf("*data %lx, it %lx\n",
				    *data, it) );
    return 0;
}

SysStatus
TestObj :: func3(uval *data, uval it)
{
    passert( *data == it, err_printf("*data!=it\n") );
    Scheduler::Yield();
    passert( *data == it, err_printf("*data %lx, it %lx\n",
				    *data, it) );
    return 0;
}

struct Entry {
    Entry()	     { allocated = 0; deleted = 0; inuse = 0; }
    void reinit()    { allocated = 0; deleted = 0; inuse = 0; testobj = 0; }
    TestObjRef   testobj;
    BLock     lock;
    sval            allocated;
    sval            deleted;
    sval            inuse;
    DEFINE_GLOBAL_NEW(Entry);
};

struct ThreadArgs {
    Entry *objects;
    uval  iterations;
    uval  doneFlag;
    uval  allocator_id;
    uval  deallocator_id;
    uval  accessor_id;
    SimpleThread *allocatorThread;
    SimpleThread *deallocatorThread;
    SimpleThread *accessorThread;
    DEFINE_GLOBAL_NEW(ThreadArgs);
};

#define NUM_OBJECTS	100

BlockBarrier *tstglockbarrier;
sval    *tstglockDone;

// need an allocator function that chooses a random location in the array,
// using linear probing if entry is full (must sync around allocate)
SysStatus
allocator(ThreadArgs *args)
{
    Entry *objects=args->objects;
    uval iterations=args->iterations;
    uval myid=args->allocator_id;
    uval i;
    uval pos;
    Entry *o;

    BaseRandom random;

    cprintf("Allocator(%ld)\n", myid);

    for ( i = 0; i < iterations && !*tstglockDone; i++ ) {
	for ( pos = random.getVal() % NUM_OBJECTS;/*EMPTY*/;
	     pos=(pos+1) % NUM_OBJECTS) {
	    if ( *tstglockDone ) break;
	    o = &objects[pos];
	    if ( o->allocated )
		continue;
	    o->lock.acquire();
	    if ( ! o->allocated && ! o->deleted && ! o->inuse) {
		o->testobj=TestObj::Create();
		o->allocated = 1;
		o->deleted   = 0;
		o->lock.release();
		break;
	    } else {
		o->lock.release();
	    }
	}
    }
    cprintf("allocator %ld all done\n", myid);
    tstglockbarrier->enter();
    return 0;
}

// need a deallocator function that chooses a random location, using
// linear probing if the entry is not full (must sync around deallocate)

SysStatus
deallocator(ThreadArgs *args)
{
    Entry *objects=args->objects;
    uval iterations=args->iterations;
    uval myid=args->deallocator_id;
    uval doneFlag=args->doneFlag;
    uval i;
    uval pos;
    SysStatus rc;
    Entry *o;

    BaseRandom random;

    cprintf("Deallocator(%ld)\n", myid);

    for ( i = 0; i < iterations && !*tstglockDone; i++ ) {
	for ( pos = random.getVal() % NUM_OBJECTS;/*EMPTY*/;
	     pos = (pos+1) % NUM_OBJECTS) {
	    if ( *tstglockDone ) break;
	    o = &objects[pos];
	    if ( !o->allocated )
		continue;
	    o->lock.acquire();
	    if ( o->allocated && !o->deleted ) {
		rc = DREF(o->testobj)->destroy();
		if ( rc != 0 ) {
		    cprintf("Deallocation failed\n");
		}
		o->deleted = 1;
		// let accessor mark entry is free
		o->lock.release();
		break;
	    } else {
		o->lock.release();
	    }
	}
    }
    if ( doneFlag ) {
	cprintf("Setting Done flag\n");
	*tstglockDone = doneFlag;
    }
    cprintf("deallocator %ld all done\n", myid);
    tstglockbarrier->enter();
    return 0;
}

// need an accessor using multiple functions, only synching to verify that
// object is deleted after it gets an error on use, after which it marks the
// entry as empty (all under a lock).

SysStatus
accessor( ThreadArgs *args )
{
    Entry *objects=args->objects;
    uval myid=args->accessor_id;
    uval pos;
    SysStatus rc;
    Entry *o;

    BaseRandom random;

    cprintf("Accessor(%ld)\n", myid);

    while ( !*tstglockDone ) {
	for ( pos = random.getVal() % NUM_OBJECTS;/*EMPTY*/;
	     pos=(pos+1) % NUM_OBJECTS) {
	    if ( *tstglockDone ) break;

	    o = &objects[pos];
	    o->lock.acquire();
	    if ( !o->allocated ) {
		o->lock.release();
		continue;
	    }
	    o->inuse++;
	    o->lock.release();

	    rc = DREF(o->testobj)->func1((uval *)&(o->testobj),(uval)o->testobj);
	    if ( rc != 0 ) { cprintf("_A_"); goto failure; }
	    rc = DREF(o->testobj)->func2((uval *)&(o->testobj),(uval)o->testobj);
	    if ( rc != 0 ) { cprintf("_B_"); goto failure; }
	    rc = DREF(o->testobj)->func3((uval *)&(o->testobj),(uval)o->testobj);
	    if ( rc != 0 ) { cprintf("_C_"); goto failure; }

	    o->lock.acquire();
	    o->inuse--;
	    o->lock.release();
	    break;

    failure:
	    o->lock.acquire();
	    o->inuse--;
	    if ( o->deleted && o->inuse == 0) {
		// as expected
		o->allocated = 0;
		o->deleted = 0;
	    } else if ( !o->deleted ) {
		tassert(0, err_printf("access failed\n"));
	    }
	    o->lock.release();

	    break;
	}
    }
    cprintf("accessor %ld all done\n", myid);
    tstglockbarrier->enter();
    return 0;
}


void tstglock( uval iterations )
{
    cprintf("tstglock(%ld)\n", iterations);


    uval          id = 0;
    uval          vp;
    const uval    numtimes = 3;
    const uval    numw = numtimes * 3;
    ThreadArgs    *args;
    uval i;
    tstglockbarrier = new BlockBarrier( numw + 1 );
//    GlobalUpdate((void **)&barrier);
    tstglockDone    = (sval *)allocGlobal(sizeof(uval));
//    GlobalUpdate((void **)&tstglockDone);
    *tstglockDone = 0;

    Entry *objects = new Entry[NUM_OBJECTS];
    ThreadArgs *threadArgs = new ThreadArgs[numtimes];

    // start some workers
    cprintf("Starting workers NumVP=%ld\n", NumVP);

    for (i = 0;i < numtimes; i++ ) {
	vp=i % NumVP;

	args=&(threadArgs[i]);
	args->objects=objects;
	args->iterations=iterations;
	args->doneFlag=(i == (numtimes-1));
	args->allocator_id=id++;
	args->deallocator_id=id++;
	args->accessor_id=id++;

	args->allocatorThread=SimpleThread
	    ::Create(SimpleThread::function(allocator), args, vp);
	passert(args->allocatorThread != 0,
		err_printf("Thread create failed\n"));
	args->deallocatorThread=SimpleThread
	    ::Create(SimpleThread::function(deallocator), args,vp);
	passert(args->deallocatorThread != 0,
		err_printf("Thread create failed\n"));
	args->accessorThread=SimpleThread
	    ::Create(SimpleThread::function(accessor), args, vp);
	passert(args->accessorThread != 0,
		err_printf("Thread create failed\n"));
    }

    tassert( id == numw, err_printf("Oops id!=numw\n") );

    tstglockbarrier->enter();

    cprintf("Workers finished\n");

    for ( i = 0; i < NUM_OBJECTS; i++ ) {
	Entry *o;
	SysStatus rc;
	o = &objects[i];
	o->lock.acquire();
	if ( o->allocated && !o->deleted ) {
	    cprintf("Freeing object %ld at end\n", i );
	    rc = DREF(o->testobj)->destroy();
	    passert( rc == 0, err_printf("Deallocation failed\n") );
	}
	o->reinit();
	o->lock.release();
    }

    for (i = 0;i < numtimes; i++ ) {
	args=&(threadArgs[i]);
	(void)SimpleThread::Join(args->allocatorThread);
	(void)SimpleThread::Join(args->deallocatorThread);
	(void)SimpleThread::Join(args->accessorThread);
    }

    delete[] threadArgs;
    delete tstglockbarrier;
    delete[] objects;
    freeGlobal((void *)tstglockDone,sizeof(uval));

    cprintf("All done\n");
}
// --------------------------------------------------------------------------

void
gatherStatsReal()
{
    uval testsToRun;
    Stats.setNumbTestsToRun(1);

    Stats.testCount=0;
    for (testsToRun=0; testsToRun<Stats.numbTestsToRun; testsToRun++) {
	cprintf("running ipc test %ld\n", testsToRun);
	ipcTest();
	cprintf("running user ipc test %ld\n", testsToRun);
	userIPCTest();
	cprintf("running user ipc 2 test %ld\n", testsToRun);
	userIPCTest2();
	cprintf("running user ipc 3 test %ld\n", testsToRun);
	userIPCTest3();
	cprintf("running user ipc 4 test %ld\n", testsToRun);
	userIPCTest4();
	cprintf("running page fault 1 test %ld\n", testsToRun);
	pfTest1();
	if (! runningRegress) {
	    cprintf("running page fault 2 test %ld\n", testsToRun);
	    pfTest2();
	    cprintf("running page fault 3 test %ld\n", testsToRun);
	    pfTest3();
	    //cprintf("running page fault 4 test %ld\n", testsToRun);
	    //pfTest4();
	}
	cprintf("running thread test %ld\n", testsToRun);
	threadTest();
	Stats.incTestCount();
    }
    Stats.printResults(Stats.MAX_EXPER);
}

void
gatherStats()
{
    VPNum i;

#if 0
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);
#endif /* #if 0 */

    if (runningRegress) {
	// run on 1 and N processors when running regress
	NumVP = 1;
	gatherStatsReal();
	NumVP = (DREFGOBJ(TheProcessRef)->ppCount());
	if (NumVP > 1) {
	    gatherStatsReal();
	}
	return;
    }

    VPNum const numPPs = DREFGOBJ(TheProcessRef)->ppCount();
    for (i=1; i<=numPPs; i++) {
	NumVP = i;
	cprintf("About to start running tests for %ld VPs on %ld PPs\n",
		i, numPPs);
	gatherStatsReal();
	Stats.storeOverallResults(i);
    }

#if 0
    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);
#endif /* #if 0 */

    Stats.printOverallResults(Stats.MAX_EXPER);
}

void
untimedRegressTests()
{
    extern uval mhTest();
    mhTest();
}

typedef void (*function)(void);

void
runtest(function func, char *name, uval which)
{
    uval testsToRun;

    Stats.testCount=0;
    for (testsToRun=0; testsToRun<Stats.numbTestsToRun; testsToRun++) {
	cprintf("running %s test %ld\n", name, testsToRun);
	func();
	Stats.incTestCount();
    }
    Stats.printResults(which);
    Stats.storeOverallResults(NumVP);
    Stats.printOverallResults(which);
}

SysStatus
RemoteDie(void *p)
{
    cprintf("Testing remote destruction; maybe\n");
    StubWire::RestartDaemon();
    StubSampleService::Die();
    DREFGOBJ(TheProcessRef)->kill();
    tassert(0, err_printf("oops\n"));
    return 0;
}

void
TestRemoteDie()
{
    MakeMP(NumVP);
    SimpleThread::Create(RemoteDie, 0, SysTypes::DSPID(0, NumVP-1));
}

void
DebugTest()
{
    StubWire::RestartDaemon();
#if 0
    cprintf("Doing bad address access\n");
    // add newlines to make string proper
    asm("\n\
	.set noat\n\
	.set noreorder\n\
	dli $1, 0x0\n\
	ld $0, 0($1)\n\
	.set reorder\n\
	.set at\n\
	");
    cprintf("Done bad address access\n");
#elif 1
    cprintf("Doing bad address access\n");
    *(volatile uval *)0;
    cprintf("Done bad address access\n");
#elif 0
    cprintf("Doing breakpoint\n");
    breakpoint();
    cprintf("Done breakpoint\n");
#endif /* #if 0 */
    StubWire::SuspendDaemon();
}

//FIXME - c++ says main must return an int - what about 64 bits?
int
main(uval argc, char **argv)
{
    NativeProcess();

    if (argc > 1) {
	if (strcmp(argv[1], "regress") == 0) {
	    Stats.numbIters[stats::THREAD]           = 20;
	    Stats.numbReqPerIter[stats::THREAD]      = 1;
	    Stats.numbIters[stats::IPC]              = 80;
	    Stats.numbReqPerIter[stats::IPC]         = 1;
	    Stats.numbIters[stats::USER_IPC]         = 80;
	    Stats.numbReqPerIter[stats::USER_IPC]    = 1;
	    Stats.numbIters[stats::USER_IPC2]        = 80;
	    Stats.numbReqPerIter[stats::USER_IPC2]   = 1;
	    Stats.numbIters[stats::USER_IPC3]        = 80;
	    Stats.numbReqPerIter[stats::USER_IPC3]   = 1;
	    Stats.numbIters[stats::USER_IPC4]        = 80;
	    Stats.numbReqPerIter[stats::USER_IPC4]   = 1;
	    Stats.numbIters[stats::PAGE_FAULT1]      = 1;
	    Stats.numbReqPerIter[stats::PAGE_FAULT1] = 128;
	    Stats.numbIters[stats::PAGE_FAULT2]      = 1;
	    Stats.numbReqPerIter[stats::PAGE_FAULT2] = 128;
	    Stats.numbIters[stats::PAGE_FAULT3]      = 1;
	    Stats.numbReqPerIter[stats::PAGE_FAULT3] = 128;
	    Stats.numbIters[stats::PAGE_FAULT4]      = 1;
	    Stats.numbReqPerIter[stats::PAGE_FAULT4] = 128;

	    runningRegress = 1;
	    Stats.setNumbTestsToRun(1);

	    NumVP = DREFGOBJ(TheProcessRef)->ppCount();
	    gatherStats();
	    untimedRegressTests();
	    StubSampleService::Die();

	    return 0;
	}
    }
    uval wasRunning;
    wasRunning = _SGETUVAL(StubWire::RestartDaemon());

    Stats.numbIters[stats::THREAD]     = 2000;
    Stats.numbReqPerIter[stats::THREAD] = 1;

    Stats.numbIters[stats::IPC]        = 8000;
    Stats.numbReqPerIter[stats::IPC]    = 1;

    Stats.numbIters[stats::USER_IPC]        = 8000;
    Stats.numbReqPerIter[stats::USER_IPC]    = 1;

    Stats.numbIters[stats::USER_IPC2]        = 8000;
    Stats.numbReqPerIter[stats::USER_IPC2]    = 1;

    Stats.numbIters[stats::USER_IPC3]        = 8000;
    Stats.numbReqPerIter[stats::USER_IPC3]    = 1;

    Stats.numbIters[stats::USER_IPC4]        = 8000;
    Stats.numbReqPerIter[stats::USER_IPC4]    = 1;

    // these are really one experiment, two sets of stats
    Stats.numbIters[stats::PAGE_FAULT1] = 4;
    Stats.numbReqPerIter[stats::PAGE_FAULT1] = 512;

    Stats.numbIters[stats::PAGE_FAULT2] = 4;
    Stats.numbReqPerIter[stats::PAGE_FAULT2] = 512;

    Stats.numbIters[stats::PAGE_FAULT3] = 4;
    Stats.numbReqPerIter[stats::PAGE_FAULT3] = 512;

    Stats.numbIters[stats::PAGE_FAULT4] = 4;
    Stats.numbReqPerIter[stats::PAGE_FAULT4] = 512;

#if 0 // short tests for testing
    Stats.numbIters[stats::THREAD]     = 20;
    Stats.numbIters[stats::IPC]        = 80;
    Stats.numbIters[stats::USER_IPC]   = 80;
    Stats.numbIters[stats::USER_IPC2]   = 80;
    Stats.numbIters[stats::USER_IPC3]   = 80;
    Stats.numbIters[stats::USER_IPC4]   = 80;
    Stats.numbIters[stats::PAGE_FAULT1] = 1;
    Stats.numbReqPerIter[stats::PAGE_FAULT1] = 128;
    Stats.numbIters[stats::PAGE_FAULT2] = 1;
    Stats.numbReqPerIter[stats::PAGE_FAULT2] = 128;
    Stats.numbIters[stats::PAGE_FAULT3] = 1;
    Stats.numbReqPerIter[stats::PAGE_FAULT3] = 128;
    Stats.numbIters[stats::PAGE_FAULT4] = 1;
    Stats.numbReqPerIter[stats::PAGE_FAULT4] = 128;
#endif /* #if 0 // short tests for testing */

    Stats.setNumbTestsToRun(1);

    NumVP = DREFGOBJ(TheProcessRef)->ppCount();
    printf("NumVP is %ld\n", uval(NumVP));

//  BREAKPOINT;

#if 0
    StubWire::SuspendDaemon();
    Scheduler::DelayMicrosecs(1000000);
    //gatherStats();
    pfTest2();
#endif /* #if 0 */

    printf("entering user test loop\n");

    while (1) {
	SysStatusUval ssu;
	char *p;
	StubWire::RestartDaemon();
	printf("[?] > ");
	char buf[20];

	char *ret = fgets(buf, 20, stdin);
	tassert(ret, err_printf("fgets failed\n"));

	p = buf;

	StubWire::SuspendDaemon();

	switch (*p) {
	case '0':
	    caseZero();
	    break;
	case '1':
	    caseOne();
	    break;
	case 'l':
	    caseLs();
	    break;
	case 'A':
	    AllocTest();
	    break;
	case 'C':
	    extern uval mhTest();
	    mhTest();
	    break;
	case 'D':
	    if (wasRunning) StubWire::RestartDaemon();
	    TestRemoteDie();
	    return 0;
	case 'F':
	    testSharedFCM(1);
	    break;
	case 'f':
	    testSharedFCM(0);
	    break;
	case 'G':
	    tstglock(3);
	    break;
	case 'r':
	    testPartitionedFCM(0, 0);
	    break;
        case 'R':
            testPartitionedFCM(0, 1);
            break;
	case 'I':
	    runtest(ipcTest, "ipc", Stats.IPC);
	    break;
	case 'L':
	    lockTest();
	    break;
	case 'M':
	    misc();
	    break;
	case 'N':
	    StubWire::RestartDaemon();
	    printf("NumVPs > ");


	    ret = fgets(buf, 20, stdin);
	    tassert(ret, err_printf("fgets failed\n"));

	    ssu = atoi(p);
	    if (_SUCCESS(ssu)) {
		NumVP = _SGETUVAL(ssu);
	    } else {
		NumVP = 0;
	    }

	    if (NumVP < 1) {
		printf("Must be integer > 0, assuming 1\n");
		NumVP = 1;
	    }
	    MakeMP(NumVP);
	    break;
	case 'O':
	    StubWire::RestartDaemon();
	    pageoutTest();
	    break;
	case 'o':
	    pageoutTestFRComputation();
	    break;
	case 'P':
	    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::BEGIN_PERF_MON,0);
	    runtest(pfTest1, "page fault 1", Stats.PAGE_FAULT1);
	    DREFGOBJ(TheProcessRef)->perfMon(ProcessServer::END_PERF_MON,0);
	    break;
	case 'Q':
	    if (wasRunning) StubWire::RestartDaemon();
	    StubSampleService::Die();
	    return 0;
	case 'S':
	    gatherStats();
	    break;
	case 'T':
	    runtest(threadTest, "thread", Stats.THREAD);
	    break;
	case 'U':
	    runtest(userIPCTest, "user ipc", Stats.USER_IPC);
	    break;
	case 'V':
	    vpTest();
	    break;
	case 'W':
	    writeTest();
	    break;
	case 'Z':
	    socketTest();
	    break;
	default:
	    printf("Enter the test:\n");
	    printf("  0 - path name handling\n");
	    printf("  1 - path name appending\n");
	    printf("  l - simple ls\n");
	    printf("  A - concurrent alloc test\n");
	    printf("  C - simple Clustered Object test\n");
	    printf("  D - TestRemoteDie - kill from other VP\n");
	    printf("  F/f - shared FCM test: delete-at-end/no-delete\n");
	    printf("  G - tstglock: clustered object destruction tests\n");
    	    printf("  r - replicated FCM test: no-delete\n");
	    printf("  I - concurrent IPC test\n");
	    printf("  L - concurrent lock\n");
	    printf("  M - run misc set of tests (used to be main)\n");
	    printf("  N - Specify number of vps to use for mp tests\n");
	    printf("  O - concurrent pageout test; file\n");
	    printf("  o - concurrent pageout test; swap\n");
	    printf("  P - concurrent page fault\n");
	    printf("  Q - exit user test loop\n");
	    printf("  S - gather stats\n");
	    printf("  T - concurrent thread creation test\n");
	    printf("  U - user-mode IPC test\n");
	    printf("  V - vpTest\n");
	    printf("  W - write test\n");
	    printf("  Z - socket experiment\n");
	    printf("  ? - print out this menu\n");
	    printf("\n");
	    break;
	}
    }
}
