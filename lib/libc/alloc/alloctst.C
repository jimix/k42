/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: alloctst.C,v 1.40 2003/11/24 12:33:35 mostrows Exp $
 *****************************************************************************/

#include <sys/sysIncs.H>
#include <sys/KernelInfo.H>
//#include <stdio.h>
#include <misc/BaseRandom.H>
#include <scheduler/Scheduler.H>
#include <sync/Barrier.H>
#include <usr/ProgExec.H>
#include <alloc/PageAllocator.H>

#include <misc/hardware.H>

// used to trigger simos events

extern "C" {
    void tstEvent_startworker();
    void tstEvent_endworker();
    void tstEvent_starttest();
    void tstEvent_endtest();
}

#ifdef NO_LONGER_NEEDED
void tstEvent_starttest()   { /* empty body */ }
void tstEvent_endtest()     { /* empty body */ }
void tstEvent_startworker() { /* empty body */ }
void tstEvent_endworker()   { /* empty body */ }
#endif /* #ifdef NO_LONGER_NEEDED */

#define ARRAY 32
template<class ALLOC> static void
simpleTest(uval numTests)
{
    //uval j;
    uval vp = Scheduler::GetVP();
    struct testStruc {
	uval   x, y;
	struct testStruc *next;
	uval8 bogus[200];
	//uval8 bogus[900];
    };

    struct testStruc *head = NULL;
    struct testStruc *ptr;

    // scale down largest normal test value; see below for values
    if (numTests == 100000) numTests = 10000;

    uval i;

    //err_printf("allocating %ld structures of size %ld\n", numTests,
    //       (uval) sizeof(struct testStruc));
    for ( i=0;i<numTests;i++ ) {
	if ((i%1000) == 0) {
	    cprintf("a");
	    Scheduler::Yield();
	}
	//for (j=0; j < (vp+1)*i; j++) {}
	ptr = (testStruc *)ALLOC::alloc(sizeof(struct testStruc));
#if (_SIZEUVAL == 8)
	    ptr->x = 0x1111111111110000+vp;
	    ptr->y = 0x1111111110001111+vp;
#else /* #if (_SIZEUVAL == 8) */
	    ptr->x = 0x11110000+vp;
	    ptr->y = 0x10001111+vp;
#endif /* #if (_SIZEUVAL == 8) */
	ptr->next = head;
	head = ptr;
    }

    //err_printf("Deallocating\n");
    for ( i=0;i<numTests;i++ ) {
	if ((i%1000) == 0) {
	    cprintf("f");
	    Scheduler::Yield();
	}
	ptr = head;
	tassert( ptr, err_printf("woops\n"));
#if (_SIZEUVAL == 8)
	    tassert( ptr->x == 0x1111111111110000+vp, err_printf("woops\n"));
	    tassert( ptr->y == 0x1111111110001111+vp, err_printf("woops\n"));
#else /* #if (_SIZEUVAL == 8) */
	    tassert( ptr->x == 0x11110000+vp, err_printf("woops\n"));
	    tassert( ptr->y == 0x10001111+vp, err_printf("woops\n"));
#endif /* #if (_SIZEUVAL == 8) */
	head = ptr->next;
	//for (j=0; j < (vp+1)*i; j++) {}
	ALLOC::free(ptr, sizeof(struct testStruc));
    }
}

#include <defines/template_bugs.H>
#ifdef EXPLICIT_TEMPLATE_INSTANTIATION
// TEMPLATE INSTANTIATION
template
  void
  simpleTest<AllocPinnedLocalStrict>(uval);
template
  void
  simpleTest<AllocPinnedGlobal>(uval);
template
  void
  simpleTest<AllocPinnedGlobalPadded>(uval);
template
  void
  simpleTest<AllocLocalStrict>(uval);
template
  void
  simpleTest<AllocGlobal>(uval);
template
  void
  simpleTest<AllocGlobalPadded>(uval);
#endif /* #ifdef EXPLICIT_TEMPLATE_INSTANTIATION */

