/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: PageFaultTest.C,v 1.34 2004/07/11 21:59:28 andrewb Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *    - unpinnedPageAllocator: allocates Pages in a virtual memory range
 *    - HAT: Hardware address Translation
 * **************************************************************************/

#include "kernIncs.H"
#include "mem/PageAllocatorKernUnpinned.H"
#include "mem/FCMPrimitive.H"
#include "mem/RegionDefault.H"

#define NUM_TEST_PAGES (10)       // number of pages we wanna test
uval testPages[NUM_TEST_PAGES];

void
testPageFaults(void)
{
    SysStatus rc;
    int i;
    int cnt = NUM_TEST_PAGES;

    // changes cnt here if necessary

    // reservation of Pages
    for (i=0 ; i<cnt ; i++) {
	rc = DREFGOBJ(ThePageAllocatorRef)->
	    allocPagesAligned(testPages[i], PAGE_SIZE, PAGE_SIZE);
	tassert(_SUCCESS(rc), err_printf("woops\n"));
    }

    // now go through the pages and start touching them creating page faults
    for (i=0 ; i<cnt ; i++) {
	uval* testAddr = (uval*) testPages[i];
	cprintf("\n>>Touching Virtual Address <%p>\n",testAddr);
	uval testvalue = 0;
	if (i & 1)
	    testvalue = *testAddr;  // read access
	else
	    *testAddr = 0xcafebabe;         // write access
	cprintf("<<Done Touching Vaddr <%p>, value <0x%lx>\n",
							testAddr, testvalue);
    }
    // last checking on those address
    for (i=0 ; i<cnt ; i++) {
	uval* testAddr = (uval*) testPages[i];
	uval testvalue = *testAddr;  // read access
	cprintf("Val of Address <%p> = %lx\n",testAddr,testvalue);
    }
}

struct PgAllocTest {
    uval fake1;
    uval fake2;
    uval fake3[128];

    inline void * operator new(size_t size)
    {
	tassert(size==sizeof(PgAllocTest), err_printf("bad size\n"));
	return allocGlobal(sizeof(PgAllocTest));
    }

    inline void operator delete(void *ptr, uval /*size*/)
    { freeGlobal(ptr,sizeof(PgAllocTest)); }
};


#define NUM_CHUNKS (10)

void
testPageFaultAllocs(void) {
    uval i;
    PgAllocTest* chunks[NUM_CHUNKS];

    for (uval cnt = 10; cnt -- ;) {
	cprintf("Allocating the chunks\n");
	for (i=0 ; i< NUM_CHUNKS ; i++)
	    chunks[i] = new PgAllocTest();
	cprintf("Writing the chunks\n");
	for (i=0 ; i< NUM_CHUNKS ; i++) {
	    chunks[i]->fake1 = 999;
	    chunks[i]->fake2 = 888;
	}
	cprintf("Reading the chunks\n");
	for (i=0 ; i< NUM_CHUNKS ; i++) {
	    cprintf("CH[%ld] <%ld,%ld>\n",i,
		    chunks[i]->fake1,chunks[i]->fake2);
	}
	cprintf("Deallocating the chunks\n");
	for (i=0 ; i< NUM_CHUNKS ; i++)
	    delete (chunks[i]);
    }
}

#if 0
// Note, this test doesn't work on powerpc in 32-bit address space mode
//   in kernel
// primitive fcm does not support shared mappings any more
// primitiveshared fcm no longer exists
void
testSharedFCM()
{
    RegionRef regShared1, regShared2, regPriv;
    FCMRef    fcm;
    uval vaddrShared1, vaddrShared2, vaddrPriv;
    uval fileOffset;
    const uval testval = 0x1234321;
    SysStatus rc;
    const uval sizeShared = SEGMENT_SIZE + SEGMENT_SIZE;
    const uval alignShared = SEGMENT_SIZE;
    const uval sizePriv = PAGE_SIZE + SEGMENT_SIZE;
    const uval alignPriv = PAGE_SIZE;

    cprintf(" -- testSharedFCM\n");


#if 0
    cprintf(" -- Creating primitive fcm\n");
    // test failed attempt to share an FCM
    rc = FCMPrimitive<PageList<AllocGlobal>,AllocGlobal>::Create(fcm);
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    cprintf(" -- trying failed attempt to access shared\n");
    rc = RegionDefault::CreateFixedLen(regShared1, GOBJK(TheProcessRef),
				       vaddrShared1, sizeShared, alignShared,
				       fcm, 0, AccessMode::writeUserWriteSup);
    err_printf(" -- verify failure message above\n");
    tassert(_SUCCESS(rc), err_printf("ooops\n"));
    cprintf(" -- Destroying region\n");
    rc = DREF(regShared1)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Destroying fcm\n");
    rc = DREF(fcm)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));
