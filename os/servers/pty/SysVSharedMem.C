/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: SysVSharedMem.C,v 1.14 2004/12/20 20:24:47 marc Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Sys V Shared Memory
 * **************************************************************************/

#include <sys/sysIncs.H>
#include <usr/ProgExec.H>

#include "SysVSharedMem.H"
#include <stub/StubFR.H>
#include <stub/StubFRComputation.H>
#include <stub/StubRegionDefault.H>
#include <mem/Access.H>
#include <sync/atomic.h>
#include "meta/MetaSysVSharedMem.H"
#include <sys/ipc.h>
#include <sys/stat.h>
#include <defines/paging.H>

/* static */ SysVSharedMem *SysVSharedMem::obj;

/* static */ void
SysVSharedMem::ClassInit()
{
    MetaSysVSharedMem::init();
    obj = new SysVSharedMem();
    obj->shmID = 1;
    obj->lock.init();
}

/* static */ SysStatusUval
SysVSharedMem::MakeRegion(sval32 key, uval size, uval flags)
{
    SysStatus rc = 0;
    uval addr;
    ObjectHandle frOH;
    RegionInfo *ri;
    uval perm;
    uval accMode;


    if (flags & SHM_HUGETLB) {
	rc = StubFRComputation::_CreateLargePage(frOH, LARGE_PAGES_SIZE);
	size = ALIGN_UP(size, LARGE_PAGES_SIZE);
    } else {
	rc = StubFRComputation::_Create(frOH);
    }
    if (_FAILURE(rc)) return rc;

    perm = (flags & (S_IRWXU|S_IRWXG|S_IRWXO));
    if (!(perm & (S_IWUSR | S_IRUSR))) {
	tassertWrn(0,"Region perms are neither read or write.\n");
	perm |= S_IWUSR;
    }
    // FIXME: this is wrong
    if (perm & S_IWUSR) {
	accMode = AccessMode::writeUserWriteSup;
    } else {
	accMode = AccessMode::readUserReadSup;
    }

    // FIXME: The only reason to bind the region in the servers
    // address space is to "reserve" at least one region somewhere
    // since the shamt cannot fail due to absence of memory resources.

    /*
     * Note: We cannot assume that all processes have the reion mapped
     * at the same address, so keeping track of this by our address
     * value is stupid.
     */

    rc = StubRegionDefault::_CreateFixedLenExt(
	addr, size, 0, frOH, 0, accMode, 0, RegionType::K42Region);

    if (_FAILURE(rc)) {
	Obj::ReleaseAccess(frOH);
	return rc;
    }
    ri = new RegionInfo;
    if (ri == NULL) {
	Obj::ReleaseAccess(frOH);
	rc = DREFGOBJ(TheProcessRef)->regionDestroy(addr);
	tassert(_SUCCESS(rc), err_printf("region destroy @ 0x%lx: failed.\n",
					 addr));
	return _SERROR(2520, 0, ENOMEM);
    }
    ri->frOH = frOH;
    ri->addr = addr;
    // FIXME: need some sort of reuse policy!!
    ri->shmID = FetchAndAddSynced(&(obj->shmID), 1);
    tassert(ri->shmID > 0,
	    err_printf("Blew the max shmget(2) id values\n"));

    ri->shmInfo.shm_perm.__key	= key;
    ri->shmInfo.shm_perm.uid	= 0;	// uid of owner
    ri->shmInfo.shm_perm.gid	= 0;	// gid of owner
    ri->shmInfo.shm_perm.cuid	= 0;	// uid of creator
    ri->shmInfo.shm_perm.cgid	= 0;	// gid of creator
    ri->shmInfo.shm_perm.mode	= perm;	// perms
    ri->shmInfo.shm_perm.__seq	= ri->shmID; // FIXME: I THINK

    ri->shmInfo.shm_segsz	= size;	// size of segment (bytes)
    ri->shmInfo.shm_atime	= 0;	// last attach time
    ri->shmInfo.shm_dtime	= 0;	// last dettach time
    // Should be time now
    ri->shmInfo.shm_ctime	= 2;	// last change time by shmctl()
    ri->shmInfo.shm_cpid	= 1;	// pid of creator
    ri->shmInfo.shm_lpid	= 1;	// pid of last shm operation
    ri->shmInfo.shm_nattch	= 0;	// number of current attaches

    AutoLock<BLock> al(&obj->lock);
    if(key != IPC_PRIVATE) {
	obj->regionsByKey.add(key, ri);
    }
    obj->regionsByShmID.add(ri->shmID, ri);
    obj->regionsByAddr.add(addr, ri);

    return _SRETUVAL(ri->shmID);
}


/* static */ SysStatusUval
SysVSharedMem::ShmGet(sval32 key, uval size, uval flags)
{
    RegionInfo *ri = NULL;
    uval sid;

    // get this logic out of the way
    obj->lock.acquire();
    if ((key != IPC_PRIVATE) &&
	(obj->regionsByKey.find(key, ri))) {
	sid = ri->shmID;
    } else {
	sid = 0;
    }
    obj->lock.release();

    if (ri == NULL) {
	// Regions does not exist
	if ((key != IPC_PRIVATE) && !(flags & IPC_CREAT)) {
	    // We are NOT creating
	    return _SERROR(2048, 0, ENOENT);
	}

	// Make the Region
	return MakeRegion(key, size, flags);
    } else {
	if (flags & IPC_EXCL) {
	    // require exclusive region
	    return _SERROR(2049, 0, EEXIST);
	}
	return _SRETUVAL(sid);
    }
}


/* static */ SysStatusUval
SysVSharedMem::ShmAttach(sval32 id, uval flags, ObjectHandle &frOH,
			 __CALLER_PID pid)
{
    RegionInfo *ri;
    SysStatus rc;

    if (obj->regionsByShmID.find(id, ri) == 0) {
	return _SERROR(2521, 0, EINVAL);
    }

    StubFR tempFR(StubObj::UNINITIALIZED);
    tempFR.setOH(ri->frOH);
    rc = tempFR._giveAccess(frOH, pid);

    if (_SUCCESS(rc)) {
	return _SRETUVAL(ri->shmInfo.shm_segsz);
    }

    return rc;
}

/* static */ SysStatusUval
SysVSharedMem::ShmDetach(uval addr)
{
    RegionInfo *ri;
    AutoLock<BLock> al(&obj->lock);

    obj->regionsByAddr.find(addr, ri);

    // update ri's info
    return 0;
}

/* static */ SysStatus
SysVSharedMem::ShmControl(uval32 id, uval32 cmd, struct shmid_ds &buf)
{
    RegionInfo *ri = NULL;
    SysStatus rc = 0;
    AutoLock<BLock> al(&obj->lock);

    if (!obj->regionsByShmID.find(id, ri)) {
	return _SERROR(2053, 0, EINVAL);
    }

    switch (cmd) {
	case IPC_STAT:
	    memcpy(&buf, &ri->shmInfo, sizeof(buf));
	    break;
	case IPC_SET:
	    memcpy(&ri->shmInfo, &buf, sizeof(buf));
	    break;
	case IPC_RMID:
	    tassertWrn(0, "IPC_RMID of %d requested\n", id);
	    break;
	case SHM_LOCK:
	    tassertWrn(0, "SHM_LOCK of %d requested\n", id);
	    break;
	case SHM_UNLOCK:
	    tassertWrn(0, "SHM_UNLOCK of %d requested\n", id);
	    break;
	default:
	    rc = _SERROR(2052, 0, EINVAL);
	    break;
    }

    return rc;
}