struct testinfo {
    uval pool;
    uval numTests;
};


static void
doAllocationTest(uval input)
{
    uval i,j,n;
    uval32 randomState = Scheduler::GetVP();
    uval pool = ((struct testinfo *)input)->pool;
    uval numTests = ((struct testinfo *)input)->numTests;

    // these are local, so can run test concurrenty on two processors
    void *valsL[ARRAY];
    void *valsG[ARRAY];
    void *valsP[ARRAY];
    uval sizes[ARRAY];

    for (i=0;i<ARRAY;i++) sizes[i]=0;

    for (i=0; i<2/*00*/; i++) {
	//err_printf("%ld: started alloc test, numTests = %ld\n", i, numTests);
	if (pool == AllocPool::PINNED) {
	    simpleTest<AllocPinnedLocalStrict>(numTests);
	    simpleTest<AllocPinnedGlobal>(numTests);
	    simpleTest<AllocPinnedGlobalPadded>(numTests);
	} else {
	    simpleTest<AllocLocalStrict>(numTests);
	    simpleTest<AllocGlobal>(numTests);
	    simpleTest<AllocGlobalPadded>(numTests);
	}
	//err_printf("done simple alloc test\n");
    }

    for (i=0;i<numTests;i++) {
	if ((i%1000) == 0) {
	    cprintf("t");
	    Scheduler::Yield();
	}
	j = BaseRandom::GetLC(&randomState) & AllocPool::MAX_BLOCK_SIZE-1;
	n = BaseRandom::GetLC(&randomState) & (ARRAY-1);
	if (sizes[n]) {
	    //cprintf("freeing %lx %ld\n", vals[n], sizes[n]);
	    if (pool == AllocPool::PINNED) {
		freePinnedLocalStrict(valsL[n], sizes[n]);
		freePinnedGlobal(valsG[n], sizes[n]);
		freePinnedGlobalPadded(valsP[n], sizes[n]);
	    } else {
		freeLocalStrict(valsL[n], sizes[n]);
		freeGlobal(valsG[n], sizes[n]);
		freeGlobalPadded(valsP[n], sizes[n]);
	    }
	    sizes[n] = 0;
	} else {
	    //cprintf("allocating %ld --- ", j);
	    if (pool == AllocPool::PINNED) {
		valsL[n] = allocPinnedLocalStrict(j);
		valsG[n] = allocPinnedGlobal(j);
		valsP[n] = allocPinnedGlobalPadded(j);
	    } else {
		valsL[n] = allocLocalStrict(j);
		valsG[n] = allocGlobal(j);
		valsP[n] = allocGlobalPadded(j);
	    }
	    //cprintf(" got  %lx\n", vals[n]);
	    sizes[n] = j;
	}
    }

    //err_printf("doing final free\n");
    for (n=0;n<ARRAY;n++) {
	if (sizes[n]) {
	    if (pool == AllocPool::PINNED) {
		freePinnedLocalStrict(valsL[n], sizes[n]);
		freePinnedGlobal(valsG[n], sizes[n]);
		freePinnedGlobalPadded(valsP[n], sizes[n]);
	    } else {
		freeLocalStrict(valsL[n], sizes[n]);
		freeGlobal(valsG[n], sizes[n]);
		freeGlobalPadded(valsP[n], sizes[n]);
	    }
	}
    }
    //err_printf("done alloc test\n");
}



void
testAlloc(uval parg, uval doDefault)
{
    struct testinfo todo;
    todo.pool = parg;

//    sval c;
//    if (doDefault) {
	if (KernelInfo::OnSim()) {
	    todo.numTests = 100;
	} else {
	    todo.numTests = 100000;
	}
#if 0
    } else {
	cprintf("choose number of loops (1) - 10, (2) - 100, (3) - 1000"
		" (4) - 100000: ");

	c = getchar();
	cprintf("<%c>\n", (char) c);
	if (c=='1') todo.numTests = 10;
	else if (c=='2') todo.numTests = 100;
	else if (c=='3') todo.numTests = 1000;
	else if (c=='4') todo.numTests = 100000;
	else todo.numTests = 100;
    }
