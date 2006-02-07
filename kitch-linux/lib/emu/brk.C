/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: brk.C,v 1.26 2005/01/11 21:03:07 bob Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating brk() - change data segment size
 * **************************************************************************/
#include <sys/sysIncs.H>
#include <sys/BaseProcess.H>
#include <alloc/PageAllocatorUser.H>
#include <scheduler/Scheduler.H>
#include "linuxEmul.H"
#include <stub/StubRegionDefault.H>
#include <stub/StubFRComputation.H>
#include <mem/Access.H>
#include <usr/ProgExec.H>
#include <defines/paging.H>
#include <trace/traceMem.h>

#include <unistd.h>

/*
 * return current brk value on success -1 on failure.
 * This matches linux semantics; glibc maintains __curbrk.
 * See <glibc>/sysdeps/generic/sbrk.c for details.
 */

extern "C" sval __k42_linux_brk(void *newRegion);

static uval currBrk = 0;
static uval currRegionEnd = 0;
static uval currRegionStart = 0;
void resetBrk()
{
    currBrk = 0;
    currRegionEnd = 0;
    currRegionStart = 0;
}


sval
baseBrk(void *newRegion, uval for32=0) 
{
    enum {BIND_SIZE = 1024 * 1024 * 512};

    if (unlikely(currRegionEnd == 0)) {
	/* We need memory from the system. */
	uval size = BIND_SIZE;	/* Pretty big huh. (yup!) */
	SysStatus rc = -1;
	ObjectHandle frOH;
#ifdef LARGE_PAGES_BRK_SIZE
	const uval largePageSize = LARGE_PAGES_BRK_SIZE; // kludge
	uval largeBrkHeap = ProgExec::ExecXferInfo->largeBrkHeap;
	if (largeBrkHeap != 0) {
	    size = largeBrkHeap;
	    if (largeBrkHeap >= largePageSize) {
		rc = StubFRComputation::_CreateLargePage(frOH, largePageSize);
		if(_FAILURE(rc)) {
		    err_printf(
			"Failed to make large page FR for Brk Heap %lx\n", rc);
		}
	    }
	}
#endif
	if (_FAILURE(rc)) {
	    rc = StubFRComputation::_Create(frOH);
	}
	if (_SUCCESS(rc)) {
	    if (for32 == 0) {
		// bind anywhere for 64-bit program
#if 1
		if (largeBrkHeap == 0) {
#endif
		    rc = StubRegionDefault::_CreateFixedLenExt(
			currRegionStart, size, 0, frOH, 0,
			(uval)(AccessMode::writeUserWriteSup), 0, 
			RegionType::ForkCopy);
#if 1
		} else {
		    err_printf("FIXME implementing hack for large page brk\n");
		    err_printf("FIXME K42 returns an unsafe segment with hack\n");
		    currRegionStart = 0x17000000000;
		    rc = StubRegionDefault::_CreateFixedLenWithinRangeExt(
			currRegionStart, 0x18000000000, size, 0, frOH, 0,
			(uval)(AccessMode::writeUserWriteSup), 0, 
			RegionType::ForkCopy);
		}
#endif
	    } else {
		// bind in first 4Gig for 32-bit program
		rc = StubRegionDefault::_CreateFixedLenWithinRangeExt(
		    currRegionStart, 0x100000000, size, 0, frOH, 0,
		    (uval)(AccessMode::writeUserWriteSup), 0, 
		    RegionType::ForkCopy);
	    }
	}

	if (_FAILURE(rc)) {
	    return -ENOMEM;
	}

	TraceOSMemBrk((uval64)currRegionStart, (uval64)size);

	currBrk = currRegionStart;
	currRegionEnd = currRegionStart + size;
    }

    if (unlikely(currRegionStart > (uval)newRegion)) {
	/* we have been asked to assign the base. */
	return (sval)currBrk;
    }

    if (unlikely(currBrk > (uval)newRegion)) {
	if (currRegionStart > (uval)newRegion) {
	    newRegion = (void*)currRegionStart;
	}
	memset(newRegion, 0, currBrk - ((uval)newRegion));
	currBrk = (uval)newRegion;
	return (sval)currBrk;
    }

    if (unlikely((uval)newRegion > currRegionEnd)) {
	passertWrn(0, "Hit brk() memory limit\n");
	return -ENOMEM;
    }

    currBrk = (uval)newRegion;
    return (sval)newRegion;
}


sval
__k42_linux_brk(void *newRegion)
{
    sval ret;
    uval for32 = 0;
    SYSCALL_ENTER();
    ret = baseBrk(newRegion, for32);
    SYSCALL_EXIT();
    return ret;
}

extern "C" sval
__k42_linux_brk_32(void *newRegion) {
    sval ret;
    uval for32 = 1;
    SYSCALL_ENTER();
    ret = baseBrk(newRegion, for32);
    SYSCALL_EXIT();
    return ret;
}
