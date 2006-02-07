/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: client.C,v 1.56 2001/12/14 04:24:03 peterson Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description:
 *   giant fiel to provide all kinds of scenarios for testing the functionality
 *   of the STUB-COMPILER
 * **************************************************************************/

#include "kernIncs.H"
#include <sys/syscalls.H>

#include "mem/PageAllocatorKern.H"
#include "stub/StubBonnie.H"
#include "meta/MetaBonnie.H"
#include <cobj/TypeMgr.H>

#define FULL_TEST 1

void
stubTest1()
{
    cprintf("Starting the test\n");


    (void)StubBonnie::typeID();

#if FULL_TEST
    long var = 97;
    StubBonnie *mc1 = new StubBonnie(1,2);
    mc1->null();
    mc1->simple(1,2,3,4,5);
    mc1->reftest(var);
    cprintf("var = %ld\n",var);
#endif /* #if FULL_TEST */

    StubBonnie *mc2 = new StubBonnie(4,5,6);

#if FULL_TEST
    mc2->simple(1,2,3,4,5);
    mc2->reftest(var);
    cprintf("var = %ld\n",var);

    ohh ov = { 1, 2 };
    uval ni;
    for (uval i=0;i<5;i++) {
	mc2->cohhr(ov);
	mc2->cohhp(&ov);
	mc2->m2ohh(ov,ni);
	cprintf("0x%lx: x=%ld y=%ld\n",ni,ov.x,ov.y);
    }
    char modstr1[20] = "dolly";
    mc2->string2("",modstr1);
    cprintf("MODSTR >%s\n",modstr1);
    char modstr2[20] = "What's up";
    mc2->string2("Yooh-Big-Brother",modstr2);
    cprintf("MODSTR >%s\n",modstr2);

#endif /* #if FULL_TEST */

    {
	char iobuf1[20] = "arg1in";
	char  obuf2[30] = "wrong1";
	char iobuf3[25] = "arg2in";
	char  obuf4[35] = "wrong2";

	mc2->string4(iobuf1,obuf2,iobuf3,obuf4);
	cprintf("RES <%s> <%s> <%s> <%s>\n",iobuf1,obuf2,iobuf3,
			obuf4);
    }

    {
	char obuf1[25] = "wrong1";
	char obuf2[30] = "wrong2";
	char obuf3[35] = "wrong3";
	char obuf4[40] = "wrong4";

	mc2->string5(obuf1,obuf2,obuf3,obuf4);
	cprintf("RES <%s> <%s> <%s> <%s>\n",obuf1,obuf2,obuf3,obuf4);
    }

    {
	uval inbuf[3]  = { 1001, 1002, 1003 };
	uval outbuf[10];

	mc2->arraytest(inbuf,outbuf);
	cprintf("RES <%ld> <%ld> <%ld> <%ld> <%ld>\n",
		outbuf[0],outbuf[1],outbuf[2],outbuf[3],outbuf[4]);
	char ibuf[] = "calling-arg";
	char obuf[1024];
	mc2->inout(ibuf,obuf);
	cprintf("RES <%s>\n",obuf);

	for (uval i = 0; i < 10 ; i++) outbuf[i] = i;
	uval blen = 10;
	mc2->varray(outbuf,blen,"Say Hello");
    }

    {
	char strbuf[20] = "string8";
	uval len        = 16;
	SysStatus rc;
	for (int i =0; i < 2; i++) {
	    rc = mc2->overflow(strbuf,&len);
	    if (rc >= 0) {
		cprintf("R <%ld> <%s>\n",len,strbuf);
	    } else {
		cprintf("R err=%lx\n",rc);
	    }
	    len = 4; // this should generate overflow next time around
	}

	uval buf[10];
	len = 0;
	cprintf("testing PPC overflow  ...\n");
	rc = mc2->ppc_overflow(buf,10000,&len,len); // error in stub
	cprintf("R err=%lx\n",rc);
	len = 10000;
	rc = mc2->ppc_overflow(buf,10,&len,len); // error in xobj
	cprintf("R err=%lx\n",rc);
    }

    {
	uval outbuf[10];
	uval len  = 10;
	uval mlen = 10;  /* we gonna change that in the second round */

	cprintf("testing variable length ...\n");

	SysStatus rc;
	for (int i =0; i < 2; i++) {
	    rc = mc2->garray(outbuf,len,mlen);
	    if (rc >= 0) {
		cprintf("R <%ld>\n",len);
	    } else {
		cprintf("R err=%lx\n",rc);
	    }
	    mlen = 6; // this should generate overflow next time around
	}
    }

    {
	char myclass[32];
	mc1->getName(myclass,32);
	cprintf("mc1 = %s\n",myclass);
	StubBonnie *mc3 = new StubBonnieGrandChild();
	mc3->getName(myclass,32);
	cprintf("mc3 = %s\n",myclass);
    }

    {
	SysStatus rc0,rc1,rc2;
	rc0 = StubBonnie::isBaseOf(StubBonnieGrandChild::typeID());
	rc1 = StubBonnieGrandChild::isBaseOf(StubBonnie::typeID());
	rc2 = MetaObj::isBaseOf(StubBonnieGrandChild::typeID());
	cprintf("isDerived = %lx %lx %lx (expect 1 0 1)\n",rc0,rc1,rc2);

	char tname[128];

	rc1 = StubBonnieGrandChild::typeName(tname,128);
	cprintf("typename >%s<\n",tname);
	DREFGOBJ(TheTypeMgrRef)->dumpTree();   // local dump
	DREFGOBJ(TheTypeMgrRef)->dumpTree(1);  // global dump
    }

    {
	mc2->testBadge(99);
    }

}