#endif
    //allocLocal[todo.pool].printStats();
    err_printf("doing numTests %ld on pool %ld\n", todo.numTests, todo.pool);
    //Scheduler::ScheduleFunction(doAllocationTest, (uval)&todo);
    doAllocationTest((uval)&todo);
    Scheduler::Yield();
    //allocLocal[todo.pool].printStats();
}

/************************************************************************/

const uval testSize = 16/* *1024 */;

static SysStatus
remoteSide(uval uvalp)
{
    void **p = (void **)uvalp;
    err_printf("test: Freeing %p %p\n", p[0], p[1]);

    freePinnedGlobal(p[0], testSize);
    freeGlobal(p[1], testSize);

    return 0;
}

void
testRemoteFree()
{
    VPNum numvp = DREFGOBJ(TheProcessRef)->vpCount();
    VPNum myvp = Scheduler::GetVP();
    SysStatus rc, retrc;
    LMalloc *lm;
    VPNum othervp;
    void *pp, *pu;

    othervp = (myvp + (numvp/2)) % numvp;

    err_printf("Doing remote free test: me %ld other %ld\n", myvp, othervp);

    if (sval(AllocPool::index(testSize)) >= 0) {
	lm = allocLocal[AllocPool::PINNED].global(AllocPool::index(testSize));
	err_printf("P: size %ld, lm: size %ld, mid %ld, nodeid %lx\n",
		   testSize, lm->getSize(), lm->getMallocID(),
		   lm->getNodeID());
	lm = allocLocal[AllocPool::PAGED].global(AllocPool::index(testSize));
	err_printf("U: size %ld, lm: size %ld, mid %ld, nodeid %lx\n",
		   testSize, lm->getSize(), lm->getMallocID(),
		   lm->getNodeID());
    }

    pp = allocPinnedGlobal(testSize);
    pu = allocGlobal(testSize);

    err_printf("test: Allocated P %p, U %p\n", pp, pu);

    void *p[2] = {pp, pu};
    rc = MPMsgMgr::SendSyncUval(Scheduler::GetEnabledMsgMgr(),
				SysTypes::DSPID(0, othervp),
				remoteSide, uval(p), retrc);
    tassert(_SUCCESS(rc), err_printf("remote call failed\n"));
}

/************************************************************************/

#define CONC_ARRAY_SIZE 1000

struct ConcTestInfo {
    uval     numTest;
    uval     pool;
    Barrier *bar;

    // can't do local strict in shared test, since "local strict"
    //uval *valsL[CONC_ARRAY_SIZE];
    uval *valsG[CONC_ARRAY_SIZE];
    uval *valsP[CONC_ARRAY_SIZE];
    uval  sizes[CONC_ARRAY_SIZE];
};


