/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: forkTest.C,v 1.9 2005/06/28 19:48:45 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: test code for user level.
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <sys/systemAccess.H>

void
forkCheck(uval a1, uval a2, uval page, uval check)
{
    passert(*(uval*)(a1+page*PAGE_SIZE)==check,
	    err_printf("forkCheck1 %lx %lx\n",a1,page));
    if (a2) {
	passert(*(uval*)(a2+page*PAGE_SIZE)==check,
	    err_printf("forkCheck2 %lx %lx\n",a2,page));
    }
    // if two pages verify they are different
    if (a1 && a2) {
	*(uval*)(a1+page*PAGE_SIZE+8)=1;
	passert(*(uval*)(a2+page*PAGE_SIZE+8)==0,
		err_printf("forkCheck3 %lx %lx\n",a2,page));
    }
}

int
main(uval argc, char *argv[])
{
    NativeProcess();

    uval paddr, caddr, ccaddr, size;
    SysStatus rc;
    ObjectHandle frOH, childfrOH;
    size = 8*PAGE_SIZE;

    rc = StubFRComputation::_Create(frOH);
    passert(_SUCCESS(rc), err_printf("woops\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	paddr, size, 0, frOH, 0, (uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);
    passert(_SUCCESS(rc), err_printf("woops2\n"));

    Obj::ReleaseAccess(frOH);

    // initialize pages 0, 1 , 4 - leave two uninitalized

    *(uval*)(paddr) = 0xf0;
    *(uval*)(paddr+PAGE_SIZE) = 0xf1;
    *(uval*)(paddr+4*PAGE_SIZE) = 0xf4;
    *(uval*)(paddr+6*PAGE_SIZE) = 0xf6;


    // fork copy
    rc = DREFGOBJ(TheProcessRef)->forkCopy(paddr, childfrOH);
    passert(_SUCCESS(rc), err_printf("woops3\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	caddr, size, 0, childfrOH, 0,
	(uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);
    passert(_SUCCESS(rc), err_printf("woops4\n"));

    Obj::ReleaseAccess(childfrOH);

    // fork copy the child
    rc = DREFGOBJ(TheProcessRef)->forkCopy(caddr, childfrOH);
    passert(_SUCCESS(rc), err_printf("woops3a\n"));

    rc = StubRegionDefault::_CreateFixedLenExt(
	ccaddr, size, 0, childfrOH, 0,
	(uval)(AccessMode::writeUserWriteSup), 0,
	RegionType::K42Region);
    passert(_SUCCESS(rc), err_printf("woops4a\n"));

    Obj::ReleaseAccess(childfrOH);

    // page zero first on parent
    forkCheck(paddr, caddr, 0, 0xf0);
    // page one first on child
    forkCheck(caddr, paddr, 1, 0xf1);
    // page two first on parent
    forkCheck(paddr, caddr, 2, 0x0);
    // page three first on child
    forkCheck(caddr, paddr, 3, 0x0);
    // now check second child
    forkCheck(paddr, ccaddr, 6, 0xf6);
    forkCheck(ccaddr, paddr, 7, 0);

    rc = DREFGOBJ(TheProcessRef)->regionDestroy(paddr);
    passert(_SUCCESS(rc), err_printf("woops5\n"));

    rc = DREFGOBJ(TheProcessRef)->regionDestroy(caddr);
    passert(_SUCCESS(rc), err_printf("woops5a\n"));

    forkCheck(ccaddr, 0, 4, 0xf4);
    forkCheck(ccaddr, 0, 5, 0);

    rc = DREFGOBJ(TheProcessRef)->regionDestroy(ccaddr);
    passert(_SUCCESS(rc), err_printf("woops6\n"));

    err_printf("forkTest success\n");

    return 0;
}