#endif

    cprintf("\n -- Creating fcm\n");
    rc = FCMPrimitiveShared::Create(fcm);
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    cprintf("\n -- Creating region using non-shared mode\n");
    rc = RegionDefault::CreateFixedLen(regPriv, GOBJK(TheProcessRef),
				       vaddrPriv, PAGE_SIZE, alignShared, fcm,
				       0, AccessMode::writeUserWriteSup);
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Got region at %lx size %lx\n", vaddrPriv, sizePriv);
    cprintf(" -- Accessing non-shared region at %lx\n", vaddrPriv);
    *(volatile uval *)vaddrPriv;
    cprintf(" -- Destroying non-shared region\n");
    rc = DREF(regPriv)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    cprintf("\n -- Creating region using shared mode\n");
    rc = RegionDefault::CreateFixedLen(regShared1, GOBJK(TheProcessRef),
				       vaddrShared1, sizeShared, alignShared,
				       fcm, 0, AccessMode::writeUserWriteSup);
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Got region at %lx size %lx\n", vaddrShared1, sizeShared);

    cprintf(" -- Creating read-only region using shared mode\n");
    rc = RegionDefault::CreateFixedLen(regShared2, GOBJK(TheProcessRef),
				       vaddrShared2, sizeShared, alignShared,
				       fcm, 0, AccessMode::readUserReadSup);
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Got region at %lx size %lx\n", vaddrShared2, sizeShared);

    cprintf("\n -- Accessing shared region at %lx\n", vaddrShared1);
    *(volatile uval *)vaddrShared1 = testval;
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1 + PAGE_SIZE);
    *(volatile uval *)(vaddrShared1+PAGE_SIZE) = testval+1;
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1+SEGMENT_SIZE);
    *(volatile uval *)(vaddrShared1+SEGMENT_SIZE) = testval+2;
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1+SEGMENT_SIZE);
    *(volatile uval *)(vaddrShared1+SEGMENT_SIZE);
    cprintf(" -- Accessing read-only shared region at %lx\n", vaddrShared2);
    *(volatile uval *)vaddrShared2;

    cprintf("\n -- Creating region using non-shared mode\n");
    rc = RegionDefault::CreateFixedLen(regPriv, GOBJK(TheProcessRef),
				       vaddrPriv, sizePriv, alignPriv, fcm,
				       0, AccessMode::writeUserWriteSup);
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Got region at %lx size %lx\n", vaddrPriv, sizePriv);

    tassert(vaddrShared1 != vaddrPriv, err_printf("shared == priv\n"));

    cprintf("\n -- Accessing non-shared region at %lx\n", vaddrPriv);
    if (*(volatile uval *)vaddrPriv != testval) {
	err_printf("Bad value at %lx: 0x%lx != %lx\n",
		   vaddrPriv, *(volatile uval *)vaddrPriv, testval);
	passert(0, err_printf("oops\n"));
    }
    *(volatile uval *)(vaddrPriv);
    cprintf(" -- Accessing non-shared region at %lx\n",vaddrPriv+SEGMENT_SIZE);
    if (*(volatile uval *)(vaddrPriv+SEGMENT_SIZE) != testval+2) {
	err_printf("Bad value at %lx: 0x%lx != %lx\n", vaddrPriv+SEGMENT_SIZE,
		   *(volatile uval *)(vaddrPriv+SEGMENT_SIZE), testval+2);
	passert(0, err_printf("oops\n"));
    }

    fileOffset = 0;
    cprintf("\n -- unmapping page %lx\n", fileOffset);
    rc = DREF((FCMPrimitiveShared **)fcm)->unmapPage(fileOffset);
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1);
    *(volatile uval *)vaddrShared1;
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1 + PAGE_SIZE);
    *(volatile uval *)(vaddrShared1+PAGE_SIZE);
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1+SEGMENT_SIZE);
    *(volatile uval *)(vaddrShared1+SEGMENT_SIZE);
    cprintf(" -- Accessing non-shared region at %lx\n", vaddrPriv);
    if (*(volatile uval *)vaddrPriv != testval) {
	err_printf("Bad value at %lx: 0x%lx != %lx\n",
		   vaddrPriv, *(volatile uval *)vaddrPriv, testval);
	passert(0, err_printf("oops\n"));
    }
    cprintf(" -- Accessing read-only shared region at %lx\n", vaddrShared2);
    *(volatile uval *)vaddrShared2;

    fileOffset = SEGMENT_SIZE;
    cprintf("\n -- unmapping page %lx\n", fileOffset);
    rc = DREF((FCMPrimitiveShared **)fcm)->unmapPage(fileOffset);
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1);
    *(volatile uval *)vaddrShared1;
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1 + PAGE_SIZE);
    *(volatile uval *)(vaddrShared1+PAGE_SIZE);
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1+SEGMENT_SIZE);
    *(volatile uval *)(vaddrShared1+SEGMENT_SIZE);
    cprintf(" -- Accessing non-shared region at %lx\n",vaddrPriv+SEGMENT_SIZE);
    if (*(volatile uval *)(vaddrPriv+SEGMENT_SIZE) != testval+2) {
	err_printf("Bad value at %lx: 0x%lx != %lx\n", vaddrPriv+SEGMENT_SIZE,
		   *(volatile uval *)(vaddrPriv+SEGMENT_SIZE), testval+2);
	passert(0, err_printf("oops\n"));
    }

    cprintf("\n -- Destroying shared region\n");
    rc = DREF(regShared1)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    cprintf(" -- Creating region using shared mode\n");
    rc = RegionDefault::CreateFixedLen(regShared1, GOBJK(TheProcessRef),
				       vaddrShared1, sizeShared, alignShared,
				       fcm, 0, AccessMode::writeUserWriteSup);
    tassert(_SUCCESS(rc), err_printf("oops\n"));
    cprintf(" -- Got region at %lx size %lx\n", vaddrShared1, sizeShared);

    cprintf("\n -- Accessing shared region at %lx\n", vaddrShared1);
    *(volatile uval *)vaddrShared1;
    cprintf(" -- Accessing shared region at %lx\n", vaddrShared1 + PAGE_SIZE);
    *(volatile uval *)(vaddrShared1+PAGE_SIZE);

    cprintf(" -- Accessing non-shared region at %lx\n", vaddrPriv);
    if (*(volatile uval *)vaddrPriv != testval) {
	err_printf("Bad value at %lx: 0x%lx != %lx\n",
		   vaddrPriv, *(volatile uval *)vaddrPriv, testval);
	passert(0, err_printf("oops\n"));
    }

    cprintf("\n -- Destroying non-shared region\n");
    rc = DREF(regPriv)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    cprintf(" -- Destroying shared region\n");
    rc = DREF(regShared1)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    cprintf(" -- Destroying read-only shared region\n");
    rc = DREF(regShared2)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));