static SysStatus
ConcAllocWorker(uval input)
{
    uval i, size, n;
    VPNum myvp = Scheduler::GetVP();
    ConcTestInfo *p = (ConcTestInfo *)input;
    uval numTests = p->numTest;
    uval8 pool = p->pool;
    Barrier *bar = p->bar;
    uval32 randomState = Scheduler::GetVP();
    uval numSizeOptions;
    const uval maxNumSizeOptions = 100;
    const uval baseSize = sizeof(uval) /*PAGE_SIZE*/;
    const uval maxSize = AllocPool::MAX_BLOCK_SIZE*4 /*PAGE_SIZE*128*/;
    uval sizeOptions[maxNumSizeOptions];

    // use power-of-2 sizes
    for (numSizeOptions=0;numSizeOptions<maxNumSizeOptions;numSizeOptions++) {
	sizeOptions[numSizeOptions] = baseSize << numSizeOptions;
	if (sizeOptions[numSizeOptions] > maxSize) {
	    break;
	}
    }

    cprintf("Worker %ld started, pool %d, numTests %ld\n"
	    "\t%ld sizes from %ld to %ld\n", myvp, pool, numTests,
	    numSizeOptions, sizeOptions[0], sizeOptions[numSizeOptions-1]);

    bar->enter();

#if 0
    for (i = 0; i < numTests; i++) {
	if ((i%128) == 0) {
	    cprintf("t");
	    Scheduler::Yield();
	}
	bar->enter();
    }
    bar->enter();
    err_printf("All done hack test\n");
    return 0;
#endif /* #if 0 */


#if 0
    static uval64 lock;
    static uval flag;

    flag = lock = 0;

    bar->enter();

    if (myvp != 0) {
	err_printf("CompareAndStoreper running\n");
	while (1) {
	    uval64 tmp;
	    do {
		tmp = lock;
	    } while (!CompareAndStore64Synced(&lock, tmp, tmp | 2));
	    if (flag) {
		err_printf("All done\n");
		return 0;
	    }
	}
    } else {
	uval i;
	err_printf("Zeroer running\n");
	for (i = 0; 1; i++) {
	    if ((i % (16*1024)) == 0) {
		err_printf("iter %ld\n", i);
	    }
	    uval64 tmp;
	    do {
		tmp = lock;
	    } while (!CompareAndStore64Synced(&lock, tmp, tmp | 1));
	    Scheduler::Yield();
	    //Scheduler::DelayMicrosecs(1000);
	    lock = 0;
	    //Scheduler::Yield();
	    Scheduler::DelayMicrosecs(10);
	    if ((lock & 1) != 0) {
		err_printf("Lock screwed up: %lld, iter %ld\n", lock, i);
		flag = 1;
		return 0;
	    }
	}
    }

#endif /* #if 0 */


    for (i = 0; i < numTests; i++) {
	if ((i%1024) == 0) {
	    cprintf("t");
	    Scheduler::Yield();
	}
	do {
	    n = BaseRandom::GetLC(&randomState) & (CONC_ARRAY_SIZE-1);
	    size = p->sizes[n];
	} while (size == uval(-1) ||
		 ! CompareAndStoreSynced(&p->sizes[n], size, uval(-1)));

	passert(size != uval(-1), err_printf("oops\n"));
	//cprintf("Got slot %ld\n", n);

	if (size != 0) {
	    //cprintf("%ld: freeing %ld: %p %p %p\n", myvp, size,
	    //    p->valsO[n], p->valsG[n], p->valsP[n]);
	    if ( //(*p->valsL[n] != (uval(p->valsL[n])^uval(-1))) ||
		 (*p->valsG[n] != (uval(p->valsG[n])^uval(-1))) ||
		 (*p->valsP[n] != (uval(p->valsP[n])^uval(-1))) ) {
		cprintf("Alloc data error\n");
		breakpoint();
	    }
	    if (pool == AllocPool::PINNED) {
		//freePinnedLocalStrict(p->valsL[n], size);
		freePinnedGlobal(p->valsG[n], size);
		freePinnedGlobalPadded(p->valsP[n], size);
	    } else {
		//freeLocalStrict(p->valsL[n], size);
		freeGlobal(p->valsG[n], size);
		freeGlobalPadded(p->valsP[n], size);
	    }
	    // make sure everyone sees the other variable updates before
	    // marking slot free
	    SyncBeforeRelease();
	    p->sizes[n] = 0;
	} else {
	    // slot free, allocate new chunks
	    size =
		sizeOptions[BaseRandom::GetLC(&randomState) % numSizeOptions];
	    //cprintf("%ld: allocating %ld:", myvp, size);
	    if (pool == AllocPool::PINNED) {
		//p->valsL[n] = (uval *)allocPinnedLocalStrict(size);
		p->valsG[n] = (uval *)allocPinnedGlobal(size);
		p->valsP[n] = (uval *)allocPinnedGlobalPadded(size);
	    } else {
		//p->valsL[n] = (uval *)allocLocalStrict(size);
		p->valsG[n] = (uval *)allocGlobal(size);
		p->valsP[n] = (uval *)allocGlobalPadded(size);
	    }
	    //*p->valsL[n] = uval(p->valsL[n])^uval(-1);
	    *p->valsG[n] = uval(p->valsG[n])^uval(-1);
	    *p->valsP[n] = uval(p->valsP[n])^uval(-1);
	    //cprintf(" %p %p %p\n",
	    //    p->valsO[n], p->valsG[n], p->valsP[n]);
	    // make sure everyone sees the other variable updates before
	    // marking slot used
	    SyncBeforeRelease();
	    p->sizes[n] = size;
	}
    }

    cprintf("All done\n");

    bar->enter();

    return 0;
}

