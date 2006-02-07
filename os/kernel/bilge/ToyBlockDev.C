/******************************************************************************
 * K42: (C) Copyright IBM Corp. 2000, 2001.
 * All Rights Reserved
 *
 * This file is distributed under the GNU LGPL. You should have
 * received a copy of the license along with K42; see the file LICENSE.html
 * in the top-level directory for more details.
 *
 * $Id: ToyBlockDev.C,v 1.23 2005/01/10 15:30:21 dilma Exp $
 *****************************************************************************/
/*****************************************************************************
 * Module Description: Implementation of block-device interface for toy disk
 * **************************************************************************/

#include "kernIncs.H"
#include <io/FileLinux.H>
#include <cobj/CObjRootSingleRep.H>
#include <time.h>

#include <io/FileLinuxServer.H>
#include <meta/MetaFileLinuxServer.H>
#include "ToyBlockDev.H"

#include <io/PAPageServer.H>
#include <meta/MetaPAPageServer.H>

#include <stub/StubFRPA.H>
#include "KernToyDisk.H"
#include "KernBogusDisk.H"
#include <stub/StubRegionFSComm.H>

void
ToyBlockDev::ClassInit()
{
    KernToyDisk::ClassInit();
    BlockDevBase::ClassInit();
}

/*virtual*/ SysStatus
ToyBlockDev::init(const char* name, int diskid)
{
    err_printf("In ToyBlockDev::init for disk %s, diskid %d\n",
	       name, diskid);

    uval sDiskID;
    if (KernelInfo::OnSim() == SIM_SIMOSPPC) {
	    switch (diskid) {
	    case 0:
		    sDiskID = _BootInfo->simosDisk0Number;
		    break;
	    case 1:
		    sDiskID = _BootInfo->simosDisk1Number;
		    break;
	    default:
		    sDiskID = 0; // get rid of warning about uninitialized use
		    passertMsg(0, "Invalid diskid argument %d\n",
			       diskid);
	    }
    } else {
	passertMsg(KernelInfo::OnSim() == SIM_MAMBO, "?");
	sDiskID = diskid;
    }

    SysStatus rc;
    if (KernelInfo::OnSim() == SIM_SIMOSPPC) {
	KernToyDisk *kd;
	rc = KernToyDisk::Create(kd, sDiskID);
	kdisk = kd;
    } else {
	passertMsg(KernelInfo::OnSim() == SIM_MAMBO, "?");
	KernBogusDisk *kd;
	rc = KernBogusDisk::Create(kd, sDiskID);
	kdisk = kd;
    }

    if (_FAILURE(rc)) {
	err_printf("Creation of KernToyDisk/KernBogusDisk for disk number "
		   "%ld failed\n", sDiskID);
	isValid = 0;
	return rc;
    } else {
	isValid = 1;
    }

    ObjectHandle dummy;
    ObjectHandle node;
    const int perm = S_IFBLK|S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
    dummy.init();

    // FIXME FIXME FIXME! Kludge to get polling timeouts better on mambo
    // The right fix would be to fix K42's initialization dependencies
    if (KernelInfo::OnSim() == SIM_MAMBO) {
	while (1) {
	    rc = BlockDevBase::init(name, makedev(222,0), perm, dummy, node);
	    if (_SUCCESS(rc)) break;
	    if (_FAILURE(rc) && _SGENCD(rc) != ENOENT) return rc;
	    // the device was not ready yet?
	    Scheduler::DelayMicrosecs(10000);
	}
    } else {
	rc = BlockDevBase::init(name, makedev(222,0), perm, dummy, node);
	_IF_FAILURE_RET(rc);
    }

    devSize = _SGETUVAL(kdisk->_getDevSize());
    err_printf("devSize is %ld\n", devSize);
    blkSize = _SGETUVAL(kdisk->_getBlockSize());
    err_printf("blkSize is %ld\n", blkSize);
    return 0;
}

/* virtual */ SysStatus
ToyBlockDev::putBlock(uval physAddr, uval len, uval objOffset)
{
    passertMsg(0,"Direct I/O to ToyDisk not supported\n");
    return 0;
}

/* virtual */ SysStatus
ToyBlockDev::getBlock(uval physAddr, uval len, uval objOffset)
{
    passertMsg(0,"Direct I/O to ToyDisk not supported\n");
    return 0;
}

/*virtual*/ SysStatus
ToyBlockDev::_write(__in uval srcAddr,
		    __in uval objOffset,
		    __in uval len,
		    __XHANDLE xhandle)
{
    if (!isValid) return _SERROR(2387, 0, 0);

    SysStatus rc;
    SysStatus rc2;
    ClientData *cd = clnt(xhandle);
    PinnedMapping pm;
    uval addr = 0;
    int ret = 0;

    rc = fixupAddrPreWrite(&pm, addr, srcAddr, len, cd);
    _IF_FAILURE_RET(rc);

    rc = kdisk->_writeVirtual(objOffset, (char*)addr, len);

    rc2 = fixupAddrPostWrite(&pm, addr, srcAddr, len, cd);

    if (ret<0) {
	rc =  _SERROR(1942, 0, -ret);
    } else {
	_IF_FAILURE_RET(rc2);
	rc = 0;
    }

    return rc;
}

/* virtual */ SysStatus
ToyBlockDev::_getBlock(__inout uval srcAddr, __inout uval len,
		       __in uval objOffset,__XHANDLE xhandle)
{

    if (!isValid) return _SERROR(2388, 0, 0);

    SysStatus rc, rc2;
    ClientData *cd = clnt(xhandle);
    PinnedMapping pm;
    int ret =0 ;
    uval addr;

    rc = fixupAddrPreRead(&pm, addr, srcAddr, len, cd);
    _IF_FAILURE_RET(rc);

    rc = kdisk->_readVirtual(objOffset, (char*)addr, len);

    rc2 = fixupAddrPostRead(&pm, addr, srcAddr, len, cd);

    if (_FAILURE(rc)) {
	rc =  _SERROR(2445, 0, -ret);
    } else {
	_IF_FAILURE_RET(rc2);
	rc = 0;
    }

    return rc;
}

/* virtual */ SysStatusUval
ToyBlockDev::_ioctl(__in uval req,
		      __inoutbuf(size:size:size) char* buf,
		      __inout uval &size)
{
    return _SERROR(1975, 0, EOPNOTSUPP);
}