void
stubTest2()
{

    struct TestStruct {
	uval x,y;

	// for test purposes allocate a whole new page so we make
	// sure that we do not get into a page that already has beens
	// served with a pageFault
	inline void * operator new(size_t size)
	{
	    SysStatus rc;
	    (void) size;		// unused
	    uval addr;
	    rc = (DREFGOBJ(ThePageAllocatorRef)->allocPages(addr, PAGE_SIZE));
	    tassert(_SUCCESS(rc), err_printf("woops\n"));
	    return (void *)addr;
	}
    };

    StubBonnie *mc1 = new StubBonnie(1,2);

    TestStruct *ts = new TestStruct();

    for (uval i=0;i<1;i++) {
	// now call one of the internal functions
	mc1->cohhp((ohh*)ts);
    }

}

/* Entry Point for Test Program */

#include "Bonnie.H"

static void
initStubTests()
{
    static uval initialized = 0;
    if (!initialized) {
	initialized = 1;
	Bonnie::init();            // initialize the IStubBonnie internal class
    }
}

void
stubgenTest(void)
{
    initStubTests();
    // this is simple entry for Hubertus Stubgeneration test

    stubTest1();           // run the test program
}

void
stubgenTestPgFault(void)
{
    initStubTests();
    // this is simple entry for Hubertus Stubgeneration test

    stubTest2();           // run the test program
}

#include <sys/ppccore.H>

void
testMethIndexError()
{
    uval testidx[3] = { 0, XBaseObj::FIRST_METHOD-1, 99 };

    StubBonnie *mc1 = new StubBonnie(1,2);

    // here we handcode a PPC with erroneous methodnumbers
    for (int i=0 ; i<3 ; i++) {
	SysStatus ppc_rc;
	ObjectHandle oh = mc1->getOH();
	uval methnum = testidx[i];

	PPC_CALL(ppc_rc, oh.commID(), oh.xhandle(), methnum);

	cprintf("testMethIndexError err=%lx\n",ppc_rc);
    }
}

void testObjectVersion()
{
    SysStatus rc;
    ObjectHandle handle;

    cprintf("testObjectVersion started\n");
    StubBadgeTester mc1(MetaBadgeTester::changeXH | MetaBadgeTester::read);

    // now fake a different version on the xhandle that we got.....

    handle = mc1.getOH();
    handle._xhandle += 32;  // create another version of this beast
    mc1.setOH(handle);

    // and see whether we catch the version error
    rc = mc1.testRead();
    tassert(!_SUCCESS(rc), err_printf("should have got an error here\n"));
    cprintf("testObjectVersion err=%lx\n",rc);
}

void
testBadge()
{
    initStubTests();

    DREFGOBJ(TheTypeMgrRef)->dumpTree(0);

    StubBadgeTester oread(MetaBadgeTester::read);
    StubBadgeTester owrite(MetaBadgeTester::write);
    StubBadgeTester onone(MetaBadgeTester::none);
    StubBadgeTester orw(MetaBadgeTester::read | MetaBadgeTester::write);

    StubBadgeTester *objs[4] = { &oread, &owrite, &orw, &onone };
    char *info[4] = {"read", "write", "readwrite", "none" };

    for (int i=0; i < 4; i++) {
	SysStatus rcs[4];
	rcs[0] = objs[i]->testRead();
	rcs[1] = objs[i]->testWrite();
	rcs[2] = objs[i]->testReadWrite();
	rcs[3] = objs[i]->testAny();
	cprintf("obj<%s>: r=%lx w=%lx rw=%lx a=%lx\n",
		info[i],rcs[0],rcs[1],rcs[2],rcs[3]);
    }

    // now test whether we get the same XObj handled to us
    StubBadgeTester oread1(MetaBadgeTester::read);
    StubBadgeTester owrite1(MetaBadgeTester::write);
    StubBadgeTester onone1(MetaBadgeTester::none);
    StubBadgeTester orw1(MetaBadgeTester::read | MetaBadgeTester::write);

    (void)oread1;
    (void)owrite1;
    (void)onone1;
    (void)orw1;

    testMethIndexError();
    testObjectVersion();
}