void
ConcTestAlloc(uval pool, uval numTests)
{
    uval i, n, numVP;
    ConcTestInfo *testinfo;
    VPNum myvp = Scheduler::GetVP();
    SysStatus rc;

    if (pool == AllocPool::PINNED) {
	testinfo = (ConcTestInfo *)allocPinnedGlobal(sizeof(ConcTestInfo));
    } else {
	testinfo = (ConcTestInfo *)allocGlobal(sizeof(ConcTestInfo));
    }
    numVP = DREFGOBJ(TheProcessRef)->ppCount();

    BlockBarrier bar(numVP);

    testinfo->numTest = numTests;
    testinfo->pool = pool;
    testinfo->bar = &bar;

    for (i = 0; i < CONC_ARRAY_SIZE; i++) {
	testinfo->sizes[i] = 0;
	//testinfo->valsL[i] =(uval *)uval(-1);
	testinfo->valsG[i] =(uval *)uval(-1);
	testinfo->valsP[i] =(uval *)uval(-1);
    }

    cprintf("Doing concurrent alloc test: pool %ld, num %ld, vps %ld\n",
	    testinfo->pool, testinfo->numTest, numVP);

    for (i = 0; i < numVP; i++) {
	if (i == myvp) continue;
	rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
				     SysTypes::DSPID(0, i),
				     ConcAllocWorker, uval(testinfo));
	tassert(_SUCCESS(rc), err_printf("remote call failed\n"));
    }
    ConcAllocWorker(uval(testinfo));

    cprintf("doing final free\n");
    for (n=0;n<CONC_ARRAY_SIZE;n++) {
	if (testinfo->sizes[n]) {
	    passert(testinfo->sizes[n] != uval(-1), cprintf("oops\n"));
	    if (pool == AllocPool::PINNED) {
		//freePinnedLocalStrict(testinfo->valsL[n],testinfo->sizes[n]);
		freePinnedGlobal(testinfo->valsG[n], testinfo->sizes[n]);
		freePinnedGlobalPadded(testinfo->valsP[n], testinfo->sizes[n]);
	    } else {
		//freeLocalStrict(testinfo->valsL[n], testinfo->sizes[n]);
		freeGlobal(testinfo->valsG[n], testinfo->sizes[n]);
		freeGlobalPadded(testinfo->valsP[n], testinfo->sizes[n]);
	    }
	}
    }

    if (pool == AllocPool::PINNED) {
	freePinnedGlobal(testinfo, sizeof(ConcTestInfo));
    } else {
	freeGlobal(testinfo, sizeof(ConcTestInfo));
    }

    cprintf("done alloc test\n");
}

/************************************************************************/

#define NUMA_NUM_ARRAYS 1
#define NUMA_BLOCK_SIZE 128		// at least scacheline size

void *
NumaTestAllocPinned()
{
    return allocPinnedGlobal(NUMA_BLOCK_SIZE);
}
void *
NumaTestAlloc()
{
    return allocGlobal(NUMA_BLOCK_SIZE);
}
void
NumaTestFreePinned(void *p)
{
    freePinnedGlobal(p, NUMA_BLOCK_SIZE);
}
void
NumaTestFree(void *p)
{
    freeGlobal(p, NUMA_BLOCK_SIZE);
}


