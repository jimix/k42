/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testKernelPinnedFR.C,v 1.2 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A test for FRKernelPinned memory regions
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <sys/systemAccess.H>
#include <stub/StubFRKernelPinned.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>

void
dump_buf(char *buf, unsigned int len)
{
    unsigned int i;

    for (i = 0; i < len; i++) {
	printf("%02x ", buf[i]);
	if (i % 16 == 15)
		printf("\n");
    }
    
    if (i % 16 != 0)
	printf("\n");

}

int
main(int argc, char **argv)
{
    NativeProcess();

    ObjectHandle frOH;
    uval kaddr;
    uval uaddr;
    char *buf;
    SysStatus rc;

    rc = StubFRKernelPinned::_Create(frOH, kaddr, 100);
    if (_FAILURE(rc)) {
	printf("FRKernelPinned::_Create() failed\n");
	return -1;
    }

    /* Create a Region to allow the process to reference the memory */
    rc = StubRegionDefault::_CreateFixedLenExt(uaddr, 100, 0, frOH, 0,
	    AccessMode::writeUserWriteSup, 0, RegionType::UseSame);

    if (_FAILURE(rc)) {
	printf("_CreateFixedLen() failed: 0x%016lx", rc);
	return -1;
    }
    
    printf("got address 0x%016lx -> 0x%016lx\n", uaddr, kaddr);

    buf = (char *)uaddr;

    *(buf + 99) = 3;

    dump_buf(buf, 100);

    return 0;
}
