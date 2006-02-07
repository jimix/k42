/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: shmat.C,v 1.15 2005/05/02 20:09:46 butrico Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: code for simulating mmap() - map files or devices
 *     into memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include "linuxEmul.H"
#include <stub/StubFR.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>


// Real shmat returns an address, which can be confused with error condition.
// Return in, and add another parameter for shm address.  Let ipc() syscall
// gate and glibc sort out the mess.
// FIXME: the system call is being invoked directly, i.e., it's not coming
// from glibc (glibc/libc/sysdeps/unix/sysv/linux/shmat.c) or from ipc.C,
// so let's provide the "real shmat". We're also defining shmatByIPC to
// be invoked by ipc.C ...

#define shmat orig_shmat
#include <stub/StubSysVSharedMem.H>
#include <sys/shm.h>
#undef shmat


// Internal function called by both shmat and shmatByIPC.
sval
shmatInternal(int shmid, const void *shmaddr, int shmflg, void**raddr)
{
    ObjectHandle frOH;
    SysStatusUval rc;
    uval size;
    uval addr = (uval)shmaddr;
    int err = 0;

    rc =  StubSysVSharedMem::ShmAttach(shmid, shmflg, frOH);
    if (_FAILURE(rc)) {
	err = -_SGENCD(rc);
	goto out;
    }

    size = _SGETUVAL(rc);

    if (addr == 0) {
	/*
	 * <quote>
	 * system tries to find an unmapped region in the range 1 -
	 * 1.5G starting from the upper value and coming down from
	 * there.
	 */
	rc = StubRegionDefault::_CreateFixedLenExt(
	    addr, size, 0, frOH, 0, AccessMode::writeUserWriteSup, 0,
	    RegionType::UseSame);
    } else {
	{
            #include <misc/linkage.H>
	    err_printf("marc needs this traceback unless its from regress\n");
	    uval callchain[20], i;
	    GetCallChainSelf(0, callchain, 20);
	    for (i=0; i<20 && callchain[i]; i++) {
		err_printf("%lx ", callchain[i]);
		if ((i&3) == 3) err_printf("\n");
	    }
	    err_printf("\n");
	}
	rc = StubRegionDefault::_CreateFixedAddrLenExt(
	    addr, size, frOH, 0, AccessMode::writeUserWriteSup, 0,
	    RegionType::UseSame);
    }
    if (_FAILURE(rc)) {
	// FIXME: need to detach as well
	err = -_SGENCD(rc);
    }

    Obj::ReleaseAccess(frOH);
    *raddr = (void *)addr;

  out:
    return err;
}

int
shmatByIPC(int shmid, const void *shmaddr, int shmflg, void**raddr)
{
    uval ret;
    SYSCALL_ENTER();
    ret = shmatInternal(shmid, shmaddr, shmflg, raddr);
    SYSCALL_EXIT();
#if 0					// maa debugging
    err_printf("%s pid:0x%lx 0x%x 0x%lx 0x%x %p  ret= %lx\n", __func__,
	       DREFGOBJ(TheProcessRef)->getPID(),
	       shmid, uval(shmaddr), shmflg, raddr, uval(ret));
#endif
    return (int) ret;
}