void *
NumaTestAllocPinnedStrict()
{
    return allocPinnedLocalStrict(NUMA_BLOCK_SIZE);
}
void *
NumaTestAllocStrict()
{
    return allocLocalStrict(NUMA_BLOCK_SIZE);
}
void
NumaTestFreePinnedStrict(void *p)
{
    freePinnedLocalStrict(p, NUMA_BLOCK_SIZE);
}
void
NumaTestFreeStrict(void *p)
{
    freeLocalStrict(p, NUMA_BLOCK_SIZE);
}


struct NumaTestInfo {
    uval     numTest;
    uval     pool;
    uval     arraysize;
    Barrier *bar;
    uval     selffree;
    SysTime totalAlloc, totalFree;

    uval     valFull[NUMA_NUM_ARRAYS];
    uval   **vals[NUMA_NUM_ARRAYS];
};

static SysStatus
NumaAllocWorker(uval input)
{
    uval i, j;
    VPNum myvp = Scheduler::GetVP();
    NumaTestInfo *p = (NumaTestInfo *)input;
    const uval numTests = p->numTest;
    const uval arraysize = p->arraysize;
    const uval selffree = p->selffree;
    SysTime start, end, total, totalfree;
    uval8 pool = p->pool;
    uval array;
    uval **v;
    Barrier *bar = p->bar;

    cprintf("Alloc Worker %ld started, pool %d, numTests %ld, size %d\n",
	    myvp, pool, numTests, NUMA_BLOCK_SIZE);

    bar->enter();

    total = 0; totalfree = 0;
    for (i = 0; i < numTests; i++) {
	if ((i%1024) == 0) {
	    cprintf("t");
	}
	array = i % NUMA_NUM_ARRAYS;

	// wait for next array entry to be available for filling
	j = 0;
	while (p->valFull[array]) {
	    if ((++j % 1024) == 0) {
		Scheduler::Yield();
	    }
	}
	SyncAfterAcquire();

	v = p->vals[array];
	start = Scheduler::SysTimeNow();
	tstEvent_startworker();
	if (selffree != 2 && pool == AllocPool::PINNED) {
	    for (j = 0; j < arraysize; j++) {
		//v[j] = (uval *)NumaTestAllocPinned();
		v[j] = (uval *)allocPinnedGlobal(NUMA_BLOCK_SIZE);
		*v[j] = uval(-1);
	    }
	} else if (selffree != 2) {
	    for (j = 0; j < arraysize; j++) {
		//v[j] = (uval *)NumaTestAlloc();
		v[j] = (uval *)allocGlobal(NUMA_BLOCK_SIZE);
		*v[j] = uval(-1);
	    }
	} else if (pool == AllocPool::PINNED) {
	    for (j = 0; j < arraysize; j++) {
		//v[j] = (uval *)NumaTestAllocPinnedStrict();
		v[j] = (uval *)allocPinnedLocalStrict(NUMA_BLOCK_SIZE);
		*v[j] = uval(-1);
	    }
	} else {
	    for (j = 0; j < arraysize; j++) {
		//v[j] = (uval *)NumaTestAllocStrict();
		v[j] = (uval *)allocLocalStrict(NUMA_BLOCK_SIZE);
		*v[j] = uval(-1);
	    }
	}
	tstEvent_endworker();
	end = Scheduler::SysTimeNow();
	total += end - start;
	// mark arry as full
	SyncBeforeRelease();
	p->valFull[array] = 1;

	if (selffree != 0) {
	    v = p->vals[array];
	    start = Scheduler::SysTimeNow();
	    tstEvent_startworker();
	    if (selffree == 1 && pool == AllocPool::PINNED) {
		for (j = 0; j < arraysize; j++) {
		    //NumaTestFreePinned(v[j]);
		    freePinnedGlobal(v[j], NUMA_BLOCK_SIZE);
		}
	    } else if (selffree == 1) {
		for (j = 0; j < arraysize; j++) {
		    //NumaTestFree(v[j]);
		    freeGlobal(v[j], NUMA_BLOCK_SIZE);
		}
	    } else if (pool == AllocPool::PINNED) {
		for (j = 0; j < arraysize; j++) {
		    //NumaTestFreePinnedStrict(v[j]);
		    freePinnedLocalStrict(v[j], NUMA_BLOCK_SIZE);
		}
	    } else {
		for (j = 0; j < arraysize; j++) {
		    //NumaTestFreeStrict(v[j]);
		    freeLocalStrict(v[j], NUMA_BLOCK_SIZE);
		}
	    }
	    tstEvent_endworker();
	    end = Scheduler::SysTimeNow();
	    totalfree += end - start;
	    p->valFull[array] = 0;
	}

    }

    // cast values to match printf format specs
    cprintf("All done: time %lld/%ld (selfree %lld/%ld)\n",
	    total, (long)(total / arraysize / numTests),
	    totalfree, (long)(totalfree / arraysize / numTests));

    if (selffree != 0) {
	p->totalFree = totalfree;
    }
    p->totalAlloc = total;
    bar->enter();

    return 0;
}