#if 0
    // verify fault fails after destroying region
    cprintf(" -- Accessing region after removal; should fail\n");
    *(volatile uval *)vaddrShared1 = 0;
#endif

    cprintf(" -- Destroying fcm\n");
    rc = DREF(fcm)->destroy();
    tassert(_SUCCESS(rc), err_printf("oops\n"));

    cprintf("testSharedFCM all done\n");
}
#endif


void
TestNumaPageAlloc()
{
    uval i;
    uval p;
    SysStatus rc;
    VPNum myvp = Scheduler::GetVP();
    VPNum numvp = DREFGOBJ(TheProcessRef)->vpCount();
    VPNum node = (myvp + (numvp/2)) % numvp;

    err_printf("Allocating from node %ld, I'm %ld\n", node, myvp);

    for (i = 0; i < 10000; i++) {
	rc = DREFGOBJK(ThePinnedPageAllocatorRef)->
	    allocPages(p, PAGE_SIZE, PageAllocator::PAGEALLOC_NOBLOCK
		       | PageAllocator::PAGEALLOC_FIXED, node);
	if (_SUCCESS(rc)) {
	    if (DREFGOBJK(ThePinnedPageAllocatorRef)->addrToNumaNode(p) !=
		node) {
		err_printf("page %lx node %ld != %ld, i %ld\n", p, 
			   DREFGOBJK(ThePinnedPageAllocatorRef)->
			   addrToNumaNode(p), node, i);
		break;
	    }
	} else {
	    err_printf("No more pages: i %ld\n", i);
	    break;
	}
    }
    err_printf("Test all done\n");
}
