/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: testIOMapping.C,v 1.2 2005/06/28 19:48:46 rosnbrg Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: A test for FRKernelPinned memory regions
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <stdio.h>
#include <sys/systemAccess.H>
#include <stub/StubNetDev.H>
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
    uval kaddr = 0x80040000;
    uval uaddr;
    char *buf;
    SysStatus rc;

    rc = StubNetDev::_Create(frOH, kaddr, 0x40000);
    if (_FAILURE(rc)) {
	printf("FRKernelPinned::_Create() failed\n");
	return -1;
    }

    uval mode = AccessMode::writeUserWriteSup | AccessMode::io_uncached;
    /* Create a Region to allow the process to reference the memory */
    rc = StubRegionDefault::_CreateFixedLenExt(uaddr, 0x40000, 0, frOH, 0,
					       mode, 0, RegionType::UseSame);

    if (_FAILURE(rc)) {
	printf("_CreateFixedLen() failed: 0x%016lx", rc);
	return -1;
    }
    
    printf("got address 0x%016lx -> 0x%016lx\n", uaddr, kaddr);

    buf = (char *)uaddr;

    dump_buf(buf + 0x1000, 0x100);

    return 0;
}