static SysStatus
NumaFreeWorker(uval input)
{
    uval i, j;
    VPNum myvp = Scheduler::GetVP();
    NumaTestInfo *p = (NumaTestInfo *)input;
    const uval numTests = p->numTest;
    const uval arraysize = p->arraysize;
    SysTime start, end, total;
    uval8 pool = p->pool;
    uval array;
    uval **v;
    Barrier *bar = p->bar;

    cprintf("Freeer %ld started, pool %d, numTests %ld, asize %ld, size %d\n",
	    myvp, pool, numTests, arraysize, NUMA_BLOCK_SIZE);

    passert(p->selffree == 0, err_printf("FreeWorker with selffree\n"));

    bar->enter();

    total = 0;
    for (i = 0; i < numTests; i++) {
	if ((i%1024) == 0) {
	    cprintf("t");
	}
	array = i % NUMA_NUM_ARRAYS;

	// wait for next array entry to be filled
	j = 0;
	while (!p->valFull[array]) {
	    if ((++j % 1024) == 0) {
		Scheduler::Yield();
	    }
	}
	SyncAfterAcquire();

	v = p->vals[array];
	start = Scheduler::SysTimeNow();
	tstEvent_startworker();
	if (pool == AllocPool::PINNED) {
	    for (j = 0; j < arraysize; j++) {
		//NumaTestFreePinned(v[j]);
		freePinnedGlobal(v[j], NUMA_BLOCK_SIZE);
	    }
	} else {
	    for (j = 0; j < arraysize; j++) {
		//NumaTestFree(v[j]);
		freeGlobal(v[j], NUMA_BLOCK_SIZE);
	    }
	}
	tstEvent_endworker();
	end = Scheduler::SysTimeNow();
	total += end - start;

	// mark arry as empty
	SyncBeforeRelease();
	p->valFull[array] = 0;
    }

    // cast values to match printf format specs
    cprintf("All done: time %lld/%ld\n",
	    total, (long) (total / arraysize / numTests));

    p->totalFree = total;

    bar->enter();

    return 0;
}

void
NumaAllocFreeTest(uval pool, uval numTests, uval arraysize, uval selffree)
{
    uval i, j;
    NumaTestInfo *testinfo;
    SysStatus rc;
    VPNum myvp, othervp, numvp, numneeded;

    numneeded = selffree ? 1 : 2;

    if (pool == AllocPool::PINNED) {
	testinfo = (NumaTestInfo *)
	    allocPinnedGlobalPadded(sizeof(NumaTestInfo));
    } else {
	testinfo = (NumaTestInfo *)allocGlobalPadded(sizeof(NumaTestInfo));
    }

    numvp = DREFGOBJ(TheProcessRef)->ppCount();
    if (numvp < numneeded) {
	cprintf("Must have at least %ld processors for the test\n", numneeded);
	return;
    }

    myvp = Scheduler::GetVP();
    othervp = (myvp + (numvp/2)) % numvp;

    BlockBarrier bar(numneeded);

    testinfo->arraysize = arraysize;
    testinfo->numTest = numTests;
    testinfo->pool = pool;
    testinfo->selffree = selffree;
    testinfo->bar = &bar;

    for (i = 0; i < NUMA_NUM_ARRAYS; i++) {
	testinfo->valFull[i] = 0;
	testinfo->vals[i] = (uval **)((pool == AllocPool::PINNED)
	    ? allocPinnedGlobalPadded(arraysize * sizeof(uval *))
	    : allocGlobalPadded(arraysize * sizeof(uval *)));
	for (j = 0; j < arraysize; j++) {
	    testinfo->vals[i][j] = (uval *)uval(-1);
	}
    }

    cprintf("Numa alloc/free test: pool %ld, asize %ld, n %ld, selffree %c\n",
	    testinfo->pool, testinfo->arraysize, testinfo->numTest,
	    selffree == 0 ? 'N' : (selffree == 1 ? 'G' : 'L'));

    //allocLocal[pool].printStats();

    tstEvent_starttest();

    if (! selffree) {
	rc = MPMsgMgr::SendAsyncUval(Scheduler::GetEnabledMsgMgr(),
				     SysTypes::DSPID(0, othervp),
				     NumaFreeWorker, uval(testinfo));
	tassert(_SUCCESS(rc), err_printf("remote call failed\n"));
    }
    NumaAllocWorker(uval(testinfo));

    tstEvent_endtest();

    for (i = 0; i < NUMA_NUM_ARRAYS; i++) {
	tassert(!testinfo->valFull[i], err_printf("%ld not empty\n", i));
	if (pool == AllocPool::PINNED) {
	    freePinnedGlobalPadded(testinfo->vals[i],arraysize*sizeof(uval *));
	} else {
	    freeGlobalPadded(testinfo->vals[i],arraysize*sizeof(uval *));
	}
    }

    // cast values to match printf format specs
    cprintf("All done: alloc %lld/%ld free %lld/%ld\n",
      testinfo->totalAlloc, (long) (testinfo->totalAlloc / arraysize / numTests),
      testinfo->totalFree, (long) (testinfo->totalFree / arraysize / numTests));

    if (pool == AllocPool::PINNED) {
	freePinnedGlobalPadded(testinfo, sizeof(NumaTestInfo));
    } else {
	freeGlobalPadded(testinfo, sizeof(NumaTestInfo));
    }

    //allocLocal[pool].printStats();

    cprintf("done alloc test\n");
}

void
RemotePageAlloc()
{
    uval i;
    uval p;
    SysStatus rc;
    VPNum myvp = Scheduler::GetVP();
    VPNum numvp = DREFGOBJ(TheProcessRef)->ppCount();
    VPNum node = (myvp + (numvp/2)) % numvp;

    cprintf("Allocating from node %ld, I'm %ld\n", node, myvp);

    for (i = 0; i < 1000; i++) {
	cprintf("i = %ld\n", i);
	rc = DREFGOBJ(ThePageAllocatorRef)->
	    allocPages(p, PAGE_SIZE, PageAllocator::PAGEALLOC_FIXED, node);
	if (!_SUCCESS(rc)) {
	    cprintf("No more pages: i %ld\n", i);
	    break;
	}
	*(uval *)p = 0;
    }
    cprintf("Test all done\n");
}





#if 0
// just a reminder of some performance tests that were in KernelInit.C
	extern void testRemoteFree();
	extern void NumaAllocFreeTest(uval pool, uval numTests,
				      uval arraysize, uval selffree);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 10, 1);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 100, 1);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 1000, 1);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 10, 2);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 100, 2);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 1000, 2);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 10, 0);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 100, 0);
	NumaAllocFreeTest(AllocPool::PAGED, 200, 1000, 0);
#endif /* #if 0 */
